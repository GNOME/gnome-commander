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

#ifndef __GNOME_CMD_FILE_LIST_H__
#define __GNOME_CMD_FILE_LIST_H__

#include "gnome-cmd-file.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-clist.h"

#define GNOME_CMD_TYPE_FILE_LIST         (gnome_cmd_file_list_get_type())
#define GNOME_CMD_FILE_LIST(obj)          GTK_CHECK_CAST (obj, GNOME_CMD_TYPE_FILE_LIST, GnomeCmdFileList)
#define GNOME_CMD_IS_FILE_LIST(obj)       GTK_CHECK_TYPE (obj, GNOME_CMD_TYPE_FILE_LIST)


GtkType gnome_cmd_file_list_get_type ();


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

  private:

    void create_column_titles();

  public:

    struct Private;

    Private *priv;

    gboolean realized;
    gboolean modifier_click;

    void *operator new (size_t size);
    void operator delete (void *p)      {  g_object_unref (p);  }

    operator GtkObject * ()             {  return GTK_OBJECT (this);       }
    operator GtkWidget * ()             {  return GTK_WIDGET (this);       }
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

    GnomeCmdCon *con;
    GnomeCmdDir *cwd, *lwd;         // current & last working dir
    GnomeCmdDir *connected_dir;

    GnomeCmdFileList(GtkSignalFunc handler=NULL, GtkObject *object=NULL);
    ~GnomeCmdFileList();

    int size();
    bool empty();
    // int size()                          {  return g_list_length (get_visible_files());  }
    // bool empty()                        {  return get_visible_files()==NULL;            }    // FIXME should be: size()==0
    void clear();

    void append_file(GnomeCmdFile *f);
    gboolean insert_file(GnomeCmdFile *f);      // Returns TRUE if file added to shown file list, FALSE otherwise
    gboolean remove_file(GnomeCmdFile *f);
    gboolean remove_file(const gchar *uri_str);
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

    GList *get_visible_files();                 // Returns a list with all files shown in the file list. The list is the same as that in the file list it self so make a copy and ref the files if needed
    GList *get_selected_files();                // Returns a list with all selected files. The list returned is a copy and should be freed when no longer needed. The files in the list is however not refed before returning
    GList *get_marked_files();                  // Returns a list with all marked files. The list is the same as that in the file list it self so make a copy and ref the files if needed
                                                // A marked file is a file that has been selected with ins etc. The file that is currently focused is not marked

    GnomeCmdFile *get_focused_file();           // Returns the currently focused file if any. The returned file is not reffed. The ".." file is returned if focused
    GnomeCmdFile *get_selected_file();          // Returns the currently focused file if any. The returned file is not reffed. The ".." file is NOT returned if focused
    GnomeCmdFile *get_first_selected_file();    // Returns the first selected file if any or the focused one otherwise. The returned file is not reffed. The ".." file is NOT returned if focused

    gboolean file_is_wanted(GnomeCmdFile *f);

    void update_file(GnomeCmdFile *f);
    void show_files(GnomeCmdDir *dir);
    void show_dir_tree_size(GnomeCmdFile *f);

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

    void (* file_clicked)        (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *button);
    void (* file_released)       (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *button);
    void (* list_clicked)        (GnomeCmdFileList *fl, GdkEventButton *button);
    void (* empty_space_clicked) (GnomeCmdFileList *fl, GdkEventButton *button);
    void (* files_changed)       (GnomeCmdFileList *fl);
    void (* dir_changed)         (GnomeCmdFileList *fl, GnomeCmdDir *dir);
};


inline void *GnomeCmdFileList::operator new (size_t size)
{
    return g_object_new (GNOME_CMD_TYPE_FILE_LIST, "n-columns", GnomeCmdFileList::NUM_COLUMNS, NULL);
}

inline GnomeCmdFileList::GnomeCmdFileList(GtkSignalFunc handler, GtkObject *object)
{
    realized = FALSE;
    modifier_click = FALSE;
    con = NULL;
    cwd = NULL;
    lwd = NULL;
    connected_dir = NULL;

    create_column_titles();

    if (handler)
        gtk_signal_connect (*this, "files-changed", handler, object);
}

inline GnomeCmdFileList::~GnomeCmdFileList()
{
    gnome_cmd_dir_unref (cwd);
    gnome_cmd_dir_unref (lwd);
}

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


extern GtkTargetEntry drag_types[];
extern GtkTargetEntry drop_types[];

#endif // __GNOME_CMD_FILE_LIST_H__
