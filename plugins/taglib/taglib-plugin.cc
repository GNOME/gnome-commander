/**
 * @file taglib-plugin.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 * @copyright (C) 2025 Andrey Kutejko\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libgcmd.h>

#include <iostream>
#include <tag.h>
#include <fileref.h>

#include <id3v1tag.h>
#include <id3v2tag.h>
#include <id3v2header.h>
#include <id3v2frame.h>
#include <apetag.h>
#include <flacfile.h>
#include <mpcfile.h>
#include <mpegfile.h>
#include <vorbisfile.h>
#include <xiphcomment.h>


using namespace std;


extern "C" GObject *create_plugin ();
extern "C" GnomeCmdPluginInfo *get_plugin_info ();


#define GNOME_CMD_TYPE_TAGLIB_PLUGIN (gnome_cmd_taglib_plugin_get_type ())
G_DECLARE_FINAL_TYPE (GnomeCmdTaglibPlugin, gnome_cmd_taglib_plugin, GNOME_CMD, TAGLIB_PLUGIN, GObject)

struct _GnomeCmdTaglibPlugin
{
    GObject parent;
};

static void gnome_cmd_file_metadata_extractor_init (GnomeCmdFileMetadataExtractorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GnomeCmdTaglibPlugin, gnome_cmd_taglib_plugin, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GNOME_CMD_TYPE_FILE_METADATA_EXTRACTOR, gnome_cmd_file_metadata_extractor_init))


static GStrv supported_tags (GnomeCmdFileMetadataExtractor *);
static GStrv summary_tags (GnomeCmdFileMetadataExtractor *);
static gchar *class_name (GnomeCmdFileMetadataExtractor *, const gchar *);
static gchar *tag_name (GnomeCmdFileMetadataExtractor *, const gchar *);
static gchar *tag_description (GnomeCmdFileMetadataExtractor *, const gchar *);
static void extract_metadata (GnomeCmdFileMetadataExtractor *, GnomeCmdFileDescriptor *, GnomeCmdFileMetadataExtractorAddTag, gpointer);

static bool getAudioProperties(const TagLib::AudioProperties *properties, GnomeCmdFileMetadataExtractorAddTag add, gpointer user_data);
static bool readTags(GnomeCmdTaglibPlugin *plugin, const TagLib::Ogg::XiphComment *oggTag, GnomeCmdFileMetadataExtractorAddTag add, gpointer user_data);

static const char *TAG_FILE_PUBLISHER  = "File.Publisher";

struct AudioTag
{
    const char *id;
    const char *name;
    const char *description;
};

static AudioTag TAG_AUDIO_ALBUMARTIST              = { "Audio.AlbumArtist",                N_("Album Artist"),                 N_("Artist of the album.") };
static AudioTag TAG_AUDIO_ALBUMGAIN                = { "Audio.AlbumGain",                  N_("Album Gain"),                   N_("Gain adjustment of the album.") };
static AudioTag TAG_AUDIO_ALBUMPEAKGAIN            = { "Audio.AlbumPeakGain",              N_("Album Peak Gain"),              N_("Peak gain adjustment of album.") };
static AudioTag TAG_AUDIO_ALBUMTRACKCOUNT          = { "Audio.AlbumTrackCount",            N_("Album Track Count"),            N_("Total number of tracks on the album.") };
static AudioTag TAG_AUDIO_ALBUM                    = { "Audio.Album",                      N_("Album"),                        N_("Name of the album.") };
static AudioTag TAG_AUDIO_ARTIST                   = { "Audio.Artist",                     N_("Artist"),                       N_("Artist of the track.") };
static AudioTag TAG_AUDIO_BITRATE                  = { "Audio.Bitrate",                    N_("Bitrate"),                      N_("Bitrate in kbps.") };
static AudioTag TAG_AUDIO_BITRATE_WITH_UNITS       = { "Audio.Bitrate.WithUnits",          N_("Bitrate"),                      N_("Bitrate with units (kbps).") };
static AudioTag TAG_AUDIO_CHANNELS                 = { "Audio.Channels",                   N_("Channels"),                     N_("Number of channels in the audio (2 = stereo).") };
static AudioTag TAG_AUDIO_CODECVERSION             = { "Audio.CodecVersion",               N_("Codec Version"),                N_("Codec version.") };
static AudioTag TAG_AUDIO_CODEC                    = { "Audio.Codec",                      N_("Codec"),                        N_("Codec encoding description.") };
static AudioTag TAG_AUDIO_COMMENT                  = { "Audio.Comment",                    N_("Comment"),                      N_("Comments on the track.") };
static AudioTag TAG_AUDIO_COPYRIGHT                = { "Audio.Copyright",                  N_("Copyright"),                    N_("Copyright message.") };
static AudioTag TAG_AUDIO_COVERALBUMTHUMBNAILPATH  = { "Audio.CoverAlbumThumbnailPath",    N_("Cover Album Thumbnail Path"),   N_("File path to thumbnail image of the cover album.") };
static AudioTag TAG_AUDIO_DISCNO                   = { "Audio.DiscNo",                     N_("Disc Number"),                  N_("Specifies which disc the track is on.") };
static AudioTag TAG_AUDIO_DURATION                 = { "Audio.Duration",                   N_("Duration"),                     N_("Duration of track in seconds.") };
static AudioTag TAG_AUDIO_DURATIONMMSS             = { "Audio.Duration.MMSS",              N_("Duration [MM:SS]"),             N_("Duration of track as MM:SS.") };
static AudioTag TAG_AUDIO_GENRE                    = { "Audio.Genre",                      N_("Genre"),                        N_("Type of music classification for the track as defined in ID3 spec.") };
static AudioTag TAG_AUDIO_ISNEW                    = { "Audio.IsNew",                      N_("Is New"),                       N_("Set to “1” if track is new to the user (default “0”).") };
static AudioTag TAG_AUDIO_ISRC                     = { "Audio.ISRC",                       N_("ISRC"),                         N_("ISRC (international standard recording code).") };
static AudioTag TAG_AUDIO_LASTPLAY                 = { "Audio.LastPlay",                   N_("Last Play"),                    N_("When track was last played.") };
static AudioTag TAG_AUDIO_LYRICS                   = { "Audio.Lyrics",                     N_("Lyrics"),                       N_("Lyrics of the track.") };
static AudioTag TAG_AUDIO_MBALBUMARTISTID          = { "Audio.MBAlbumArtistID",            N_("MB album artist ID"),           N_("MusicBrainz album artist ID in UUID format.") };
static AudioTag TAG_AUDIO_MBALBUMID                = { "Audio.MBAlbumID",                  N_("MB Album ID"),                  N_("MusicBrainz album ID in UUID format.") };
static AudioTag TAG_AUDIO_MBARTISTID               = { "Audio.MBArtistID",                 N_("MB Artist ID"),                 N_("MusicBrainz artist ID in UUID format.") };
static AudioTag TAG_AUDIO_MBTRACKID                = { "Audio.MBTrackID",                  N_("MB Track ID"),                  N_("MusicBrainz track ID in UUID format.") };
static AudioTag TAG_AUDIO_PERFORMER                = { "Audio.Performer",                  N_("Performer"),                    N_("Name of the performer/conductor of the music.") };
static AudioTag TAG_AUDIO_PLAYCOUNT                = { "Audio.PlayCount",                  N_("Play Count"),                   N_("Number of times the track has been played.") };
static AudioTag TAG_AUDIO_RELEASEDATE              = { "Audio.ReleaseDate",                N_("Release Date"),                 N_("Date track was released.") };
static AudioTag TAG_AUDIO_SAMPLERATE               = { "Audio.SampleRate",                 N_("Sample Rate"),                  N_("Sample rate in Hz.") };
static AudioTag TAG_AUDIO_TITLE                    = { "Audio.Title",                      N_("Title"),                        N_("Title of the track.") };
static AudioTag TAG_AUDIO_TRACKGAIN                = { "Audio.TrackGain",                  N_("Track Gain"),                   N_("Gain adjustment of the track.") };
static AudioTag TAG_AUDIO_TRACKNO                  = { "Audio.TrackNo",                    N_("Track Number"),                 N_("Position of track on the album.") };
static AudioTag TAG_AUDIO_TRACKPEAKGAIN            = { "Audio.TrackPeakGain",              N_("Track Peak Gain"),              N_("Peak gain adjustment of track.") };
static AudioTag TAG_AUDIO_YEAR                     = { "Audio.Year",                       N_("Year"),                         N_("Year.") };
static AudioTag TAG_AUDIO_MPEG_CHANNELMODE         = { "Audio.MPEG.ChannelMode",           N_("Channel Mode"),                 N_("MPEG channel mode.") };
static AudioTag TAG_AUDIO_MPEG_COPYRIGHTED         = { "Audio.MPEG.Copyrighted",           N_("Copyrighted"),                  N_("“1” if the copyrighted bit is set.") };
static AudioTag TAG_AUDIO_MPEG_LAYER               = { "Audio.MPEG.Layer",                 N_("Layer"),                        N_("MPEG layer.") };
static AudioTag TAG_AUDIO_MPEG_ORIGINAL            = { "Audio.MPEG.Original",              N_("Original Audio"),               N_("“1” if the “original” bit is set.") };
static AudioTag TAG_AUDIO_MPEG_VERSION             = { "Audio.MPEG.Version",               N_("MPEG Version"),                 N_("MPEG version.") };

static AudioTag *AUDIO_TAGS[] = {
    &TAG_AUDIO_ALBUMARTIST,
    &TAG_AUDIO_ALBUMGAIN,
    &TAG_AUDIO_ALBUMPEAKGAIN,
    &TAG_AUDIO_ALBUMTRACKCOUNT,
    &TAG_AUDIO_ALBUM,
    &TAG_AUDIO_ARTIST,
    &TAG_AUDIO_BITRATE,
    &TAG_AUDIO_BITRATE_WITH_UNITS,
    &TAG_AUDIO_CHANNELS,
    &TAG_AUDIO_CODECVERSION,
    &TAG_AUDIO_CODEC,
    &TAG_AUDIO_COMMENT,
    &TAG_AUDIO_COPYRIGHT,
    &TAG_AUDIO_COVERALBUMTHUMBNAILPATH,
    &TAG_AUDIO_DISCNO,
    &TAG_AUDIO_DURATION,
    &TAG_AUDIO_DURATIONMMSS,
    &TAG_AUDIO_GENRE,
    &TAG_AUDIO_ISNEW,
    &TAG_AUDIO_ISRC,
    &TAG_AUDIO_LASTPLAY,
    &TAG_AUDIO_LYRICS,
    &TAG_AUDIO_MBALBUMARTISTID,
    &TAG_AUDIO_MBALBUMID,
    &TAG_AUDIO_MBARTISTID,
    &TAG_AUDIO_MBTRACKID,
    &TAG_AUDIO_PERFORMER,
    &TAG_AUDIO_PLAYCOUNT,
    &TAG_AUDIO_RELEASEDATE,
    &TAG_AUDIO_SAMPLERATE,
    &TAG_AUDIO_TITLE,
    &TAG_AUDIO_TRACKGAIN,
    &TAG_AUDIO_TRACKNO,
    &TAG_AUDIO_TRACKPEAKGAIN,
    &TAG_AUDIO_YEAR,
    &TAG_AUDIO_MPEG_CHANNELMODE,
    &TAG_AUDIO_MPEG_COPYRIGHTED,
    &TAG_AUDIO_MPEG_LAYER,
    &TAG_AUDIO_MPEG_ORIGINAL,
    &TAG_AUDIO_MPEG_VERSION
};

struct ID3Tag
{
    const char *frame;
    const char *id;
    const char *name;
    const char *description;
    bool disabled;
};

static ID3Tag TAG_ID3_BAND               = { "TPE2", "ID3.Band",                  N_("Band"),                     N_("Additional information about the performers in the recording.") };
static ID3Tag TAG_ID3_ALBUMSORTORDER     = { "TSOA", "ID3.AlbumSortOrder",        N_("Album Sort Order"),         N_("String which should be used instead of the album name for sorting purposes.") };
static ID3Tag TAG_ID3_AUDIOCRYPTO        = { "AENC", "ID3.AudioCrypto",           N_("Audio Encryption"),         N_("Frame indicates if the audio stream is encrypted, and by whom."), true };
static ID3Tag TAG_ID3_AUDIOSEEKPOINT     = { "ASPI", "ID3.AudioSeekPoint",        N_("Audio Seek Point"),         N_("Fractional offset within the audio data, providing a starting point from which to find an appropriate point to start decoding.") };
static ID3Tag TAG_ID3_BPM                = { "TBPM", "ID3.BPM",                   N_("BPM"),                      N_("BPM (beats per minute).") };
static ID3Tag TAG_ID3_BUFFERSIZE         = { "RBUF", "ID3.BufferSize",            N_("Buffer Size"),              N_("Recommended buffer size."), true };
static ID3Tag TAG_ID3_CDID               = { "MCDI", "ID3.CDID",                  N_("CD ID"),                    N_("Music CD identifier."), true };
static ID3Tag TAG_ID3_COMMERCIAL         = { "COMR", "ID3.Commercial",            N_("Commercial"),               N_("Commercial frame.") };
static ID3Tag TAG_ID3_COMPOSER           = { "TCOM", "ID3.Composer",              N_("Composer"),                 N_("Composer.") };
static ID3Tag TAG_ID3_CONDUCTOR          = { "TPE3", "ID3.Conductor",             N_("Conductor"),                N_("Conductor.") };
static ID3Tag TAG_ID3_CONTENTGROUP       = { "TIT1", "ID3.ContentGroup",          N_("Content Group"),            N_("Content group description.") };
static ID3Tag TAG_ID3_CONTENTTYPE        = { "TCON", "ID3.ContentType",           N_("Content Type"),             N_("Type of music classification for the track as defined in ID3 spec.") };
static ID3Tag TAG_ID3_COPYRIGHT          = { "TCOP", "ID3.Copyright",             N_("Copyright"),                N_("Copyright message.") };
static ID3Tag TAG_ID3_CRYPTOREG          = { "ENCR", "ID3.CryptoReg",             N_("Encryption Registration"),  N_("Encryption method registration.") };
static ID3Tag TAG_ID3_DATE               = { "TDAT", "ID3.Date",                  N_("Date"),                     N_("Date.") };
static ID3Tag TAG_ID3_ENCODEDBY          = { "TENC", "ID3.EncodedBy",             N_("Encoded By"),               N_("Person or organisation that encoded the audio file. This field may contain a copyright message, if the audio file also is copyrighted by the encoder.") };
static ID3Tag TAG_ID3_ENCODERSETTINGS    = { "TSSE", "ID3.EncoderSettings",       N_("Encoder Settings"),         N_("Software.") };
static ID3Tag TAG_ID3_ENCODINGTIME       = { "TDEN", "ID3.EncodingTime",          N_("Encoding Time"),            N_("Encoding time.") };
static ID3Tag TAG_ID3_EQUALIZATION       = { "EQUA", "ID3.Equalization",          N_("Equalization"),             N_("Equalization."), true };
static ID3Tag TAG_ID3_EQUALIZATION2      = { "EQU2", "ID3.Equalization2",         N_("Equalization"),             N_("Equalisation curve predefine within the audio file.") };
static ID3Tag TAG_ID3_EVENTTIMING        = { "ETCO", "ID3.EventTiming",           N_("Event Timing"),             N_("Event timing codes."), true };
static ID3Tag TAG_ID3_FILEOWNER          = { "TFLT", "ID3.FileOwner",             N_("File Owner"),               N_("File owner.") };
static ID3Tag TAG_ID3_FILETYPE           = { "TFLT", "ID3.FileType",              N_("File Type"),                N_("File type.") };
static ID3Tag TAG_ID3_GENERALOBJECT      = { "GEOB", "ID3.GeneralObject",         N_("General Object"),           N_("General encapsulated object.") };
static ID3Tag TAG_ID3_GROUPINGREG        = { "GRID", "ID3.GroupingReg",           N_("Grouping Registration"),    N_("Group identification registration.") };
static ID3Tag TAG_ID3_INITIALKEY         = { "TKEY", "ID3.InitialKey",            N_("Initial Key"),              N_("Initial key.") };
static ID3Tag TAG_ID3_INVOLVEDPEOPLE     = { "TIPL", "ID3.InvolvedPeople",        N_("Involved People"),          N_("Involved people list.") };
static ID3Tag TAG_ID3_INVOLVEDPEOPLE2    = { "IPLS", "ID3.InvolvedPeople2",       N_("Involved People 2"),        N_("Involved people list.") };
static ID3Tag TAG_ID3_LANGUAGE           = { "TLAN", "ID3.Language",              N_("Language"),                 N_("Language.") };
static ID3Tag TAG_ID3_LINKEDINFO         = { "LINK", "ID3.LinkedInfo",            N_("Linked Info"),              N_("Linked information.") };
static ID3Tag TAG_ID3_LYRICIST           = { "TEXT", "ID3.Lyricist",              N_("Lyricist"),                 N_("Lyricist.") };
static ID3Tag TAG_ID3_MEDIATYPE          = { "TMED", "ID3.MediaType",             N_("Media Type"),               N_("Media type.") };
static ID3Tag TAG_ID3_MIXARTIST          = { "TPE4", "ID3.MixArtist",             N_("Mix Artist"),               N_("Interpreted, remixed, or otherwise modified by.") };
static ID3Tag TAG_ID3_MOOD               = { "TMOO", "ID3.Mood",                  N_("Mood"),                     N_("Mood.") };
static ID3Tag TAG_ID3_MPEGLOOKUP         = { "MLLT", "ID3.MPEG.Lookup",           N_("MPEG Lookup"),              N_("MPEG location lookup table."), true };
static ID3Tag TAG_ID3_MUSICIANCREDITLIST = { "TMCL", "ID3.MusicianCreditList",    N_("Musician Credit List"),     N_("Musician credits list.") };
static ID3Tag TAG_ID3_NETRADIOOWNER      = { "TRSO", "ID3.NetRadioOwner",         N_("Net Radio Owner"),          N_("Internet radio station owner.") };
static ID3Tag TAG_ID3_NETRADIOSTATION    = { "TRSN", "ID3.NetRadiostation",       N_("Net Radiostation"),         N_("Internet radio station name.") };
static ID3Tag TAG_ID3_ORIGALBUM          = { "TOAL", "ID3.OriginalAlbum",         N_("Original Album"),           N_("Original album.") };
static ID3Tag TAG_ID3_ORIGARTIST         = { "TOPE", "ID3.OriginalArtist",        N_("Original Artist"),          N_("Original artist.") };
static ID3Tag TAG_ID3_ORIGFILENAME       = { "TOFN", "ID3.OriginalFileName",      N_("Original File Name"),       N_("Original file name.") };
static ID3Tag TAG_ID3_ORIGLYRICIST       = { "TOLY", "ID3.OriginalLyricist",      N_("Original Lyricist"),        N_("Original lyricist.") };
static ID3Tag TAG_ID3_ORIGRELEASETIME    = { "TDOR", "ID3.OriginalReleaseTime",   N_("Original Release Time"),    N_("Original release time.") };
static ID3Tag TAG_ID3_ORIGYEAR           = { "TORY", "ID3.OriginalYear",          N_("Original Year"),            N_("Original release year.") };
static ID3Tag TAG_ID3_OWNERSHIP          = { "OWNE", "ID3.Ownership",             N_("Ownership"),                N_("Ownership frame."), true };
static ID3Tag TAG_ID3_PARTINSET          = { "TPOS", "ID3.PartInSet",             N_("Part of a Set"),            N_("Part of a set the audio came from.") };
static ID3Tag TAG_ID3_PERFORMERSORTORDER = { "TSOP", "ID3.PerformerSortOrder",    N_("Performer Sort Order"),     N_("Performer sort order.") };
static ID3Tag TAG_ID3_PICTURE            = { "APIC", "ID3.Picture",               N_("Picture"),                  N_("Attached picture."), true };
static ID3Tag TAG_ID3_PLAYCOUNTER        = { "PCNT", "ID3.PlayCounter",           N_("Play Counter"),             N_("Number of times a file has been played.") };
static ID3Tag TAG_ID3_PLAYLISTDELAY      = { "TDLY", "ID3.PlaylistDelay",         N_("Playlist Delay"),           N_("Playlist delay.") };
static ID3Tag TAG_ID3_POPULARIMETER      = { "POPM", "ID3.Popularimeter",         N_("Popularimeter"),            N_("Rating of the audio file.") };
static ID3Tag TAG_ID3_POSITIONSYNC       = { "POSS", "ID3.PositionSync",          N_("Position Sync"),            N_("Position synchronisation frame."), true };
static ID3Tag TAG_ID3_PRIVATE            = { "PRIV", "ID3.Private",               N_("Private"),                  N_("Private frame.") };
static ID3Tag TAG_ID3_PRODUCEDNOTICE     = { "TPRO", "ID3.ProducedNotice",        N_("Produced Notice"),          N_("Produced notice.") };
static ID3Tag TAG_ID3_PUBLISHER          = { "TPUB", "ID3.Publisher",             N_("Publisher"),                N_("Publisher.") };
static ID3Tag TAG_ID3_RECORDINGDATES     = { "TRDA", "ID3.RecordingDates",        N_("Recording Dates"),          N_("Recording dates.") };
static ID3Tag TAG_ID3_RECORDINGTIME      = { "TDRC", "ID3.RecordingTime",         N_("Recording Time"),           N_("Recording time.") };
static ID3Tag TAG_ID3_RELEASETIME        = { "TDRL", "ID3.ReleaseTime",           N_("Release Time"),             N_("Release time.") };
static ID3Tag TAG_ID3_REVERB             = { "RVRB", "ID3.Reverb",                N_("Reverb"),                   N_("Reverb."), true };
static ID3Tag TAG_ID3_SETSUBTITLE        = { "TSST", "ID3.SetSubtitle",           N_("Set Subtitle"),             N_("Subtitle of the part of a set this track belongs to.") };
static ID3Tag TAG_ID3_SIGNATURE          = { "SIGN", "ID3.Signature",             N_("Signature"),                N_("Signature frame.") };
static ID3Tag TAG_ID3_SONGLEN            = { "TLEN", "ID3.SongLength",            N_("Song length"),              N_("Length of the song in milliseconds.") };
static ID3Tag TAG_ID3_SUBTITLE           = { "TIT3", "ID3.Subtitle",              N_("Subtitle"),                 N_("Subtitle.") };
static ID3Tag TAG_ID3_SYNCEDLYRICS       = { "SYLT", "ID3.Syncedlyrics",          N_("Synchronized Lyrics"),      N_("Synchronized lyric.") };
static ID3Tag TAG_ID3_SYNCEDTEMPO        = { "SYTC", "ID3.SyncedTempo",           N_("Synchronized Tempo"),       N_("Synchronized tempo codes."), true };
static ID3Tag TAG_ID3_TAGGINGTIME        = { "TDTG", "ID3.TaggingTime",           N_("Tagging Time"),             N_("Tagging time.") };
static ID3Tag TAG_ID3_TERMSOFUSE         = { "USER", "ID3.TermsOfUse",            N_("Terms of Use"),             N_("Terms of use.") };
static ID3Tag TAG_ID3_TIME               = { "TIME", "ID3.Time",                  N_("Time"),                     N_("Time.") };
static ID3Tag TAG_ID3_TITLESORTORDER     = { "TSOT", "ID3.TitleSortOrder",        N_("Title Sort Order"),         N_("Title sort order.") };
static ID3Tag TAG_ID3_UNIQUEFILEID       = { "UFID", "ID3.UniqueFileID",          N_("Unique File ID"),           N_("Unique file identifier.") };
static ID3Tag TAG_ID3_UNSYNCEDLYRICS     = { "USLT", "ID3.UnsyncedLyrics",        N_("Unsynchronized Lyrics"),    N_("Unsynchronized lyric.") };
static ID3Tag TAG_ID3_USERTEXT           = { "TXXX", "ID3.UserText",              N_("User Text"),                N_("User defined text information.") };
static ID3Tag TAG_ID3_VOLUMEADJ          = { "RVAD", "ID3.VolumeAdj",             N_("Volume Adjustment"),        N_("Relative volume adjustment."), true };
static ID3Tag TAG_ID3_VOLUMEADJ2         = { "RVA2", "ID3.VolumeAdj2",            N_("Volume Adjustment"),        N_("Relative volume adjustment.") };
static ID3Tag TAG_ID3_WWWARTIST          = { "WOAR", "ID3.WWWArtist",             N_("WWW Artist"),               N_("Official artist.") };
static ID3Tag TAG_ID3_WWWAUDIOFILE       = { "WOAF", "ID3.WWWAudioFile",          N_("WWW Audio File"),           N_("Official audio file webpage.") };
static ID3Tag TAG_ID3_WWWAUDIOSOURCE     = { "WOAS", "ID3.WWWAudioSource",        N_("WWW Audio Source"),         N_("Official audio source webpage.") };
static ID3Tag TAG_ID3_WWWCOMMERCIALINFO  = { "WCOM", "ID3.WWWCommercialInfo",     N_("WWW Commercial Info"),      N_("URL pointing at a webpage containing commercial information.") };
static ID3Tag TAG_ID3_WWWCOPYRIGHT       = { "WCOP", "ID3.WWWCopyright",          N_("WWW Copyright"),            N_("URL pointing at a webpage that holds copyright.") };
static ID3Tag TAG_ID3_WWWPAYMENT         = { "WPAY", "ID3.WWWPayment",            N_("WWW Payment"),              N_("URL pointing at a webpage that will handle the process of paying for this file.") };
static ID3Tag TAG_ID3_WWWPUBLISHER       = { "WPUB", "ID3.WWWPublisher",          N_("WWW Publisher"),            N_("URL pointing at the official webpage for the publisher.") };
static ID3Tag TAG_ID3_WWWRADIOPAGE       = { "WORS", "ID3.WWWRadioPage",          N_("WWW Radio Page"),           N_("Official internet radio station homepage.") };
static ID3Tag TAG_ID3_WWWUSER            = { "WXXX", "ID3.WWWUser",               N_("WWW User"),                 N_("User defined URL link.") };

static ID3Tag *ID3_TAGS[] = {
    &TAG_ID3_BAND,
    &TAG_ID3_ALBUMSORTORDER,
    &TAG_ID3_AUDIOCRYPTO,
    &TAG_ID3_AUDIOSEEKPOINT,
    &TAG_ID3_BPM,
    &TAG_ID3_BUFFERSIZE,
    &TAG_ID3_CDID,
    &TAG_ID3_COMMERCIAL,
    &TAG_ID3_COMPOSER,
    &TAG_ID3_CONDUCTOR,
    &TAG_ID3_CONTENTGROUP,
    &TAG_ID3_CONTENTTYPE,
    &TAG_ID3_COPYRIGHT,
    &TAG_ID3_CRYPTOREG,
    &TAG_ID3_DATE,
    &TAG_ID3_ENCODEDBY,
    &TAG_ID3_ENCODERSETTINGS,
    &TAG_ID3_ENCODINGTIME,
    &TAG_ID3_EQUALIZATION,
    &TAG_ID3_EQUALIZATION2,
    &TAG_ID3_EVENTTIMING,
    &TAG_ID3_FILEOWNER,
    &TAG_ID3_FILETYPE,
    &TAG_ID3_GENERALOBJECT,
    &TAG_ID3_GROUPINGREG,
    &TAG_ID3_INITIALKEY,
    &TAG_ID3_INVOLVEDPEOPLE,
    &TAG_ID3_INVOLVEDPEOPLE2,
    &TAG_ID3_LANGUAGE,
    &TAG_ID3_LINKEDINFO,
    &TAG_ID3_LYRICIST,
    &TAG_ID3_MEDIATYPE,
    &TAG_ID3_MIXARTIST,
    &TAG_ID3_MOOD,
    &TAG_ID3_MPEGLOOKUP,
    &TAG_ID3_MUSICIANCREDITLIST,
    &TAG_ID3_NETRADIOOWNER,
    &TAG_ID3_NETRADIOSTATION,
    &TAG_ID3_ORIGALBUM,
    &TAG_ID3_ORIGARTIST,
    &TAG_ID3_ORIGFILENAME,
    &TAG_ID3_ORIGLYRICIST,
    &TAG_ID3_ORIGRELEASETIME,
    &TAG_ID3_ORIGYEAR,
    &TAG_ID3_OWNERSHIP,
    &TAG_ID3_PARTINSET,
    &TAG_ID3_PERFORMERSORTORDER,
    &TAG_ID3_PICTURE,
    &TAG_ID3_PLAYCOUNTER,
    &TAG_ID3_PLAYLISTDELAY,
    &TAG_ID3_POPULARIMETER,
    &TAG_ID3_POSITIONSYNC,
    &TAG_ID3_PRIVATE,
    &TAG_ID3_PRODUCEDNOTICE,
    &TAG_ID3_PUBLISHER,
    &TAG_ID3_RECORDINGDATES,
    &TAG_ID3_RECORDINGTIME,
    &TAG_ID3_RELEASETIME,
    &TAG_ID3_REVERB,
    &TAG_ID3_SETSUBTITLE,
    &TAG_ID3_SIGNATURE,
    &TAG_ID3_SONGLEN,
    &TAG_ID3_SUBTITLE,
    &TAG_ID3_SYNCEDLYRICS,
    &TAG_ID3_SYNCEDTEMPO,
    &TAG_ID3_TAGGINGTIME,
    &TAG_ID3_TERMSOFUSE,
    &TAG_ID3_TIME,
    &TAG_ID3_TITLESORTORDER,
    &TAG_ID3_UNIQUEFILEID,
    &TAG_ID3_UNSYNCEDLYRICS,
    &TAG_ID3_USERTEXT,
    &TAG_ID3_VOLUMEADJ,
    &TAG_ID3_VOLUMEADJ2,
    &TAG_ID3_WWWARTIST,
    &TAG_ID3_WWWAUDIOFILE,
    &TAG_ID3_WWWAUDIOSOURCE,
    &TAG_ID3_WWWCOMMERCIALINFO,
    &TAG_ID3_WWWCOPYRIGHT,
    &TAG_ID3_WWWPAYMENT,
    &TAG_ID3_WWWPUBLISHER,
    &TAG_ID3_WWWRADIOPAGE,
    &TAG_ID3_WWWUSER
};

struct VorbisTag
{
    const char *id;
    const char *name;
    const char *description;
};

static VorbisTag TAG_VORBIS_CONTACT        = { "Vorbis.Contact",           N_("Contact"),          N_("Contact information for the creators or distributors of the track.") };
static VorbisTag TAG_VORBIS_DESCRIPTION    = { "Vorbis.Description",       N_("Description"),      N_("A textual description of the data.") };
static VorbisTag TAG_VORBIS_LICENSE        = { "Vorbis.License",           N_("License"),          N_("License information.") };
static VorbisTag TAG_VORBIS_LOCATION       = { "Vorbis.Location",          N_("Location"),         N_("Location where track was recorded.") };
static VorbisTag TAG_VORBIS_MAXBITRATE     = { "Vorbis.MaximumBitrate",    N_("Maximum bitrate"),  N_("Maximum bitrate in kbps.") };
static VorbisTag TAG_VORBIS_MINBITRATE     = { "Vorbis.MinimumBitrate",    N_("Minimum bitrate"),  N_("Minimum bitrate in kbps.") };
static VorbisTag TAG_VORBIS_NOMINALBITRATE = { "Vorbis.NominalBitrate",    N_("Nominal bitrate"),  N_("Nominal bitrate in kbps.") };
static VorbisTag TAG_VORBIS_ORGANIZATION   = { "Vorbis.Organization",      N_("Organization"),     N_("Organization producing the track.") };
static VorbisTag TAG_VORBIS_VENDOR         = { "Vorbis.Vendor",            N_("Vendor"),           N_("Vorbis vendor ID.") };
static VorbisTag TAG_VORBIS_VERSION        = { "Vorbis.Version",           N_("Vorbis Version"),   N_("Vorbis version.") };

static VorbisTag *VORBIS_TAGS[] = {
    &TAG_VORBIS_CONTACT,
    &TAG_VORBIS_DESCRIPTION,
    &TAG_VORBIS_LICENSE,
    &TAG_VORBIS_LOCATION,
    &TAG_VORBIS_MAXBITRATE,
    &TAG_VORBIS_MINBITRATE,
    &TAG_VORBIS_NOMINALBITRATE,
    &TAG_VORBIS_ORGANIZATION,
    &TAG_VORBIS_VENDOR,
    &TAG_VORBIS_VERSION
};


bool getAudioProperties(const TagLib::AudioProperties *properties, GnomeCmdFileMetadataExtractorAddTag add, gpointer user_data)
{
    if (!properties)
        return false;

    char buffer[200];

    snprintf (buffer, sizeof(buffer), "%i", properties->bitrate());
    add(TAG_AUDIO_BITRATE.id, buffer, user_data);

    snprintf (buffer, sizeof(buffer), "%i kbps", properties->bitrate());
    add(TAG_AUDIO_BITRATE_WITH_UNITS.id, buffer, user_data);

    snprintf (buffer, sizeof(buffer), "%i", properties->sampleRate());
    add(TAG_AUDIO_SAMPLERATE.id, buffer, user_data);

    snprintf (buffer, sizeof(buffer), "%i", properties->channels());
    add(TAG_AUDIO_CHANNELS.id, buffer, user_data);

    snprintf (buffer, sizeof(buffer), "%i", properties->length());
    add(TAG_AUDIO_DURATION.id, buffer, user_data);

    snprintf (buffer, sizeof(buffer), "%02i:%02i", properties->length() / 60, properties->length() % 60);
    add(TAG_AUDIO_DURATIONMMSS.id, buffer, user_data);

    const TagLib::MPEG::Properties *mpegProperties = dynamic_cast<const TagLib::MPEG::Properties *>(properties);

    if (mpegProperties)
    {
        static const gchar *channel_mode_names[] =
        {
            N_("Stereo"),
            N_("Joint stereo"),
            N_("Dual channel"),
            N_("Single channel")
        };

        static const gchar *layer_names[] =
        {
            N_("Undefined"),
            N_("Layer I"),
            N_("Layer II"),
            N_("Layer III")
        };

        static const gchar *version_names[] =
        {
            "MPEG 1",
            "MPEG 2",
            "MPEG 2.5",
            N_("Reserved")
        };

        static const gchar *emphasis_names[]
#ifdef __GNUC__
        __attribute__ ((unused))
#endif
        =
        {
            N_("None"),
            N_("10-15ms"),
            N_("Reserved"),
            "CCIT J17"
        };

        add(TAG_AUDIO_MPEG_CHANNELMODE.id, _(channel_mode_names[mpegProperties->channelMode()]), user_data);
        add(TAG_AUDIO_MPEG_LAYER.id, _(layer_names[mpegProperties->layer()]), user_data);

        switch (mpegProperties->version())
        {
            case TagLib::MPEG::Header::Version1:
            case TagLib::MPEG::Header::Version2:
            case TagLib::MPEG::Header::Version2_5:
                add(TAG_AUDIO_MPEG_VERSION.id, _(version_names[mpegProperties->version()]), user_data);
                break;

            default:
                add(TAG_AUDIO_MPEG_VERSION.id, _(version_names[1]), user_data);
                break;
        }

        // cout << "MPEG.Emphasis     " << mpegProperties->emphasis() << endl;

        snprintf(buffer, sizeof(buffer), "%u", mpegProperties->isCopyrighted());
        add(TAG_AUDIO_MPEG_COPYRIGHTED.id, buffer, user_data);

        snprintf(buffer, sizeof(buffer), "%u", mpegProperties->isOriginal());
        add(TAG_AUDIO_MPEG_ORIGINAL.id, buffer, user_data);

        return true;
    }

    const TagLib::Vorbis::Properties *oggProperties = dynamic_cast<const TagLib::Vorbis::Properties *>(properties);

    if (oggProperties)
    {
        if (oggProperties->vorbisVersion())
        {
            snprintf(buffer, sizeof(buffer), "%i", oggProperties->vorbisVersion());
            add(TAG_VORBIS_VERSION.id, buffer, user_data);
        }
        if (oggProperties->bitrateMinimum())
        {
            snprintf(buffer, sizeof(buffer), "%i", oggProperties->bitrateMinimum());
            add(TAG_VORBIS_MINBITRATE.id, buffer, user_data);
        }
        if (oggProperties->bitrateMaximum())
        {
            snprintf(buffer, sizeof(buffer), "%i", oggProperties->bitrateMaximum());
            add(TAG_VORBIS_MAXBITRATE.id, buffer, user_data);
        }
        if (oggProperties->bitrateNominal())
        {
            snprintf(buffer, sizeof(buffer), "%i", oggProperties->bitrateNominal());
            add(TAG_VORBIS_NOMINALBITRATE.id, buffer, user_data);
        }

        return true;
    }

    const TagLib::FLAC::Properties *flacProperties = dynamic_cast<const TagLib::FLAC::Properties *>(properties);

    if (flacProperties)
    {
        add(TAG_AUDIO_CODEC.id, "FLAC", user_data);
        if (flacProperties->bitsPerSample())
            cout << "Audio.FLAC.SampleWidth  " << flacProperties->bitsPerSample()<< endl;

        return true;
    }

    const TagLib::MPC::Properties *mpcProperties = dynamic_cast<const TagLib::MPC::Properties *>(properties);

    if (mpcProperties)
    {
        if (oggProperties && oggProperties->vorbisVersion())
            cout << "Audio.MPC.Version  " << mpcProperties->mpcVersion() << endl;

        return true;
    }

    return true;
}


static ID3Tag *id3TagByFrameId(const string &frameId)
{
    for (auto id3tag : ID3_TAGS)
        if (!id3tag->disabled && frameId == id3tag->frame)
            return id3tag;
    return nullptr;
}


static void readTags(GnomeCmdTaglibPlugin *plugin, const TagLib::ID3v2::Tag *id3v2Tag, GnomeCmdFileMetadataExtractorAddTag add, gpointer user_data)
{
    if (!id3v2Tag)
        return;

    const TagLib::ID3v2::FrameList frameList = id3v2Tag->frameList();

    for (TagLib::ID3v2::FrameList::ConstIterator i = frameList.begin(); i!=frameList.end(); ++i)
    {
        string frame((*i)->frameID().data(), 4);
        string val = (*i)->toString().to8Bit(true);

        if (frame == "TRCK")
        {
            string::size_type pos = val.rfind('/');
            unsigned n = 0;

            if (pos != string::npos && sscanf(val.c_str() + pos, "/%u", &n) && n)
                add(TAG_AUDIO_ALBUMTRACKCOUNT.id, val.c_str() + pos + 1, user_data);
        }
        else if (frame == "TSRC")
        {
            add(TAG_AUDIO_ISRC.id, val.c_str(), user_data);
        }
        else if (auto tag = id3TagByFrameId(frame))
        {
            add(tag->id, val.c_str(), user_data);

            if (tag == &TAG_ID3_PUBLISHER)
                add(TAG_FILE_PUBLISHER, val.c_str(), user_data);
        }
    }
}


bool readTags(GnomeCmdTaglibPlugin *plugin, const TagLib::Ogg::XiphComment *oggTag, GnomeCmdFileMetadataExtractorAddTag add, gpointer user_data)
{
    add(TAG_VORBIS_VENDOR.id, oggTag->vendorID().to8Bit(true).c_str(), user_data);

    for (TagLib::Ogg::FieldListMap::ConstIterator i = oggTag->fieldListMap().begin(); i != oggTag->fieldListMap().end(); ++i)
    {
        string id(i->first.to8Bit(true));
        string val = i->second.toString().to8Bit(true);

        if (id == "COMMENT")              { add(TAG_AUDIO_COMMENT.id,          val.c_str(), user_data); }
        else if (id == "COPYRIGHT")       { add(TAG_AUDIO_COPYRIGHT.id,        val.c_str(), user_data); }
        else if (id == "PERFORMER")       { add(TAG_AUDIO_PERFORMER.id,        val.c_str(), user_data); }
        else if (id == "ISRC")            { add(TAG_AUDIO_ISRC.id,             val.c_str(), user_data); }
        else if (id == "CONTACT")         { add(TAG_VORBIS_CONTACT.id,         val.c_str(), user_data); }
        else if (id == "DESCRIPTION")     { add(TAG_VORBIS_DESCRIPTION.id,     val.c_str(), user_data); }
        else if (id == "LICENSE")         { add(TAG_VORBIS_LICENSE.id,         val.c_str(), user_data); }
        else if (id == "LOCATION")        { add(TAG_VORBIS_LOCATION.id,        val.c_str(), user_data); }
        else if (id == "ORGANIZATION")    { add(TAG_VORBIS_ORGANIZATION.id,    val.c_str(), user_data); }
        else if (id == "PACKAGE_VERSION") { add(TAG_VORBIS_VERSION.id,         val.c_str(), user_data); }
    }

    return true;
}


static bool getTag(GnomeCmdTaglibPlugin *plugin, TagLib::File *file, const TagLib::Tag *tag, GnomeCmdFileMetadataExtractorAddTag add, gpointer user_data)
{
    if (!tag || tag->isEmpty())
        return false;

    add(TAG_AUDIO_ALBUM.id, tag->album().to8Bit(true).c_str(), user_data);
    add(TAG_AUDIO_TITLE.id, tag->title().to8Bit(true).c_str(), user_data);
    add(TAG_AUDIO_ALBUMARTIST.id, tag->artist().to8Bit(true).c_str(), user_data);

    char buffer[32];
    if (tag->year())
    {
        snprintf(buffer, sizeof(buffer), "%u", tag->year());
        add(TAG_AUDIO_YEAR.id, buffer, user_data);
    }
    if (tag->track())
    {
        snprintf(buffer, sizeof(buffer), "%02u", tag->track());
        add(TAG_AUDIO_TRACKNO.id, buffer, user_data);
    }
    add(TAG_AUDIO_GENRE.id, tag->genre().to8Bit(true).c_str(), user_data);
    add(TAG_AUDIO_COMMENT.id, tag->comment().to8Bit(true).c_str(), user_data);

    const TagLib::Ogg::XiphComment *oggTag = dynamic_cast<const TagLib::Ogg::XiphComment *>(tag);

    if (oggTag)
        return readTags(plugin, oggTag, add, user_data);

    const TagLib::APE::Tag *apeTag = dynamic_cast<const TagLib::APE::Tag *>(tag);

    if (apeTag)
        return true;

    TagLib::MPEG::File *mpegFile = dynamic_cast<TagLib::MPEG::File *>(file);

    if (mpegFile)
        readTags(plugin, mpegFile->ID3v2Tag(), add, user_data);

    TagLib::FLAC::File *flacFile = dynamic_cast<TagLib::FLAC::File *>(file);

    if (flacFile)
        readTags(plugin, flacFile->ID3v2Tag(), add, user_data);

    return true;
}


GStrv supported_tags (GnomeCmdFileMetadataExtractor *plugin)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    for (auto tag : AUDIO_TAGS)
        g_strv_builder_add (builder, tag->id);
    for (auto tag : ID3_TAGS)
        if (!tag->disabled)
            g_strv_builder_add (builder, tag->id);
    for (auto tag : VORBIS_TAGS)
        g_strv_builder_add (builder, tag->id);
    GStrv result = g_strv_builder_end (builder);
    g_strv_builder_unref (builder);
    return result;
}


GStrv summary_tags (GnomeCmdFileMetadataExtractor *plugin)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    g_strv_builder_add (builder, TAG_AUDIO_ALBUMARTIST.id);
    g_strv_builder_add (builder, TAG_AUDIO_TITLE.id);
    g_strv_builder_add (builder, TAG_AUDIO_BITRATE_WITH_UNITS.id);
    g_strv_builder_add (builder, TAG_AUDIO_DURATIONMMSS.id);
    GStrv result = g_strv_builder_end (builder);
    g_strv_builder_unref (builder);
    return result;
}


gchar *class_name (GnomeCmdFileMetadataExtractor *plugin, const gchar *cls)
{
    if (!g_strcmp0("Audio", cls))
        return g_strdup(_("Audio"));
    return nullptr;
}


gchar *tag_name (GnomeCmdFileMetadataExtractor *plugin, const gchar *tag_id)
{
    for (auto tag : AUDIO_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->name));
    for (auto tag : ID3_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->name));
    for (auto tag : VORBIS_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->name));
    return nullptr;
}


gchar *tag_description (GnomeCmdFileMetadataExtractor *plugin, const gchar *tag_id)
{
    for (auto tag : AUDIO_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->description));
    for (auto tag : ID3_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->description));
    for (auto tag : VORBIS_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->description));
    return nullptr;
}


void extract_metadata (GnomeCmdFileMetadataExtractor *plugin, GnomeCmdFileDescriptor *fd, GnomeCmdFileMetadataExtractorAddTag add, gpointer user_data)
{
    GFile *file = gnome_cmd_file_descriptor_get_file (fd);
    gchar *fname = g_file_get_path (file);

    TagLib::FileRef f(fname, true, TagLib::AudioProperties::Accurate);

    g_free (fname);

    if (f.isNull())
        return;

    getAudioProperties(f.audioProperties(), add, user_data);
    getTag(GNOME_CMD_TAGLIB_PLUGIN(plugin), f.file(), f.tag(), add, user_data);
}


static void gnome_cmd_taglib_plugin_class_init (GnomeCmdTaglibPluginClass *klass)
{
}


static void gnome_cmd_file_metadata_extractor_init (GnomeCmdFileMetadataExtractorInterface *iface)
{
    iface->supported_tags = supported_tags;
    iface->summary_tags = summary_tags;
    iface->class_name = class_name;
    iface->tag_name = tag_name;
    iface->tag_description = tag_description;
    iface->extract_metadata = extract_metadata;
}


static void gnome_cmd_taglib_plugin_init (GnomeCmdTaglibPlugin *plugin)
{
}


GObject *create_plugin ()
{
    return G_OBJECT (g_object_new (GNOME_CMD_TYPE_TAGLIB_PLUGIN, NULL));
}


GnomeCmdPluginInfo *get_plugin_info ()
{
    static const char *authors[] = {
        "Andrey Kutejko <andy128k@gmail.com>",
        nullptr
    };

    static GnomeCmdPluginInfo info = {
        .plugin_system_version = GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
        .name = "TagLib",
        .version = PACKAGE_VERSION,
        .copyright = "Copyright \xc2\xa9 2025 Andrey Kutejko",
        .comments = nullptr,
        .authors = (gchar **) authors,
        .documenters = nullptr,
        .translator = nullptr,
        .webpage = "https://gcmd.github.io"
    };

    if (!info.comments)
        info.comments = g_strdup (_("A plugin for extracting metadata using TagLib library."));

    return &info;
}
