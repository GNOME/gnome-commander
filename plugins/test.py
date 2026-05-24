#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

from plugin_helper import Plugin


class TestPlugin(Plugin):
    DIALOGS = True

    async def startup(self):
        self.send_message('info', {
            'name': 'Test Plugin',
            'version': '1.0',
            'comments': 'This is an example plugin adding some menu items. Take a look at how it was implemented.',
            'copyright': 'Copyright © 2026 Wladimir Palant',
            'authors': ['Wladimir Palant'],
            'webpage': 'https://gnome.pages.gitlab.gnome.org/gnome-commander/',
        })
        self.send_message('ready')

    async def main_menu_items(self, data: dict) -> list[dict]:
        return [{
            'label': 'Test Plugin: Age Calculator',
            'action': 'age-calculator',
            'parameter': '',
        }]

    async def context_menu_items(self, state: dict) -> list[dict]:
        # Number of selected files should be a good starting point for the age?
        return [{
            'label': 'Test Plugin: Age Calculator',
            'action': 'age-calculator',
            'parameter': str(len(state['active_selected_files'])),
        }]

    async def menu_activated(self, data: dict) -> None:
        if data['action'] != 'age-calculator':
            return

        state = {
            'age': data['parameter'],
            'years': True,
            'months': False,
            'days': False,
        }

        while True:
            result = await self.show_dialog({
                'title': 'Age Calculator',
                'child': {
                    'type': 'VBox',
                    'children': [{
                        'type': 'Text',
                        'label': 'To run the age calculator please enter your age below and select the units.',
                    }, {
                        'type': 'Input',
                        'id': 'age',
                        'label': 'Your _age:',
                        'value': state['age'],
                    }, {
                        'type': 'Group',
                        'label': 'Units:',
                        'child': {
                            'type': 'RadioGroup',
                            'children': [{
                                'type': 'Checkbox',
                                'label': '_Years',
                                'id': 'years',
                                'checked': state['years'],
                            }, {
                                'type': 'Checkbox',
                                'label': '_Months',
                                'id': 'months',
                                'checked': state['months'],
                            }, {
                                'type': 'Checkbox',
                                'label': '_Days',
                                'id': 'days',
                                'checked': state['days'],
                            }]
                        },
                    }],
                },
                'buttons': [{
                    'id': 'cancel',
                    'label': '_Cancel',
                    'cancel': True,
                }, {
                    'id': 'accept',
                    'label': 'Ca_lculate Age',
                    'default': True,
                }],
            })
            if result is None:
                return

            button, state = result
            if button != 'accept':
                return

            try:
                age = int(state['age'])
                break
            except ValueError:
                await self.show_dialog({
                    'title': 'Invalid age',
                    'child': {
                        'type': 'Text',
                        'label': 'You’ve entered an invalid age, it has to be a number!',
                    },
                    'buttons': [{
                        'id': 'repeat',
                        'label': '_Try Again',
                        'cancel': True,
                        'default': True,
                    }],
                })

        if not state['days']:
            age *= 30
            if not state['months']:
                age *= 12

        await self.show_dialog({
            'title': 'Your Age',
            'child': {
                'type': 'Text',
                'label': f'Your age is:\n\n{age / 360:.1f} years\n{age / 30:.1f} months\n{age} days\n\nApproximately at least.',
            },
            'buttons': [{
                'id': 'ok',
                'label': '_Acknowledge',
                'cancel': True,
                'default': True,
            }],
        })


if __name__ == '__main__':
    TestPlugin().run_forever()
