/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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

#define GNOME_CMD_FILE_SELECTOR(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_file_selector_get_type (), GnomeCmdFileSelector)
#define GNOME_CMD_FILE_SELECTOR_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_file_selector_get_type (), GnomeCmdFileSelectorClass)
#define GNOME_CMD_IS_FILE_SELECTOR(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_file_selector_get_type ())

#define DIR_HISTORY_SIZE 20

typedef struct _GnomeCmdFileSelector GnomeCmdFileSelector;
typedef struct _GnomeCmdFileSelectorPrivate GnomeCmdFileSelectorPrivate;
typedef struct _GnomeCmdFileSelectorClass GnomeCmdFileSelectorClass;


#include "gnome-cmd-file-list.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-dir.h"

G_BEGIN_DECLS

struct _GnomeCmdFileSelector
{
    GtkVBox vbox;

    GtkWidget *con_btns_hbox;
    GtkWidget *con_hbox;
    GtkWidget *dir_indicator;
    GtkWidget *root_btn;
    GtkWidget *dir_label;
    GtkWidget *scrolledwindow;
    GtkWidget *info_label;
    GtkWidget *list_widget;
    GnomeCmdFileList *list;
    GtkWidget *con_combo;
    GtkWidget *vol_label;

    GnomeCmdFileSelectorPrivate *priv;
};


struct _GnomeCmdFileSelectorClass
{
    GtkVBoxClass parent_class;

    void (* changed_dir)       (GnomeCmdFileSelector *fs,
                                GnomeCmdDir *dir);
};


typedef enum {
    LEFT,
    RIGHT
} FileSelectorID;


GtkType
gnome_cmd_file_selector_get_type         (void);

GtkWidget*
gnome_cmd_file_selector_new              (void);

GnomeCmdDir*
gnome_cmd_file_selector_get_directory    (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_set_directory    (GnomeCmdFileSelector *fs,
                                          GnomeCmdDir *dir);

void
gnome_cmd_file_selector_goto_directory   (GnomeCmdFileSelector *fs,
                                          const gchar *dir);

void
gnome_cmd_file_selector_reload           (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_start_editor      (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_first            (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_back             (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_forward           (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_last              (GnomeCmdFileSelector *fs);

gboolean
gnome_cmd_file_selector_can_back          (GnomeCmdFileSelector *fs);

gboolean
gnome_cmd_file_selector_can_forward       (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_set_active       (GnomeCmdFileSelector *fs,
                                          gboolean value);

void
gnome_cmd_file_selector_update_connections (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_set_connection (GnomeCmdFileSelector *fs,
                                        GnomeCmdCon *con,
                                        GnomeCmdDir *start_dir);

GnomeCmdCon *
gnome_cmd_file_selector_get_connection (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_update_style (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_show_mkdir_dialog (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_show_new_textfile_dialog (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_cap_paste (GnomeCmdFileSelector *fs);

gboolean
gnome_cmd_file_selector_keypressed (GnomeCmdFileSelector *fs,
                                    GdkEventKey *event);

void
gnome_cmd_file_selector_create_symlink (GnomeCmdFileSelector *fs, GnomeCmdFile *finfo);

void
gnome_cmd_file_selector_create_symlinks (GnomeCmdFileSelector *fs, GList *files);

void
gnome_cmd_file_selector_update_conbuttons_visibility (GnomeCmdFileSelector *fs);

void
gnome_cmd_file_selector_show_filter (GnomeCmdFileSelector *fs, gchar c);

G_END_DECLS

#endif // __GNOME_CMD_FILE_SELECTOR_H__
