#!/usr/bin/env python3
import argparse
import os
import re
import pathlib
import subprocess
import sys
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


def run():
    parser = argparse.ArgumentParser(
        description='Create a portable build of the application.'
    )
    parser.add_argument('output', help='ZIP file to be created')
    args = parser.parse_args()

    repo_dir = os.path.dirname(__file__)

    pathlib.Path(repo_dir, 'build.rs').touch()   # Force re-run
    output = subprocess.check_output(
        ['cargo', 'build', '-vv', '--release'],
        cwd=repo_dir,
        env=dict(
            os.environ,
            GLOBAL_LOCALE_DIR='locale',
            GLOBAL_PLUGIN_DIR='plugins',
            GLOBAL_SCHEMA_DIR='settings',
            SETTINGS_KEYFILE='settings/settings.ini',
        )
    )

    plugins_dir = os.path.join(repo_dir, 'plugins')
    locale_dir = find_env(output, 'LOCAL_LOCALE_DIR')
    schema_dir = find_env(output, 'LOCAL_SCHEMA_DIR')
    binary_path = find_binary(locale_dir, 'gnome-commander')

    with zipfile.ZipFile(args.output, 'w') as archive:
        archive.write(binary_path, os.path.basename(binary_path))
        archive.write(os.path.join(schema_dir, 'gschemas.compiled'),
                      'settings/gschemas.compiled')
        for filename in os.listdir(plugins_dir):
            if not filename.endswith('.py') or filename == 'test.py':
                continue
            archive.write(os.path.join(plugins_dir, filename),
                          f'plugins/{filename}')
        for dirpath, _, filenames in os.walk(locale_dir):
            for filename in filenames:
                path = os.path.join(dirpath, filename)
                archive.write(path,
                              'locale/' + os.path.relpath(path, locale_dir))


if __name__ == '__main__':
    run()
