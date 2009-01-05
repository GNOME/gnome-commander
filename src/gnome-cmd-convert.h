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

#ifndef __GNOME_CMD_CONVERT_H__
#define __GNOME_CMD_CONVERT_H__

typedef gchar *(*GnomeCmdConvertFunc) (gchar *string);

gchar *gcmd_convert_unchanged (gchar *string);

gchar *gcmd_convert_ltrim (gchar *string);
gchar *gcmd_convert_rtrim (gchar *string);
gchar *gcmd_convert_strip (gchar *string);

gchar *gcmd_convert_lowercase (gchar *string);
gchar *gcmd_convert_uppercase (gchar *string);
gchar *gcmd_convert_sentence_case (gchar *string);
gchar *gcmd_convert_initial_caps (gchar *string);
gchar *gcmd_convert_toggle_case (gchar *string);

#endif // __GNOME_CMD_CONVERT_H__
