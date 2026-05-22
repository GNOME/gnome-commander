#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

from gettext import gettext as _
import re
from typing import Any, Generator
from xml.dom import minidom
import zipfile

from plugin_helper import Plugin


def N_(text: str) -> str:
    return text


def resolve_aliases(tags: dict[str, tuple[str, str, str] | str]) -> dict[str, tuple[str, str, str]]:
    result = {}
    for key, value in tags.items():
        if isinstance(value, str):
            new_value = tags[value]
            if isinstance(new_value, str):
                raise AssertionError('Bad reference in TAGS table')
            result[key] = new_value
        else:
            result[key] = value
    return result


NAMESPACES = {
    'dc': 'http://purl.org/dc/elements/1.1/',
    'meta': 'urn:oasis:names:tc:opendocument:xmlns:meta:1.0',
    'office': 'urn:oasis:names:tc:opendocument:xmlns:office:1.0',
    'xlink': 'http://www.w3.org/1999/xlink',
    'ep': 'http://schemas.openxmlformats.org/officeDocument/2006/extended-properties',
    'cp': 'http://schemas.openxmlformats.org/package/2006/metadata/core-properties',
    'dcterms': 'http://purl.org/dc/terms/',
}

TAGS = resolve_aliases({
    # https://docs.oasis-open.org/office/OpenDocument/v1.4/os/part3-schema/OpenDocument-v1.4-os-part3-schema.html#a_3_2__office_meta_
    'dc:date': ('Doc.DateModified', N_('Date Modified'), N_('Date and time when the document was last modified')),
    'dc:description': ('Doc.Description', N_('Description'), N_('Description of a document')),
    'dc:language': ('Doc.Language', N_('Language'), N_('Default language of a document')),
    'dc:subject': ('Doc.Subject', N_('Subject'), N_('Subject of a document')),
    'dc:title': ('Doc.Title', N_('Title'), N_('Title of a document')),
    'meta:auto-reload': ('Doc.AutoReload', N_('Auto Reload'), N_('Delay after which the document is reloaded or replaced by another document')),
    'meta:creation-date': ('Doc.DateCreated', N_('Date Created'), N_('Date and time when a document was created')),
    'meta:editing-cycles': ('Doc.RevisionCount', N_('Revision Count'), N_('Number of times a document has been edited')),
    'meta:editing-duration': ('Doc.EditingDuration', N_('Editing Duration'), N_('Total time spent editing a document')),
    'meta:generator': ('Doc.Generator', N_('Generator'), N_('Identifies the document producer that was used to create or last modify the document')),
    'meta:initial-creator': ('Doc.InitialCreator', N_('Initial Creator'), N_('Name of the initial creator of a document')),
    'meta:keyword': ('Doc.Keyword', N_('Keyword'), N_('Keyword pertaining to a document')),
    'meta:print-date': ('Doc.PrintDate', N_('Print Date'), N_('Date and time when a document was last printed')),
    'meta:printed-by': ('Doc.PrintedBy', N_('Printed By'), N_('Name of the last person who printed a document')),
    'meta:template': ('Doc.Template', N_('Template'), N_('Document template that was used to create a document')),

    # https://docs.oasis-open.org/office/OpenDocument/v1.4/os/part3-schema/OpenDocument-v1.4-os-part3-schema.html#element-meta_document-statistic
    'meta:cell-count': ('Doc.CellCount', N_('Cell Count'), N_('Number of table cells that a document producer has counted in a document')),
    'meta:character-count': ('Doc.CharacterCount', N_('Character Count'), N_('Number of characters that a document producer has counted in a document')),
    'meta:draw-count': ('Doc.DrawCount', N_('Draw Count'), N_('Number of drawing shapes that a document producer has counted in a document')),
    'meta:frame-count': ('Doc.FrameCount', N_('Frame Count'), N_('Number of text boxes that a document producer has counted in a document')),
    'meta:image-count': ('Doc.ImageCount', N_('Image Count'), N_('Number of images that a document producer has counted in a document')),
    'meta:non-whitespace-character-count': ('Doc.NonWhitespaceCharacterCount', N_('Non-Whitespace Character Count'), N_('Number of non-white space characters that a document producer has counted in a document')),
    'meta:object-count': ('Doc.ObjectCount', N_('Object Count'), N_('Number of embedded objects stored in OpenDocument format that a document producer has counted in a document')),
    'meta:ole-object-count': ('Doc.OLEObjectCount', N_('OLE Object Count'), N_('Number of embedded objects stored in a different format than OpenDocument that a document producer has counted in a document')),
    'meta:page-count': ('Doc.PageCount', N_('Page Count'), N_('Number of pages that a document producer has calculated for a document')),
    'meta:paragraph-count': ('Doc.ParagraphCount', N_('Paragraph Count'), N_('Number of paragraphs that a document producer has counted in a document')),
    'meta:row-count': ('Doc.RowCount', N_('Row Count'), N_('Number of lines contained in a document instance')),
    'meta:sentence-count': ('Doc.SentenceCount', N_('Sentence Count'), N_('Number of sentences that a document producer has counted in a document')),
    'meta:syllable-count': ('Doc.SyllableCount', N_('Syllable Count'), N_('Number of syllables that a document producer has counted in a document')),
    'meta:table-count': ('Doc.TableCount', N_('Table Count'), N_('Number of table elements counted in a document')),
    'meta:word-count': ('Doc.WordCount', N_('Word Count'), N_('Number of words that a document producer has counted in a document')),

    # ECMA-376 5th Edition, Part 1, A.6.2; https://learn.microsoft.com/en-us/dotnet/api/documentformat.openxml.extendedproperties?view=openxml-3.0.1
    'ep:Template': 'meta:template',
    'ep:Manager': ('Doc.Manager', N_('Manager'), N_('Name of a supervisor associated with the document')),
    'ep:Company': ('Doc.Company', N_('Company'), N_('Name of a company associated with the document')),
    'ep:Pages': 'meta:page-count',
    'ep:Words': 'meta:word-count',
    'ep:Characters': 'meta:character-count',
    'ep:PresentationFormat': ('Doc.PresentationFormat', N_('Presentation Format'), N_('Intended format for a presentation document')),
    'ep:Lines': 'meta:row-count',
    'ep:Paragraphs': 'meta:paragraph-count',
    'ep:Slides': ('Doc.SlideCount', N_('Slide Count'), N_('Number of slides in a presentation document')),
    'ep:Notes': ('Doc.NoteCount', N_('Note Count'), N_('Number of slides in a presentation containing notes')),
    'ep:TotalTime': 'meta:editing-duration',
    'ep:HiddenSlides': ('Doc.HiddenSlideCount', N_('Hidden Slide Count'), N_('Number of hidden slides in a presentation document')),
    'ep:MMClips': ('Doc.MMClipCount', N_('Multimedia Clip Count'), N_('Number of sound or video clips that are present in the document')),
    'ep:ScaleCrop': ('Doc.Scale', N_('Scale Crop'), N_('Display mode of the document thumbnail')),
    'ep:LinksUpToDate"': ('Doc.LinksUpToDate', N_('Links Up-to-Date'), N_('Indicates whether hyperlinks in a document are up-to-date')),
    'ep:CharactersWithSpaces': 'meta:character-count',
    'ep:SharedDoc': ('Doc.SharedDoc', N_('Shared Document'), N_('Indicates if this document is currently shared between multiple producers')),
    'ep:HyperlinkBase': ('Doc.HyperlinkBase', N_('Relative Hyperlink Base'), N_('Base string used for evaluating relative hyperlinks in this document')),
    'ep:Application': 'meta:generator',
    'ep:AppVersion': ('Doc.AppVersion', N_('Application Version'), N_('Version of the application which produced this document')),
    'ep:DocSecurity 1': ('Doc.Security1', N_('Password-Protected'), N_('Security level of this document: password-protected')),
    'ep:DocSecurity 2': ('Doc.Security2', N_('Read-Only Recommended'), N_('Security level of this document: recommended to be opened as read-only')),
    'ep:DocSecurity 4': ('Doc.Security4', N_('Read-Only Enforced'), N_('Security level of this document: enforced to be opened as read-only')),
    'ep:DocSecurity 8': ('Doc.Security8', N_('Locked for Annotation'), N_('Security level of this document: locked for annotation')),

    # https://learn.microsoft.com/en-us/dotnet/api/documentformat.openxml.packaging.ipackageproperties?view=openxml-3.0.1
    'cp:category': ('Doc.Category', N_('Category'), N_('Category of the document')),
    'cp:contentStatus': ('Doc.Status', N_('Content Status'), N_('Status of the content, for example “Draft”, “Reviewed”, and “Final”')),
    'cp:contentType': 'dc:type',
    'cp:created': 'meta:creation-date',
    'cp:creator': 'meta:initial-creator',
    'cp:description': 'dc:description',
    'cp:identifier': 'dc:identifier',
    'cp:keywords': ('Doc.Keywords', N_('Keywords'), N_('Keywords to support searching and indexing')),
    'cp:language': 'dc:language',
    'cp:lastModifiedBy': ('Doc.LastModifiedBy', N_('Last Modified By'), N_('Name of the person who last modified a document')),
    'cp:lastPrinted': ('Doc.LastPrinted', N_('Last Printed'), N_('Date and time of the last printing')),
    'cp:modified': 'dc:date',
    'cp:revision': 'meta:editing-cycles',
    'cp:subject': 'dc:subject',
    'cp:title': 'dc:title',
    'cp:version': ('Doc.Version', N_('Version'), N_('Version number, set by the user or by the application')),

    # https://www.dublincore.org/specifications/dublin-core/dcmi-terms/#section-3
    # https://pypdf.readthedocs.io/en/stable/modules/XmpInformation.html
    'dc:contributor': ('Doc.Contributor', N_('Contributor'), N_('Entity responsible for making contributions to the document')),
    'dc:coverage': ('Doc.Coverage', N_('Coverage'), N_('Spatial or temporal topic of the document, spatial applicability of the document, or jurisdiction under which the document is relevant')),
    'dc:creator': 'meta:initial-creator',
    'dc:format': ('Doc.Format', N_('Format'), N_('The file format of the document')),
    'dc:identifier': ('Doc.Identifier', N_('Identifier'), N_('Unique identifier of the document')),
    'dc:publisher': ('Doc.Publisher', N_('Publisher'), N_('Entity responsible for making the document available')),
    'dc:relationship': ('Doc.Relationship', N_('Relationship'), N_('Relationship to other document')),
    'dc:rights': ('Doc.Rights', N_('Rights'), N_('Information about rights held in and over the document')),
    'dc:source': ('Doc.Source', N_('Source'), N_('A related document from which this document is derived')),
    'dc:type': ('Doc.Type', N_('Type'), N_('The nature or genre of the document')),

    # https://www.dublincore.org/specifications/dublin-core/dcmi-terms/#section-2
    'dcterms:created': 'meta:creation-date',
    'dcterms:modified': 'dc:date',

    # https://pypdf.readthedocs.io/en/stable/modules/XmpInformation.html
    # These aren’t actual tag names but rather match pdfpy property names.
    'pdf:keywords': 'cp:keywords',
    'pdf:pdfversion': ('Doc.PDFVersion', N_('PDF Version'), N_('The PDF version of the document')),
    'pdf:producer': 'meta:generator',
    'xmp:create_date': 'meta:creation-date',
    'xmp:modify_date': 'dc:date',
    'xmp:metadata_date': ('Doc.MetadataDate', N_('Metadata Date'), N_('Date and time that any metadata for this resource was last changed')),
    'xmp:creator_tool': ('Doc.InitialGenerator', N_('Initial Generator'), N_('Name of the first known tool used to create the document')),
    'pdfaid:part': ('Doc.PDFAPart', N_('PDF/A Part'), N_('The part of the PDF/A standard that the document conforms to (e.g., 1, 2, 3)')),
    'pdfaid:conformance': ('Doc.PDFAConformance', N_('PDF/A Conformance'), N_('The conformance level within the PDF/A standard (e.g., A, B, U)')),
})

BITMASKS = {'ep:DocSecurity'}

PDF_METADATA = {
    # ISO 32000-2:2020 section 14.3.3
    'title': 'dc:title',
    'author': 'dc:creator',
    'subject': 'dc:description',
    'creator': 'xmp:creator_tool',
    'producer': 'pdf:producer',
    'creation_date': 'xmp:create_date',
    'modification_date': 'xmp:modify_date',
    'keywords': 'pdf:keywords',
}


class DocumentMetadataPlugin(Plugin):
    async def startup(self) -> None:
        self.send_message('info', {
            'name': 'Document Metadata',
            'version': '1.0',
            'comments': (
                _('A plugin for extracting metadata from documents in OpenDocument (OpenOffice/LibreOffice), Open XML (Microsoft Office) and PDF formats.') +
                '\n\n' +
                _('PDF handling requires pypdf package for Python to be installed on your system.')
            ),
            'copyright': 'Copyright © 2026 Wladimir Palant',
            'authors': ['Wladimir Palant'],
            'webpage': 'https://gnome.pages.gitlab.gnome.org/gnome-commander/',
        })

        try:
            import pypdf
            self.has_pypdf = True
        except ImportError:
            self.has_pypdf = False
            self.send_message(
                'error',
                _('Package pypdf for Python is not installed on your system, PDF handling has been disabled.')
            )

        self.send_message('ready')

    async def extract_metadata(self, data: dict) -> list[dict]:
        sources = [read_zip_metadata(data['path'])]
        if self.has_pypdf:
            sources.append(read_pdf_metadata(data['path']))

        tags = []
        for source in sources:
            for key, value in source:
                if key.startswith('user-defined '):
                    key = key.split(' ', 1)[1]
                    tags.append(('Doc.UserDefined.' + key,
                                _('User-Defined:') + ' ' + key, '', value))
                else:
                    tag, name, description = TAGS[key]
                    tags.append((tag, _(name), _(description), value))
        return [{
            "class": _("Document"),
            "tags": tags,
        }]

    async def list_supported_tags(self, data: dict) -> list[tuple[str, list[tuple[str, str]]]]:
        response = []
        seen = set()
        for tag, name, description in TAGS.values():
            if tag not in seen:
                seen.add(tag)
                response.append((tag, _(name)))
        return [(_("Document"), response)]


def node_name(node: minidom.Node) -> str:
    for namespace, uri in NAMESPACES.items():
        if uri == node.namespaceURI:
            return namespace + ':' + (node.localName or '')
    return (node.namespaceURI or '') + ':' + (node.localName or '')


def node_text(element: minidom.Element) -> str:
    text = ''
    for child in element.childNodes:
        if child.nodeType == child.TEXT_NODE:
            text += child.nodeValue
        elif child.nodeType == child.ELEMENT_NODE:
            text += node_text(child)
    return text


def read_zip_metadata(path: str) -> Generator[tuple[str, str], None, None]:
    try:
        input = zipfile.ZipFile(path)
    except:
        return

    try:
        for info in input.infolist():
            if info.filename == 'meta.xml' or (info.filename.startswith('docProps/') and info.filename.endswith('.xml')):
                try:
                    dom = minidom.parse(input.open(info))
                except:
                    continue

                if info.filename == 'meta.xml':
                    yield from read_odf_metadata(dom)
                else:
                    yield from read_oxml_metadata(dom)
    finally:
        input.close()


def read_odf_metadata(dom: minidom.Document) -> Generator[tuple[str, str], None, None]:
    for meta in dom.getElementsByTagNameNS(NAMESPACES['office'], 'meta'):
        for child in meta.childNodes:
            if child.nodeType != child.ELEMENT_NODE:
                continue
            name = node_name(child)
            text = node_text(child)

            if name == 'dc:creator':
                # ODF interprets dc:creator tag in an unusual way
                name = 'cp:lastModifiedBy'

            if name in TAGS and text:
                yield name, text
            elif child.getAttributeNS(NAMESPACES['xlink'], 'href'):
                yield name, child.getAttributeNS(NAMESPACES['xlink'], 'href')
            elif name == 'meta:auto-reload' and child.getAttributeNS(NAMESPACES['meta'], 'delay'):
                yield name, child.getAttributeNS(NAMESPACES['meta'], 'delay')
            elif name == 'meta:document-statistic':
                for attribute in child.attributes.values():
                    name = node_name(attribute)
                    text = attribute.nodeValue
                    if name in TAGS and text:
                        yield name, text
            elif name == 'meta:user-defined' and text:
                name = child.getAttributeNS(NAMESPACES['meta'], 'name')
                if name:
                    yield 'user-defined ' + name, text


def read_oxml_metadata(dom: minidom.Document) -> Generator[tuple[str, str], None, None]:
    def convert_bitmask(name, value) -> Generator[tuple[str, str], None, None]:
        try:
            value = int(value)
        except:
            return
        for i in range(32):
            mask = 1 << i
            if (value & mask) > 0 and f'{name} {mask}' in TAGS:
                yield f'{name} {mask}', 'true'

    if not dom.documentElement:
        return
    for child in dom.documentElement.childNodes:
        if child.nodeType != child.ELEMENT_NODE:
            continue
        name = node_name(child)
        text = node_text(child)
        if name in BITMASKS:
            yield from convert_bitmask(name, text)
        elif name in TAGS and text:
            yield name, text
        elif (name == 'http://schemas.openxmlformats.org/officeDocument/2006/custom-properties:property'
                and text and child.getAttribute('name')):
            yield 'user-defined ' + child.getAttribute('name'), text


def read_pdf_metadata(path: str) -> Generator[tuple[str, str], None, None]:
    try:
        input = open(path, 'rb')
    except:
        return

    try:
        header = input.read(8)
        match = re.search(rb'^%PDF-(\d\.\d)$', header)
        if not match:
            # Not a PDF file
            return
        yield 'pdf:pdfversion', match.group(1).decode()

        from pypdf import PdfReader
        from pypdf.errors import WrongPasswordError, PyPdfError
        try:
            reader = PdfReader(input)
        except WrongPasswordError:
            yield 'ep:DocSecurity 1', 'true'
            return
        except PyPdfError:
            return

        xmp_metadata = reader.xmp_metadata
        if xmp_metadata:
            for attr in TAGS.keys():
                if attr.split(':', 1)[0] not in ('dc', 'pdf', 'xmp', 'xmpmm', 'pdfaid'):
                    continue
                prop = attr.replace(':', '_')
                value = getattr(xmp_metadata, prop, None)
                if value:
                    if isinstance(value, list):
                        for entry in value:
                            yield attr, str(entry)
                    elif isinstance(value, dict):
                        for entry in value.values():
                            yield attr, str(entry)
                    else:
                        yield attr, str(value)
            for name, value in xmp_metadata.custom_properties.items():
                yield 'user-defined ' + name, str(value)

        def equals_xmp_value(value: Any, xmp_value: Any) -> bool:
            if isinstance(xmp_value, list):
                return any(value == v for v in xmp_value)
            elif isinstance(xmp_value, dict):
                return any(value == v for v in xmp_value.values())
            else:
                return value == xmp_value

        metadata = reader.metadata
        if metadata:
            for key, mapping in PDF_METADATA.items():
                value = getattr(metadata, key)
                if not value or (xmp_metadata and equals_xmp_value(value, getattr(xmp_metadata, mapping.replace(':', '_')))):
                    continue
                yield mapping, str(value)

    finally:
        input.close()


if __name__ == '__main__':
    DocumentMetadataPlugin()
