#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

import bz2
import datetime
from gettext import gettext as _
import gzip
import lzma
import os
import shutil
import subprocess
import tarfile
import time
import traceback
from typing import Any, Generator, Optional
import zipfile

from plugin_helper import Plugin

try:
    from compression import zstd
    has_zstd = True
except ImportError:
    has_zstd = False


def resolve_directories(parent: str, files: list[str]) -> Generator[str, None, None]:
    def inner(file: str) -> Generator[str, None, None]:
        path = os.path.join(parent, file)
        if os.path.isdir(path):
            for name in os.listdir(path):
                yield from inner(os.path.join(file, name))
        else:
            yield file

    for file in files:
        yield from inner(file)


def sanitize_path(parent: str, path: str) -> str:
    # Sanitize path similarly to how ZipFile.extract() does it.
    if os.path.sep != '/':
        path = path.replace('/', os.path.sep)
    if os.path.altsep and os.path.altsep != '/':
        path = path.replace(os.path.altsep, os.path.sep)
    _, _, path = os.path.splitroot(path)
    invalid_parts = ('', os.path.curdir, os.path.pardir)
    path = os.path.sep.join(part for part in path.split(os.path.sep)
                            if part not in invalid_parts)

    parent = os.path.realpath(parent)
    path = os.path.realpath(os.path.join(parent, path))
    if os.path.commonpath([parent, path]) != parent:
        raise ValueError(
            f'File path {path} would be extracted outside destination directory'
        )
    return path


class CompressionFormat:
    MULTI_FILE = False
    CAN_APPEND = False
    FILE_EXTENSIONS: list[str] = []

    def can_uncompress(self, path: str) -> bool:
        return False

    def compress(self, source_dir: str, files: list[str], destination: str, file_extension: str):
        raise NotImplementedError()

    async def uncompress(self, file: str, destination: str, plugin: 'ArchivesPlugin'):
        raise NotImplementedError()


class ZipArchives(CompressionFormat):
    MULTI_FILE = True
    CAN_APPEND = True
    FILE_EXTENSIONS = ['zip']

    def can_uncompress(self, path):
        try:
            with zipfile.ZipFile(path) as _:
                return True
        except:
            return False

    def compress(self, source_dir, files, destination, file_extension):
        with zipfile.ZipFile(destination, mode='a', compression=zipfile.ZIP_DEFLATED) as archive:
            for file in files:
                archive.write(os.path.join(source_dir, file), arcname=file)

    async def uncompress(self, path, destination, plugin):
        overwrite_mode = None
        with zipfile.ZipFile(path, mode='r') as archive:
            for member in archive.infolist():
                filepath = sanitize_path(destination, member.filename)
                if not filepath and not member.is_dir():
                    print(
                        f'Skipping archive entry "{member.filename}", empty file name.'
                    )
                    continue
                filepath = os.path.normpath(
                    os.path.join(destination, filepath))

                if member.is_dir():
                    os.makedirs(filepath, exist_ok=True)
                else:
                    if os.path.exists(filepath):
                        if overwrite_mode:
                            mode = overwrite_mode
                        else:
                            mode, apply_all = await plugin.query_overwrite(filepath)
                            if mode == 'cancel':
                                return
                            if apply_all:
                                overwrite_mode = mode
                        if mode != 'overwrite':
                            continue
                    os.makedirs(os.path.dirname(filepath), exist_ok=True)
                    with archive.open(member) as source, open(filepath, 'wb') as target:
                        shutil.copyfileobj(source, target)

                timestamp = datetime.datetime(*member.date_time).timestamp()
                try:
                    os.utime(filepath, (time.time(), timestamp))
                except:
                    pass

                if member.create_system == 3:  # UNIX
                    permissions = member.external_attr >> 16
                    if permissions & 0o777:
                        try:
                            os.chmod(filepath, permissions & 0o777)
                        except:
                            pass


class TarArchives(CompressionFormat):
    MULTI_FILE = True
    CAN_APPEND = False
    FILE_EXTENSIONS = ['tar', 'tar.gz', 'tar.bz2', 'tar.xz',
                       *(['tar.zst'] if has_zstd else [])]

    def can_uncompress(self, path):
        try:
            return tarfile.is_tarfile(path)
        except:
            return False

    def compress(self, source_dir, files, destination, file_extension):
        mode = {
            'tar.gz': 'w:gz',
            'tar.bz2': 'w:bz2',
            'tar.xz': 'w:xz',
            'tar.zst': 'w:zst',
        }.get(file_extension, 'w:')
        with tarfile.open(destination, mode=mode) as archive:
            for file in files:
                archive.add(os.path.join(source_dir, file), file)

    async def uncompress(self, file, destination, plugin):
        overwrite_mode = None
        with tarfile.open(file, mode='r') as archive:
            for member in archive.getmembers():
                member = tarfile.tar_filter(member, destination)
                filepath = os.path.join(destination, member.name)
                if not member.isdir() and os.path.exists(filepath):
                    if overwrite_mode:
                        mode = overwrite_mode
                    else:
                        mode, apply_all = await plugin.query_overwrite(filepath)
                        if mode == 'cancel':
                            return
                        if apply_all:
                            overwrite_mode = mode
                    if mode != 'overwrite':
                        continue
                archive.extract(member, destination, filter='tar')


class SevenZipArchives(CompressionFormat):
    MULTI_FILE = True
    CAN_APPEND = True

    def __init__(self):
        self.command_path = shutil.which('7z') or shutil.which('7za')
        self.FILE_EXTENSIONS = [] if self.command_path is None else ['7z']

    def can_uncompress(self, path):
        if not self.command_path:
            return False

        try:
            subprocess.check_output(
                [self.command_path, 'l', path],
                stderr=subprocess.DEVNULL
            )
            return True
        except:
            return False

    def compress(self, source_dir, files, destination, file_extension):
        proc = subprocess.Popen(
            [self.command_path, 'u', '-t7z', '-sae', '-aoa', destination, *files],
            cwd=source_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        (_, stderr) = proc.communicate()
        if proc.returncode != 0:
            raise Exception(stderr.decode(errors='replace').strip())

    async def uncompress(self, file, destination, plugin):
        proc = subprocess.Popen(
            [self.command_path, 'x', '-aou', file],
            cwd=destination, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        (_, stderr) = proc.communicate()
        if proc.returncode != 0:
            raise Exception(stderr.decode(errors='replace').strip())


class SingleFileCompressionFormat(CompressionFormat):
    def create_archive(self, path: str, mode: str) -> Any:
        raise NotImplementedError()

    def can_uncompress(self, path):
        try:
            self.create_archive(path, 'r').peek(16)
            return True
        except:
            return False

    def compress(self, source_dir, files, destination, file_extension):
        with open(os.path.join(source_dir, files[0]), 'rb') as input:
            with self.create_archive(destination, mode='wb') as archive:
                shutil.copyfileobj(input, archive)

    async def uncompress(self, file, destination, plugin):
        if os.path.exists(destination):
            if await plugin.query_overwrite_one(destination) != 'overwrite':
                return
        with open(destination, 'wb') as output:
            with self.create_archive(file, mode='rb') as archive:
                shutil.copyfileobj(archive, output)


class GzipArchives(SingleFileCompressionFormat):
    FILE_EXTENSIONS = ['gz']

    def create_archive(self, path, mode):
        return gzip.GzipFile(path, mode=mode)


class Bzip2Archives(SingleFileCompressionFormat):
    FILE_EXTENSIONS = ['bz2']

    def create_archive(self, path, mode):
        return bz2.BZ2File(path, mode=mode)


class LzmaArchives(SingleFileCompressionFormat):
    FILE_EXTENSIONS = ['xz']

    def create_archive(self, path, mode):
        return lzma.open(path, mode=mode)


class ZstdArchives(SingleFileCompressionFormat):
    FILE_EXTENSIONS = ['zst']

    def create_archive(self, path, mode):
        return zstd.open(path, mode=mode)


class ArchivesPlugin(Plugin):
    PERSISTENT_SETTINGS = ['compression_format']
    DIALOGS = True
    FORMATS = [
        ZipArchives(), TarArchives(), GzipArchives(), Bzip2Archives(), LzmaArchives(),
        *([ZstdArchives()] if has_zstd else []), SevenZipArchives(),
    ]

    async def startup(self):
        self.send_message('info', {
            'name': 'Archives',
            'version': '1.0',
            'comments': _('A plugin adding context menu items to create and extract compressed archives.'),
            'copyright': 'Copyright © 2026 Wladimir Palant',
            'authors': ['Wladimir Palant'],
            'webpage': 'https://gnome.pages.gitlab.gnome.org/gnome-commander/',
        })
        self.send_message('ready')

    @classmethod
    def can_compress(cls, source_dir: str, files: list[str]) -> list[CompressionFormat]:
        if not files:
            return []

        multiple = len(files) > 1 or os.path.isdir(
            os.path.join(source_dir, files[0]))
        return list(filter(lambda format: not multiple or format.MULTI_FILE, cls.FORMATS))

    @classmethod
    def can_uncompress(cls, path: str) -> Optional[CompressionFormat]:
        for format in cls.FORMATS:
            if format.can_uncompress(path):
                return format
        return None

    async def compress(self, source_dir: str | None, files: list[str], target_dir: str | None) -> None:
        if source_dir is None:
            return
        formats = self.can_compress(source_dir, files)
        if not formats:
            return

        file_extensions = [
            ext for format in formats for ext in format.FILE_EXTENSIONS]
        compression_format = await self.compression_format
        if compression_format not in file_extensions:
            compression_format = file_extensions[0]

        target_path = os.path.join(
            target_dir or source_dir, f'archive.{compression_format}')
        result = await self.show_dialog({
            'title': _('Create Archive'),
            'modal': True,
            'child': {
                'type': 'VBox',
                'children': [{
                    'type': 'Input',
                    'id': 'target',
                    'label': _('Co_mpress selected files to:'),
                    'value': target_path,
                    'vertical': True,
                }, {
                    'type': 'Group',
                    'label': _('Compression format:'),
                    'child': {
                        'type': 'RadioGroup',
                        'vertical': True,
                        'children': [{
                            'type': 'Checkbox',
                            'id': ext,
                            'label': ext,
                            'checked': ext == compression_format,
                        } for ext in file_extensions]
                    }
                }],
            },
            'buttons': [{
                'label': _('_Cancel'),
                'id': 'cancel',
                'cancel': True,
            }, {
                'label': _('C_reate'),
                'id': 'accept',
                'default': True,
            }]
        })
        if result is None or result[0] == 'cancel':
            return

        target_path = result[1]['target']
        for ext in file_extensions:
            if result[1][ext]:
                compression_format = ext
                break
        self.compression_format = compression_format

        if os.path.isdir(target_path):
            target_path = os.path.join(
                target_path,  f'archive.{compression_format}')

        for f in formats:
            if compression_format in f.FILE_EXTENSIONS:
                format = f
                break

        if os.path.exists(target_path) and not format.CAN_APPEND:
            if await self.query_overwrite_one(target_path) != 'overwrite':
                return

        try:
            format.compress(
                source_dir,
                list(resolve_directories(source_dir, files)),
                target_path, compression_format,
            )
        except Exception as e:
            traceback.print_exc()
            await self.show_dialog({
                'title': _('Archive creation failed'),
                'modal': True,
                'child': {
                    'type': 'VBox',
                    'children': [{
                        'type': 'Text',
                        'label': _('Creating "{}" failed.').replace('{}', target_path),
                    }, {
                        'type': 'Text',
                        'label': str(e),
                    }],
                },
                'buttons': [{
                    'label': _('_Dismiss'),
                    'id': 'dismiss',
                    'cancel': True,
                    'default': True,
                }],
            })

    async def uncompress(self, source_dir: str | None, file: str | None, target_dir: str | None) -> None:
        if source_dir is None or file is None:
            return
        source = os.path.join(source_dir, file)
        format = self.can_uncompress(source)
        if not format:
            return

        target_path = target_dir or source_dir
        if not format.MULTI_FILE:
            basename, ext = os.path.splitext(file)
            target_path = os.path.join(
                target_path, basename if ext else 'unpacked'
            )
        result = await self.show_dialog({
            'title': _('Extract Archive'),
            'modal': True,
            'child': {
                'type': 'VBox',
                'children': [{
                    'type': 'Input',
                    'id': 'target_path',
                    'label': _('Ex_tract {} to:').replace('{}', file),
                    'value': target_path,
                    'vertical': True,
                }],
            },
            'buttons': [{
                'label': '_Cancel',
                'id': 'cancel',
                'cancel': True,
            }, {
                'label': '_Extract',
                'id': 'accept',
                'default': True,
            }]
        })
        if result is None or result[0] != 'accept':
            return

        target_path = os.path.normpath(
            os.path.join(source_dir, result[1]['target_path'])
        )
        try:
            await format.uncompress(source, target_path, self)
        except Exception as e:
            traceback.print_exc()
            await self.show_dialog({
                'title': _('Extracting failed'),
                'modal': True,
                'child': {
                    'type': 'VBox',
                    'children': [{
                        'type': 'Text',
                        'label': _('Extracting "{}" failed.').replace('{}', file),
                    }, {
                        'type': 'Text',
                        'label': str(e),
                    }],
                },
                'buttons': [{
                    'label': _('_Dismiss'),
                    'id': 'dismiss',
                    'cancel': True,
                    'default': True,
                }],
            })

    async def query_overwrite_one(self, path: str) -> str:
        result = await self.show_dialog({
            'title': 'File exists',
            'modal': True,
            'child': {
                'type': 'Text',
                'label': _('Destination file "{}" already exists. Overwrite?').replace('{}', path),
            },
            'buttons': [{
                'label': '_Cancel',
                'id': 'cancel',
                'cancel': True,
            }, {
                'label': '_Overwrite',
                'id': 'overwrite',
            }],
        })
        if not result:
            return 'cancel'
        else:
            return result[0]

    async def query_overwrite(self, path: str) -> tuple[str, bool]:
        result = await self.show_dialog({
            'title': 'File exists',
            'modal': True,
            'child': {
                'type': 'VBox',
                'children': [{
                    'type': 'Text',
                    'label': _('Destination file "{}" already exists. Overwrite?').replace('{}', path),
                }, {
                    'type': 'Checkbox',
                    'id': 'apply_all',
                    'label': _('Apply to _all'),
                }],
            },
            'buttons': [{
                'label': '_Cancel',
                'id': 'cancel',
                'cancel': True,
            }, {
                'label': '_Overwrite',
                'id': 'overwrite',
            }, {
                'label': '_Skip',
                'id': 'skip',
            }],
        })
        if not result:
            return ('cancel', True)
        else:
            return (result[0], result[1]['apply_all'])

    @classmethod
    def compress_item(cls) -> dict:
        return {
            'label': _('Create Archive…'),
            'action': 'compress',
        }

    @classmethod
    def uncompress_item(cls) -> dict:
        return {
            'label': _('Extract Archive…'),
            'action': 'uncompress',
        }

    async def main_menu_items(self, data: dict) -> list[dict]:
        return [
            self.compress_item(),
            self.uncompress_item(),
        ]

    async def context_menu_items(self, state: dict) -> list[dict]:
        dir = state['active_directory_path']
        selected = state['active_selected_files']
        file = state['active_focused_file']

        items = []
        if dir is not None and self.can_compress(dir, selected):
            items.append(self.compress_item())
        if dir is not None and file is not None and self.can_uncompress(os.path.join(dir, file)):
            items.append(self.uncompress_item())
        return items

    async def menu_activated(self, data: dict) -> None:
        action = data['action']
        state = data['state']
        if action == 'compress':
            await self.compress(
                state['active_directory_path'],
                state['active_selected_files'],
                state['inactive_directory_path']
            )
        elif action == 'uncompress':
            await self.uncompress(
                state['active_directory_path'],
                state['active_focused_file'],
                state['inactive_directory_path'],
            )


if __name__ == '__main__':
    ArchivesPlugin().run_forever()
