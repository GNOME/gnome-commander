/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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

/* Code for ID3 and Ogg Vorbis data processing comes from EasyTAG and is adjusted to GNOME Commander needs.

    id3tag.c - 2001/02/16

    EasyTAG - Tag editor for MP3 and Ogg Vorbis files
    Copyright (C) 2001-2003  Jerome Couderc <easytag@gmail.com> */


#include <config.h>

#include <stdio.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags-id3.h"
#include "id3lib/id3_bugfix.h"
#include "id3lib/genres.h"

#ifdef HAVE_ID3
#include <id3.h>
#endif

static char empty_string[] = "";

#ifndef HAVE_ID3
static char no_support_for_id3lib_tags_string[] = N_("<ID3 tags not supported>");
#endif



#ifdef HAVE_ID3
typedef
struct
{
    ID3_FrameID id3;
    GnomeCmdTag gcmd;
} TAG;


static int id3_tagcmp(const void *t1, const void *t2)
{
    return (int)(((TAG *) t1)->id3) - (int)(((TAG *) t2)->id3);
}


enum {ID3FID_GENRE=ID3FID_LASTFRAMEID+1,
      ID3FID_NUM_FRAMES};


static GnomeCmdTag *get_gcmd_tag(const ID3_FrameID id3_tag)
{
    static TAG tag[] = {{ID3FID_NOFRAME, TAG_NONE},
                        {ID3FID_AUDIOCRYPTO, TAG_ID3_AUDIOCRYPTO},
                        {ID3FID_PICTURE, TAG_ID3_PICTURE},
                        {ID3FID_AUDIOSEEKPOINT, TAG_ID3_AUDIOSEEKPOINT},
                        {ID3FID_COMMENT, TAG_ID3_COMMENT},
                        {ID3FID_COMMERCIAL, TAG_ID3_COMMERCIAL},
                        {ID3FID_CRYPTOREG, TAG_ID3_CRYPTOREG},
                        {ID3FID_EQUALIZATION2, TAG_ID3_EQUALIZATION2},
                        {ID3FID_EQUALIZATION, TAG_ID3_EQUALIZATION},
                        {ID3FID_EVENTTIMING, TAG_ID3_EVENTTIMING},
                        {ID3FID_GENERALOBJECT, TAG_ID3_GENERALOBJECT},
                        {ID3FID_GROUPINGREG, TAG_ID3_GROUPINGREG},
                        {ID3FID_INVOLVEDPEOPLE, TAG_ID3_INVOLVEDPEOPLE},
                        {ID3FID_LINKEDINFO, TAG_ID3_LINKEDINFO},
                        {ID3FID_CDID, TAG_ID3_CDID},
                        {ID3FID_MPEGLOOKUP, TAG_ID3_MPEGLOOKUP},
                        {ID3FID_OWNERSHIP, TAG_ID3_OWNERSHIP},
                        {ID3FID_PRIVATE, TAG_ID3_PRIVATE},
                        {ID3FID_PLAYCOUNTER, TAG_ID3_PLAYCOUNTER},
                        {ID3FID_POPULARIMETER, TAG_ID3_POPULARIMETER},
                        {ID3FID_POSITIONSYNC, TAG_ID3_POSITIONSYNC},
                        {ID3FID_BUFFERSIZE, TAG_ID3_BUFFERSIZE},
                        {ID3FID_VOLUMEADJ2, TAG_ID3_VOLUMEADJ2},
                        {ID3FID_VOLUMEADJ, TAG_ID3_VOLUMEADJ},
                        {ID3FID_REVERB, TAG_ID3_REVERB},
                        {ID3FID_SIGNATURE, TAG_ID3_SIGNATURE},
                        {ID3FID_SYNCEDLYRICS, TAG_ID3_SYNCEDLYRICS},
                        {ID3FID_SYNCEDTEMPO, TAG_ID3_SYNCEDTEMPO},
                        {ID3FID_ALBUM, TAG_ID3_ALBUM},
                        {ID3FID_BPM, TAG_ID3_BPM},
                        {ID3FID_COMPOSER, TAG_ID3_COMPOSER},
                        {ID3FID_CONTENTTYPE, TAG_ID3_CONTENTTYPE},
                        {ID3FID_COPYRIGHT, TAG_ID3_COPYRIGHT},
                        {ID3FID_DATE, TAG_ID3_DATE},
                        {ID3FID_ENCODINGTIME, TAG_ID3_ENCODINGTIME},
                        {ID3FID_PLAYLISTDELAY, TAG_ID3_PLAYLISTDELAY},
                        {ID3FID_ORIGRELEASETIME, TAG_ID3_ORIGRELEASETIME},
                        {ID3FID_RECORDINGTIME, TAG_ID3_RECORDINGTIME},
                        {ID3FID_RELEASETIME, TAG_ID3_RELEASETIME},
                        {ID3FID_TAGGINGTIME, TAG_ID3_TAGGINGTIME},
                        {ID3FID_INVOLVEDPEOPLE2, TAG_ID3_INVOLVEDPEOPLE2},
                        {ID3FID_ENCODEDBY, TAG_ID3_ENCODEDBY},
                        {ID3FID_LYRICIST, TAG_ID3_LYRICIST},
                        {ID3FID_FILETYPE, TAG_ID3_FILETYPE},
                        {ID3FID_TIME, TAG_ID3_TIME},
                        {ID3FID_CONTENTGROUP, TAG_ID3_CONTENTGROUP},
                        {ID3FID_TITLE, TAG_ID3_TITLE},
                        {ID3FID_SUBTITLE, TAG_ID3_SUBTITLE},
                        {ID3FID_INITIALKEY, TAG_ID3_INITIALKEY},
                        {ID3FID_LANGUAGE, TAG_ID3_LANGUAGE},
                        {ID3FID_SONGLEN, TAG_ID3_SONGLEN},
                        {ID3FID_MUSICIANCREDITLIST, TAG_ID3_MUSICIANCREDITLIST},
                        {ID3FID_MEDIATYPE, TAG_ID3_MEDIATYPE},
                        {ID3FID_MOOD, TAG_ID3_MOOD},
                        {ID3FID_ORIGALBUM, TAG_ID3_ORIGALBUM},
                        {ID3FID_ORIGFILENAME, TAG_ID3_ORIGFILENAME},
                        {ID3FID_ORIGLYRICIST, TAG_ID3_ORIGLYRICIST},
                        {ID3FID_ORIGARTIST, TAG_ID3_ORIGARTIST},
                        {ID3FID_ORIGYEAR, TAG_ID3_ORIGYEAR},
                        {ID3FID_FILEOWNER, TAG_ID3_FILEOWNER},
                        {ID3FID_LEADARTIST, TAG_ID3_LEADARTIST},
                        {ID3FID_BAND, TAG_ID3_BAND},
                        {ID3FID_CONDUCTOR, TAG_ID3_CONDUCTOR},
                        {ID3FID_MIXARTIST, TAG_ID3_MIXARTIST},
                        {ID3FID_PARTINSET, TAG_ID3_PARTINSET},
                        {ID3FID_PRODUCEDNOTICE, TAG_ID3_PRODUCEDNOTICE},
                        {ID3FID_PUBLISHER, TAG_ID3_PUBLISHER},
                        {ID3FID_TRACKNUM, TAG_ID3_TRACKNUM},
                        {ID3FID_RECORDINGDATES, TAG_ID3_RECORDINGDATES},
                        {ID3FID_NETRADIOSTATION, TAG_ID3_NETRADIOSTATION},
                        {ID3FID_NETRADIOOWNER, TAG_ID3_NETRADIOOWNER},
                        {ID3FID_SIZE, TAG_ID3_SIZE},
                        {ID3FID_ALBUMSORTORDER, TAG_ID3_ALBUMSORTORDER},
                        {ID3FID_PERFORMERSORTORDER, TAG_ID3_PERFORMERSORTORDER},
                        {ID3FID_TITLESORTORDER, TAG_ID3_TITLESORTORDER},
                        {ID3FID_ISRC, TAG_ID3_ISRC},
                        {ID3FID_ENCODERSETTINGS, TAG_ID3_ENCODERSETTINGS},
                        {ID3FID_SETSUBTITLE, TAG_ID3_SETSUBTITLE},
                        {ID3FID_USERTEXT, TAG_ID3_USERTEXT},
                        {ID3FID_YEAR, TAG_ID3_YEAR},
                        {ID3FID_UNIQUEFILEID, TAG_ID3_UNIQUEFILEID},
                        {ID3FID_TERMSOFUSE, TAG_ID3_TERMSOFUSE},
                        {ID3FID_UNSYNCEDLYRICS, TAG_ID3_UNSYNCEDLYRICS},
                        {ID3FID_WWWCOMMERCIALINFO, TAG_ID3_WWWCOMMERCIALINFO},
                        {ID3FID_WWWCOPYRIGHT, TAG_ID3_WWWCOPYRIGHT},
                        {ID3FID_WWWAUDIOFILE, TAG_ID3_WWWAUDIOFILE},
                        {ID3FID_WWWARTIST, TAG_ID3_WWWARTIST},
                        {ID3FID_WWWAUDIOSOURCE, TAG_ID3_WWWAUDIOSOURCE},
                        {ID3FID_WWWRADIOPAGE, TAG_ID3_WWWRADIOPAGE},
                        {ID3FID_WWWPAYMENT, TAG_ID3_WWWPAYMENT},
                        {ID3FID_WWWPUBLISHER, TAG_ID3_WWWPUBLISHER},
                        {ID3FID_WWWUSER, TAG_ID3_WWWUSER},
                        {ID3FID_GENRE, TAG_ID3_GENRE}};


    if (id3_tag>=ID3FID_NUM_FRAMES)
        return &(tag[0].gcmd);

    TAG t;

    t.id3 = id3_tag;

    TAG *elem = (TAG *) bsearch(&t,tag,sizeof(tag)/sizeof(TAG),sizeof(TAG),id3_tagcmp);

    return elem ? &(elem->gcmd) : &(tag[0].gcmd);
}


static gchar *get_genre (guint genre_code)
{
    if (genre_code>=ID3_INVALID_GENRE)    /* empty */
        return "";
    else if (genre_code>GENRE_MAX)        /* unknown tag */
        return _("Unknown");
    else                                  /* known tag */
        return id3_genres[genre_code];
}


static void ID3Frame_GetString(GHashTable *metadata, const ID3Frame *frame)
{
    ID3Field *field = NULL;
    size_t size;
    gchar *s = NULL;

    ID3_FrameID frame_ID = ID3Frame_GetID(frame);
    GnomeCmdTag *id = get_gcmd_tag(frame_ID);

    switch (frame_ID)
    {
        case ID3FID_WWWARTIST:
        case ID3FID_WWWAUDIOFILE:
        case ID3FID_WWWAUDIOSOURCE:
        case ID3FID_WWWCOMMERCIALINFO:
        case ID3FID_WWWCOPYRIGHT:
        case ID3FID_WWWPAYMENT:
        case ID3FID_WWWPUBLISHER:
        case ID3FID_WWWRADIOPAGE:
        case ID3FID_WWWUSER:
            field = ID3Frame_GetField(frame, ID3FN_URL);
            if (!field)  return;

            break;

        case ID3FID_PRIVATE:
            return;

        case ID3FID_AUDIOCRYPTO:
        case ID3FID_BUFFERSIZE:
        case ID3FID_CDID:
        case ID3FID_EQUALIZATION:
        case ID3FID_EVENTTIMING:
        case ID3FID_METACRYPTO:
        case ID3FID_MPEGLOOKUP:
        case ID3FID_OWNERSHIP:
        case ID3FID_POSITIONSYNC:
        case ID3FID_REVERB:
        case ID3FID_SYNCEDTEMPO:
        case ID3FID_VOLUMEADJ:
            g_hash_table_replace(metadata, id, g_strdup("(unimplemented)"));
            return;

        case ID3FID_PLAYCOUNTER:
            field = ID3Frame_GetField(frame, ID3FN_COUNTER);
            if (!field)  return;

            g_hash_table_replace(metadata, id, g_strdup_printf ("%u", ID3Field_GetINT(field)));
            return;

        default:
            field = ID3Frame_GetField(frame, ID3FN_TEXT);
            if (!field)  return;

            break;
    }

    size = ID3Field_Size (field);
    s = g_try_malloc0 (size+1);   // since glib >= 2.8

    if (!s)  return;

    ID3Field_GetASCII(field, s, size);
    g_strstrip (s);

    switch (frame_ID)
    {
        case ID3FID_CONTENTTYPE:
            {
                /*
                 * We manipulate only the name of the genre
                 * Genre is written like this :
                 *    - "(<genre_id>)"              -> "(3)"
                 *    - "<genre_name>"              -> "Dance"
                 *    - "(<genre_id>)<refinement>"  -> "(3)EuroDance"
                 */
                gchar *tmp;
                gchar *s0 = s;

                if ((s[0]=='(') && (tmp=strchr(s,')')) && (strlen((tmp+1))>0))  // Convert a genre written as '(3)EuroDance' into 'EuroDance'
                {
                    s = g_strdup(tmp+1);
                    g_free(s0);
                }
                else
                if ((s[0]=='(') && strchr(s,')'))                               // Convert a genre written as '(3)' into 'EuroDance'
                {
                    s = g_strdup(get_genre(atoi(s+1)));
                    g_free(s0);
                }

                g_hash_table_replace(metadata, get_gcmd_tag(ID3FID_GENRE), g_strdup(s));
            }
            break;

        case ID3FID_TRACKNUM:
            {
                gchar *s0 = s;
                s = g_strdup_printf("%02u", atoi(s));                           // Just to have numbers like this : '01', '05', '12', ...
                g_free(s0);
            }
            break;

        default:
            break;
    }

    if (*s)
        g_hash_table_replace(metadata, id, s);
    else
        g_free(s);
}


static void Read_MPEG_Header_Info (GHashTable *metadata, ID3Tag *id3_tag)
{
    static const gchar *layer_names[] =
    {
        N_("Undefined"),
        N_("Layer III"),
        N_("Layer II"),
        N_("Layer I")
    };

    static const gchar *version_names[] =
    {
        "MPEG2.5",
        N_("Reserved"),
        "MPEG2",
        "MPEG1"
    };

    static const gchar *channel_mode_names[] =
    {
        N_("Stereo"),
        N_("Joint stereo"),
        N_("Dual channel"),
        N_("Single channel")
    };

    static const gchar *emphasis_names[] =
    {
        N_("None"),
        N_("10-15ms"),
        N_("Reserved"),
        N_("CCIT J17")
    };

    const Mp3_Headerinfo *headerInfo = ID3Tag_GetMp3HeaderInfo(id3_tag);

    if (!headerInfo)
        return;

    switch (headerInfo->layer)
    {
        case MPEGLAYER_I:
        case MPEGLAYER_II:
        case MPEGLAYER_III:
        case MPEGLAYER_UNDEFINED:
            g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_MPEGLAYER), g_strdup(_(layer_names[headerInfo->layer])));
            break;

        default:
            break;
    }

    switch (headerInfo->version)
    {
        case MPEGVERSION_2_5:
        case MPEGVERSION_2:
        case MPEGVERSION_1:
            g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_MPEGVERSION), g_strdup(_(version_names[headerInfo->version])));
            break;
        case MPEGVERSION_Reserved:
            g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_MPEGVERSION), g_strdup(_("Reserved")));
            break;

        default:
            break;
    }

    switch (headerInfo->channelmode)
    {
        case MP3CHANNELMODE_STEREO:
        case MP3CHANNELMODE_JOINT_STEREO:
        case MP3CHANNELMODE_DUAL_CHANNEL:
            g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_CHANNELMODE), g_strdup(_(channel_mode_names[headerInfo->channelmode])));
            g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_CHANNELS), g_strdup("2"));
            break;

        case MP3CHANNELMODE_SINGLE_CHANNEL:
            g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_CHANNELMODE), g_strdup(_(channel_mode_names[headerInfo->channelmode])));
            g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_CHANNELS), g_strdup("1"));
            break;

        default:
            break;
    }

    switch (headerInfo->emphasis)
    {
        case MPEGLAYER_I:
        case MPEGLAYER_II:
        case MPEGLAYER_III:
        case MPEGLAYER_UNDEFINED:
            g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_EMPHASIS), g_strdup(_(emphasis_names[headerInfo->emphasis])));
            break;

        default:
            break;
    }

    g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_SAMPLERATE), g_strdup_printf ("%u", headerInfo->frequency));

    if (headerInfo->vbr_bitrate>0)
        g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_BITRATE), g_strdup_printf ("%u", headerInfo->vbr_bitrate/1000));
    else
        g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_BITRATE), g_strdup_printf ("%u", headerInfo->bitrate/1000));

    g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_DURATION), g_strdup_printf ("%u", headerInfo->time));

    g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_DURATIONMMSS), g_strdup_printf ("%u:%02u", headerInfo->time/60, headerInfo->time%60));

    g_hash_table_replace(metadata, gcmd_tags_get_pointer_to_tag(TAG_ID3_FRAMES), g_strdup_printf ("%u", headerInfo->frames));
}
#endif


// inline
void gcmd_tags_id3lib_load_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);
    g_return_if_fail (finfo->info != NULL);

#ifdef HAVE_ID3
    if (finfo->id3.accessed)  return;

    finfo->id3.accessed = TRUE;

    if (!gnome_cmd_file_is_local(finfo))  return;

    finfo->id3.metadata = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, g_free);

    ID3Tag *id3_tag = ID3Tag_New();

    ID3Tag_Link(id3_tag, gnome_cmd_file_get_real_path(finfo));

    ID3TagConstIterator *iter = ID3Tag_CreateConstIterator(id3_tag);
    const ID3Frame *frame;

    Read_MPEG_Header_Info(finfo->id3.metadata, id3_tag);

    for (frame=ID3TagConstIterator_GetNext(iter); frame; frame=ID3TagConstIterator_GetNext(iter))
        ID3Frame_GetString(finfo->id3.metadata, frame);

    ID3TagConstIterator_Delete(iter);
    ID3Tag_Delete(id3_tag);
#endif
}


void gcmd_tags_id3lib_free_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);

#ifdef HAVE_ID3
    if (finfo->id3.accessed)
        g_hash_table_destroy(finfo->id3.metadata);
    finfo->id3.metadata = NULL;
#endif
}


// ID3FN_NOFIELD = 0,    /**< No field */
// ID3FN_TEXTENC,        /**< Text encoding (unicode or ASCII) */
// ID3FN_TEXT,           /**< Text field */
// ID3FN_URL,            /**< A URL */
// ID3FN_DATA,           /**< Data field */
// ID3FN_DESCRIPTION,    /**< Description field */
// ID3FN_OWNER,          /**< Owner field */
// ID3FN_EMAIL,          /**< Email field */
// ID3FN_RATING,         /**< Rating field */
// ID3FN_FILENAME,       /**< Filename field */
// ID3FN_LANGUAGE,       /**< Language field */
// ID3FN_PICTURETYPE,    /**< Picture type field */
// ID3FN_IMAGEFORMAT,    /**< Image format field */
// ID3FN_MIMETYPE,       /**< Mimetype field */
// ID3FN_COUNTER,        /**< Counter field */
// ID3FN_ID,             /**< Identifier/Symbol field */
// ID3FN_VOLUMEADJ,      /**< Volume adjustment field */
// ID3FN_NUMBITS,        /**< Number of bits field */
// ID3FN_VOLCHGRIGHT,    /**< Volume chage on the right channel */
// ID3FN_VOLCHGLEFT,     /**< Volume chage on the left channel */
// ID3FN_PEAKVOLRIGHT,   /**< Peak volume on the right channel */
// ID3FN_PEAKVOLLEFT,    /**< Peak volume on the left channel */
// ID3FN_TIMESTAMPFORMAT,/**< SYLT Timestamp Format */
// ID3FN_CONTENTTYPE,    /**< SYLT content type */

// ID3FID_ALBUM,             /**< Album/Movie/Show title */
// ID3FID_ALBUMSORTORDER,    /**< Album sort order */
// ID3FID_AUDIOCRYPTO,       /**< Audio encryption */
// ID3FID_AUDIOSEEKPOINT,    /**< Audio seek point index */
// ID3FID_BAND,              /**< Band/orchestra/accompaniment */
// ID3FID_BPM,               /**< BPM (beats per minute) */
// ID3FID_BUFFERSIZE,        /**< Recommended buffer size */
// ID3FID_CDID,              /**< Music CD identifier */
// ID3FID_COMMENT,           /**< Comments */
// ID3FID_COMMERCIAL,        /**< Commercial frame */
// ID3FID_COMPOSER,          /**< Composer */
// ID3FID_CONDUCTOR,         /**< Conductor/performer refinement */
// ID3FID_CONTENTGROUP,      /**< Content group description */
// ID3FID_CONTENTTYPE,       /**< Content type */
// ID3FID_COPYRIGHT,         /**< Copyright message */
// ID3FID_CRYPTOREG,         /**< Encryption method registration */
// ID3FID_DATE,              /**< Date */
// ID3FID_ENCODEDBY,         /**< Encoded by */
// ID3FID_ENCODERSETTINGS,   /**< Software/Hardware and settings used for encoding */
// ID3FID_ENCODINGTIME,      /**< Encoding time */
// ID3FID_EQUALIZATION,      /**< Equalization */
// ID3FID_EQUALIZATION2,     /**< Equalisation (2) */
// ID3FID_EVENTTIMING,       /**< Event timing codes */
// ID3FID_FILEOWNER,         /**< File owner/licensee */
// ID3FID_FILETYPE,          /**< File type */
// ID3FID_GENERALOBJECT,     /**< General encapsulated object */
// ID3FID_GROUPINGREG,       /**< Group identification registration */
// ID3FID_INITIALKEY,        /**< Initial key */
// ID3FID_INVOLVEDPEOPLE,    /**< Involved people list */
// ID3FID_INVOLVEDPEOPLE2,   /**< Involved people list */
// ID3FID_ISRC,              /**< ISRC (international standard recording code) */
// ID3FID_LANGUAGE,          /**< Language(s) */
// ID3FID_LEADARTIST,        /**< Lead performer(s)/Soloist(s) */
// ID3FID_LINKEDINFO,        /**< Linked information */
// ID3FID_LYRICIST,          /**< Lyricist/Text writer */
// ID3FID_MEDIATYPE,         /**< Media type */
// ID3FID_METACOMPRESSION,   /**< Compressed meta frame (id3v2.2.1) */
// ID3FID_METACRYPTO,        /**< Encrypted meta frame (id3v2.2.x) */
// ID3FID_MIXARTIST,         /**< Interpreted, remixed, or otherwise modified by */
// ID3FID_MOOD,              /**< Mood */
// ID3FID_MPEGLOOKUP,        /**< MPEG location lookup table */
// ID3FID_MUSICIANCREDITLIST,/**< Musician credits list */
// ID3FID_NETRADIOOWNER,     /**< Internet radio station owner */
// ID3FID_NETRADIOSTATION,   /**< Internet radio station name */
// ID3FID_ORIGALBUM,         /**< Original album/movie/show title */
// ID3FID_ORIGARTIST,        /**< Original artist(s)/performer(s) */
// ID3FID_ORIGFILENAME,      /**< Original filename */
// ID3FID_ORIGLYRICIST,      /**< Original lyricist(s)/text writer(s) */
// ID3FID_ORIGRELEASETIME,   /**< Original release time */
// ID3FID_ORIGYEAR,          /**< Original release year */
// ID3FID_OWNERSHIP,         /**< Ownership frame */
// ID3FID_PARTINSET,         /**< Part of a set */
// ID3FID_PERFORMERSORTORDER,/**< Performer sort order */
// ID3FID_PICTURE,           /**< Attached picture */
// ID3FID_PLAYCOUNTER,       /**< Play counter */
// ID3FID_PLAYLISTDELAY,     /**< Playlist delay */
// ID3FID_POPULARIMETER,     /**< Popularimeter */
// ID3FID_POSITIONSYNC,      /**< Position synchronisation frame */
// ID3FID_PRIVATE,           /**< Private frame */
// ID3FID_PRODUCEDNOTICE,    /**< Produced notice */
// ID3FID_PUBLISHER,         /**< Publisher */
// ID3FID_RECORDINGDATES,    /**< Recording dates */
// ID3FID_RECORDINGTIME,     /**< Recording time */
// ID3FID_RELEASETIME,       /**< Release time */
// ID3FID_REVERB,            /**< Reverb */
// ID3FID_SEEKFRAME,         /**< Seek frame */
// ID3FID_SETSUBTITLE,       /**< Set subtitle */
// ID3FID_SIGNATURE,         /**< Signature frame */
// ID3FID_SIZE,              /**< Size */
// ID3FID_SONGLEN,           /**< Length */
// ID3FID_SUBTITLE,          /**< Subtitle/Description refinement */
// ID3FID_SYNCEDLYRICS,      /**< Synchronized lyric/text */
// ID3FID_SYNCEDTEMPO,       /**< Synchronized tempo codes */
// ID3FID_TAGGINGTIME,       /**< Tagging time */
// ID3FID_TERMSOFUSE,        /**< Terms of use */
// ID3FID_TIME,              /**< Time */
// ID3FID_TITLE,             /**< Title/songname/content description */
// ID3FID_TITLESORTORDER,    /**< Title sort order */
// ID3FID_TRACKNUM,          /**< Track number/Position in set */
// ID3FID_UNIQUEFILEID,      /**< Unique file identifier */
// ID3FID_UNSYNCEDLYRICS,    /**< Unsynchronized lyric/text transcription */
// ID3FID_USERTEXT,          /**< User defined text information */
// ID3FID_VOLUMEADJ,         /**< Relative volume adjustment */
// ID3FID_VOLUMEADJ2,        /**< Relative volume adjustment (2) */
// ID3FID_WWWARTIST,         /**< Official artist/performer webpage */
// ID3FID_WWWAUDIOFILE,      /**< Official audio file webpage */
// ID3FID_WWWAUDIOSOURCE,    /**< Official audio source webpage */
// ID3FID_WWWCOMMERCIALINFO, /**< Commercial information */
// ID3FID_WWWCOPYRIGHT,      /**< Copyright/Legal infromation */
// ID3FID_WWWPAYMENT,        /**< Payment */
// ID3FID_WWWPUBLISHER,      /**< Official publisher webpage */
// ID3FID_WWWRADIOPAGE,      /**< Official internet radio station homepage */
// ID3FID_WWWUSER,           /**< User defined URL link */
// ID3FID_YEAR,              /**< Year */


const gchar *gcmd_tags_id3lib_get_value(GnomeCmdFile *finfo, guint tag)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_ID3
    gcmd_tags_id3lib_load_metadata(finfo);

    const gchar *value = (const gchar *) g_hash_table_lookup(finfo->id3.metadata, &tag);

    return value ? value : empty_string;
#else
    return no_support_for_id3lib_tags_string;
#endif
}


const gchar *gcmd_tags_id3lib_get_value_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_ID3
    return NULL;
#else
    return no_support_for_id3lib_tags_string;
#endif
}


const gchar *gcmd_tags_id3lib_get_title_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_ID3
    return empty_string;
#else
    return no_support_for_id3lib_tags_string;
#endif
}


const gchar *gcmd_tags_id3lib_get_description_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_ID3
    return empty_string;
#else
    return no_support_for_id3lib_tags_string;
#endif
}
