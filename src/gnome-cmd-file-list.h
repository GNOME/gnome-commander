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
#include "gnome-cmd-collection.h"
#include "gnome-cmd-types.h"
#include "filter.h"

#define GNOME_CMD_TYPE_FILE_LIST              (gnome_cmd_file_list_get_type ())
#define GNOME_CMD_FILE_LIST(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE_LIST, GnomeCmdFileList))
#define GNOME_CMD_FILE_LIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE_LIST, GnomeCmdFileListClass))
#define GNOME_CMD_IS_FILE_LIST(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE_LIST))
#define GNOME_CMD_IS_FILE_LIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE_LIST))
#define GNOME_CMD_FILE_LIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE_LIST, GnomeCmdFileListClass))


extern "C" GType gnome_cmd_file_list_get_type ();


/* DnD target names */
#define TARGET_MC_DESKTOP_ICON_TYPE     (gchar*) "application/x-mc-desktop-icon"
#define TARGET_URI_LIST_TYPE            (gchar*) "text/uri-list"
#define TARGET_TEXT_PLAIN_TYPE          (gchar*) "text/plain"
#define TARGET_URL_TYPE                 (gchar*) "_NETSCAPE_URL"

/* Standard DnD types */
enum TargetType
{
    TARGET_MC_DESKTOP_ICON,
    TARGET_URI_LIST,
    TARGET_TEXT_PLAIN,
    TARGET_URL,
    TARGET_NTARGETS
};


using GtkTreeIterPtr = std::unique_ptr<GtkTreeIter, decltype(&gtk_tree_iter_free)>;


struct GnomeCmdFileList
{
    GtkWidget parent;

  private:

    void create_column_titles();

  public:

    struct Private;

    Private *priv;

    GtkWidget *tab_label_pin {nullptr};
    GtkWidget *tab_label_text {nullptr};

    gboolean realized {FALSE};
    gboolean modifier_click {FALSE};

    gboolean locked {FALSE};

    void *operator new (size_t size);
    void operator delete (void *p)      {  g_object_unref (p);  }

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

    GnomeCmdCon *con {nullptr};
    GnomeCmdDir *cwd {nullptr};     // current working dir
    GnomeCmdDir *lwd {nullptr};     // last working dir
    GnomeCmdDir *connected_dir {nullptr};

    GnomeCmdFileList(ColumnID sort_col, GtkSortType sort_order);
    ~GnomeCmdFileList();

    guint size();
    bool empty()                          {  return size() == 0; }
    void clear();

    void reload();

    enum TraverseControl {
        TRAVERSE_BREAK,
        TRAVERSE_CONTINUE,
    };

    TraverseControl traverse_files(std::function<TraverseControl (GnomeCmdFile *f, GtkTreeIter *, GtkListStore *)> visitor);

    void append_file(GnomeCmdFile *f);
    gboolean insert_file(GnomeCmdFile *f);      // Returns TRUE if file added to shown file list, FALSE otherwise
    gboolean remove_file(GnomeCmdFile *f);
    void remove_files(GList *files);
    void remove_all_files()             {  clear();  }

    gboolean has_file(const GnomeCmdFile *f);

    void focus_file_at_row (GtkTreeIter *row);

    void select_all();
    void unselect_all();
    void select_all_files();
    void unselect_all_files();

    void toggle_file(GtkTreeIter *iter);
    void toggle();
    void toggle_and_step();
    void toggle_with_pattern (Filter &pattern, gboolean mode);

    void select(Filter &pattern)                       {  toggle_with_pattern(pattern, TRUE);                           }
    void unselect(Filter &pattern)                     {  toggle_with_pattern(pattern, FALSE);                          }
    void invert_selection();

    void select_row(GtkTreeIter *row);
    GnomeCmdFile *get_file_at_row(GtkTreeIter *row);
    GtkTreeIterPtr get_row_from_file(GnomeCmdFile *f);
    void focus_file(const gchar *focus_file, gboolean scroll_to_file=TRUE);
    void focus_prev();
    void focus_next();

    void sort();

    /**
     * Returns a list with all files shown in the file list. The list is
     * the same as that in the file list it self so make a copy and ref
     * the files if needed
     */
    GList *get_visible_files();

    /**
     * Same as `get_visible_files` but returns a vector
     */
    std::vector<GnomeCmdFile *> get_all_files();

    /**
     * Returns a list with all selected files. The list returned is a
     * copy and should be freed when no longer needed. The files in the
     * list is however not refed before returning
     */
    GList *get_selected_files();

    /**
     * Returns a collection of all selected files.
     * A marked file is a file that has been selected with ins etc. The file that is currently focused is not marked.
     */
    GnomeCmd::Collection<GnomeCmdFile *> get_marked_files();

    GtkTreeIterPtr get_focused_file_iter();

    /**
     * Returns the currently focused file if any. The returned file is
     * not reffed. The ".." file is returned if focused
     */
    GnomeCmdFile *get_focused_file();

    /**
     * Returns the currently focused file if any. The returned file is
     * not reffed. The ".." file is NOT returned if focused
     */
    GnomeCmdFile *get_selected_file();

    /**
     * Returns the first selected file if any or the focused one
     * otherwise. The returned file is not reffed. The ".." file is NOT
     * returned if focused
     */
    GnomeCmdFile *get_first_selected_file();

    gboolean file_is_wanted(GnomeCmdFile *f);

    void update_file(GnomeCmdFile *f);
    void show_files(GnomeCmdDir *dir);
    void show_dir_tree_size(GnomeCmdFile *f);
    void show_visible_tree_sizes();

    void show_column(ColumnID col, gboolean value);
    void resize_column(ColumnID col, gint width);

    ColumnID get_sort_column() const;
    GtkSortType get_sort_order() const;

    void invalidate_tree_size();

    void set_base_dir(gchar *dir);

    /**
     * Establish a connection via gnome_cmd_con_open() if it does not
     * already exist, or just set the file list to the last or the
     * current working dir.
     */
    void set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir = nullptr);
    void set_directory(GnomeCmdDir *dir);
    void goto_directory(const gchar *dir);

    void update_style();

    enum DndMode
    {
        COPY,
        MOVE,
        LINK
    };

    void init_dnd();
    void drop_files(DndMode dndMode, GFileCopyFlags gFileCopyFlags, GList *uri_list, GnomeCmdDir *dir);

    GtkTreeIterPtr get_dest_row_at_pos (gint drag_x, gint drag_y);
    GtkTreeIterPtr get_dest_row_at_coords (gdouble x, gdouble y);
// private:
    void set_selected_at_iter(GtkTreeIter *iter, gboolean selected);
    void select_iter(GtkTreeIter *iter);
    void unselect_iter(GtkTreeIter *iter);
    bool is_selected_iter(GtkTreeIter *iter);

    bool do_file_specific_action (GnomeCmdFile *f);
};


inline void *GnomeCmdFileList::operator new (size_t size)
{
    return g_object_new (GNOME_CMD_TYPE_FILE_LIST, nullptr);
}

inline GnomeCmdFileList::~GnomeCmdFileList()
{
    gnome_cmd_dir_unref (cwd);
    gnome_cmd_dir_unref (lwd);
}

inline void GnomeCmdFileList::remove_files (GList *files)
{
    for (; files; files = files->next)
        remove_file(static_cast<GnomeCmdFile *>(files->data));
}

inline GnomeCmdFile *GnomeCmdFileList::get_selected_file()
{
    GnomeCmdFile *f = get_focused_file();

    return !f || f->is_dotdot ? nullptr : f;
}

void gnome_cmd_file_list_show_delete_dialog (GnomeCmdFileList *fl, gboolean forceDelete = FALSE);
void gnome_cmd_file_list_show_properties_dialog (GnomeCmdFileList *fl);
void gnome_cmd_file_list_show_rename_dialog (GnomeCmdFileList *fl);
void gnome_cmd_file_list_show_selpat_dialog (GnomeCmdFileList *fl, gboolean mode);

void gnome_cmd_file_list_cap_cut (GnomeCmdFileList *fl);
void gnome_cmd_file_list_cap_copy (GnomeCmdFileList *fl);

void gnome_cmd_file_list_show_quicksearch (GnomeCmdFileList *fl, gchar c);

gboolean gnome_cmd_file_list_quicksearch_shown (GnomeCmdFileList *fl);

struct GnomeCmdFileListButtonEvent
{
    GtkTreeIter *iter;
    GnomeCmdFile *file;
    gint button;
    gint n_press;
    double x;
    double y;
    gint state;
};

extern "C" void gnome_cmd_show_new_textfile_dialog(GtkWindow *parent_window, GnomeCmdFileList *fl);

// FFI
extern "C" GList *gnome_cmd_file_list_get_visible_files (GnomeCmdFileList *fl);
extern "C" GList *gnome_cmd_file_list_get_selected_files (GnomeCmdFileList *fl);
extern "C" GnomeCmdFile *gnome_cmd_file_list_get_focused_file(GnomeCmdFileList *fl);
extern "C" GnomeCmdDir *gnome_cmd_file_list_get_cwd(GnomeCmdFileList *fl);
extern "C" GnomeCmdCon *gnome_cmd_file_list_get_connection(GnomeCmdFileList *fl);
extern "C" GnomeCmdDir *gnome_cmd_file_list_get_directory(GnomeCmdFileList *fl);

extern "C" gboolean gnome_cmd_file_list_is_locked (GnomeCmdFileList *fl);

extern "C" void gnome_cmd_file_list_reload (GnomeCmdFileList *fl);

extern "C" void gnome_cmd_file_list_set_connection(GnomeCmdFileList *fl, GnomeCmdCon *con, GnomeCmdDir *start_dir);
extern "C" void gnome_cmd_file_list_focus_file(GnomeCmdFileList *fl, const gchar *focus_file, gboolean scroll_to_file);

extern "C" void gnome_cmd_file_list_toggle_with_pattern(GnomeCmdFileList *fl, Filter *pattern, gboolean mode);

extern "C" void gnome_cmd_file_list_goto_directory(GnomeCmdFileList *fl, const gchar *dir);
