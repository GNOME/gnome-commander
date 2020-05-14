/** 
 * @file gnome-cmd-file-popmenu.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2020 Uwe Scholz\n
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

#include "gnome-cmd-file-list.h"

#define GNOME_CMD_TYPE_FILE_POPMENU              (gnome_cmd_file_popmenu_get_type ())
#define GNOME_CMD_FILE_POPMENU(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE_POPMENU, GnomeCmdFilePopmenu))
#define GNOME_CMD_FILE_POPMENU_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE_POPMENU, GnomeCmdFilePopmenuClass))
#define GNOME_CMD_IS_FILE_POPMENU(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE_POPMENU))
#define GNOME_CMD_IS_FILE_POPMENU_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE_POPMENU))
#define GNOME_CMD_FILE_POPMENU_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE_POPMENU, GnomeCmdFilePopmenuClass))


struct GnomeCmdFilePopmenuPrivate;


struct GnomeCmdFilePopmenu
{
    GtkMenu parent;

    GnomeCmdFilePopmenuPrivate *priv;
};


struct GnomeCmdFilePopmenuClass
{
    GtkMenuClass parent_class;
};


GtkWidget *gnome_cmd_file_popmenu_new (GnomeCmdFileList *fl);

GtkType gnome_cmd_file_popmenu_get_type ();

GtkUIManager *get_file_popup_ui_manager (GnomeCmdFileList *gnomeCmdFileList);
void add_open_with_entries (GtkUIManager *ui_manager, GnomeCmdFileList *gnomeCmdFileList);