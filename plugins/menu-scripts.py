#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

import asyncio
import functools
from gettext import gettext as _
import json
import os
import re
import shlex

from plugin_helper import Plugin


@functools.cache
def get_scripts_directory() -> str:
    try:
        import platformdirs
        return os.path.join(platformdirs.user_config_dir('gnome-commander'), 'scripts')
    except ImportError:
        if 'XDG_CONFIG_HOME' in os.environ:
            config_dir = os.environ['XDG_CONFIG_HOME']
        else:
            config_dir = os.path.expanduser('~/.config')
        return os.path.join(config_dir, 'gnome-commander', 'scripts')


def read_info(path: str) -> dict:
    info = {}
    with open(path, 'r') as input:
        for line in input:
            match = re.search(r'^\s*#\s*([\w\-]+)\s*:(.*)', line)
            if match:
                if match.group(1) == 'name':
                    info['name'] = match.group(2).strip()
                elif match.group(1) == 'term':
                    info['term'] = match.group(2).strip() == 'true'
                elif match.group(1) == 'single-file':
                    info['single-file'] = match.group(2).strip() == 'true'
    return info


class MenuScriptsPlugin(Plugin):
    COMMANDS = True

    async def startup(self):
        self.send_message('info', {
            'name': 'Menu Scripts',
            'version': '1.0',
            'comments': (
                _('A plugin adding context menu items for every script in {}.').replace('{}', get_scripts_directory()) +
                '\n\n' +
                _(
                    'Scripts can contain special comments. '
                    'The special comment #name:MyScript can be used to indicate the script name to '
                    'be displayed in the context menu. The special comment #term:true indicates '
                    'that the script should be run in the embedded terminal if possible and in the '
                    'terminal window otherwise. The special comment #single-file:true indicates '
                    'that the script should be run once for each selected file.'
                ) +
                '\n\n' +
                _(
                    'By default (unless single-file special comment is present) the script will '
                    'receive all selected files as command line arguments. Holding the Shift key '
                    'while clicking the menu item will force it to be run once for each file.'
                )
            ),
            'copyright': 'Copyright © 2026 Wladimir Palant',
            'authors': ['Wladimir Palant'],
            'webpage': 'https://gnome.pages.gitlab.gnome.org/gnome-commander/',
        })
        self.send_message('ready')

    async def main_menu_items(self, data: dict) -> list[dict]:
        return []

    async def context_menu_items(self, state: dict) -> list[dict]:
        if not state['active_directory_path'] or not state['active_selected_files']:
            return []

        dir = get_scripts_directory()
        if not os.path.isdir(dir):
            return []

        items = []
        for file in os.listdir(dir):
            path = os.path.join(dir, file)
            if not os.path.isfile(path) or not os.access(path, os.X_OK):
                continue
            info = read_info(path)
            items.append({
                'label': info.get('name') or file,
                'action': 'run-script',
                'parameter': json.dumps({
                    'path': path,
                    'term': info.get('term', False),
                    'single-file': info.get('single-file', False),
                }),
            })
        return items

    async def menu_activated(self, data: dict) -> None:
        state = data['state']
        parameter = json.loads(data['parameter'])
        modifiers = data['modifiers']

        dir = state['active_directory_path']
        if not dir:
            return

        files = state['active_selected_files']
        if not files:
            return

        single_file = parameter['single-file'] or (
            modifiers['shift'] and not modifiers['control'] and not modifiers['alt']
        )

        if not single_file or len(files) == 1:
            # We only need to run a single command
            command = (shlex.quote(parameter['path']) + ' ' +
                       ' '.join(shlex.quote(os.path.join(dir, file)) for file in files))
            await self.run_command(command, 'any' if parameter['term'] else 'background')
        else:
            # Multiple commands, try not to open a dozen terminal windows
            if parameter['term']:
                # If the embedded terminal is available try chaining commands there
                command = (shlex.quote(parameter['path']) + ' ' +
                           shlex.quote(os.path.join(dir, files[0])))
                result = await self.run_command(command, 'embedded')
                if len(result) == 0:
                    for file in files[1:]:
                        command = (shlex.quote(parameter['path']) + ' ' +
                                   shlex.quote(os.path.join(dir, file)))
                        while len(await self.run_command(command, 'embedded')):
                            await asyncio.sleep(0.1)
                    return
            # Either we run commands in background or the embedded terminal is busy
            for file in files:
                command = (shlex.quote(parameter['path']) + ' ' +
                           shlex.quote(os.path.join(dir, file)))
                await self.run_command(command, 'any' if parameter['term'] else 'background')


if __name__ == '__main__':
    MenuScriptsPlugin().run_forever()
