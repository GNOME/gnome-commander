#!/usr/bin/env python3
import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import venv
import zipfile


def find_env(output: bytes, name: str) -> str:
    for match in re.finditer(
        rb"cargo:+rustc-env=(\w+)=(.*?)$", output, flags=re.MULTILINE
    ):
        if match.group(1).decode() == name:
            return match.group(2).decode()
    print(
        f"Unexpected: Did not find environment variable {name} in build script output",
        file=sys.stderr,
    )
    sys.exit(1)


def find_binary(dir: str, name: str) -> str:
    while os.path.dirname(dir) != dir:
        dir = os.path.dirname(dir)
        path = os.path.join(dir, name)
        if os.path.exists(path):
            return path
    print(f"Unexpected: Could not find {name} binary", file=sys.stderr)
    sys.exit(1)


def zip_recursive(archive: zipfile.ZipFile, dir: str, prefix: str) -> None:
    for dirpath, _, filenames in os.walk(dir):
        for filename in filenames:
            path = os.path.join(dirpath, filename)
            archive.write(path, prefix + os.path.relpath(path, dir))


def locate_lib(name: str, binary_path: str) -> bytes | None:
    result = subprocess.run(
        ["ldd", binary_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    ).stdout
    for line in result.splitlines():
        match = re.search(rb"=>\s*(\S+)", line)
        if match and name.encode() in match.group(1):
            return match.group(1)
    return None


def build_docs(source: str, target: str, media_subdir: str) -> None:
    def source_file(language: str, *names) -> str:
        return os.path.join(source, language, *names)

    def target_file(language: str, *names) -> str:
        return os.path.join(target, language, *names)

    def source_media(language: str, *names) -> str:
        return source_file(language, media_subdir, *names)

    def target_media(language: str, *names) -> str:
        return target_file(language, media_subdir, *names)

    with open(os.path.join(source, "LINGUAS")) as input:
        languages = input.read().strip().splitlines()

    sources = []
    os.mkdir(target_file("C"))
    for name in os.listdir(source_file("C")):
        if os.path.isfile(source_file("C", name)):
            sources.append(name)
            shutil.copy2(source_file("C", name), target_file("C", name))

    media = []
    os.mkdir(target_media("C"))
    for name in os.listdir(source_media("C")):
        if os.path.isfile(source_media("C", name)):
            media.append(name)
            shutil.copy2(source_media("C", name), target_media("C", name))

    for language in languages:
        os.mkdir(target_file(language))
        subprocess.check_call(
            [
                "msgfmt",
                source_file(language, f"{language}.po"),
                "-o",
                target_file(language, f"{language}.mo"),
            ]
        )
        subprocess.check_call(
            [
                "itstool",
                "-m",
                target_file(language, f"{language}.mo"),
                "--lang",
                language,
                "-o",
                target_file(language),
                *(source_file("C", name) for name in sources),
            ]
        )
        os.unlink(target_file(language, f"{language}.mo"))

        os.mkdir(target_media(language))
        for name in media:
            if os.path.isfile(source_media(language, name)):
                shutil.copy2(source_media(language, name), target_media(language, name))
            else:
                # Python cannot write symlinks to ZIP archives
                shutil.copy2(source_media("C", name), target_media(language, name))


def run() -> None:
    parser = argparse.ArgumentParser(
        description="Create a portable build of the application."
    )
    parser.add_argument("output", help="ZIP file to be created")
    args = parser.parse_args()

    repo_dir = os.path.dirname(__file__)

    pathlib.Path(repo_dir, "gnome-commander", "build.rs").touch()  # Force re-run
    output = subprocess.check_output(
        ["cargo", "build", "-vv", "--release"],
        cwd=repo_dir,
        env=dict(
            os.environ,
            GLOBAL_LOCALE_DIR="../locale",
            GLOBAL_PLUGIN_DIR="../plugins",
            GLOBAL_SCHEMA_DIR="../settings",
            SETTINGS_KEYFILE="../settings/settings.ini",
            GLOBAL_HELP_DIR="../doc",
        ),
    )

    plugins_dir = os.path.join(repo_dir, "plugins")
    docs_source = os.path.join(repo_dir, "doc")
    locale_dir = find_env(output, "LOCAL_LOCALE_DIR")
    schema_dir = find_env(output, "LOCAL_SCHEMA_DIR")
    binary_path = find_binary(locale_dir, "gnome-commander")

    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.write(os.path.join(repo_dir, "PORTABLE"), "README")
        archive.write(binary_path, "bin/" + os.path.basename(binary_path))
        archive.write(
            os.path.join(schema_dir, "gschemas.compiled"), "settings/gschemas.compiled"
        )
        for filename in os.listdir(plugins_dir):
            if not filename.endswith(".py") or filename == "test.py":
                continue
            archive.write(os.path.join(plugins_dir, filename), f"plugins/{filename}")
        with tempfile.TemporaryDirectory() as venv_dir:
            venv.EnvBuilder(with_pip=True).create(venv_dir)
            subprocess.check_call(
                [
                    os.path.join(venv_dir, "bin", "pip3"),
                    "install",
                    "pypdf",
                    "exifread",
                    "mutagen",
                ]
            )

            venv_lib_dir = None
            for name in os.listdir(os.path.join(venv_dir, "lib")):
                if name.startswith("python"):
                    venv_lib_dir = os.path.join(venv_dir, "lib", name, "site-packages")
                    break
            if venv_lib_dir:
                for name in os.listdir(venv_lib_dir):
                    path = os.path.join(venv_lib_dir, name)
                    if not os.path.isdir(path) or name == "pip" or "dist-info" in name:
                        continue
                    zip_recursive(archive, path, f"plugins/{name}/")
            else:
                print(
                    "Failed finding virtual environment's site-packages directory, packaging modules skipped",
                    file=sys.stderr,
                )
        zip_recursive(archive, locale_dir, "locale/")

        with tempfile.TemporaryDirectory() as docs_output:
            build_docs(docs_source, docs_output, "figures")
            zip_recursive(archive, docs_output, "doc/")

        for lib in ["libvte-2.91-gtk4", "libicuuc", "libicudata"]:
            path = locate_lib(lib, binary_path)
            if path:
                archive.write(path, "lib/" + os.path.basename(path).decode())
            else:
                print(
                    f"Failed to locate {lib}, packaging dependency skipped",
                    file=sys.stderr,
                )
        archive.writestr(
            zipfile.ZipInfo.from_file(binary_path, os.path.basename(binary_path)),
            """
#!/bin/sh
BASE_DIR=`dirname $0`
LD_LIBRARY_PATH=$BASE_DIR/lib $BASE_DIR/bin/gnome-commander
""".strip(),
        )

        archive.write(
            os.path.join(repo_dir, "pixmaps", "gnome-commander.svg"),
            "gnome-commander.svg",
        )
        archive.writestr(
            "org.gnome.gnome-commander.desktop",
            """
[Desktop Entry]
# Change this to the real path of the portable build and copy the
# file into ~/.local/share/applications
Exec=/home/me/Downloads/gnome-commander-portable/gnome-commander
Name=Gnome Commander
Icon=gnome-commander
Terminal=false
Type=Application
StartupNotify=true
""".strip(),
        )


if __name__ == "__main__":
    run()
