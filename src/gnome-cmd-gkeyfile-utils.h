/**
 * @file gnome-cmd-gkeyfile-utils.h
 * @brief Utility functions for working with GKeyFile data structures from GLib
 * @note Originally taken from the GnuCash project
 * (gnc-gkeyfile-utils.h, David Hampton <hampton@employees.org>)
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

#ifndef __GNOME_CMD_GKEYFILE_UTILS_H__
#define __GNOME_CMD_GKEYFILE_UTILS_H__

GKeyFile *gcmd_key_file_load_from_file (const gchar *file,
					gboolean ignore_error);


gboolean gcmd_key_file_save_to_file (const gchar *file,
				     GKeyFile *key_file);

#endif // __GNOME_CMD_GKEYFILE_UTILS_H__

