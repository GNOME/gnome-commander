/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2015 Mamoru TASAKA

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>

void gnome_cmd_mime_config(void)
{
	const gchar *old_xdg_data_dirs = g_getenv("XDG_DATA_DIRS");
	gchar *new_xdg_data_dirs = 
		g_strdup_printf(
				"%s:%s",
				MIMETOP_DIR,
				(old_xdg_data_dirs ? old_xdg_data_dirs : "/usr/share/:/usr/share/local/")
			);
	g_setenv("XDG_DATA_DIRS", new_xdg_data_dirs, 1);
	g_free(new_xdg_data_dirs);
}
