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


def read_template(path):
    if not os.path.exists(path):
        print(
            f'Translation template {path} does not exist. Aborting...',
            file=sys.stderr
        )
        sys.exit(1)

    ids = {}
    template = polib.pofile(path)
    for entry in template:
        ids[entry.msgid] = entry
    return ids


def process_translation(path, template):
    if not os.path.exists(path):
        print(
            f'Translation file {path} does not exist, skipping...',
            file=sys.stderr
        )
        return

    removed = []
    modified = False
    pofile = polib.pofile(path)
    for entry in pofile:
        if entry.msgid not in template:
            removed.append(entry)
        elif entry.comment != template[entry.msgid].comment or entry.occurrences != template[entry.msgid].occurrences:
            entry.comment = template[entry.msgid].comment
            entry.occurrences = template[entry.msgid].occurrences
            modified = True
    if not removed and not modified:
        return

    for entry in removed:
        pofile.remove(entry)
    pofile.save(path)
    print(
        f'Removed {len(removed)} translation(s) from {path}'
    )


def run():
    parser = argparse.ArgumentParser(
        description='Remove translated strings which are no longer to be found in the translation template und update context information for the others.'
    )
    parser.add_argument(metavar='gnome-commander.main.pot', dest='template',
                        help='The translation template containing currently existing strings')
    parser.add_argument(metavar='lang.po', nargs='+', dest='translations',
                        help='The translation file to be cleaned up. Will be modified in place.')
    args = parser.parse_args()

    template = read_template(args.template)
    for path in args.translations:
        process_translation(path, template)


if __name__ == '__main__':
    run()
