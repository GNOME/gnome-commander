#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import asyncio
from collections.abc import Callable
import gettext
from gettext import gettext as _
import json
import os
import struct
import sys
from typing import Any, Awaitable, Optional


class SettingDescriptor:
    def __init__(self, key: str):
        self.key = key

    async def __get__(self, instance: 'Plugin', owner) -> Any:
        return await instance.send_api_request_with_response('get-setting', self.key)

    def __set__(self, instance: 'Plugin', value: Any):
        instance.send_api_request('set-setting', (self.key, value))


class Plugin:
    def __init__(self) -> None:
        self._tasks: set[asyncio.Task] = set()
        self._incoming = bytearray()
        self._initialized = False
        self._max_request_id = 0
        self._pending_api_requests: dict[int, tuple[str, asyncio.Future]] = {}

        # Do not configure gettext if text domain isn't the default "messages"
        if (gettext.textdomain() == 'messages' and
                'GETTEXT_DOMAIN' in os.environ and 'GETTEXT_DIR' in os.environ):
            gettext.bindtextdomain(
                os.environ['GETTEXT_DOMAIN'], os.environ['GETTEXT_DIR']
            )
            gettext.textdomain(os.environ['GETTEXT_DOMAIN'])

        os.set_blocking(sys.stdin.buffer.fileno(), False)

        self._apis = []
        if len(self.PERSISTENT_SETTINGS) > 0:
            self._apis.append({
                'name': 'persistent_settings',
                'version': '1.0',
            })
            for key in self.PERSISTENT_SETTINGS:
                setattr(type(self), key, SettingDescriptor(key))

        if self.DIALOGS:
            self._apis.append({
                'name': 'dialogs',
                'version': '1.0',
            })

        if self.extract_metadata and self.list_supported_tags:
            self._apis.append({
                'name': 'extract_metadata',
                'version': '1.0',
            })

        if self.main_menu_items and self.context_menu_items and self.menu_activated:
            self._apis.append({
                'name': 'menus',
                'version': '1.0',
            })

    def run_forever(self):
        if len(sys.argv) > 1:
            asyncio.run(self.handle_command_line())
        else:
            loop = asyncio.new_event_loop()
            loop.add_reader(sys.stdin.buffer, self.handle_incoming, loop)
            loop.run_forever()

    async def handle_command_line(self) -> None:
        parser = argparse.ArgumentParser(
            description='This command line interface is provided for debugging purposes only.'
        )
        subparsers = parser.add_subparsers()

        if any(api['name'] == 'extract_metadata' for api in self._apis):
            subparser = subparsers.add_parser('extract-metadata')
            subparser.add_argument('path')
            subparser.add_argument('--uri', '-u')
            subparser.set_defaults(func=self.extract_metadata)

            subparser = subparsers.add_parser('list-supported-tags')
            subparser.set_defaults(func=self.list_supported_tags)

        if any(api['name'] == 'menus' for api in self._apis):
            subparser = subparsers.add_parser('main-menu-items')
            subparser.set_defaults(func=self.main_menu_items)

            subparser = subparsers.add_parser('context-menu-items')
            subparser.add_argument('--active-directory-path', '-p')
            subparser.add_argument('--active-directory-uri', '-u')
            subparser.add_argument('--active-focused-file', '-f')
            subparser.add_argument('--active-selected-files', '-s', nargs='*')
            subparser.add_argument('--inactive-directory-path', '-P')
            subparser.add_argument('--inactive-directory-uri', '-U')
            subparser.add_argument('--inactive-focused-file', '-F')
            subparser.add_argument('--inactive-selected-files', '-S', nargs='*')
            subparser.set_defaults(func=self.context_menu_items)

        os.set_blocking(sys.stdin.buffer.fileno(), True)
        args = parser.parse_args()
        json.dump(await args.func(args.__dict__), sys.stdout)
        sys.exit(0)

    def send_message(self, type: str, payload: Any = None) -> None:
        message = json.dumps({
            'type': type,
            'payload': payload,
        }).encode()
        sys.stdout.buffer.write(struct.pack('=I', len(message)) + message)
        sys.stdout.buffer.flush()

    def receive_bytes(self, size: int) -> None:
        if len(self._incoming) < size:
            try:
                incoming = sys.stdin.buffer.read(size - len(self._incoming))
                if incoming is None:
                    return
                if incoming == b'':  # EOF
                    sys.exit(0)
            except BlockingIOError:
                return
            self._incoming.extend(incoming)

    def receive_message(self) -> Optional[tuple[str, dict]]:
        self.receive_bytes(4)
        if len(self._incoming) < 4:
            return None

        size, = struct.unpack('=I', self._incoming[0:4])
        self.receive_bytes(4 + size)
        if len(self._incoming) < 4 + size:
            return None

        payload = self._incoming[4:4 + size]
        del self._incoming[0:4 + size]

        message = json.loads(payload)
        return message['type'], message['payload']

    def fail(self, error: str) -> None:
        self.send_message('error', error)
        self.send_message('failed')
        sys.exit(1)

    def send_api_request(self, name: str, data: Any = None) -> int:
        id = self._max_request_id
        self._max_request_id += 1
        self.send_message('api-request', [id, {name: data}])
        return id

    async def send_api_request_with_response(self, name: str, data: Any = None) -> Any:
        id = self.send_api_request(name, data)
        loop = asyncio.get_running_loop()
        future = loop.create_future()
        self._pending_api_requests[id] = (name, future)
        return await future

    async def handle_api_request(self, id: int, name: str, data: Any) -> None:
        method = name.replace('-', '_')
        if hasattr(self, method):
            response = await getattr(self, method)(data)
            if method != 'menu_activated':
                self.send_message('api-response', [id, {
                    name: response,
                }])

    def handle_incoming(self, loop: asyncio.AbstractEventLoop) -> None:
        while message := self.receive_message():
            type, payload = message

            if not self._initialized:
                if type != 'apis':
                    print(
                        f'Unexpected message "{type}" from application, expected "apis".',
                        file=sys.stderr
                    )
                    self.fail(_('Unsupported application protocol'))
                for api in self._apis:
                    if not any(available['name'] == api['name'] and available['version'] == api['version'] for available in payload):
                        self.fail(_('Unsupported application protocol'))
                    self.send_message('register', api)

                self._initialized = True

                task = loop.create_task(self.startup())
                self._tasks.add(task)
                task.add_done_callback(self._tasks.discard)
            elif type == 'api-request':
                id, request = payload
                if isinstance(request, dict):
                    name, data = next(iter(request.items()))
                else:
                    name = request
                    data = None

                task = loop.create_task(
                    self.handle_api_request(id, name, data)
                )
                self._tasks.add(task)
                task.add_done_callback(self._tasks.discard)
            elif type == 'api-response':
                id, response = payload
                if id in self._pending_api_requests:
                    name, future = self._pending_api_requests[id]
                    del self._pending_api_requests[id]
                    future.set_result(response[name])

    async def show_dialog(self, spec: dict) -> Optional[tuple[str, dict[str, Any]]]:
        return await self.send_api_request_with_response('show-dialog', spec)

    # Properties and methods that can be overwritten by subclasses
    PERSISTENT_SETTINGS: list[str] = []
    DIALOGS = False

    async def startup(self):
        raise NotImplementedError()

    extract_metadata: Optional[Callable[..., Awaitable[list[dict]]]] = None

    list_supported_tags: Optional[
        Callable[..., list[tuple[str, Awaitable[list[tuple[str, str]]]]]]
    ] = None

    main_menu_items: Optional[Callable[..., Awaitable[list[dict]]]] = None

    context_menu_items: Optional[Callable[..., Awaitable[list[dict]]]] = None

    menu_activated: Optional[Callable[..., Awaitable[None]]] = None
