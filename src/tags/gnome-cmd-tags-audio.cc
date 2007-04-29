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

#include <config.h>

#include <stdio.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags-audio.h"
#include "gnome-cmd-tags-id3.h"

using namespace std;


static char empty_string[] = "";


const gchar *gcmd_tags_audio_get_value(GnomeCmdFile *finfo, const GnomeCmdTag tag)
{
    const gchar *ret_val = NULL;

    switch (tag)
    {
        case TAG_AUDIO_ALBUM:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_ALBUM);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_ALBUMARTIST:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_LEADARTIST);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_ALBUMGAIN:
            break;
        case TAG_AUDIO_ALBUMPEAKGAIN:
            break;
        case TAG_AUDIO_ALBUMTRACKCOUNT:
            break;
        case TAG_AUDIO_ARTIST:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_LEADARTIST);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_BITRATE:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_BITRATE);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_CHANNELS:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_CHANNELS);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_CODECVERSION:
            break;
        case TAG_AUDIO_CODEC:
            break;
        case TAG_AUDIO_COMMENT:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_COMMENT);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_COVERALBUMTHUMBNAILPATH:
            break;
        case TAG_AUDIO_DISCNO:
            break;
        case TAG_AUDIO_DURATION:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_DURATION);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_GENRE:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_GENRE);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_ISNEW:
            break;
        case TAG_AUDIO_LASTPLAY:
            break;
        case TAG_AUDIO_LYRICS:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_ORIGLYRICIST);
            if (ret_val && *ret_val)  break;
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_SYNCEDLYRICS);
            if (ret_val && *ret_val)  break;
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_UNSYNCEDLYRICS);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_MBALBUMARTISTID:
            break;
        case TAG_AUDIO_MBALBUMID:
            break;
        case TAG_AUDIO_MBARTISTID:
            break;
        case TAG_AUDIO_MBTRACKID:
            break;
        case TAG_AUDIO_PERFORMER:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_LEADARTIST);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_PLAYCOUNT:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_PLAYCOUNTER);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_RELEASEDATE:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_ORIGRELEASETIME);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_SAMPLERATE:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_SAMPLERATE);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_TITLE:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_TITLE);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_TRACKGAIN:
            break;
        case TAG_AUDIO_TRACKNO:
            ret_val = gcmd_tags_id3lib_get_value(finfo, TAG_ID3_TRACKNUM);
            if (ret_val && *ret_val)  break;
            break;
        case TAG_AUDIO_TRACKPEAKGAIN:
            break;

        default:
            break;
    }

    return ret_val;
}


const gchar *gcmd_tags_audio_get_value_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    return empty_string;
}

