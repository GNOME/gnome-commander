/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2015 Uwe Scholz

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

#ifndef __GNOME_CMD_HINTBOX_H__
#define __GNOME_CMD_HINTBOX_H__

#define GNOME_CMD_TYPE_HINT_BOX  (gnome_cmd_hint_box_get_type ())

GType      gnome_cmd_hint_box_get_type () G_GNUC_CONST;
GtkWidget *gnome_cmd_hint_box_new (const gchar *hint);

#endif // __GNOME_CMD_HINTBOX_H__
