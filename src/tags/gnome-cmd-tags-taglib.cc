/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include <stdio.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags.h"
#include "utils.h"
#include "dict.h"

#ifdef HAVE_ID3
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
#endif


using namespace std;


#ifdef HAVE_ID3
static DICT<GnomeCmdTag> id3v2tags(TAG_NONE);
static DICT<GnomeCmdTag> oggtags(TAG_NONE);


inline bool getAudioProperties(GnomeCmdFileMetadata &metadata, const TagLib::AudioProperties *properties)
{
    if (!properties)
        return false;

    metadata.addf(TAG_AUDIO_BITRATE,"%i",properties->bitrate());
    metadata.addf(TAG_AUDIO_SAMPLERATE,"%i",properties->sampleRate());
    metadata.addf(TAG_AUDIO_CHANNELS,"%i",properties->channels());
    metadata.addf(TAG_AUDIO_DURATION,"%i",properties->length());
    metadata.addf(TAG_AUDIO_DURATIONMMSS,"%02i:%02i",properties->length()/60,properties->length()%60);

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

        static const gchar *emphasis_names[] =
        {
            N_("None"),
            N_("10-15ms"),
            N_("Reserved"),
            "CCIT J17"
        };

        metadata.add(TAG_AUDIO_MPEG_CHANNELMODE, _(channel_mode_names[mpegProperties->channelMode()]));
        metadata.add(TAG_AUDIO_MPEG_LAYER, _(layer_names[mpegProperties->layer()]));

        switch (mpegProperties->version())
        {
            case TagLib::MPEG::Header::Version1:
            case TagLib::MPEG::Header::Version2:
            case TagLib::MPEG::Header::Version2_5:
                metadata.add(TAG_AUDIO_MPEG_VERSION, _(version_names[mpegProperties->version()]));
                break;

            default:
                metadata.add(TAG_AUDIO_MPEG_VERSION, _(version_names[1]));
                break;
        }

        // cout << "MPEG.Emphasis     " << mpegProperties->emphasis() << endl;
        metadata.addf(TAG_AUDIO_MPEG_COPYRIGHTED, "%u", mpegProperties->isCopyrighted());
        metadata.addf(TAG_AUDIO_MPEG_ORIGINAL, "%u", mpegProperties->isOriginal());

        return true;
    }

    const TagLib::Vorbis::Properties *oggProperties = dynamic_cast<const TagLib::Vorbis::Properties *>(properties);

    if (oggProperties)
    {
        if (oggProperties->vorbisVersion())
            metadata.addf(TAG_VORBIS_VERSION,"%i",oggProperties->vorbisVersion());
        if (oggProperties->bitrateMinimum())
            metadata.addf(TAG_VORBIS_MINBITRATE,"%i",oggProperties->bitrateMinimum());
        if (oggProperties->bitrateMaximum())
            metadata.addf(TAG_VORBIS_MAXBITRATE,"%i",oggProperties->bitrateMaximum());
        if (oggProperties->bitrateNominal())
            metadata.addf(TAG_VORBIS_NOMINALBITRATE,"%i",oggProperties->bitrateNominal());

        return true;
    }

    const TagLib::FLAC::Properties *flacProperties = dynamic_cast<const TagLib::FLAC::Properties *>(properties);

    if (flacProperties)
    {
        metadata.add(TAG_AUDIO_CODEC,"FLAC");
        if (flacProperties->sampleWidth())
            cout << "Audio.FLAC.SampleWidth  " << flacProperties->sampleWidth()<< endl;

        return true;
    }

    const TagLib::MPC::Properties *mpcProperties = dynamic_cast<const TagLib::MPC::Properties *>(properties);

    if (mpcProperties)
    {
        if (oggProperties->vorbisVersion())
            cout << "Audio.MPC.Version  " << mpcProperties->mpcVersion() << endl;

        return true;
    }

    return true;
}


inline void readTags(GnomeCmdFileMetadata &metadata, const TagLib::ID3v2::Tag *id3v2Tag)
{
    if (!id3v2Tag)
        return;

    const TagLib::ID3v2::FrameList frameList = id3v2Tag->frameList();

    for (TagLib::ID3v2::FrameList::ConstIterator i = frameList.begin(); i!=frameList.end(); ++i)
    {
        string id((*i)->frameID().data(),4);
        GnomeCmdTag tag = id3v2tags[id.c_str()];

        string val;

        switch (tag)
        {
            case TAG_NONE:
                continue;

            case TAG_ID3_AUDIOCRYPTO:
            case TAG_ID3_BUFFERSIZE:
            case TAG_ID3_CDID:
            case TAG_ID3_EQUALIZATION:
            case TAG_ID3_EVENTTIMING:
            case TAG_ID3_MPEGLOOKUP:
            case TAG_ID3_OWNERSHIP:
            case TAG_ID3_PICTURE:
            case TAG_ID3_POSITIONSYNC:
            case TAG_ID3_REVERB:
            case TAG_ID3_SYNCEDTEMPO:
            case TAG_ID3_VOLUMEADJ:
                val = "(unimplemented)";
                continue;

            default:
                val = (*i)->toString().to8Bit(true);
                break;
        }

        switch (tag)
        {
            case TAG_ID3_PUBLISHER:
                metadata.add(TAG_FILE_PUBLISHER,val);
                break;

            case TAG_ID3_POPULARIMETER:
                metadata.add(TAG_FILE_RANK,val);
                break;

            case TAG_AUDIO_ALBUMTRACKCOUNT:
                {
                    string::size_type pos = val.rfind('/');
                    unsigned n = 0;

                    if (pos==string::npos || !sscanf(val.c_str()+pos, "/%u", &n) || !n)
                        continue;

                    metadata.addf(tag, "%u", n);
                    DEBUG('t', "\t%s (%s) = %u\n", id.c_str(), gcmd_tags_get_name(tag), n);
                }
                continue;

            default:
                break;
        }

        metadata.add(tag,val);
        DEBUG('t', "\t%s (%s) = %s\n", id.c_str(), gcmd_tags_get_name(tag), val.c_str());
    }
}


inline bool readTags(GnomeCmdFileMetadata &metadata, const TagLib::Ogg::XiphComment *oggTag)
{
    metadata.add(TAG_VORBIS_VENDOR, oggTag->vendorID().to8Bit(true));

    for (TagLib::Ogg::FieldListMap::ConstIterator i = oggTag->fieldListMap().begin(); i != oggTag->fieldListMap().end(); ++i)
    {
        string id(i->first.to8Bit(true));
        GnomeCmdTag tag = oggtags[id.c_str()];

        string val;

        switch (tag)
        {
            case TAG_NONE:
                continue;

            default:
                val = i->second.toString().to8Bit(true);
                break;
        }

        metadata.add(tag,val);
        DEBUG('t', "\t%s (%s) = %s\n", id.c_str(), gcmd_tags_get_name(tag), val.c_str());
    }

    return true;
}


inline bool getTag(GnomeCmdFileMetadata &metadata, TagLib::File *file, const TagLib::Tag *tag)
{
    if (!tag || tag->isEmpty())
        return false;

    metadata.add(TAG_AUDIO_ALBUM, tag->album().to8Bit(true));
    metadata.add(TAG_AUDIO_TITLE, tag->title().to8Bit(true));
    metadata.add(TAG_AUDIO_ALBUMARTIST, tag->artist().to8Bit(true));
    if (tag->year())
        metadata.addf(TAG_AUDIO_YEAR, "%u", tag->year());
    if (tag->track())
        metadata.addf(TAG_AUDIO_TRACKNO, "%02u", tag->track());
    metadata.add(TAG_AUDIO_GENRE, tag->genre().to8Bit(true));
    metadata.add(TAG_AUDIO_COMMENT, tag->comment().to8Bit(true));

    const TagLib::Ogg::XiphComment *oggTag = dynamic_cast<const TagLib::Ogg::XiphComment *>(tag);

    if (oggTag)
        return readTags(metadata, oggTag);

    const TagLib::APE::Tag *apeTag = dynamic_cast<const TagLib::APE::Tag *>(tag);

    if (apeTag)
        return true;

    TagLib::MPEG::File *mpegFile = dynamic_cast<TagLib::MPEG::File *>(file);

    if (mpegFile)
        readTags(metadata, mpegFile->ID3v2Tag());

    TagLib::FLAC::File *flacFile = dynamic_cast<TagLib::FLAC::File *>(file);

    if (flacFile)
        readTags(metadata, flacFile->ID3v2Tag());

    return true;
}
#endif


void gcmd_tags_taglib_init()
{
#ifdef HAVE_ID3
    static struct
    {
        GnomeCmdTag tag;
        const gchar *name;
    }
    id3v2_data[] = {
                    {TAG_AUDIO_ALBUMTRACKCOUNT,"TRCK"},  // Total no. of tracks on the album
                    {TAG_ID3_ALBUMSORTORDER,"TSOA"},  // Album sort order
                    {TAG_ID3_AUDIOCRYPTO,"AENC"},  // Audio encryption
                    {TAG_ID3_AUDIOSEEKPOINT,"ASPI"},  // Audio seek point index
                    {TAG_ID3_BAND,"TPE2"},  // Band/orchestra/accompaniment
                    {TAG_ID3_BPM,"TBPM"},  // BPM (beats per minute)
                    {TAG_ID3_BUFFERSIZE,"RBUF"},  // Recommended buffer size
                    {TAG_ID3_CDID,"MCDI"},  // Music CD identifier
                    {TAG_ID3_COMMERCIAL,"COMR"},  // Commercial frame
                    {TAG_ID3_COMPOSER,"TCOM"},  // Composer
                    {TAG_ID3_CONDUCTOR,"TPE3"},  // Conductor/performer refinement
                    {TAG_ID3_CONTENTGROUP,"TIT1"},  // Content group description
                    {TAG_ID3_COPYRIGHT,"TCOP"},  // Copyright message
                    {TAG_ID3_COPYRIGHT,"WCOP"},  // Copyright/Legal information
                    {TAG_ID3_CRYPTOREG,"ENCR"},  // Encryption method registration
                    {TAG_ID3_ENCODEDBY,"TENC"},  // Encoded by
                    {TAG_ID3_ENCODERSETTINGS,"TSSE"},  // Software/Hardware and settings used for encoding
                    {TAG_ID3_ENCODINGTIME,"TDEN"},  // Encoding time
                    {TAG_ID3_EQUALIZATION2,"EQU2"},  // Equalisation
                    {TAG_ID3_EVENTTIMING,"ETCO"},  // Event timing codes
                    {TAG_ID3_FILETYPE,"TFLT"},  // File type
                    {TAG_ID3_GENERALOBJECT,"GEOB"},  // General encapsulated object
                    {TAG_ID3_GROUPINGREG,"GRID"},  // Group identification registration
                    {TAG_ID3_INITIALKEY,"TKEY"},  // Initial key
                    {TAG_ID3_INVOLVEDPEOPLE,"TIPL"},  // Involved people list
                    {TAG_AUDIO_ISRC,"TSRC"},  // ISRC (international standard recording code)
                    {TAG_ID3_LANGUAGE,"TLAN"},  // Language
                    {TAG_ID3_LINKEDINFO,"LINK"},  // Linked information
                    {TAG_ID3_LYRICIST,"TEXT"},  // Lyricist/Text writer
                    {TAG_ID3_MEDIATYPE,"TMED"},  // Media type
                    {TAG_ID3_MIXARTIST,"TPE4"},  // Interpreted, remixed, or otherwise modified by
                    {TAG_ID3_MOOD,"TMOO"},  // Mood
                    {TAG_ID3_MPEGLOOKUP,"MLLT"},  // MPEG location lookup table
                    {TAG_ID3_MUSICIANCREDITLIST,"TMCL"},  // Musician credits list
                    {TAG_ID3_NETRADIOOWNER,"TRSO"},  // Internet radio station owner
                    {TAG_ID3_NETRADIOSTATION,"TRSN"},  // Internet radio station name
                    {TAG_ID3_ORIGALBUM,"TOAL"},  // Original album/movie/show title
                    {TAG_ID3_ORIGARTIST,"TOPE"},  // Original artist/performer
                    {TAG_ID3_ORIGFILENAME,"TOFN"},  // Original filename
                    {TAG_ID3_ORIGLYRICIST,"TOLY"},  // Original lyricist/text writer
                    {TAG_ID3_ORIGRELEASETIME,"TDOR"},  // Original release time
                    {TAG_ID3_OWNERSHIP,"OWNE"},  // Ownership frame
                    {TAG_ID3_PARTINSET,"TOWN"},  // File owner/licensee
                    {TAG_ID3_PARTINSET,"TPOS"},  // Part of a set
                    {TAG_ID3_PERFORMERSORTORDER,"TSOP"},  // Performer sort order
                    {TAG_ID3_PICTURE,"APIC"},  // Attached picture
                    {TAG_ID3_PLAYCOUNTER,"PCNT"},  // Play counter
                    {TAG_ID3_PLAYLISTDELAY,"TDLY"},  // Playlist delay
                    {TAG_ID3_POPULARIMETER,"POPM"},  // Popularimeter
                    {TAG_ID3_POSITIONSYNC,"POSS"},  // Position synchronisation frame
                    {TAG_ID3_PRODUCEDNOTICE,"TPRO"},  // Produced notice
                    {TAG_ID3_PUBLISHER,"TPUB"},  // Publisher
                    {TAG_ID3_RECORDINGTIME,"TDRC"},  // Recording time
                    {TAG_ID3_RELEASETIME,"TDRL"},  // Release time
                    {TAG_ID3_REVERB,"RVRB"},  // Reverb
                    {TAG_ID3_SETSUBTITLE,"TSST"},  // Set subtitle
                    {TAG_ID3_SIGNATURE,"SIGN"},  // Signature frame
                    {TAG_ID3_SONGLEN,"TLEN"},  // Length
                    {TAG_ID3_SUBTITLE,"TIT3"},  // Subtitle/Description refinement
                    {TAG_ID3_SYNCEDLYRICS,"SYLT"},  // Synchronised lyric/text
                    {TAG_ID3_SYNCEDTEMPO,"SYTC"},  // Synchronised tempo codes
                    {TAG_ID3_TAGGINGTIME,"TDTG"},  // Tagging time
                    {TAG_ID3_TERMSOFUSE,"USER"},  // Terms of use
                    {TAG_ID3_TITLESORTORDER,"TSOT"},  // Title sort order
                    {TAG_ID3_UNIQUEFILEID,"UFID"},  // Unique file identifier
                    {TAG_ID3_UNSYNCEDLYRICS,"USLT"},  // Unsynchronised lyric/text transcription
                    {TAG_ID3_USERTEXT,"TXXX"},  // User defined text information frame
                    {TAG_ID3_VOLUMEADJ2,"RVA2"},  // Relative volume adjustment
                    {TAG_ID3_WWWAUDIOFILE,"WOAF"},  // Official audio file webpage
                    {TAG_ID3_WWWAUDIOSOURCE,"WOAR"},  // Official artist/performer webpage
                    {TAG_ID3_WWWAUDIOSOURCE,"WOAS"},  // Official audio source webpage
                    {TAG_ID3_WWWCOMMERCIALINFO,"WCOM"},  // Commercial information
                    {TAG_ID3_WWWPAYMENT,"WPAY"},  // Payment
                    {TAG_ID3_WWWPUBLISHER,"WPUB"},  // Publishers official webpage
                    {TAG_ID3_WWWRADIOPAGE,"WORS"},  // Official Internet radio station homepage
                    {TAG_ID3_WWWUSER,"WXXX"}   // User defined URL link frame
                   };

    load_data(id3v2tags,id3v2_data,sizeof(id3v2_data)/sizeof(id3v2_data[0]));

    static struct
    {
        GnomeCmdTag tag;
        gchar *name;
    }
    ogg_data[] = {
                  {TAG_AUDIO_COMMENT,"COMMENT"},
                  {TAG_AUDIO_COPYRIGHT,"COPYRIGHT"},
                  {TAG_AUDIO_PERFORMER,"PERFORMER"},
                  {TAG_AUDIO_ISRC,"ISRC"},
                  {TAG_VORBIS_CONTACT,"CONTACT"},
                  {TAG_VORBIS_DESCRIPTION,"DESCRIPTION"},
                  {TAG_VORBIS_LICENSE,"LICENSE"},
                  {TAG_VORBIS_LOCATION,"LOCATION"},
                  {TAG_VORBIS_ORGANIZATION,"ORGANIZATION"},
                  {TAG_VORBIS_VERSION,"VERSION"}
                 };

    load_data(oggtags,ogg_data,sizeof(ogg_data)/sizeof(ogg_data[0]));
#endif
}


void gcmd_tags_taglib_load_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);
    g_return_if_fail (finfo->info != NULL);

#ifdef HAVE_ID3
    if (finfo->metadata && finfo->metadata->is_accessed(TAG_AUDIO))  return;

    if (!finfo->metadata)
        finfo->metadata = new GnomeCmdFileMetadata;

    if (!finfo->metadata)  return;

    finfo->metadata->mark_as_accessed(TAG_AUDIO);
    finfo->metadata->mark_as_accessed(TAG_APE);
    finfo->metadata->mark_as_accessed(TAG_FLAC);
    finfo->metadata->mark_as_accessed(TAG_ID3);
    finfo->metadata->mark_as_accessed(TAG_VORBIS);

    if (!gnome_cmd_file_is_local(finfo))  return;

    const gchar *fname = gnome_cmd_file_get_real_path (finfo);

    DEBUG('t', "Loading audio metadata for '%s'\n", fname);

    TagLib::FileRef f(fname, true, TagLib::AudioProperties::Accurate);

    if (f.isNull())
        return;

    getAudioProperties(*finfo->metadata, f.audioProperties());
    getTag(*finfo->metadata, f.file(), f.tag());
#endif
}
