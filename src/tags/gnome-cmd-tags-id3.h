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

#ifndef __GNOME_CMD_TAGS_ID3_H__
#define __GNOME_CMD_TAGS_ID3_H__

#include <config.h>

#include "gnome-cmd-file.h"
#include "gnome-cmd-tags.h"

G_BEGIN_DECLS

void gcmd_tags_id3lib_init();
void gcmd_tags_id3lib_shutdown();
gboolean gcmd_tags_id3lib_is_supported(void);
void gcmd_tags_id3lib_load_metadata(GnomeCmdFile *finfo);
void gcmd_tags_id3lib_free_metadata(GnomeCmdFile *finfo);
const gchar *gcmd_tags_id3lib_get_value(GnomeCmdFile *finfo, guint tag);
const gchar *gcmd_tags_id3lib_get_value_by_name(GnomeCmdFile *finfo, const gchar *tag_name);
const gchar *gcmd_tags_id3lib_get_title_by_name(GnomeCmdFile *finfo, const gchar *tag_name);
const gchar *gcmd_tags_id3lib_get_description_by_name(GnomeCmdFile *finfo, const gchar *tag_name);

G_END_DECLS

#endif // __GNOME_CMD_TAGS_ID3_H__
