#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
from collections.abc import Callable
import gettext
from gettext import gettext as _
import json
import os
import struct
import sys
from typing import Any, Optional


class Plugin:
    def __init__(self):
        locale_dir = os.path.normpath(
            os.path.join(os.path.dirname(__file__), '..',
                         '..', '..', 'share', 'locale')
        )
        if os.path.exists(locale_dir):
            gettext.bindtextdomain('gnome-commander', locale_dir)
        gettext.textdomain('gnome-commander')

        apis = []
        if self.extract_metadata and self.list_supported_tags:
            apis.append({
                'name': 'extract_metadata',
                'version': '1.0',
            })

        if len(sys.argv) > 1:
            self.handle_command_line(apis)

        type, payload = self.receive_message()
        if type != 'apis':
            print(
                f'Unexpected message "{type}" from application, expected "apis".',
                file=sys.stderr
            )
            self.fail(_('Unsupported application protocol'))
        for api in apis:
            if not any(available['name'] == api['name'] and available['version'] == api['version'] for available in payload):
                self.fail(_('Unsupported application protocol'))
            self.send_message('register', api)

        self.startup()

    def handle_command_line(self, apis: list[dict]):
        parser = argparse.ArgumentParser(
            description='This command line interface is provided for debugging purposes only.'
        )
        subparsers = parser.add_subparsers()

        if any(api['name'] == 'extract_metadata' for api in apis):
            subparser = subparsers.add_parser('extract-metadata')
            subparser.add_argument('path')
            subparser.add_argument('--uri', '-u')
            subparser.set_defaults(func=self.extract_metadata)

            subparser = subparsers.add_parser('list-supported-tags')
            subparser.set_defaults(func=self.list_supported_tags)

        args = parser.parse_args()
        json.dump(args.func(args.__dict__), sys.stdout)
        sys.exit(0)

    def startup(self):
        raise NotImplementedError()

    def send_message(self, type: str, payload: Any = None):
        message = json.dumps({
            'type': type,
            'payload': payload,
        }).encode()
        sys.stdout.buffer.write(struct.pack('=I', len(message)) + message)
        sys.stdout.buffer.flush()

    def receive_bytes(self, size: int) -> bytes:
        result = b''
        while len(result) < size:
            incoming = sys.stdin.buffer.read(size - len(result))
            if not len(incoming):
                sys.exit(0)
            result += incoming
        return result

    def receive_message(self) -> tuple[str, dict]:
        size, = struct.unpack('=I', self.receive_bytes(4))
        message = json.loads(self.receive_bytes(size))
        return message['type'], message['payload']

    def fail(self, error: str):
        self.send_message('error', error)
        self.send_message('failed')
        sys.exit(1)

    def process_incoming(self):
        while True:
            type, payload = self.receive_message()
            if type == 'api-request':
                id, request = payload
                if isinstance(request, dict):
                    name, data = next(iter(request.items()))
                else:
                    name = request
                    data = None
                method = name.replace('-', '_')
                if hasattr(self, method):
                    response = getattr(self, method)(data)
                    self.send_message('api-response', [id, {
                        name: response,
                    }])

    extract_metadata: Optional[Callable[..., list[dict]]] = None
    list_supported_tags: Optional[
        Callable[..., list[tuple[str, list[tuple[str, str]]]]]
    ] = None
