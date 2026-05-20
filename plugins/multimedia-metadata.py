#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

from gettext import gettext as _
from typing import Any

from plugin_common import Plugin


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


class InfoExtractor:
    # Cross-section of the attributes for all Info types across formats supported by mutagen
    TAGS = resolve_aliases({
        'album_gain': ('MM.AlbumGain', N_('Album Gain'), N_('Replay Gain for this album')),
        'album_peak': ('MM.AlbumPeak', N_('Album Peak'), N_('Peak data for this album')),
        'bitrate': ('MM.Bitrate', N_('Bitrate'), N_('Bitrate in bps')),
        'bits_per_sample': ('MM.BitsPerSample', N_('Bits per Sample'), N_('The audio sample size')),
        'channels': ('MM.Channels', N_('Channels'), N_('Number of channels')),
        'codec': 'codec_name',
        'codec_description': ('MM.CodecDescription', N_('Codec Description'), N_('Further information on the codec used')),
        'codec_name': ('MM.CodecName', N_('Codec Name'), N_('Name and maybe version of the codec used')),
        'codec_type': ('MM.CodecType', N_('Codec Type'), N_('Name of the codec type')),
        'encoder_info': ('MM.EncoderInfo', N_('Encoder Info'), N_('Encoder name and possibly version')),
        'encoder_settings': ('MM.EncoderSettings', N_('Encoder Settings'), N_('A guess about the settings used for encoding')),
        'fps': ('MM.FPS', N_('Frames per Second'), N_('Video frames per second')),
        'layer': ('MM.Layer', N_('Layer'), N_('Coding algorithm layer (1, 2, or 3)')),
        'length': ('MM.Length', N_('Length'), N_('Length in seconds')),
        'protected': ('MM.Protected', N_('Protected'), N_('Whether or not the file is “protected”')),
        'sample_rate': ('MM.SampleRate', N_('Sample Rate'), N_('Sample rate in Hz')),
        'title_gain': ('MM.TitleGain', N_('Title Gain'), N_('Replay Gain for this song')),
        'title_peak': ('MM.TitlePeak', N_('Title Peak'), N_('Peak data for this song')),
        'track_gain': 'title_gain',
        'track_peak': 'title_peak',
        'version': ('MM.Version', N_('Version'), N_('Format version')),
    })

    def class_name() -> str:
        return _('Multimedia')

    @classmethod
    def extract(cls, file) -> dict[str, Any] | None:
        if not file.info:
            return None

        tags = []
        for key in cls.TAGS:
            value = getattr(file.info, key, None)
            if value:
                tag, name, description = cls.TAGS[key]
                tags.append((tag, _(name), _(description), str(value)))
        return {
            'class': cls.class_name(),
            'tags': tags,
        }


class ID3Extractor:
    TAGS = {
        'TPE2': ('ID3.Band', N_('Band'), N_('Additional information about the performers in the recording.')),
        'TSOA': ('ID3.AlbumSortOrder', N_('Album Sort Order'), N_('String which should be used instead of the album name for sorting purposes.')),
        'AENC': ('ID3.AudioCrypto', N_('Audio Encryption'), N_('Frame indicates if the audio stream is encrypted, and by whom.')),
        'ASPI': ('ID3.AudioSeekPoint', N_('Audio Seek Point'), N_('Fractional offset within the audio data, providing a starting point from which to find an appropriate point to start decoding.')),
        'TBPM': ('ID3.BPM', N_('BPM'), N_('BPM (beats per minute).')),
        'RBUF': ('ID3.BufferSize', N_('Buffer Size'), N_('Recommended buffer size.')),
        'MCDI': ('ID3.CDID', N_('CD ID'), N_('Music CD identifier.')),
        'COMR': ('ID3.Commercial', N_('Commercial'), N_('Commercial frame.')),
        'TCOM': ('ID3.Composer', N_('Composer'), N_('Composer.')),
        'TPE3': ('ID3.Conductor', N_('Conductor'), N_('Conductor.')),
        'TIT1': ('ID3.ContentGroup', N_('Content Group'), N_('Content group description.')),
        'TCON': ('ID3.ContentType', N_('Content Type'), N_('Type of music classification for the track as defined in ID3 spec.')),
        'TCOP': ('ID3.Copyright', N_('Copyright'), N_('Copyright message.')),
        'ENCR': ('ID3.CryptoReg', N_('Encryption Registration'), N_('Encryption method registration.')),
        'TDAT': ('ID3.Date', N_('Date'), N_('Date.')),
        'TENC': ('ID3.EncodedBy', N_('Encoded By'), N_('Person or organisation that encoded the audio file. This field may contain a copyright message, if the audio file also is copyrighted by the encoder.')),
        'TSSE': ('ID3.EncoderSettings', N_('Encoder Settings'), N_('Software.')),
        'TDEN': ('ID3.EncodingTime', N_('Encoding Time'), N_('Encoding time.')),
        'EQUA': ('ID3.Equalization', N_('Equalization'), N_('Equalization.')),
        'EQU2': ('ID3.Equalization2', N_('Equalization'), N_('Equalisation curve predefine within the audio file.')),
        'ETCO': ('ID3.EventTiming', N_('Event Timing'), N_('Event timing codes.')),
        'TFLT': ('ID3.FileOwner', N_('File Owner'), N_('File owner.')),
        'TFLT': ('ID3.FileType', N_('File Type'), N_('File type.')),
        'GEOB': ('ID3.GeneralObject', N_('General Object'), N_('General encapsulated object.')),
        'GRID': ('ID3.GroupingReg', N_('Grouping Registration'), N_('Group identification registration.')),
        'TKEY': ('ID3.InitialKey', N_('Initial Key'), N_('Initial key.')),
        'TIPL': ('ID3.InvolvedPeople', N_('Involved People'), N_('Involved people list.')),
        'IPLS': ('ID3.InvolvedPeople2', N_('Involved People 2'), N_('Involved people list.')),
        'TLAN': ('ID3.Language', N_('Language'), N_('Language.')),
        'LINK': ('ID3.LinkedInfo', N_('Linked Info'), N_('Linked information.')),
        'TEXT': ('ID3.Lyricist', N_('Lyricist'), N_('Lyricist.')),
        'TMED': ('ID3.MediaType', N_('Media Type'), N_('Media type.')),
        'TPE4': ('ID3.MixArtist', N_('Mix Artist'), N_('Interpreted, remixed, or otherwise modified by.')),
        'TMOO': ('ID3.Mood', N_('Mood'), N_('Mood.')),
        'MLLT': ('ID3.MPEG.Lookup', N_('MPEG Lookup'), N_('MPEG location lookup table.')),
        'TMCL': ('ID3.MusicianCreditList', N_('Musician Credit List'), N_('Musician credits list.')),
        'TRSO': ('ID3.NetRadioOwner', N_('Net Radio Owner'), N_('Internet radio station owner.')),
        'TRSN': ('ID3.NetRadiostation', N_('Net Radiostation'), N_('Internet radio station name.')),
        'TOAL': ('ID3.OriginalAlbum', N_('Original Album'), N_('Original album.')),
        'TOPE': ('ID3.OriginalArtist', N_('Original Artist'), N_('Original artist.')),
        'TOFN': ('ID3.OriginalFileName', N_('Original File Name'), N_('Original file name.')),
        'TOLY': ('ID3.OriginalLyricist', N_('Original Lyricist'), N_('Original lyricist.')),
        'TDOR': ('ID3.OriginalReleaseTime', N_('Original Release Time'), N_('Original release time.')),
        'TORY': ('ID3.OriginalYear', N_('Original Year'), N_('Original release year.')),
        'OWNE': ('ID3.Ownership', N_('Ownership'), N_('Ownership frame.')),
        'TPOS': ('ID3.PartInSet', N_('Part of a Set'), N_('Part of a set the audio came from.')),
        'TSOP': ('ID3.PerformerSortOrder', N_('Performer Sort Order'), N_('Performer sort order.')),
        'APIC': ('ID3.Picture', N_('Picture'), N_('Attached picture.')),
        'PCNT': ('ID3.PlayCounter', N_('Play Counter'), N_('Number of times a file has been played.')),
        'TDLY': ('ID3.PlaylistDelay', N_('Playlist Delay'), N_('Playlist delay.')),
        'POPM': ('ID3.Popularimeter', N_('Popularimeter'), N_('Rating of the audio file.')),
        'POSS': ('ID3.PositionSync', N_('Position Sync'), N_('Position synchronisation frame.')),
        'PRIV': ('ID3.Private', N_('Private'), N_('Private frame.')),
        'TPRO': ('ID3.ProducedNotice', N_('Produced Notice'), N_('Produced notice.')),
        'TPUB': ('ID3.Publisher', N_('Publisher'), N_('Publisher.')),
        'TRDA': ('ID3.RecordingDates', N_('Recording Dates'), N_('Recording dates.')),
        'TDRC': ('ID3.RecordingTime', N_('Recording Time'), N_('Recording time.')),
        'TDRL': ('ID3.ReleaseTime', N_('Release Time'), N_('Release time.')),
        'RVRB': ('ID3.Reverb', N_('Reverb'), N_('Reverb.')),
        'TSST': ('ID3.SetSubtitle', N_('Set Subtitle'), N_('Subtitle of the part of a set this track belongs to.')),
        'SIGN': ('ID3.Signature', N_('Signature'), N_('Signature frame.')),
        'TLEN': ('ID3.SongLength', N_('Song length'), N_('Length of the song in milliseconds.')),
        'TIT3': ('ID3.Subtitle', N_('Subtitle'), N_('Subtitle.')),
        'SYLT': ('ID3.Syncedlyrics', N_('Synchronized Lyrics'), N_('Synchronized lyric.')),
        'SYTC': ('ID3.SyncedTempo', N_('Synchronized Tempo'), N_('Synchronized tempo codes.')),
        'TDTG': ('ID3.TaggingTime', N_('Tagging Time'), N_('Tagging time.')),
        'USER': ('ID3.TermsOfUse', N_('Terms of Use'), N_('Terms of use.')),
        'TIME': ('ID3.Time', N_('Time'), N_('Time.')),
        'TSOT': ('ID3.TitleSortOrder', N_('Title Sort Order'), N_('Title sort order.')),
        'UFID': ('ID3.UniqueFileID', N_('Unique File ID'), N_('Unique file identifier.')),
        'USLT': ('ID3.UnsyncedLyrics', N_('Unsynchronized Lyrics'), N_('Unsynchronized lyric.')),
        'TXXX': ('ID3.UserText', N_('User Text'), N_('User defined text information.')),
        'RVAD': ('ID3.VolumeAdj', N_('Volume Adjustment'), N_('Relative volume adjustment.')),
        'RVA2': ('ID3.VolumeAdj2', N_('Volume Adjustment'), N_('Relative volume adjustment.')),
        'WOAR': ('ID3.WWWArtist', N_('WWW Artist'), N_('Official artist.')),
        'WOAF': ('ID3.WWWAudioFile', N_('WWW Audio File'), N_('Official audio file webpage.')),
        'WOAS': ('ID3.WWWAudioSource', N_('WWW Audio Source'), N_('Official audio source webpage.')),
        'WCOM': ('ID3.WWWCommercialInfo', N_('WWW Commercial Info'), N_('URL pointing at a webpage containing commercial information.')),
        'WCOP': ('ID3.WWWCopyright', N_('WWW Copyright'), N_('URL pointing at a webpage that holds copyright.')),
        'WPAY': ('ID3.WWWPayment', N_('WWW Payment'), N_('URL pointing at a webpage that will handle the process of paying for this file.')),
        'WPUB': ('ID3.WWWPublisher', N_('WWW Publisher'), N_('URL pointing at the official webpage for the publisher.')),
        'WORS': ('ID3.WWWRadioPage', N_('WWW Radio Page'), N_('Official internet radio station homepage.')),
        'WXXX': ('ID3.WWWUser', N_('WWW User'), N_('User defined URL link.')),
    }

    def class_name() -> str:
        return 'ID3'

    @classmethod
    def extract(cls, file) -> dict[str, Any] | None:
        import mutagen
        if not isinstance(file.tags, mutagen.id3.ID3):
            return None

        def stringify_frame(frame: mutagen.id3.Frame) -> str:
            if isinstance(frame, mutagen.id3.TextFrame):
                return ' '.join(str(text) for text in frame.text)
            elif isinstance(frame, mutagen.id3.PairedTextFrame):
                return ', '.join(map(lambda entry: ' '.join(entry), frame.people))
            else:
                return str(frame)

        tags = []
        for key, value in file.tags.items():
            if value and key in cls.TAGS:
                tag, name, description = cls.TAGS[key]
                tags.append((tag, _(name), _(description),
                            stringify_frame(value)))

        return {
            'class': cls.class_name(),
            'tags': tags,
        }


class APEv2Extractor:
    TAGS = {
        # https://wiki.hydrogenaudio.org/index.php?title=APE_key
        'title': ('APEv2.Title', N_('Title'), N_('Music piece title')),
        'subtitle': ('APEv2.Subtitle', N_('Subtitle'), N_('Additional sub title')),
        'artist': ('APEv2.Artist', N_('Artist'), N_('Performing artist')),
        'album': ('APEv2.Album', N_('Album'), N_('Album name')),
        'debut album': ('APEv2.DebutAlbum', N_('Debut Album'), N_('Debut album name')),
        'publisher': ('APEv2.Publisher', N_('Publisher'), N_('Record label or publisher')),
        'conductor': ('APEv2.Conductor', N_('Conductor'), N_('Conductor')),
        'track': ('APEv2.Track', N_('Track Number'), N_('Track Number')),
        'composer': ('APEv2.Composer', N_('Composer'), N_('Name of the original composer')),
        'comment': ('APEv2.Comment', N_('Comment'), N_('User comment')),
        'copyright': ('APEv2.Copyright', N_('Copyright'), N_('Copyright holder')),
        'publicationright': ('APEv2.Publicationright', N_('Publication Right'), N_('Publication right holder')),
        'file': ('APEv2.File', N_('File'), N_('File location')),
        'isbn': ('APEv2.ISBN', N_('ISBN'), N_('ISBN number with check digit')),
        'catalog': ('APEv2.Catalog', N_('Catalog Number'), N_('Catalog number')),
        'lc': ('APEv2.LabelCode', N_('Label Code'), N_('Label Code')),
        # Spec lists this as Year but actual files have Date
        'date': ('APEv2.Date', N_('Date'), N_('Release date')),
        'record date': ('APEv2.RecordDate', N_('Record Date'), N_('Record date')),
        'record location': ('APEv2.RecordLocation', N_('Record Location'), N_('Record location')),
        'genre': ('APEv2.Genre', N_('Genre'), N_('Genre')),
        'isrc': ('APEv2.ISRC', N_('ISRC'), N_('International Standard Recording Number')),
        'abstract': ('APEv2.Abstract', N_('Abstract'), N_('Abstract link')),
        'language': ('APEv2.Language', N_('Language'), N_('Used language for music/spoken words')),
        'bibliography': ('APEv2.Bibliography', N_('Bibliography'), N_('Bibliography/discography link')),
    }

    def class_name() -> str:
        return 'APEv2'

    @classmethod
    def extract(cls, file) -> dict[str, Any] | None:
        import mutagen
        if not isinstance(file.tags, mutagen.apev2.APEv2):
            return None

        tags = []
        for key, value in file.tags.items():
            if isinstance(value, mutagen.apev2.APEBinaryValue):
                continue

            for entry in (value if isinstance(value, mutagen.apev2.APETextValue) else [value.value]):
                if not entry:
                    continue
                if key.lower() in cls.TAGS:
                    tag, name, description = cls.TAGS[key.lower()]
                    tags.append((tag, _(name), _(description), entry))
                else:
                    tags.append(('APEv2.' + key, key, '', entry))

        return {
            'class': cls.class_name(),
            'tags': tags,
        }


class VorbisExtractor:
    TAGS = resolve_aliases({
        # Vorbis comments are not standardized, there are various lists floating around.
        # Using https://exiftool.org/TagNames/Vorbis.html plus some tags found in real-life files.
        'actor': ('Vorbis.Actor', N_('Actor'), N_('Actor name')),
        'album': ('Vorbis.Album', N_('Album'), N_('Album name')),
        'artist': ('Vorbis.Artist', N_('Artist'), N_('Performing artist')),
        'comment': ('Vorbis.Comment', N_('Comment'), N_('User comment')),
        'comments': 'comment',
        'composer': ('Vorbis.Composer', N_('Composer'), N_('Name of the original composer')),
        'conductor': ('Vorbis.Conductor', N_('Conductor'), N_('Conductor')),
        'contact': ('Vorbis.Contact', N_('Contact'), N_('Contact information for the creators or distributors of the track')),
        'copyright': ('Vorbis.Copyright', N_('Copyright'), N_('Copyright holder')),
        'date': ('Vorbis.Date', N_('Date'), N_('Release date')),
        'description': ('Vorbis.Description', N_('Description'), N_('A textual description')),
        'director': ('Vorbis.Director', N_('Director'), N_('Director name')),
        'encoder': ('Vorbis.Encoder', N_('Encoder'), N_('Encoder used to produce the file')),
        'genre': ('Vorbis.Genre', N_('Genre'), N_('Genre')),
        'isrc': ('Vorbis.ISRC', N_('ISRC'), N_('International Standard Recording Number')),
        'license': ('Vorbis.License', N_('License'), N_('License information')),
        'location': ('Vorbis.Location', N_('Location'), N_('Location where track was recorded')),
        'place of recording': 'location',
        'organization': ('Vorbis.Organization', N_('Organization'), N_('Organization producing the track')),
        'label': 'organization',
        'performer': ('Vorbis.Performer', N_('Performer'), N_('Performing artist')),
        'producer': ('Vorbis.Producer', N_('Producer'), N_('Producer of the track')),
        'title': ('Vorbis.Title', N_('Title'), N_('Track title')),
        'tracknumber': ('Vorbis.TrackNumber', N_('Track Number'), N_('Track Number')),
        'version': ('Vorbis.Version', N_('Vorbis Version'), N_('Vorbis version')),
        'vendor': ('Vorbis.Vendor', N_('Vendor'), N_('Vorbis vendor ID.')),
    })

    # Exclude binary data
    BLOCKLIST = {'coverart', 'coverartmime', 'metadata_block_picture'}

    def class_name() -> str:
        return 'Vorbis'

    @classmethod
    def extract(cls, file) -> dict[str, Any] | None:
        import mutagen
        if not isinstance(file.tags, mutagen._vorbis.VCommentDict):
            return None

        tags = []
        for key, value in file.tags:
            if key in cls.BLOCKLIST or not value:
                continue

            if key.lower() in cls.TAGS:
                tag, name, description = cls.TAGS[key.lower()]
                tags.append((tag, _(name), _(description), value))
            else:
                # Some software is storing RIFF tags like IENG here. Handling them like this
                # should be sufficient.
                tags.append(('Vorbis.' + key, key, '', value))

        return {
            'class': cls.class_name(),
            'tags': tags,
        }


class MultimediaMetadataPlugin(Plugin):
    SOURCES = [InfoExtractor, ID3Extractor, APEv2Extractor, VorbisExtractor]

    def startup(self) -> None:
        self.send_message('info', {
            'name': 'Multimedia Metadata',
            'version': '1.0',
            'comments': (
                _('A plugin for extracting metadata from audio and video files.') +
                '\n\n' +
                _('Python3 package mutagen has to be install on your system for this plugin to work.')
            ),
            'copyright': 'Copyright © 2026 Wladimir Palant',
            'authors': ['Wladimir Palant'],
            'webpage': 'https://gnome.pages.gitlab.gnome.org/gnome-commander/',
        })

        try:
            import mutagen
        except ImportError:
            self.fail(
                _('Required mutagen module not found. Please install mutagen for Python3 on your system.')
            )

        self.send_message('ready')
        self.process_incoming()

    def extract_metadata(self, data: dict) -> list[dict]:
        import mutagen
        path: str = data['path']
        try:
            file = mutagen.File(path)
            if file is None:
                raise mutagen.MutagenError()
        except mutagen.MutagenError:
            return []

        return list(filter(lambda result: result is not None, (extractor.extract(file) for extractor in self.SOURCES)))

    def list_supported_tags(self, data: dict) -> list[tuple[str, list[tuple[str, str]]]]:
        response = []
        for source in self.SOURCES:
            seen = set()
            result = []
            for tag, name, description in source.TAGS.values():
                if tag not in seen:
                    seen.add(tag)
                    result.append((tag, _(name)))
            response.append((source.class_name(), result))
        return response


if __name__ == '__main__':
    MultimediaMetadataPlugin()
