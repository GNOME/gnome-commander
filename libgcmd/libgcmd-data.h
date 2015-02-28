/** 
 * @file libgcmd-data.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2015 Uwe Scholz\n
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

#ifndef __LIBGCMD_DATA_H__
#define __LIBGCMD_DATA_H__

void gnome_cmd_data_set_string (const gchar *path, const gchar *value);

void gnome_cmd_data_set_int (const gchar *path, int value);

void gnome_cmd_data_set_bool (const gchar *path, gboolean value);

void gnome_cmd_data_set_color (const gchar *path, GdkColor *color);

gchar *gnome_cmd_data_get_string (const gchar *path, const gchar *def);

gint gnome_cmd_data_get_int (const gchar *path, int def);

gboolean gnome_cmd_data_get_bool (const gchar *path, gboolean def);

void gnome_cmd_data_get_color (const gchar *path, GdkColor *color);

#endif //__LIBGCMD_DATA_H__
