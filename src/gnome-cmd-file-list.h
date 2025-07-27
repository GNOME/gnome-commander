/**
 * @file gnome-cmd-file-list.h
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

#include <memory>
#include <vector>
#include <functional>
#include "gnome-cmd-dir.h"
#include "gnome-cmd-types.h"

#define GNOME_CMD_TYPE_FILE_LIST              (gnome_cmd_file_list_get_type ())
#define GNOME_CMD_FILE_LIST(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE_LIST, GnomeCmdFileList))
#define GNOME_CMD_FILE_LIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE_LIST, GnomeCmdFileListClass))
#define GNOME_CMD_IS_FILE_LIST(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE_LIST))
#define GNOME_CMD_IS_FILE_LIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE_LIST))
#define GNOME_CMD_FILE_LIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE_LIST, GnomeCmdFileListClass))


extern "C" GType gnome_cmd_file_list_get_type ();


struct GnomeCmdFileList
{
    GtkWidget parent;

  public:

    operator GObject * () const         {  return G_OBJECT (this);         }
    operator GtkWidget * () const       {  return GTK_WIDGET (this);       }

    enum ColumnID
    {
        COLUMN_ICON,
        COLUMN_NAME,
        COLUMN_EXT,
        COLUMN_DIR,
        COLUMN_SIZE,
        COLUMN_DATE,
        COLUMN_PERM,
        COLUMN_OWNER,
        COLUMN_GROUP,
        NUM_COLUMNS
    };

    /**
     * Establish a connection via gnome_cmd_con_open() if it does not
     * already exist, or just set the file list to the last or the
     * current working dir.
     */
    void set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir = nullptr);
    void set_directory(GnomeCmdDir *dir);
    void goto_directory(const gchar *dir);
};

// FFI
extern "C" GnomeCmdCon *gnome_cmd_file_list_get_connection(GnomeCmdFileList *fl);
extern "C" GnomeCmdDir *gnome_cmd_file_list_get_directory(GnomeCmdFileList *fl);
extern "C" void gnome_cmd_file_list_set_directory(GnomeCmdFileList *fl, GnomeCmdDir *dir);

extern "C" gint /* ColumnID */ gnome_cmd_file_list_get_sort_column (GnomeCmdFileList *fl);

extern "C" void gnome_cmd_file_list_set_connection(GnomeCmdFileList *fl, GnomeCmdCon *con, GnomeCmdDir *start_dir);
extern "C" void gnome_cmd_file_list_focus_file(GnomeCmdFileList *fl, const gchar *focus_file, gboolean scroll_to_file);

extern "C" void gnome_cmd_file_list_goto_directory(GnomeCmdFileList *fl, const gchar *dir);
