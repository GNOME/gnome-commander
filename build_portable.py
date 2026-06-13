#!/usr/bin/env python3
import argparse
import os
import re
import pathlib
import subprocess
import sys
import tempfile
import venv
import zipfile


def find_env(output: bytes, name: str) -> str:
    for match in re.finditer(rb'cargo:+rustc-env=(\w+)=(.*?)$', output, flags=re.M):
        if match.group(1).decode() == name:
            return match.group(2).decode()
    print(
        f'Unexpected: Did not find environment variable {name} in build script output',
        file=sys.stderr
    )
    sys.exit(1)


def find_binary(dir: str, name: str) -> str:
    while os.path.dirname(dir) != dir:
        dir = os.path.dirname(dir)
        path = os.path.join(dir, name)
        if os.path.exists(path):
            return path
    print(f'Unexpected: Could not find {name} binary', file=sys.stderr)
    sys.exit(1)


def zip_recursive(archive: zipfile.ZipFile, dir: str, prefix: str) -> None:
    for dirpath, _, filenames in os.walk(dir):
        for filename in filenames:
            path = os.path.join(dirpath, filename)
            archive.write(path, prefix + os.path.relpath(path, dir))


def locate_lib(name: str, binary_path: str) -> bytes | None:
    result = subprocess.run(
        ['ldd', binary_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    ).stdout
    for line in result.splitlines():
        match = re.search(rb'=>\s*(\S+)', line)
        if match and name.encode() in match.group(1):
            return match.group(1)
    return None


def run() -> None:
    parser = argparse.ArgumentParser(
        description='Create a portable build of the application.'
    )
    parser.add_argument('output', help='ZIP file to be created')
    args = parser.parse_args()

    repo_dir = os.path.dirname(__file__)

    pathlib.Path(repo_dir, 'gnome-commander',
                 'build.rs').touch()   # Force re-run
    output = subprocess.check_output(
        ['cargo', 'build', '-vv', '--release'],
        cwd=repo_dir,
        env=dict(
            os.environ,
            GLOBAL_LOCALE_DIR='../locale',
            GLOBAL_PLUGIN_DIR='../plugins',
            GLOBAL_SCHEMA_DIR='../settings',
            SETTINGS_KEYFILE='../settings/settings.ini',
        )
    )

    plugins_dir = os.path.join(repo_dir, 'plugins')
    locale_dir = find_env(output, 'LOCAL_LOCALE_DIR')
    schema_dir = find_env(output, 'LOCAL_SCHEMA_DIR')
    binary_path = find_binary(locale_dir, 'gnome-commander')

    with zipfile.ZipFile(args.output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.write(binary_path, 'bin/' + os.path.basename(binary_path))
        archive.write(os.path.join(schema_dir, 'gschemas.compiled'),
                      'settings/gschemas.compiled')
        for filename in os.listdir(plugins_dir):
            if not filename.endswith('.py') or filename == 'test.py':
                continue
            archive.write(os.path.join(plugins_dir, filename),
                          f'plugins/{filename}')
        with tempfile.TemporaryDirectory() as venv_dir:
            venv.EnvBuilder(with_pip=True).create(venv_dir)
            subprocess.check_call([
                os.path.join(venv_dir, 'bin', 'pip3'),
                'install', 'pypdf', 'exifread', 'mutagen'
            ])

            venv_lib_dir = None
            for name in os.listdir(os.path.join(venv_dir, 'lib')):
                if name.startswith('python'):
                    venv_lib_dir = os.path.join(
                        venv_dir, 'lib', name, 'site-packages')
                    break
            if venv_lib_dir:
                for name in os.listdir(venv_lib_dir):
                    path = os.path.join(venv_lib_dir, name)
                    if not os.path.isdir(path) or name == 'pip' or 'dist-info' in name:
                        continue
                    zip_recursive(archive, path, f'plugins/{name}/')
            else:
                print(
                    'Failed finding virtual environment\'s site-packages directory, packaging modules skipped',
                    file=sys.stderr
                )
        zip_recursive(archive, locale_dir, 'locale/')

        for lib in ['libvte-2.91-gtk4', 'libicuuc', 'libicudata']:
            path = locate_lib(lib, binary_path)
            if path:
                archive.write(path, 'lib/' + os.path.basename(path).decode())
            else:
                print(f'Failed to locate {lib}, packaging dependency skipped',
                      file=sys.stderr)
        archive.writestr(zipfile.ZipInfo.from_file(binary_path, os.path.basename(binary_path)), '''
#!/bin/sh
BASE_DIR=`dirname $0`
LD_LIBRARY_PATH=$BASE_DIR/lib $BASE_DIR/bin/gnome-commander
'''.strip())

        archive.writestr('README', '''
Using Gnome Commander portable builds
=====================================

After uncompressing this portable build you can run gnome-commander application
in the root directory without installing it. The application will create its
settings file under settings/settings.ini, system-wide configuration files are
not accessed.

System dependencies
-------------------

This build still depends on some system libraries (Gtk4 and related) which
should usually be present in any Linux desktop install. The libvte library is
often missing however, which is why it is packaged with this build. If however
Gnome Commander fails to start due to “missing” libraries where a different
version of the library is present on your system you should install
libvte-2.91-gtk4-0 package on your system and run bin/gnome-commander instead of
the wrapper in the root directory.
'''.strip())


if __name__ == '__main__':
    run()
