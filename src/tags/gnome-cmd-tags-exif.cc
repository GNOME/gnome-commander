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
#include "gnome-cmd-tags-exif.h"

#ifdef HAVE_EXIF
#include <libexif/exif-content.h>
#endif

using namespace std;


static char empty_string[] = "";
static char int_buff[4096];

#ifndef HAVE_EXIF
static char no_support_for_libexif_tags_string[] = N_("<Exif tags not supported>");
#endif


// inline
void gcmd_tags_libexif_load_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);
    g_return_if_fail (finfo->info != NULL);

#ifdef HAVE_EXIF
    if (finfo->exif.accessed)  return;

    finfo->exif.accessed = TRUE;

    if (!gnome_cmd_file_is_local(finfo))  return;

    finfo->exif.metadata = exif_data_new_from_file(gnome_cmd_file_get_real_path(finfo));
#endif
}


void gcmd_tags_libexif_free_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);

#ifdef HAVE_EXIF
    if (finfo->exif.accessed)
        exif_data_free((ExifData *) finfo->exif.metadata);
    finfo->exif.metadata = NULL;
#endif
}


const gchar *gcmd_tags_libexif_get_value(GnomeCmdFile *finfo, guint libtag)
{
#ifdef HAVE_EXIF
    ExifData *data;
    ExifEntry *entry;
#endif

    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_EXIF
    gcmd_tags_libexif_load_metadata(finfo);
    data = (ExifData *) finfo->exif.metadata;
    entry = exif_data_get_entry(data, (ExifTag) libtag);

    if (!entry)
        return NULL;

    exif_entry_get_value(entry, int_buff, sizeof(int_buff));

    return g_strstrip(g_strdelimit(int_buff, "\t\n\r", ' '));
#else
    return no_support_for_libexif_tags_string;
#endif
}


const gchar *gcmd_tags_libexif_get_value_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_EXIF
    return NULL;
#else
    return no_support_for_libexif_tags_string;
#endif
}


const gchar *gcmd_tags_libexif_get_title_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_EXIF
    return empty_string;
#else
    return no_support_for_libexif_tags_string;
#endif
}


const gchar *gcmd_tags_libexif_get_description_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_EXIF
    return empty_string;
#else
    return no_support_for_libexif_tags_string;
#endif
}
