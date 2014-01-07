/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2014 Uwe Scholz

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

#ifndef __GNOME_CMD_LIST_POPMENU_H__
#define __GNOME_CMD_LIST_POPMENU_H__

#include "gnome-cmd-file-selector.h"

#define GNOME_CMD_TYPE_LIST_POPMENU              (gnome_cmd_list_popmenu_get_type ())
#define GNOME_CMD_LIST_POPMENU(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_LIST_POPMENU, GnomeCmdListPopmenu))
#define GNOME_CMD_LIST_POPMENU_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_LIST_POPMENU, GnomeCmdListPopmenuClass))
#define GNOME_CMD_IS_LIST_POPMENU(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_LIST_POPMENU))
#define GNOME_CMD_IS_LIST_POPMENU_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_LIST_POPMENU))
#define GNOME_CMD_LIST_POPMENU_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_LIST_POPMENU, GnomeCmdListPopmenuClass))


struct GnomeCmdListPopmenu
{
    GtkMenu parent;
};


struct GnomeCmdListPopmenuClass
{
    GtkMenuClass parent_class;
};


GtkWidget *gnome_cmd_list_popmenu_new (GnomeCmdFileSelector *fs);

GtkType gnome_cmd_list_popmenu_get_type ();

#endif // __GNOME_CMD_LIST_POPMENU_H__
