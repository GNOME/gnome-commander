#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

from gettext import gettext as _
import re
from typing import Generator

from plugin_helper import Plugin


BLOCKLIST = {
    # No point showing image dimensions, image info is displayed already.
    'Exif.Image.ImageWidth', 'Exif.Image.ImageLength',
    'Exif.PanasonicRaw.ImageWidth', 'Exif.PanasonicRaw.ImageHeight',
    'Exif.CanonPi.ImageWidth', 'Exif.CanonPi.ImageHeight',
    'Exif.NikonPreview.ImageWidth', 'Exif.NikonPreview.ImageLength',
    'Exif.Olympus.ImageWidth', 'Exif.Olympus.ImageHeight',
    'Exif.Olympus2.ImageWidth', 'Exif.Olympus2.ImageHeight',
    'Exif.Panasonic.ImageWidth', 'Exif.Panasonic.ImageHeight',
    'Exif.SamsungPreview.ImageWidth', 'Exif.SamsungPreview.ImageLength',
    'Exif.Photo.PixelXDimension', 'Exif.Photo.PixelYDimension',
    'Xmp.tiff.ImageWidth', 'Xmp.tiff.ImageHeight', 'Xmp.tiff.ImageLength',
    'Xmp.exif.PixelXDimension', 'Xmp.exif.PixelYDimension',
    # Pointer, offset and byte count values are not useful.
    'Exif.Image.ExifTag', 'Exif.Image.GPSTag',
    'Exif.Image.StripOffsets', 'Exif.Image.StripByteCounts',
    'Exif.Image.TileOffsets', 'Exif.Image.TileByteCounts',
    'Exif.Image.SubIFDs',
    'Exif.Photo.InteroperabilityTag',
    'Exif.Nikon3.Preview', 'Exif.PentaxDng.PreviewOffset', 'Exif.Pentax.PreviewOffset',
    'Exif.Samsung2.Preview',
    'Exif.Olympus2.ThumbnailOffset', 'Exif.Olympus2.ThumbnailLength',
    'Exif.SonyMinolta.ThumbnailOffset', 'Exif.SonyMinolta.ThumbnailLength',
}


class ImageMetadataPlugin(Plugin):
    async def startup(self) -> None:
        self.send_message('info', {
            'name': 'Image Metadata',
            'version': '1.0',
            'comments': (
                _('A plugin for extracting metadata from images files.') +
                '\n\n' +
                _('Python3 package exiv2 has to be install on your system for this plugin to work.')
            ),
            'copyright': 'Copyright © 2026 Wladimir Palant',
            'authors': ['Wladimir Palant'],
            'webpage': 'https://gnome.pages.gitlab.gnome.org/gnome-commander/',
        })

        try:
            import exiv2
        except ImportError:
            self.fail(
                _('Required exiv2 module not found. Please install exiv2 for Python3 on your system.')
            )
        self.send_message('ready')

    async def extract_metadata(self, data: dict) -> list[dict]:
        import exiv2
        path: str = data['path']
        try:
            image = exiv2.ImageFactory.open(path)
            image.readMetadata()
        except exiv2.Exiv2Error:
            return []

        def get_label(tag) -> str:
            try:
                return tag.tagLabel()
            except exiv2.Exiv2Error:
                return tag.key()

        def get_description(tag) -> str:
            try:
                return tag.tagDesc()
            except exiv2.Exiv2Error:
                return ''

        sources = [
            (_("EXIF"), image.exifData()),
            (_("IPTC"), image.iptcData()),
            (_("XMP"), image.xmpData()),
        ]
        response = [
            {
                "class": cls,
                "tags": [
                    (
                        fixup_tag(tag.key()), get_label(tag),
                        get_description(tag), tag.value().toString()
                    )
                    for tag in data
                    if fixup_tag(tag.key()) not in BLOCKLIST
                ],
            }
            for (cls, data) in sources
        ]
        return response

    async def list_supported_tags(self, data: dict) -> list[tuple[str, list[tuple[str, str]]]]:
        sources = [
            (_("EXIF"), exif_tags()),
            (_("IPTC"), iptc_tags()),
            (_("XMP"), xmp_tags()),
        ]
        response = []
        for cls, tags in sources:
            map = {}
            for tag, name in tags:
                tag = fixup_tag(tag)
                if tag not in BLOCKLIST and tag not in map:
                    map[tag] = name
            response.append((cls, list(map.items())))
        return response


def fixup_tag(tag: str) -> str:
    tag = re.sub(r'^Exif\.(Sub)?Image\d+\.', 'Exif.Image.', tag)
    tag = re.sub(r'^Exif\.SubThumb\d+\.', 'Exif.Thumbnail.', tag)
    tag = re.sub(
        r'^Exif\.\w+\.(ExifTag|GPSTag|StripOffsets|StripByteCounts|TileOffsets|TileByteCounts|SubIFDs)$',
        r'Exif.Image.\1',
        tag
    )
    return tag


def exif_tags() -> Generator[tuple[str, str], None, None]:
    import exiv2
    for group in exiv2.tags.ExifTags.groupList():
        for tag in group.tagList():
            yield f'Exif.{group.groupName}.{tag.name}', tag.title


def iptc_tags() -> Generator[tuple[str, str], None, None]:
    import exiv2
    for record in exiv2.datasets.IptcDataSets.application2RecordList():
        yield f'Iptc.Application2.{record.name}', record.title


def xmp_tags() -> Generator[tuple[str, str], None, None]:
    import exiv2
    for prefix in exiv2.properties.XmpProperties.registeredNamespaces().keys():
        try:
            for property in exiv2.properties.XmpProperties.propertyList(prefix):
                yield f'Xmp.{prefix}.{property.name}', property.title
        except exiv2.Exiv2Error as e:
            pass


if __name__ == '__main__':
    ImageMetadataPlugin().run_forever()
