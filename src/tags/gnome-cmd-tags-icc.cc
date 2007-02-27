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
#include "gnome-cmd-tags-icc.h"

#ifdef HAVE_LCMS
#include <icc34.h>
#include <lcms.h>
#endif

using namespace std;


static char empty_string[] = "";
static char int_buff[4096];

#ifndef HAVE_LCMS
static char no_support_for_icclib_tags_string[] = N_("<ICC tags not supported>");
#endif


// inline
void gcmd_tags_icclib_load_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);
    g_return_if_fail (finfo->info != NULL);

#ifdef HAVE_LCMS
    if (finfo->icc.accessed)  return;

    finfo->icc.accessed = TRUE;

    if (!gnome_cmd_file_is_local(finfo))  return;
#endif
}


void gcmd_tags_icclib_free_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);

#ifdef HAVE_LCMS
    finfo->icc.metadata = NULL;
#endif
}


const gchar *gcmd_tags_icclib_get_value(GnomeCmdFile *finfo, guint libclass, guint libtag)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_LCMS
    return empty_string;
#else
    return no_support_for_icclib_tags_string;
#endif
}


const gchar *gcmd_tags_icclib_get_value_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_LCMS
    return empty_string;
#else
    return no_support_for_icclib_tags_string;
#endif
}


const gchar *gcmd_tags_icclib_get_title_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_LCMS
    return empty_string;
#else
    return no_support_for_icclib_tags_string;
#endif
}


const gchar *gcmd_tags_icclib_get_description_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    g_return_val_if_fail (finfo != NULL, NULL);

#ifdef HAVE_LCMS
    return empty_string;
#else
    return no_support_for_icclib_tags_string;
#endif
}
