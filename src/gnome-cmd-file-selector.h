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

#ifndef __GNOME_CMD_FILE_SELECTOR_H__
#define __GNOME_CMD_FILE_SELECTOR_H__

#define GNOME_CMD_FILE_SELECTOR(obj)          GTK_CHECK_CAST (obj, gnome_cmd_file_selector_get_type (), GnomeCmdFileSelector)
#define GNOME_CMD_FILE_SELECTOR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnome_cmd_file_selector_get_type (), GnomeCmdFileSelectorClass)
#define GNOME_CMD_IS_FILE_SELECTOR(obj)       GTK_CHECK_TYPE (obj, gnome_cmd_file_selector_get_type ())

struct GnomeCmdMainWin;

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-dir.h"


typedef enum
{
    LEFT,
    RIGHT,
    ACTIVE,
    INACTIVE
} FileSelectorID;


struct GnomeCmdFileSelector
{
    GtkVBox vbox;

    GtkWidget *con_btns_hbox;
    GtkWidget *con_hbox;
    GtkWidget *dir_indicator;
    GtkWidget *dir_label;
    GtkWidget *scrolledwindow;
    GtkWidget *info_label;
    GtkWidget *list_widget;
    GtkWidget *con_combo;
    GtkWidget *vol_label;

  private:

    GnomeCmdFileList *list;

  public:

    class Private;

    Private *priv;

    operator GtkWidget * ()                 {  return GTK_WIDGET (this);  }
    operator GtkBox * ()                    {  return GTK_BOX (this);     }

    GnomeCmdFileList *&file_list()          {  return list;               }

    GnomeCmdDir *get_directory()            {  return file_list()->cwd;   }
    void set_directory(GnomeCmdDir *dir);
    void goto_directory(const gchar *dir);

    void reload();

    void first();
    void back();
    void forward();
    void last();

    gboolean can_back();
    gboolean can_forward();

    void set_active(gboolean value);

    GnomeCmdCon *get_connection()           {  return file_list()->con;   }
    void set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir=NULL);

    gboolean is_local()                     {  return gnome_cmd_con_is_local (get_connection ());  }
    gboolean is_active();

    void show_filter();
    void update_files();
    void update_direntry();
    void update_selected_files_label();
    void update_style();
    void update_connections();
    void update_conbuttons_visibility();
    void update_concombo_visibility();

    gboolean key_pressed(GdkEventKey *event);
};


GtkType gnome_cmd_file_selector_get_type ();
GtkWidget *gnome_cmd_file_selector_new ();

void gnome_cmd_file_selector_set_directory_to_opposite (GnomeCmdMainWin *mw, FileSelectorID fsID);

void gnome_cmd_file_selector_start_editor (GnomeCmdFileSelector *fs);

gboolean gnome_cmd_file_selector_is_local (FileSelectorID fsID);

void gnome_cmd_file_selector_show_new_textfile_dialog (GnomeCmdFileSelector *fs);

void gnome_cmd_file_selector_cap_paste (GnomeCmdFileSelector *fs);

void gnome_cmd_file_selector_create_symlink (GnomeCmdFileSelector *fs, GnomeCmdFile *f);
void gnome_cmd_file_selector_create_symlinks (GnomeCmdFileSelector *fs, GList *files);

inline FileSelectorID operator ! (FileSelectorID id)
{
    switch (id)
    {
        case LEFT:      return RIGHT;
        case RIGHT:     return LEFT;
        case INACTIVE:  return ACTIVE;
        case ACTIVE:    return INACTIVE;

        default:        return id;
    }
}

#endif // __GNOME_CMD_FILE_SELECTOR_H__
