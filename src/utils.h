/**
 * @file utils.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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
#pragma once

extern gchar *debug_flags;

inline gboolean DEBUG_ENABLED (gchar flag)
{
    return debug_flags ? strchr(debug_flags, flag) != 0 : FALSE;
}

void DEBUG (gchar flag, const gchar *fmt, ...);

void gnome_cmd_show_message (GtkWindow *parent, const gchar *message, const gchar *secondary_text=NULL);
void gnome_cmd_error_message (GtkWindow *parent, const gchar *message, GError *error);

gchar *get_gfile_attribute_string(GFileInfo *gFileInfo, const char *attribute);

gchar *get_gfile_attribute_string(GFile *gFile, const char *attribute);
