/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2008 Piotr Eljasiak

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

    operator GtkObject * ()             {  return GTK_OBJECT (this);       }
    operator GtkCList * ()              {  return GTK_CLIST (this);        }
    operator GnomeCmdCList * ()         {  return GNOME_CMD_CLIST (this);  }

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
    bool empty();
    // int size()                          {  return g_list_length (get_visible_files());  }
    // bool empty()                        {  return get_visible_files()==NULL;            }    // FIXME should be: size()==0
    void clear();

    void append_file(GnomeCmdFile *f);
    void insert_file(GnomeCmdFile *f);
    void remove_file(GnomeCmdFile *f);
    void remove_file(const gchar *uri_str);
    void remove_files(GList *files);
    void remove_all_files()             {  clear();  }

    gboolean has_file(const GnomeCmdFile *f);

    void select_all();
    void unselect_all();

    void select_pattern(const gchar *pattern, gboolean case_sens);
    void unselect_pattern(const gchar *pattern, gboolean case_sens);
    void select_all_with_same_extension();
    void unselect_all_with_same_extension();
    void invert_selection();
    void restore_selection();

    void select_row(gint row);
    GnomeCmdFile *get_file_at_row(gint row)            {  return (GnomeCmdFile *) gtk_clist_get_row_data (*this, row);  }
    gint get_row_from_file(GnomeCmdFile *f)            {  return gtk_clist_find_row_from_data (*this, f);               }
    void focus_file(const gchar *focus_file, gboolean scroll_to_file=TRUE);

    void toggle();
    void toggle_and_step();

    void sort();
    GList *sort_selection(GList *list);

    GList *get_visible_files();                 // Returns a list with all files shown in the file list. The list is the same as that in the file-list it self so make a copy and ref the files if needed
    GList *get_selected_files();                // Returns a list with all selected files. The list returned is a copy and should be freed when no longer needed. The files in the list is however not refed before returning
    GList *get_marked_files();                  // Returns a list with all marked files. The list returned is a copy and should be freed when no longer needed. The files in the list is however not refed before returning
                                                // A marked file is a file that has been selected with ins etc. The file that is currently focused is not marked

    GnomeCmdFile *get_focused_file();           // Returns the currently focused file if any. The returned file is not reffed. The ".." file is returned if focused
    GnomeCmdFile *get_selected_file();          // Returns the currently focused file if any. The returned file is not reffed. The ".." file is NOT returned if focused
    GnomeCmdFile *get_first_selected_file();    // Returns the first selected file if any or the focused one otherwise. The returned file is not reffed. The ".." file is NOT returned if focused

    gboolean file_is_wanted(GnomeCmdFile *f);

    void update_file(GnomeCmdFile *f);
    void show_files(GnomeCmdDir *dir);
    void show_dir_size(GnomeCmdFile *f);

    void show_column(ColumnID col, gboolean value)     {  gtk_clist_set_column_visibility (GTK_CLIST (this), col, value);  }

    ColumnID get_sort_column();
    static guint get_column_default_width(ColumnID col);

    void invalidate_tree_size();

    void update_style();

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


GtkType gnome_cmd_file_list_get_type ();
GtkWidget *gnome_cmd_file_list_new ();

inline int GnomeCmdFileList::size()
{
    return g_list_length (get_visible_files());
}

inline bool GnomeCmdFileList::empty()
{
    return get_visible_files()==NULL;
}

inline void GnomeCmdFileList::remove_files (GList *files)
{
    for (; files; files = files->next)
        remove_file((GnomeCmdFile *) files->data);
}

inline gboolean GnomeCmdFileList::has_file(const GnomeCmdFile *f)
{
    return g_list_index (get_visible_files(), f) != -1;
}

inline GnomeCmdFile *GnomeCmdFileList::get_selected_file()
{
    GnomeCmdFile *f = get_focused_file();

    return !f || strcmp (f->info->name, "..") == 0 ? NULL : f;
}

void gnome_cmd_file_list_compare_directories (GnomeCmdFileList *fl1, GnomeCmdFileList *fl2);

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
