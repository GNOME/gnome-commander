#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import os
import sys

try:
    import polib
except ImportError:
    print(
        'This script requires polib package to be installed.\nhttps://polib.readthedocs.io/en/latest/installation.html#installation',
        file=sys.stderr
    )
    sys.exit(1)


def process_translation(path):
    if not os.path.exists(path):
        print(
            f'Translation file {path} does not exist, skipping...',
            file=sys.stderr
        )
        return

    pofile = polib.pofile(path)
    removed = pofile.fuzzy_entries()
    if not removed:
        return

    for entry in removed:
        pofile.remove(entry)
    pofile.save(path)
    print(
        f'Removed {len(removed)} translation(s) from {path}'
    )


def run():
    parser = argparse.ArgumentParser(
        description='Remove fuzzy translation.'
    )
    parser.add_argument(metavar='lang.po', nargs='+', dest='translations',
                        help='The translation file to be cleaned up. Will be modified in place.')
    args = parser.parse_args()

    print(
        'WARNING: This is a destructive process, only to be used where many nonsensical fuzzy '
        'translations exist which cannot be repaired manually. Revert the changes if alternative '
        'approaches are possible.',
        file=sys.stderr
    )

    for path in args.translations:
        process_translation(path)


if __name__ == '__main__':
    run()
