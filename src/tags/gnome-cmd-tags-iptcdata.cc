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
#include "gnome-cmd-tags-iptcdata.h"

#ifdef HAVE_IPTC
#include <libiptcdata/iptc-data.h>
#endif

using namespace std;


static char empty_string[] = "";
static char int_buff[4096];

#ifndef HAVE_IPTC
static char no_support_for_libiptcdata_tags_string[] = N_("<IPTC tags not supported>");
#endif


// inline
void gcmd_tags_libiptcdata_load_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);
    g_return_if_fail (finfo->info != NULL);

#ifdef HAVE_IPTC
    if (finfo->iptc.accessed)  return;

    finfo->iptc.accessed = TRUE;

    if (!gnome_cmd_file_is_local(finfo))  return;

    finfo->iptc.metadata = iptc_data_new_from_jpeg(gnome_cmd_file_get_real_path(finfo));

    if (finfo->iptc.metadata)
        iptc_data_sort((IptcData *) finfo->iptc.metadata);
#endif
}


void gcmd_tags_libiptcdata_free_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);

#ifdef HAVE_IPTC
    if (finfo->iptc.metadata)
        iptc_data_unref((IptcData *) finfo->iptc.metadata);

    finfo->iptc.metadata = NULL;
#endif
}


const gchar *gcmd_tags_libiptcdata_get_value(GnomeCmdFile *finfo, guint libclass, guint libtag)
{
    g_return_val_if_fail (finfo != NULL, NULL);
#ifdef HAVE_IPTC
    gcmd_tags_libiptcdata_load_metadata(finfo);

    IptcDataSet *ds = iptc_data_get_dataset((IptcData *) finfo->iptc.metadata, (IptcRecord) libclass, (IptcTag) libtag);

    char *dest = int_buff;
    guint avail_dest = sizeof(int_buff);

    *dest = '\0';

    if (!ds)
        return NULL;

    for (ds = iptc_data_get_dataset((IptcData *) finfo->iptc.metadata, (IptcRecord) libclass, (IptcTag) libtag); ds; ds = iptc_data_get_next_dataset ((IptcData *) finfo->iptc.metadata, ds, (IptcRecord) libclass, (IptcTag) libtag))
    {
        if (avail_dest<sizeof(int_buff) && avail_dest>2)
        {
            dest = g_stpcpy(dest, " ");
            avail_dest += 1;              //  += strlen(" ")
        }

        if (iptc_dataset_get_as_str (ds, dest, avail_dest))
        {
            unsigned len = strlen(g_strstrip(dest));

            avail_dest -= len;
            dest += len;
        }

        iptc_dataset_unref(ds);
    }

    return int_buff;
#else
    return no_support_for_libiptcdata_tags_string;
#endif
}


const gchar *gcmd_tags_libiptcdata_get_value_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
#ifdef HAVE_IPTC
    IptcDataSet *ds;
    IptcRecord record;
    IptcTag tag;

    g_return_val_if_fail (finfo != NULL, NULL);

    gcmd_tags_libiptcdata_load_metadata(finfo);

    if (iptc_tag_find_by_name ("Keywords", &record, &tag) < 0)
        return NULL;

    ds = iptc_data_get_dataset((IptcData *) finfo->iptc.metadata,record,tag);

    return ds ? iptc_dataset_get_as_str(ds,int_buff,sizeof(int_buff)) : NULL;
#else
    return no_support_for_libiptcdata_tags_string;
#endif
}


const gchar *gcmd_tags_libiptcdata_get_title_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
#ifdef HAVE_IPTC
    IptcRecord record;
    IptcTag tag;

    g_return_val_if_fail (finfo != NULL, NULL);
    g_return_val_if_fail (finfo->iptc.metadata != NULL, empty_string);

    if (iptc_tag_find_by_name ("Keywords", &record, &tag) < 0)
        return empty_string;

    return iptc_tag_get_title(record,tag);
#else
    return no_support_for_libiptcdata_tags_string;
#endif
}


const gchar *gcmd_tags_libiptcdata_get_description_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
#ifdef HAVE_IPTC
    IptcRecord record;
    IptcTag tag;

    g_return_val_if_fail (finfo != NULL, NULL);
    g_return_val_if_fail (finfo->iptc.metadata != NULL, empty_string);

    if (iptc_tag_find_by_name ("Keywords", &record, &tag) < 0)
        return empty_string;

    return iptc_tag_get_description(record,tag);
#else
    return no_support_for_libiptcdata_tags_string;
#endif
}
