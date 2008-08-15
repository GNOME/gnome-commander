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

#ifndef __GNOME_CMD_FILE_LIST_H__
#define __GNOME_CMD_FILE_LIST_H__


#define GNOME_CMD_FILE_LIST(obj)          GTK_CHECK_CAST (obj, gnome_cmd_file_list_get_type (), GnomeCmdFileList)
#define GNOME_CMD_FILE_LIST_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnome_cmd_file_list_get_type (), GnomeCmdFileListClass)
#define GNOME_CMD_IS_FILE_LIST(obj)       GTK_CHECK_TYPE (obj, gnome_cmd_file_list_get_type ())

#include "gnome-cmd-file.h"
#include "gnome-cmd-clist.h"


/* DnD target names */
#define TARGET_MC_DESKTOP_ICON_TYPE     "application/x-mc-desktop-icon"
#define TARGET_URI_LIST_TYPE            "text/uri-list"
#define TARGET_TEXT_PLAIN_TYPE          "text/plain"
#define TARGET_URL_TYPE                 "_NETSCAPE_URL"

/* Standard DnD types */
enum TargetType
{
    TARGET_MC_DESKTOP_ICON,
    TARGET_URI_LIST,
    TARGET_TEXT_PLAIN,
    TARGET_URL,
    TARGET_NTARGETS
};


struct GnomeCmdFileList
{
    GnomeCmdCList parent;

    struct Private;

    Private *priv;

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

    int size();
    bool empty()                {  return size()==0;  }
    void clear();

    void append_file(GnomeCmdFile *f);
    void insert_file(GnomeCmdFile *f);
    void remove_file(GnomeCmdFile *f);
    void remove_file(const gchar *uri_str);
    void remove_files(GList *files);
    void remove_all_files()     {  clear();  }

    void select_all();
    void unselect_all();

    void sort();
    GList *sort_selection(GList *list);

    void show_column(ColumnID col, gboolean value)     {  gtk_clist_set_column_visibility (GTK_CLIST (this), col, value); }

    ColumnID get_sort_column();
    static guint get_column_default_width(ColumnID col);

    void invalidate_tree_size();

    gboolean key_pressed(GdkEventKey *event);
};


struct GnomeCmdFileListClass
{
    GnomeCmdCListClass parent_class;

    void (* file_clicked)        (GnomeCmdFileList *fl, GnomeCmdFile *finfo, GdkEventButton *button);
    void (* list_clicked)        (GnomeCmdFileList *fl, GdkEventButton *button);
    void (* empty_space_clicked) (GnomeCmdFileList *fl, GdkEventButton *button);
    void (* selection_changed)   (GnomeCmdFileList *fl);
};


extern GtkTargetEntry drag_types[];
extern GtkTargetEntry drop_types[];


GtkType gnome_cmd_file_list_get_type (void);
GtkWidget *gnome_cmd_file_list_new (void);

inline void GnomeCmdFileList::remove_files (GList *files)
{
    for (; files; files = files->next)
        remove_file((GnomeCmdFile *) files->data);
}

void gnome_cmd_file_list_show_files (GnomeCmdFileList *fl, GList *files, gboolean sort);
void gnome_cmd_file_list_update_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo);

void gnome_cmd_file_list_update_style (GnomeCmdFileList *fl);

void gnome_cmd_file_list_show_dir_size (GnomeCmdFileList *fl, GnomeCmdFile *finfo);

GList *gnome_cmd_file_list_get_all_files (GnomeCmdFileList *fl);
GList *gnome_cmd_file_list_get_selected_files (GnomeCmdFileList *fl);
GList *gnome_cmd_file_list_get_marked_files (GnomeCmdFileList *fl);

inline int GnomeCmdFileList::size()
{
    return g_list_length (gnome_cmd_file_list_get_all_files (this));
}

GnomeCmdFile *gnome_cmd_file_list_get_focused_file (GnomeCmdFileList *fl);
GnomeCmdFile *gnome_cmd_file_list_get_selected_file (GnomeCmdFileList *fl);
GnomeCmdFile *gnome_cmd_file_list_get_first_selected_file (GnomeCmdFileList *fl);

void gnome_cmd_file_list_toggle (GnomeCmdFileList *fl);
void gnome_cmd_file_list_toggle_and_step (GnomeCmdFileList *fl);

void gnome_cmd_file_list_focus_file (GnomeCmdFileList *fl, const gchar *focus_file, gboolean scroll_to_file);

void gnome_cmd_file_list_select_row (GnomeCmdFileList *fl, gint row);

void gnome_cmd_file_list_select_pattern (GnomeCmdFileList *fl, const gchar *pattern, gboolean case_sens);
void gnome_cmd_file_list_unselect_pattern (GnomeCmdFileList *fl, const gchar *pattern, gboolean case_sens);
void gnome_cmd_file_list_invert_selection (GnomeCmdFileList *fl);
void gnome_cmd_file_list_select_all_with_same_extension (GnomeCmdFileList *fl);
void gnome_cmd_file_list_unselect_all_with_same_extension (GnomeCmdFileList *fl);
void gnome_cmd_file_list_restore_selection (GnomeCmdFileList *fl);

void gnome_cmd_file_list_compare_directories (GnomeCmdFileList *fl1, GnomeCmdFileList *fl2);

GnomeCmdFile *gnome_cmd_file_list_get_file_at_row (GnomeCmdFileList *fl, gint row);

gint gnome_cmd_file_list_get_row_from_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo);

void gnome_cmd_file_list_show_advrename_dialog (GnomeCmdFileList *fl);
void gnome_cmd_file_list_show_chmod_dialog (GnomeCmdFileList *fl);
void gnome_cmd_file_list_show_chown_dialog (GnomeCmdFileList *fl);
void gnome_cmd_file_list_show_delete_dialog (GnomeCmdFileList *fl);
void gnome_cmd_file_list_show_properties_dialog (GnomeCmdFileList *fl);
void gnome_cmd_file_list_show_rename_dialog (GnomeCmdFileList *fl);
void gnome_cmd_file_list_show_selpat_dialog (GnomeCmdFileList *fl, gboolean mode);

void gnome_cmd_file_list_cap_cut (GnomeCmdFileList *fl);
void gnome_cmd_file_list_cap_copy (GnomeCmdFileList *fl);

void gnome_cmd_file_list_view (GnomeCmdFileList *fl, gint internal_viewer);

void gnome_cmd_file_list_edit (GnomeCmdFileList *fl);

void gnome_cmd_file_list_show_quicksearch (GnomeCmdFileList *fl, gchar c);

gboolean gnome_cmd_file_list_quicksearch_shown (GnomeCmdFileList *fl);

#endif // __GNOME_CMD_FILE_LIST_H__
