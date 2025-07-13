/**
 * @file gnome-cmd-file-list.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 * @copyright (C) 2024 Andrey Kutejko\n
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

#include <config.h>
#include <stdio.h>
#include <glib-object.h>
#include <algorithm>
#include <numeric>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-path.h"
#include "utils.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-file-collection.h"
#include "text-utils.h"
#include "widget-factory.h"

using namespace std;


/* Controls if file-uris should be escaped for local files when drag-N-dropping
 * Setting this seems be more portable when dropping on old file-managers as gmc etc.
 */
#define UNESCAPE_LOCAL_FILES

/* The time (in ms) it takes from that the right mouse button is clicked on a file until
 * the popup menu appears when the right btn is used to select files.
 */
#define POPUP_TIMEOUT 750


#define FL_PBAR_MAX 50


#define FILE_CLICKED_SIGNAL     "file-clicked"
#define FILE_RELEASED_SIGNAL    "file-released"
#define LIST_CLICKED_SIGNAL     "list-clicked"
#define FILES_CHANGED_SIGNAL    "files-changed"
#define DIR_CHANGED_SIGNAL      "dir-changed"
#define CON_CHANGED_SIGNAL      "con-changed"
#define FILE_ACTIVATED_SIGNAL   "file-activated"
#define CMDLINE_APPEND_SIGNAL   "cmdline-append"
#define CMDLINE_EXECUTE_SIGNAL  "cmdline-execute"


static const char *drag_types [] =
{
    TARGET_URI_LIST_TYPE,
    TARGET_TEXT_PLAIN_TYPE,
    TARGET_URL_TYPE
};

static const char *drop_types [] =
{
    TARGET_URI_LIST_TYPE,
    TARGET_URL_TYPE
};


extern "C" gboolean gnome_cmd_file_is_wanted(GnomeCmdFile *f);


extern "C" void gnome_cmd_file_list_sort (GnomeCmdFileList *fl);


enum DataColumns {
    DATA_COLUMN_FILE = GnomeCmdFileList::NUM_COLUMNS,
    DATA_COLUMN_ICON_NAME,
    DATA_COLUMN_SELECTED,
    NUM_DATA_COLUMNS,
};

const gint FILE_COLUMN = GnomeCmdFileList::NUM_COLUMNS;


struct GnomeCmdFileListPrivate
{
    gchar *base_dir;

    gchar *focus_later;

    // dropping files
    GList *dropping_files;
    GnomeCmdDir *dropping_to;

    gboolean realized;
    gboolean modifier_click;

    GnomeCmdCon *con;
    GnomeCmdDir *cwd;     // current working dir
    GnomeCmdDir *lwd;     // last working dir
    GnomeCmdDir *connected_dir;
};


static GnomeCmdFileListPrivate* file_list_priv (GnomeCmdFileList *fl);
static GtkListStore* file_list_store (GnomeCmdFileList *fl);


static GtkPopover *create_dnd_popup (GnomeCmdFileList *fl, GList *gFileGlist, GnomeCmdDir* to)
{
    auto priv = file_list_priv (fl);
    g_clear_list (&priv->dropping_files, g_object_unref);
    priv->dropping_files = gFileGlist;

    priv->dropping_to = to;

    GMenu *menu = g_menu_new ();

    GMenu *section = g_menu_new ();
    {
        GMenuItem *item = g_menu_item_new (_("_Copy here"), NULL);
        g_menu_item_set_action_and_target (item, "fl.drop-files", "i", GnomeCmdFileList::DndMode::COPY);
        g_menu_append_item (section, item);
    }
    {
        GMenuItem *item = g_menu_item_new (_("_Move here"), NULL);
        g_menu_item_set_action_and_target (item, "fl.drop-files", "i", GnomeCmdFileList::DndMode::MOVE);
        g_menu_append_item (section, item);
    }
    {
        GMenuItem *item = g_menu_item_new (_("_Link here"), NULL);
        g_menu_item_set_action_and_target (item, "fl.drop-files", "i", GnomeCmdFileList::DndMode::LINK);
        g_menu_append_item (section, item);
    }
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (section));
    g_menu_append (menu,  _("C_ancel"), "fl.drop-files-cancel");

    GtkWidget *dnd_popup = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (dnd_popup, GTK_WIDGET (fl));

    return GTK_POPOVER (dnd_popup);
}


extern "C" void gnome_cmd_file_list_drop_files (GnomeCmdFileList *fl, gint parameter)
{
    auto priv = file_list_priv (fl);

    auto mode = static_cast<GnomeCmdFileList::DndMode>(parameter);

    auto gFileGlist = priv->dropping_files;
    priv->dropping_files = nullptr;

    auto to = priv->dropping_to;
    priv->dropping_to = nullptr;

    fl->drop_files(mode, G_FILE_COPY_NONE, gFileGlist, to);
}


extern "C" void gnome_cmd_file_list_drop_files_cancel (GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);

    g_clear_list (&priv->dropping_files, g_object_unref);
    priv->dropping_to = nullptr;
}


inline gchar *strip_extension (const gchar *fname)
{
    gchar *s = g_strdup (fname);
    gchar *p = strrchr(s,'.');
    if (p && p != s)
        *p = '\0';
    return s;
}


extern "C" GIcon *gnome_cmd_icon_cache_get_file_icon(GnomeCmdFile *file, GnomeCmdLayout layout);

static void set_model_row(GnomeCmdFileList *fl, GtkTreeIter *iter, GnomeCmdFile *f, bool tree_size)
{
    auto priv = file_list_priv (fl);
    auto model = file_list_store (fl);

    gchar *dpath;
    gchar *fname;
    gchar *fext;

    GnomeCmdExtDispMode ext_disp_mode;
    GnomeCmdLayout layout;
    GnomeCmdSizeDispMode size_disp_mode;
    GnomeCmdPermDispMode perm_disp_mode;
    gchar *date_format;
    g_object_get (fl,
        "extension-display-mode", &ext_disp_mode,
        "graphical-layout-mode", &layout,
        "size-display-mode", &size_disp_mode,
        "permissions-display-mode", &perm_disp_mode,
        "date-display-format", &date_format,
        nullptr);

    if (layout == GNOME_CMD_LAYOUT_TEXT)
        gtk_list_store_set (model, iter, DATA_COLUMN_ICON_NAME, (gchar *) f->get_type_string(), -1);
    else
    {
        GIcon *icon = gnome_cmd_icon_cache_get_file_icon(f, layout);
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_ICON, icon, -1);
        g_object_unref(icon);
    }

    // Prepare the strings to show
    gchar *t1 = f->GetPathStringThroughParent();
    gchar *t2 = g_path_get_dirname (t1);
    dpath = get_utf8 (t2);
    g_free (t1);
    g_free (t2);

    if (ext_disp_mode == GNOME_CMD_EXT_DISP_STRIPPED
        && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_REGULAR)
    {
        fname = strip_extension (f->get_name());
    }
    else
    {
        fname = g_strdup(f->get_name());
    }

    if (priv->base_dir != nullptr)
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_DIR, g_strconcat(get_utf8("."), dpath + strlen(priv->base_dir), nullptr), -1);
    else
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_DIR, dpath, -1);

    if (ext_disp_mode != GNOME_CMD_EXT_DISP_WITH_FNAME)
        fext = get_utf8 (f->get_extension());
    else
        fext = nullptr;

    //Set other file information
    gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_NAME, fname, -1);
    gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_EXT, fext, -1);

    gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_SIZE, tree_size ? (gchar *) f->get_tree_size_as_str(size_disp_mode) : (gchar *) f->get_size(size_disp_mode), -1);

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY || !gnome_cmd_file_is_dotdot (f))
    {
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_DATE, (gchar *) f->get_mdate(date_format), -1);
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_PERM, (gchar *) f->get_perm(perm_disp_mode), -1);
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_OWNER, (gchar *) f->get_owner(), -1);
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_GROUP, (gchar *) f->get_group(), -1);
    }

    gtk_list_store_set (model, iter,
        DATA_COLUMN_FILE, f,
        DATA_COLUMN_SELECTED, FALSE,
        -1);

    g_free (dpath);
    g_free (fname);
    g_free (fext);
    g_free (date_format);
}


GnomeCmdFileListPrivate* file_list_priv (GnomeCmdFileList *fl)
{
    return (GnomeCmdFileListPrivate *) g_object_get_data (G_OBJECT (fl), "priv");
}


GtkTreeView* file_list_view (GnomeCmdFileList *fl)
{
    GtkTreeView* view;
    g_object_get (G_OBJECT (fl), "view", &view, nullptr);
    return view;
}


GtkListStore* file_list_store (GnomeCmdFileList *fl)
{
    GtkListStore* store;
    g_object_get (G_OBJECT (fl), "store", &store, nullptr);
    return store;
}


void GnomeCmdFileList::focus_file_at_row (GtkTreeIter *row)
{
    auto view =  file_list_view (this);
    auto store = file_list_store (this);

    GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), row);
    gtk_tree_view_set_cursor (view, path, nullptr, false);
    gtk_tree_path_free (path);
}


guint GnomeCmdFileList::size()
{
    auto store = file_list_store (this);
    return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), nullptr);
}


void GnomeCmdFileList::toggle_file(GtkTreeIter *iter)
{
    auto store = file_list_store (this);

    gboolean selected = FALSE;
    GnomeCmdFile *file = nullptr;
    gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
        DATA_COLUMN_FILE, &file,
        DATA_COLUMN_SELECTED, &selected,
        -1);

    if (gnome_cmd_file_is_dotdot (file))
        return;

    selected = !selected;
    gtk_list_store_set (store, iter, DATA_COLUMN_SELECTED, selected, -1);

    g_signal_emit_by_name (this, FILES_CHANGED_SIGNAL);
}


static gint iter_compare(GtkListStore *store, GtkTreeIter *iter1, GtkTreeIter *iter2)
{
    GtkTreePath *path1 = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter1);
    GtkTreePath *path2 = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter2);
    gint result = gtk_tree_path_compare (path1, path2);
    gtk_tree_path_free (path1);
    gtk_tree_path_free (path2);
    return result;
}


extern "C" void update_column_sort_arrows (GnomeCmdFileList *fl);


/******************************************************
 * DnD functions
 **/

static GString *build_selected_file_list (GnomeCmdFileList *fl)
{
    GString *list = g_string_new(nullptr);
    fl->traverse_files ([fl, list](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (fl->is_selected_iter(iter))
        {
            auto uriString = file->get_uri_str();
            g_string_append (list, uriString);
            g_string_append (list, "\r\n");
        }
        return GnomeCmdFileList::TRAVERSE_CONTINUE;
    });
    if (list->len == 0)
    {
        auto file = fl->get_selected_file();
        auto uriString = file->get_uri_str();
        g_string_append (list, uriString);
    }
    return list;
}


extern "C" void gnome_cmd_file_list_show_file_popup (GnomeCmdFileList *fl, GdkRectangle *point_to);


struct PopupClosure
{
    GnomeCmdFileList *fl;
    GdkRectangle point_to;
};


static gboolean on_right_mb (PopupClosure *closure)
{
    gnome_cmd_file_list_show_file_popup (closure->fl, &closure->point_to);
    g_free (closure);
    return G_SOURCE_REMOVE;
}


/*******************************
 * Callbacks
 *******************************/

void GnomeCmdFileList::focus_prev()
{
    auto view =  file_list_view (this);

    GtkTreePath *path;

    gtk_tree_view_get_cursor (view, &path, nullptr);
    gtk_tree_path_prev (path);
    gtk_tree_view_set_cursor (view, path, nullptr, false);
    gtk_tree_path_free (path);
}


void GnomeCmdFileList::focus_next()
{
    auto view =  file_list_view (this);

    GtkTreePath *path;

    gtk_tree_view_get_cursor (view, &path, nullptr);
    gtk_tree_path_next (path);
    gtk_tree_view_set_cursor (view, path, nullptr, false);
    gtk_tree_path_free (path);
}


extern "C" GMenu *gnome_cmd_list_popmenu_new ();

inline void show_list_popup (GnomeCmdFileList *fl, gint x, gint y)
{
    auto menu = gnome_cmd_list_popmenu_new ();
    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (popover, GTK_WIDGET (fl));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
    GdkRectangle rect = { x, y, 0, 0 };
    gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
    gtk_popover_popup (GTK_POPOVER (popover));
}


extern "C" void gnome_cmd_file_list_select_with_mouse (GnomeCmdFileList *fl, GtkTreeIter *iter);


static void on_button_press (GtkGestureClick *gesture, int n_press, double x, double y, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    auto row = fl->get_dest_row_at_coords (x, y);

    GnomeCmdFileListButtonEvent event = {
        .iter = row.get(),
        .file = nullptr,
        .button = button,
        .n_press = n_press,
        .x = x,
        .y = y,
        .state = get_modifiers_state (),
    };

    if (row)
    {
        event.file = fl->get_file_at_row(row.get());
        g_signal_emit_by_name (fl, LIST_CLICKED_SIGNAL, event.button, event.file);
        g_signal_emit_by_name (fl, FILE_CLICKED_SIGNAL, &event);
    }
    else
    {
        g_signal_emit_by_name (fl, LIST_CLICKED_SIGNAL, event.button, event.file);
        if (n_press == 1 && button == 3) {
            show_list_popup (fl, x, y);
        }
    }
}


static void on_file_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (event != nullptr);
    g_return_if_fail (GNOME_CMD_IS_FILE (event->file));
    auto priv = file_list_priv (fl);
    auto store = file_list_store (fl);

    LeftMouseButtonMode left_mouse_button_mode;
    RightMouseButtonMode right_mouse_button_mode;
    g_object_get (fl,
        "left-mouse-button-mode", &left_mouse_button_mode,
        "right-mouse-button-mode", &right_mouse_button_mode,
        nullptr);

    priv->modifier_click = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK);

    if (event->n_press == 2 && event->button == 1 && left_mouse_button_mode == LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
    {
        g_signal_emit_by_name (fl, FILE_ACTIVATED_SIGNAL, event->file);
    }
    else if (event->n_press == 1 && event->button == 1)
    {
        if (event->state & GDK_SHIFT_MASK)
        {
            gnome_cmd_file_list_select_with_mouse (fl, event->iter);
        }
        else if (event->state & GDK_CONTROL_MASK)
        {
            fl->toggle_file (event->iter);
        }
    }
    else if (event->n_press == 1 && event->button == 3)
    {
        if (!gnome_cmd_file_is_dotdot (event->file))
        {
            if (right_mouse_button_mode == RIGHT_BUTTON_SELECTS)
            {
                auto focus_iter = fl->get_focused_file_iter();
                if (iter_compare(store, focus_iter.get(), event->iter) == 0)
                {
                    fl->select_iter(event->iter);
                    g_signal_emit_by_name(fl, FILES_CHANGED_SIGNAL);
                    gnome_cmd_file_list_show_file_popup (fl, nullptr);
                }
                else
                {
                    if (!fl->is_selected_iter(event->iter))
                        fl->select_iter(event->iter);
                    else
                        fl->unselect_iter(event->iter);
                    g_signal_emit_by_name(fl, FILES_CHANGED_SIGNAL);
                }
            }
            else
            {
                PopupClosure *closure = g_new0 (PopupClosure, 1);
                closure->fl = fl;
                closure->point_to.x = event->x;
                closure->point_to.y = event->y;
                closure->point_to.width = 0;
                closure->point_to.height = 0;
                g_timeout_add (1, (GSourceFunc) on_right_mb, closure);
            }
        }
    }
}


static void on_file_released (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (event != nullptr);
    g_return_if_fail (GNOME_CMD_IS_FILE (event->file));
    auto priv = file_list_priv (fl);

    LeftMouseButtonMode left_mouse_button_mode;
    g_object_get (fl, "left-mouse-button-mode", &left_mouse_button_mode, nullptr);

    if (event->n_press == 1 && event->button == 1 && !priv->modifier_click && left_mouse_button_mode == LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        g_signal_emit_by_name (fl, FILE_ACTIVATED_SIGNAL, event->file);
}


static void on_button_release (GtkGestureClick *gesture, int n_press, double x, double y, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    auto row = fl->get_dest_row_at_coords (x, y);
    if (!row)
        return;

    GnomeCmdFile *f = fl->get_file_at_row(row.get());

    GnomeCmdFileListButtonEvent event = {
        .iter = row.get(),
        .file = f,
        .button = button,
        .n_press = n_press,
        .x = x,
        .y = y,
        .state = get_modifiers_state (),
    };
    g_signal_emit_by_name (fl, FILE_RELEASED_SIGNAL, &event);

    if (button == 1 && state_is_blank (event.state))
    {
        gboolean left_mouse_button_unselects;
        g_object_get (fl, "left-mouse-button-unselects", &left_mouse_button_unselects, nullptr);

        if (f && !fl->is_selected_iter(row.get()) && left_mouse_button_unselects)
            fl->unselect_all();
    }
}


static void on_realize (GnomeCmdFileList *fl, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    auto priv = file_list_priv (fl);

    update_column_sort_arrows (fl);
    priv->realized = TRUE;
}


static void on_dir_file_created (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->insert_file(f))
        g_signal_emit_by_name (fl, FILES_CHANGED_SIGNAL);
}


static void on_dir_file_deleted (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    auto priv = file_list_priv (fl);

    if (priv->cwd == dir)
        if (fl->remove_file(f))
            g_signal_emit_by_name (fl, FILES_CHANGED_SIGNAL);
}


static void on_dir_file_changed (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->has_file(f))
    {
        fl->update_file(f);
        g_signal_emit_by_name (fl, FILES_CHANGED_SIGNAL);
    }
}


static void on_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->has_file(f))
    {
        // f->invalidate_metadata(TAG_FILE);    // FIXME: should be handled in GnomeCmdDir, not here
        fl->update_file(f);

        GnomeCmdFileList::ColumnID sort_col = (GnomeCmdFileList::ColumnID) gnome_cmd_file_list_get_sort_column (fl);

        if (sort_col==GnomeCmdFileList::COLUMN_NAME || sort_col==GnomeCmdFileList::COLUMN_EXT)
            gnome_cmd_file_list_sort (fl);
    }
}


static void on_dir_list_ok (GnomeCmdDir *dir, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_ok\n");

    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    auto priv = file_list_priv (fl);

    if (priv->realized)
    {
        // gtk_widget_set_sensitive (*fl, TRUE);
        gtk_widget_set_cursor (*fl, nullptr);
    }

    if (priv->connected_dir!=dir)
    {
        if (priv->connected_dir)
        {
            g_signal_handlers_disconnect_by_func (priv->connected_dir, (gpointer) on_dir_file_created, fl);
            g_signal_handlers_disconnect_by_func (priv->connected_dir, (gpointer) on_dir_file_deleted, fl);
            g_signal_handlers_disconnect_by_func (priv->connected_dir, (gpointer) on_dir_file_changed, fl);
            g_signal_handlers_disconnect_by_func (priv->connected_dir, (gpointer) on_dir_file_renamed, fl);
        }

        g_signal_connect (dir, "file-created", G_CALLBACK (on_dir_file_created), fl);
        g_signal_connect (dir, "file-deleted", G_CALLBACK (on_dir_file_deleted), fl);
        g_signal_connect (dir, "file-changed", G_CALLBACK (on_dir_file_changed), fl);
        g_signal_connect (dir, "file-renamed", G_CALLBACK (on_dir_file_renamed), fl);

        priv->connected_dir = dir;
    }

    g_signal_emit_by_name (fl, DIR_CHANGED_SIGNAL, dir);

    DEBUG('l', "returning from on_dir_list_ok\n");
}


static gboolean set_home_connection (GnomeCmdFileList *fl)
{
    g_printerr ("Setting home connection\n");
    fl->set_connection(get_home_con ());

    return FALSE;
}


static void on_directory_deleted (GnomeCmdDir *dir, GnomeCmdFileList *fl)
{
    auto parentDir = gnome_cmd_dir_get_existing_parent(dir);
    auto parentDirPath = gnome_cmd_path_get_path (gnome_cmd_dir_get_path (parentDir));
    fl->goto_directory(parentDirPath);
    g_free (parentDirPath);
}


/**
 * Handles an error which occured when directory listing failed.
 * We expect that the error which should be reported is stored in the GnomeCmdDir object.
 * The error location is freed afterwards.
 */
static void on_dir_list_failed (GnomeCmdDir *dir, GError *error, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_failed\n");
    auto priv = file_list_priv (fl);

    if (error)
        gnome_cmd_show_message (GTK_WINDOW (gtk_widget_get_root (*fl)), _("Directory listing failed."), error->message);

    g_signal_handlers_disconnect_matched (priv->cwd, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, fl);
    priv->connected_dir = nullptr;
    gnome_cmd_dir_unref (priv->cwd);
    gtk_widget_set_cursor (*fl, nullptr);
    // gtk_widget_set_sensitive (*fl, TRUE);

    if (priv->lwd && priv->con == gnome_cmd_file_get_connection (GNOME_CMD_FILE (priv->lwd)))
    {
        g_signal_handlers_disconnect_by_func (priv->cwd, (gpointer) on_directory_deleted, fl);

        priv->cwd = priv->lwd;
        g_signal_connect (priv->cwd, "list-ok", G_CALLBACK (on_dir_list_ok), fl);
        g_signal_connect (priv->cwd, "list-failed", G_CALLBACK (on_dir_list_failed), fl);
        g_signal_connect (priv->cwd, "dir-deleted", G_CALLBACK (on_directory_deleted), fl);
        priv->lwd = nullptr;
    }
    else
        g_timeout_add (1, (GSourceFunc) set_home_connection, fl);
}


static void on_connection_close (GnomeCmdCon *con, GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);
    if (con == priv->con)
        fl->set_connection(get_home_con());
}


/*******************************
 * Gtk class implementation
 *******************************/

extern "C" void gnome_cmd_file_list_finalize (GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);
    if (priv->con)
        g_signal_handlers_disconnect_by_func (priv->con, (gpointer) on_connection_close, fl);

    g_clear_object (&priv->con);

    g_clear_object (&priv->cwd);
    g_clear_object (&priv->lwd);
    g_clear_list (&priv->dropping_files, g_object_unref);
    priv->dropping_to = nullptr;
}


extern "C" void gnome_cmd_file_list_init (GnomeCmdFileList *fl)
{
    auto view = file_list_view (fl);

    auto priv = g_new0 (GnomeCmdFileListPrivate, 1);
    g_object_set_data_full (G_OBJECT (fl), "priv", priv, g_free);

    priv->base_dir = nullptr;

    priv->focus_later = nullptr;

    priv->dropping_files = nullptr;
    priv->dropping_to = nullptr;

    priv->realized = FALSE;
    priv->modifier_click = FALSE;

    fl->init_dnd();

    GtkGesture *click_controller = gtk_gesture_click_new ();
    gtk_widget_add_controller (GTK_WIDGET (view), GTK_EVENT_CONTROLLER (click_controller));
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_controller), 0);

    g_signal_connect (click_controller, "pressed", G_CALLBACK (on_button_press), fl);
    g_signal_connect (click_controller, "released", G_CALLBACK (on_button_release), fl);

    g_signal_connect_after (fl, "realize", G_CALLBACK (on_realize), fl);
    g_signal_connect (fl, "file-clicked", G_CALLBACK (on_file_clicked), fl);
    g_signal_connect (fl, "file-released", G_CALLBACK (on_file_released), fl);
}


/***********************************
 * Public functions
 ***********************************/

inline void add_file_to_clist (GnomeCmdFileList *fl, GnomeCmdFile *f, GtkTreeIter *next)
{
    auto priv = file_list_priv (fl);
    auto store = file_list_store (fl);

    GtkTreeIter iter;
    if (next != nullptr)
        gtk_list_store_insert_before (store, &iter, next);
    else
        gtk_list_store_append (store, &iter);
    set_model_row (fl, &iter, f, false);

    // If we have been waiting for this file to show up, focus it
    if (priv->focus_later && strcmp (f->get_name(), priv->focus_later)==0)
        fl->focus_file_at_row (&iter);
}


GnomeCmdFileList::TraverseControl GnomeCmdFileList::traverse_files(
    std::function<GnomeCmdFileList::TraverseControl (GnomeCmdFile *f, GtkTreeIter *, GtkListStore *)> visitor)
{
    auto store = file_list_store (this);

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    {
        do
        {
            GnomeCmdFile *file;
            gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, FILE_COLUMN, &file, -1);

            if (visitor (file, &iter, store) == GnomeCmdFileList::TRAVERSE_BREAK)
            {
                return GnomeCmdFileList::TRAVERSE_BREAK;
            }
        }
        while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
    }
    return GnomeCmdFileList::TRAVERSE_CONTINUE;
}


/******************************************************************************
*
*   Function: GnomeCmdFileList::append_file
*
*   Purpose:  Add a file to the list
*
*   Params:   @f: The file to add
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/
void GnomeCmdFileList::append_file (GnomeCmdFile *f)
{
    add_file_to_clist (this, f, nullptr);
}


gboolean GnomeCmdFileList::insert_file(GnomeCmdFile *f)
{
    GtkSorter *sorter;
    g_object_get (G_OBJECT (this), "sorter", &sorter, nullptr);

    if (!gnome_cmd_file_is_wanted(f))
        return FALSE;

    GtkTreeIter next_iter;
    bool next_iter_found = false;
    traverse_files ([&next_iter, &next_iter_found, this, sorter, f](GnomeCmdFile *f2, GtkTreeIter *iter, GtkListStore *store) {
        if (gtk_sorter_compare (sorter, f2, f) == GTK_ORDERING_LARGER)
        {
            memmove (&next_iter, iter, sizeof (next_iter));
            next_iter_found = true;
            return TRAVERSE_BREAK;
        }
        return TRAVERSE_CONTINUE;
    });

    if (next_iter_found)
        add_file_to_clist (this, f, &next_iter);
    else
        append_file(f);

    return TRUE;
}


static gint compare_with_gtk_sorter (gconstpointer itema, gconstpointer itemb, gpointer user_data)
{
    return gtk_sorter_compare (GTK_SORTER (user_data), G_OBJECT (itema), G_OBJECT (itemb));
}


void GnomeCmdFileList::show_files(GnomeCmdDir *dir)
{
    auto priv = file_list_priv (this);
    auto view = file_list_view (this);

    clear();

    GList *files = nullptr;

    // select the files to show
    for (auto i = gnome_cmd_dir_get_files (dir); i; i = i->next)
    {
        GnomeCmdFile *f = GNOME_CMD_FILE (i->data);

        if (gnome_cmd_file_is_wanted (f))
            files = g_list_append (files, f);
    }

    // Create a parent dir file (..) if appropriate
    GError *error = nullptr;
    auto uriString = GNOME_CMD_FILE (dir)->get_uri_str();
    auto gUri = g_uri_parse(uriString, G_URI_FLAGS_NONE, &error);
    if (error)
    {
        g_warning("show_files: g_uri_parse error of %s: %s", uriString, error->message);
        g_error_free(error);
    }
    auto path = g_uri_get_path(gUri);
    if (path && strcmp (path, G_DIR_SEPARATOR_S) != 0)
        files = g_list_append (files, gnome_cmd_file_new_dotdot (dir));
    g_free (uriString);

    if (files)
    {
        GtkSorter *sorter;
        g_object_get (G_OBJECT (this), "sorter", &sorter, nullptr);

        files = g_list_sort_with_data (files, compare_with_gtk_sorter, sorter);

        for (auto i = files; i; i = i->next)
            append_file(GNOME_CMD_FILE (i->data));

        g_list_free (files);
    }

    if (gtk_widget_get_realized (GTK_WIDGET (view)))
        gtk_tree_view_scroll_to_point (view, 0, 0);
}


void GnomeCmdFileList::update_file(GnomeCmdFile *f)
{
    auto priv = file_list_priv (this);

    if (!f->needs_update())
        return;

    auto row = get_row_from_file(f);
    if (!row)
        return;

    set_model_row (this, row.get(), f, false);
}


void GnomeCmdFileList::show_dir_tree_size(GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));

    auto row = get_row_from_file(f);
    if (!row)
        return;

    set_model_row (this, row.get(), f, true);
}


void GnomeCmdFileList::show_visible_tree_sizes()
{
    invalidate_tree_size();

    traverse_files([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        set_model_row (this, iter, file, true);
        return TraverseControl::TRAVERSE_CONTINUE;
    });

    g_signal_emit_by_name (this, FILES_CHANGED_SIGNAL);
}


gboolean GnomeCmdFileList::remove_file(GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, FALSE);
    auto store = file_list_store (this);

    auto row = get_row_from_file(f);

    if (!row)                               // f not found in the shown file list...
        return FALSE;

    if (gtk_list_store_remove (store, row.get()))
        focus_file_at_row (row.get());

    return TRUE;
}


void GnomeCmdFileList::clear()
{
    auto store = file_list_store (this);
    gtk_list_store_clear (store);
}


void GnomeCmdFileList::reload()
{
    auto priv = file_list_priv (this);
    g_return_if_fail (GNOME_CMD_IS_DIR (priv->cwd));

    unselect_all();
    gnome_cmd_dir_relist_files (GTK_WINDOW (gtk_widget_get_root (*this)), priv->cwd, gnome_cmd_con_needs_list_visprog (priv->con));
}


GList *GnomeCmdFileList::get_selected_files()
{
    GList *files = nullptr;
    traverse_files([this, &files](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (is_selected_iter(iter))
            files = g_list_append (files, file);
        return TraverseControl::TRAVERSE_CONTINUE;
    });
    if (files == nullptr)
    {
        GnomeCmdFile *f = get_selected_file();
        if (f != nullptr)
            files = g_list_append (nullptr, g_object_ref (f));
    }
    return files;
}


GtkTreeIterPtr GnomeCmdFileList::get_focused_file_iter()
{
    auto view = file_list_view (this);
    auto store = file_list_store (this);

    GtkTreePath *path = nullptr;
    gtk_tree_view_get_cursor (view, &path, NULL);
    GtkTreeIter iter;
    bool found = false;
    if (path != nullptr)
        found = gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
    gtk_tree_path_free (path);
    if (found)
        return GtkTreeIterPtr(gtk_tree_iter_copy(&iter), &gtk_tree_iter_free);
    else
        return GtkTreeIterPtr(nullptr, &gtk_tree_iter_free);
}


GnomeCmdFile *GnomeCmdFileList::get_focused_file()
{
    auto iter = get_focused_file_iter();
    if (iter)
        return get_file_at_row(iter.get());
    return nullptr;
}


void GnomeCmdFileList::set_selected_at_iter(GtkTreeIter *iter, gboolean selected)
{
    auto store = file_list_store (this);
    gtk_list_store_set (store, iter, DATA_COLUMN_SELECTED, selected, -1);
    g_signal_emit_by_name (this, FILES_CHANGED_SIGNAL);
}


void GnomeCmdFileList::select_iter(GtkTreeIter *iter)
{
    set_selected_at_iter (iter, TRUE);
}


void GnomeCmdFileList::unselect_iter(GtkTreeIter *iter)
{
    set_selected_at_iter (iter, FALSE);
}


bool GnomeCmdFileList::is_selected_iter(GtkTreeIter *iter)
{
    auto store = file_list_store (this);
    gboolean selected = FALSE;
    gtk_tree_model_get (GTK_TREE_MODEL (store), iter, DATA_COLUMN_SELECTED, &selected, -1);
    return selected;
}


gboolean GnomeCmdFileList::has_file(const GnomeCmdFile *f)
{
    return TRAVERSE_BREAK == traverse_files ([f](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        return file == f ? TRAVERSE_BREAK : TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::select_all_files()
{
    traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (file && !gnome_cmd_file_is_dotdot (file))
            set_selected_at_iter (iter, !GNOME_CMD_IS_DIR(file));
        return TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::unselect_all_files()
{
    traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (!GNOME_CMD_IS_DIR(file))
           unselect_iter(iter);
        return TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::select_all()
{
    gboolean select_dirs;
    g_object_get (this, "select-dirs", &select_dirs, nullptr);
    if (select_dirs)
    {
        traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
            if (file && !gnome_cmd_file_is_dotdot (file))
                select_iter(iter);
            return TRAVERSE_CONTINUE;
        });
    }
    else
    {
        traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
            if (file && !gnome_cmd_file_is_dotdot (file) && !GNOME_CMD_IS_DIR (file))
                select_iter(iter);
            return TRAVERSE_CONTINUE;
        });
    }
    g_signal_emit_by_name (this, FILES_CHANGED_SIGNAL);
}


void GnomeCmdFileList::unselect_all()
{
    traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        unselect_iter (iter);
        return TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::toggle()
{
    auto iter = get_focused_file_iter();
    if (iter)
        toggle_file(iter.get());
}


void GnomeCmdFileList::toggle_and_step()
{
    auto store = file_list_store (this);
    auto iter = get_focused_file_iter();
    if (iter)
        toggle_file(iter.get());
    if (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), iter.get()))
        focus_file_at_row (iter.get());
}


void GnomeCmdFileList::focus_file(const gchar *fileToFocus, gboolean scrollToFile)
{
    g_return_if_fail(fileToFocus != nullptr);
    auto priv = file_list_priv (this);
    auto view = file_list_view (this);

    auto fileToFocusNormalized = g_utf8_normalize(fileToFocus, -1, G_NORMALIZE_DEFAULT);

    auto result = traverse_files ([this, view, &fileToFocusNormalized, scrollToFile](GnomeCmdFile *f, GtkTreeIter *iter, GtkListStore *store) {
        auto currentFilename = g_utf8_normalize(f->get_name(), -1, G_NORMALIZE_DEFAULT);
        if (g_strcmp0 (currentFilename, fileToFocusNormalized) == 0)
        {
            focus_file_at_row (iter);
            if (scrollToFile)
            {
                GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
                gtk_tree_view_scroll_to_cell (view, path, nullptr, false, 0.0, 0.0);
                gtk_tree_path_free (path);
            }
            g_free(currentFilename);
            return TRAVERSE_BREAK;
        }
        g_free(currentFilename);
        return TRAVERSE_CONTINUE;
    });
    g_free(fileToFocusNormalized);

    /* The file was not found, remember the filename in case the file gets
       added to the list in the future (after a FAM event etc). */
    if (result == TRAVERSE_CONTINUE)
    {
        g_free (priv->focus_later);
        priv->focus_later = g_strdup (fileToFocus);
    }
}


void GnomeCmdFileList::select_row(GtkTreeIter* row)
{
    auto store = file_list_store (this);

    GtkTreeIter iter;
    if (row == nullptr)
    {
        if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
        {
            row = &iter;
        }
    }
    if (row)
        focus_file_at_row (row);
}


GnomeCmdFile *GnomeCmdFileList::get_file_at_row (GtkTreeIter *iter)
{
    auto store = file_list_store (this);

    GnomeCmdFile *file = nullptr;
    if (iter != nullptr)
        gtk_tree_model_get (GTK_TREE_MODEL (store), iter, FILE_COLUMN, &file, -1);
    return file;
}


GtkTreeIterPtr GnomeCmdFileList::get_row_from_file(GnomeCmdFile *f)
{
    GtkTreeIterPtr result(nullptr, &gtk_tree_iter_free);
    traverse_files ([f, &result](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (file == f)
        {
            result.reset (gtk_tree_iter_copy (iter));
            return TRAVERSE_BREAK;
        }
        return TRAVERSE_CONTINUE;
    });
	return result;
}


void GnomeCmdFileList::set_base_dir (gchar *dir)
{
    g_return_if_fail (dir != nullptr);
    auto priv = file_list_priv (this);
    if (priv->base_dir) { g_free (priv->base_dir); }
    priv->base_dir = dir;
}


extern "C" void open_connection_r (GnomeCmdFileList *fl, GtkWindow *parent_window, GnomeCmdCon *con);

void GnomeCmdFileList::set_connection (GnomeCmdCon *new_con, GnomeCmdDir *start_dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (new_con));
    auto priv = file_list_priv (this);

    if (priv->con == new_con)
    {
        if (!gnome_cmd_con_should_remember_dir (new_con))
            set_directory (gnome_cmd_con_get_default_dir (new_con));
        else
            if (start_dir)
                set_directory (start_dir);
        return;
    }

    if (!gnome_cmd_con_is_open (new_con))
    {
        GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (this)));
        open_connection_r (this, parent_window, new_con);
        return;
    }

    if (priv->con)
        g_signal_handlers_disconnect_by_func (priv->con, (gpointer) on_connection_close, this);


    if (priv->lwd)
    {
        gnome_cmd_dir_cancel_monitoring (priv->lwd);
        gnome_cmd_dir_unref (priv->lwd);
        priv->lwd = nullptr;
    }
    if (priv->cwd)
    {
        g_signal_handlers_disconnect_by_func (priv->cwd, (gpointer) on_directory_deleted, this);
        gnome_cmd_dir_cancel_monitoring (priv->cwd);
        gnome_cmd_dir_unref (priv->cwd);
        priv->cwd = nullptr;
    }

    g_set_object (&priv->con, new_con);

    if (!start_dir)
        start_dir = gnome_cmd_con_get_default_dir (priv->con);

    g_signal_connect (priv->con, "close", G_CALLBACK (on_connection_close), this);

    g_signal_emit_by_name (this, CON_CHANGED_SIGNAL, priv->con);

    set_directory(start_dir);
}


void GnomeCmdFileList::set_directory(GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    auto priv = file_list_priv (this);

    if (priv->cwd == dir)
        return;

    if (priv->realized && gnome_cmd_dir_get_state (dir) != GnomeCmdDir::STATE_LISTED)
    {
        // gtk_widget_set_sensitive (*this, FALSE);
        set_cursor_busy_for_widget (*this);
    }

    gnome_cmd_dir_ref (dir);

    if (priv->lwd && priv->lwd != dir)
        gnome_cmd_dir_unref (priv->lwd);

    if (priv->cwd)
    {
        priv->lwd = priv->cwd;
        gnome_cmd_dir_cancel_monitoring (priv->lwd);
        g_signal_handlers_disconnect_matched (priv->lwd, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        if (gnome_cmd_file_is_local (GNOME_CMD_FILE (priv->lwd)) && !gnome_cmd_dir_is_monitored (priv->lwd) && gnome_cmd_dir_needs_mtime_update (priv->lwd))
            gnome_cmd_dir_update_mtime (priv->lwd);
        g_signal_handlers_disconnect_by_func (priv->cwd, (gpointer) on_directory_deleted, this);
    }

    priv->cwd = dir;
    g_signal_connect (dir, "list-ok", G_CALLBACK (on_dir_list_ok), this);
    g_signal_connect (dir, "list-failed", G_CALLBACK (on_dir_list_failed), this);
    g_signal_connect (dir, "dir-deleted", G_CALLBACK (on_directory_deleted), this);

    switch (gnome_cmd_dir_get_state (dir))
    {
        case GnomeCmdDir::STATE_EMPTY:
            gnome_cmd_dir_list_files (GTK_WINDOW (gtk_widget_get_root (*this)), dir, gnome_cmd_con_needs_list_visprog (priv->con));
            break;

        case GnomeCmdDir::STATE_LISTING:
        case GnomeCmdDir::STATE_CANCELING:
            break;

        case GnomeCmdDir::STATE_LISTED:
            // check if the dir has up-to-date file list; if not and it's a local dir - relist it
            if (gnome_cmd_file_is_local (GNOME_CMD_FILE (dir)) && !gnome_cmd_dir_is_monitored (dir) && gnome_cmd_dir_update_mtime (dir))
                gnome_cmd_dir_relist_files (GTK_WINDOW (gtk_widget_get_root (*this)), dir, gnome_cmd_con_needs_list_visprog (priv->con));
            else
                on_dir_list_ok (dir, this);
            break;

        default:
            break;
    }

    gnome_cmd_dir_start_monitoring (dir);
}


void GnomeCmdFileList::update_style()
{
    auto priv = file_list_priv (this);

    // TODO: Maybe??? gtk_cell_renderer_set_fixed_size (priv->columns[1], )
    // gtk_clist_set_row_height (*this, gnome_cmd_data.options.list_row_height);

    gchar *font_name = nullptr;
    g_object_get (this, "font-name", &font_name, nullptr);
    PangoFontDescription *font_desc = pango_font_description_from_string (font_name);
    // gtk_widget_override_font (*this, font_desc);
    pango_font_description_free (font_desc);
    g_free (font_name);

    gtk_widget_queue_draw (*this);
}


void GnomeCmdFileList::goto_directory(const gchar *in_dir)
{
    g_return_if_fail (in_dir != nullptr);
    auto priv = file_list_priv (this);

    GnomeCmdDir *new_dir = nullptr;
    gchar *focus_dir = nullptr;
    gchar *dir;

    if (g_str_has_prefix (in_dir, "~"))
    {
        strlen(in_dir) > 1
            ? dir = g_strdup_printf("%s%s" , g_get_home_dir(), in_dir+1)
            : dir = g_strdup_printf("%s" , g_get_home_dir());
    }
    else
        dir = unquote_if_needed (in_dir);

    if (strcmp (dir, "..") == 0)
    {
        // let's get the parent directory
        new_dir = gnome_cmd_dir_get_parent (priv->cwd);
        if (!new_dir)
        {
            g_free (dir);
            return;
        }
        focus_dir = strdup(GNOME_CMD_FILE (priv->cwd)->get_name());
    }
    else
    {
        // check if it's an absolute address or not
        if (dir[0] == '/')
            new_dir = gnome_cmd_dir_new (priv->con, gnome_cmd_con_create_path (priv->con, dir));
        else
            if (g_str_has_prefix (dir, "\\\\"))
            {
                GnomeCmdPath *path = gnome_cmd_con_create_path (get_smb_con (), dir);
                if (path)
                    new_dir = gnome_cmd_dir_new (get_smb_con (), path);
            }
            else
                new_dir = gnome_cmd_dir_get_child (priv->cwd, dir);
    }

    if (new_dir)
        set_directory(new_dir);

    // focus the current dir when going back to the parent dir
    if (focus_dir)
    {
        focus_file(focus_dir, FALSE);
        g_free(focus_dir);
    }

    g_free (dir);
}


void GnomeCmdFileList::invalidate_tree_size()
{
    traverse_files ([](GnomeCmdFile *f, GtkTreeIter *iter, GtkListStore *store) {
        if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
            f->invalidate_tree_size();
        return TRAVERSE_CONTINUE;
    });
}


/******************************************************
 * DnD functions
 **/
/*
static void drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data, guint info, guint32 time, GnomeCmdFileList *fl)
{
    GList *files = nullptr;

    GString *data = build_selected_file_list (fl);

    if (data->len == 0)
    {
        g_string_free (data, TRUE);
        return;
    }

    gchar **uriCharList, **uriCharListTmp = nullptr;

    switch (info)
    {
        case TARGET_URI_LIST:
        case TARGET_TEXT_PLAIN:
            gtk_selection_data_set (selection_data, gtk_selection_data_get_target (selection_data), 8, (const guchar *) data->str, data->len);
            break;

        case TARGET_URL:
            uriCharList = uriCharListTmp = g_uri_list_extract_uris (data->str);
            for (auto uriChar = *uriCharListTmp; uriChar; uriChar=*++uriCharListTmp)
            {
                auto gFile = g_file_new_for_uri(uriChar);
                files = g_list_append(files, gFile);
            }
            if (files)
                gtk_selection_data_set (selection_data, gtk_selection_data_get_target (selection_data), 8, (const guchar *) files->data, strlen ((const char *) files->data));
            g_list_foreach (files, (GFunc) g_object_unref, nullptr);
            g_strfreev(uriCharList);
            break;

        default:
            g_assert_not_reached ();
    }

    g_string_free (data, TRUE);
}


static void drag_data_received (GtkWidget *widget, GdkDragContext *context,
                                gint x, gint y, GtkSelectionData *selection_data,
                                guint info, guint32 time, GnomeCmdFileList *fl)
{
    auto row = fl->get_dest_row_at_pos (x, y);
    GnomeCmdFile *f = fl->get_file_at_row (row.get());
    GnomeCmdDir *to = fl->cwd;

    // the drop was over a directory in the list, which means that the
    // xfer should be done to that directory instead of the current one in the list
    if (f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        to = f->is_dotdot
            ? gnome_cmd_dir_get_parent (to)
            : gnome_cmd_dir_get_child (to, g_file_info_get_display_name(f->get_file_info()));

    // transform the drag data to a list with GFiles
    GList *gFileGlist = uri_strings_to_gfiles ((gchar *) gtk_selection_data_get_data (selection_data));

    GdkModifierType mask = get_modifiers_state ();

    if (!(mask & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)))
    {
        switch (gnome_cmd_data.options.mouse_dnd_default)
        {
            case GNOME_CMD_DEFAULT_DND_QUERY:
                {
                    auto dnd_popover = create_dnd_popup (fl, gFileGlist, to);

                    GdkRectangle rect = { x, y, 1, 1 };
                    gtk_popover_set_pointing_to (dnd_popover, &rect);

                    gtk_popover_popup (dnd_popover);
                }
                break;
            case GNOME_CMD_DEFAULT_DND_MOVE:
                fl->drop_files(GnomeCmdFileList::DndMode::MOVE, G_FILE_COPY_NONE, gFileGlist, to);
                break;
            case GNOME_CMD_DEFAULT_DND_COPY:
                fl->drop_files(GnomeCmdFileList::DndMode::COPY, G_FILE_COPY_NONE, gFileGlist, to);
                break;
            default:
                break;
        }
        return;
    }

    // find out what operation to perform if Shift or Control was pressed while DnD
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (gdk_drag_context_get_selected_action (context))
    {
        case GDK_ACTION_MOVE:
            fl->drop_files(GnomeCmdFileList::DndMode::MOVE, G_FILE_COPY_NONE, gFileGlist, to);
            break;

        case GDK_ACTION_COPY:
            fl->drop_files(GnomeCmdFileList::DndMode::COPY, G_FILE_COPY_NONE, gFileGlist, to);
            break;

        case GDK_ACTION_LINK:
            fl->drop_files(GnomeCmdFileList::DndMode::LINK, G_FILE_COPY_NONE, gFileGlist, to);
            break;

        default:
            g_warning ("Unknown context->action in drag_data_received");
            return;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


static gboolean drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, GnomeCmdFileList *fl)
{
    gdk_drag_status (context, gdk_drag_context_get_suggested_action (context), time);

    auto row = fl->get_dest_row_at_pos (x, y);

    if (row)
    {
        GnomeCmdFile *f = fl->get_file_at_row(row.get());

        if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY)
            row.reset ();
    }

    return FALSE;
}


static void drag_data_delete (GtkWidget *widget, GdkDragContext *drag_context, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();
    fl->remove_files(files);
    g_list_free (files);
}
*/

void GnomeCmdFileList::init_dnd()
{
    auto priv = file_list_priv (this);
    auto view = file_list_view (this);

    // set up drag source
    GdkContentFormats* drag_formats = gdk_content_formats_new (drag_types, G_N_ELEMENTS (drag_types));
    gtk_tree_view_enable_model_drag_source (view,
                                            GDK_BUTTON1_MASK,
                                            drag_formats,
                                            (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK));

    // g_signal_connect (this, "drag-data-get", G_CALLBACK (drag_data_get), this);
    // g_signal_connect (this, "drag-data-delete", G_CALLBACK (drag_data_delete), this);

    // set up drag destination
    GdkContentFormats* drop_formats = gdk_content_formats_new (drop_types, G_N_ELEMENTS (drop_types));
    gtk_tree_view_enable_model_drag_dest (view,
                                          drop_formats,
                                          (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK));

    // g_signal_connect (this, "drag-motion", G_CALLBACK (drag_motion), this);
    // g_signal_connect (this, "drag-data-received", G_CALLBACK (drag_data_received), this);
}


struct DropDoneClosure
{
    GnomeCmdDir *dir;
};


static void on_drop_done (gboolean success, gpointer user_data)
{
    DropDoneClosure *drop_done = (DropDoneClosure *) user_data;

    gnome_cmd_dir_relist_files (GTK_WINDOW (main_win), drop_done->dir, FALSE);
    main_win->focus_file_lists ();

    g_free (drop_done);
}


void GnomeCmdFileList::drop_files(DndMode dndMode, GFileCopyFlags gFileCopyFlags, GList *gFileGlist, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    DropDoneClosure *drop_done;

    switch (dndMode)
    {
        case COPY:
            drop_done = g_new0 (DropDoneClosure, 1);
            drop_done->dir = dir;
            gnome_cmd_copy_gfiles_start (GTK_WINDOW (main_win),
                                         gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         on_drop_done,
                                         drop_done);
            break;
        case MOVE:
            drop_done = g_new0 (DropDoneClosure, 1);
            drop_done->dir = dir;
            gnome_cmd_move_gfiles_start (GTK_WINDOW (main_win),
                                         gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         on_drop_done,
                                         drop_done);
            break;
        case LINK:
            drop_done = g_new0 (DropDoneClosure, 1);
            drop_done->dir = dir;
            gnome_cmd_link_gfiles_start (GTK_WINDOW (main_win),
                                         gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         on_drop_done,
                                         drop_done);
            break;
        default:
            return;
    }
}

GtkTreeIterPtr GnomeCmdFileList::get_dest_row_at_pos (gint drag_x, gint drag_y)
{
    auto view = file_list_view (this);

    gint wx, wy;
    gtk_tree_view_convert_bin_window_to_widget_coords (view, drag_x, drag_y, &wx, &wy);
    return get_dest_row_at_coords (wx, wy);
}

GtkTreeIterPtr GnomeCmdFileList::get_dest_row_at_coords (gdouble x, gdouble y)
{
    auto view = file_list_view (this);
    auto store = file_list_store (this);

    GtkTreePath *path;
    if (!gtk_tree_view_get_dest_row_at_pos (view, x, y, &path, nullptr))
        return GtkTreeIterPtr(nullptr, &gtk_tree_iter_free);

    GtkTreeIter iter;
    bool found = gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
    gtk_tree_path_free (path);
    if (found)
        return GtkTreeIterPtr(gtk_tree_iter_copy(&iter), &gtk_tree_iter_free);
    else
        return GtkTreeIterPtr(nullptr, &gtk_tree_iter_free);
}


// FFI
GList *gnome_cmd_file_list_get_selected_files (GnomeCmdFileList *fl)
{
    return fl->get_selected_files();
}

GnomeCmdFile *gnome_cmd_file_list_get_focused_file(GnomeCmdFileList *fl)
{
    return fl->get_focused_file();
}

GnomeCmdCon *gnome_cmd_file_list_get_connection(GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);
    return priv->con;
}

GnomeCmdDir *gnome_cmd_file_list_get_directory(GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);
    return priv->cwd;
}

void gnome_cmd_file_list_set_directory(GnomeCmdFileList *fl, GnomeCmdDir *dir)
{
    fl->set_directory(dir);
}

void gnome_cmd_file_list_reload (GnomeCmdFileList *fl)
{
    fl->reload();
}

void gnome_cmd_file_list_append_file(GnomeCmdFileList *fl, GnomeCmdFile *f)
{
    fl->append_file(f);
}

void gnome_cmd_file_list_set_connection(GnomeCmdFileList *fl, GnomeCmdCon *con, GnomeCmdDir *start_dir)
{
    fl->set_connection(con, start_dir);
}

void gnome_cmd_file_list_focus_file(GnomeCmdFileList *fl, const gchar *focus_file, gboolean scroll_to_file)
{
    fl->focus_file(focus_file, scroll_to_file);
}

void gnome_cmd_file_list_goto_directory(GnomeCmdFileList *fl, const gchar *dir)
{
    fl->goto_directory(dir);
}

void gnome_cmd_file_list_update_style(GnomeCmdFileList *fl)
{
    fl->update_style();
}

extern "C" void gnome_cmd_file_list_invalidate_tree_size(GnomeCmdFileList *fl)
{
    fl->invalidate_tree_size();
}

void gnome_cmd_file_list_show_files(GnomeCmdFileList *fl, GnomeCmdDir *dir)
{
    fl->show_files (dir);
}

void gnome_cmd_file_list_set_base_dir (GnomeCmdFileList *fl, gchar *dir)
{
    fl->set_base_dir (dir);
}

void gnome_cmd_file_list_toggle(GnomeCmdFileList *fl)
{
    fl->toggle();
}

void gnome_cmd_file_list_toggle_and_step(GnomeCmdFileList *fl)
{
    fl->toggle_and_step();
}

void gnome_cmd_file_list_select_all(GnomeCmdFileList *fl)
{
    fl->select_all();
}

void gnome_cmd_file_list_select_all_files(GnomeCmdFileList *fl)
{
    fl->select_all_files();
}

void gnome_cmd_file_list_unselect_all_files(GnomeCmdFileList *fl)
{
    fl->unselect_all_files();
}

void gnome_cmd_file_list_unselect_all(GnomeCmdFileList *fl)
{
    fl->unselect_all();
}

void gnome_cmd_file_list_focus_prev(GnomeCmdFileList *fl)
{
    fl->focus_prev();
}

void gnome_cmd_file_list_focus_next(GnomeCmdFileList *fl)
{
    fl->focus_next();
}

void gnome_cmd_file_list_show_dir_tree_size(GnomeCmdFileList *fl, GnomeCmdFile *f)
{
    fl->show_dir_tree_size(f);
}

void gnome_cmd_file_list_show_visible_tree_sizes(GnomeCmdFileList *fl)
{
    fl->show_visible_tree_sizes();
}

void gnome_cmd_file_list_toggle_file(GnomeCmdFileList *fl, GtkTreeIter *iter)
{
    fl->toggle_file(iter);
}
