/**
 * @file gnome-cmd-manage-bookmarks-dialog.cc
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

#include <config.h>
#include <map>
#include <set>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-manage-bookmarks-dialog.h"
#include "gnome-cmd-edit-bookmark-dialog.h"
#include "gnome-cmd-treeview.h"
#include "gnome-cmd-user-actions.h"
#include "gnome-cmd-hintbox.h"
#include "eggcellrendererkeys.h"
#include "utils.h"

using namespace std;


struct _GnomeCmdBookmarksDialog
{
    GtkDialog parent;
};


struct GnomeCmdBookmarksDialogPrivate
{
    GtkTreeView *view;

    GtkWidget *edit_button;
    GtkWidget *remove_button;
    GtkWidget *up_button;
    GtkWidget *down_button;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdBookmarksDialog, gnome_cmd_bookmarks_dialog, GTK_TYPE_DIALOG)


static GtkTreeModel *create_and_fill_model (GtkTreePath *&current_group);
static GtkTreeView *create_view_and_model ();

static void cursor_changed_callback (GtkTreeView *view, GnomeCmdBookmarksDialog *dialog);
static void row_activated_callback (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer);
static void edit_clicked_callback (GtkButton *button, GnomeCmdBookmarksDialog *dialog);
static void remove_clicked_callback (GtkButton *button, GnomeCmdBookmarksDialog *dialog);
static void up_clicked_callback (GtkButton *button, GnomeCmdBookmarksDialog *dialog);
static void down_clicked_callback (GtkButton *button, GnomeCmdBookmarksDialog *dialog);
static void size_allocate_callback (GtkWidget *widget, GtkAllocation *allocation, gpointer unused);
static void response_callback (GtkDialog *dialog, int response_id);


enum
{
    COL_GROUP,
    COL_NAME,
    COL_PATH,
    COL_SHORTCUT,
    COL_BOOKMARK,
    NUM_COLUMNS
};


const int RESPONSE_JUMP_TO = 123;


static void gnome_cmd_bookmarks_dialog_class_init (GnomeCmdBookmarksDialogClass *klass)
{
}


static void gnome_cmd_bookmarks_dialog_init (GnomeCmdBookmarksDialog *dialog)
{
    auto priv = static_cast<GnomeCmdBookmarksDialogPrivate*>(gnome_cmd_bookmarks_dialog_get_instance_private (dialog));

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
        _("_Help"), GTK_RESPONSE_HELP,
        _("_Close"), GTK_RESPONSE_CLOSE,
        _("_Jump to"), RESPONSE_JUMP_TO,
        NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG (dialog));

    GtkWidget *vbox, *hbox, *scrolled_window;

    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

    gtk_widget_set_margin_top (content_area, 10);
    gtk_widget_set_margin_bottom (content_area, 10);
    gtk_widget_set_margin_start (content_area, 10);
    gtk_widget_set_margin_end (content_area, 10);
    gtk_box_set_spacing (GTK_BOX (content_area), 12);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_vexpand (hbox, TRUE);
    gtk_box_append (GTK_BOX (content_area), hbox);

    scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled_window), TRUE);
    gtk_widget_set_hexpand (scrolled_window, TRUE);
    gtk_box_append (GTK_BOX (hbox), scrolled_window);

    priv->view = create_view_and_model ();
    gtk_window_set_default_size (GTK_WINDOW (dialog), gnome_cmd_data.bookmarks_defaults.width, gnome_cmd_data.bookmarks_defaults.height);
    gtk_widget_set_size_request (GTK_WIDGET (priv->view), 400, 250);
    g_signal_connect (priv->view, "cursor-changed", G_CALLBACK (cursor_changed_callback), dialog);
    g_signal_connect (priv->view, "row-activated", G_CALLBACK (row_activated_callback), dialog);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), GTK_WIDGET (priv->view));

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append (GTK_BOX (hbox), vbox);

    priv->edit_button = gtk_button_new_with_mnemonic (_("_Edit"));
    gtk_widget_set_sensitive (priv->edit_button, FALSE);
    g_signal_connect (priv->edit_button, "clicked", G_CALLBACK (edit_clicked_callback), dialog);
    gtk_box_append (GTK_BOX (vbox), priv->edit_button);

    priv->remove_button = gtk_button_new_with_mnemonic (_("_Remove"));
    gtk_widget_set_sensitive (priv->remove_button, FALSE);
    g_signal_connect (priv->remove_button, "clicked", G_CALLBACK (remove_clicked_callback), dialog);
    gtk_box_append (GTK_BOX (vbox), priv->remove_button);

    priv->up_button = gtk_button_new_with_mnemonic (_("_Up"));
    gtk_widget_set_sensitive (priv->up_button, FALSE);
    g_signal_connect (priv->up_button, "clicked", G_CALLBACK (up_clicked_callback), dialog);
    gtk_box_append (GTK_BOX (vbox), priv->up_button);

    priv->down_button = gtk_button_new_with_mnemonic (_("_Down"));
    gtk_widget_set_sensitive (priv->down_button, FALSE);
    g_signal_connect (priv->down_button, "clicked", G_CALLBACK (down_clicked_callback), dialog);
    gtk_box_append (GTK_BOX (vbox), priv->down_button);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), RESPONSE_JUMP_TO);
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_JUMP_TO, FALSE);

    gtk_widget_show_all (content_area);

    g_signal_connect (dialog, "size-allocate", G_CALLBACK (size_allocate_callback), NULL);
    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), NULL);

    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}


GnomeCmdBookmarksDialog *gnome_cmd_bookmarks_dialog_new (GtkWindow *parent)
{
    GnomeCmdBookmarksDialog *dialog = (GnomeCmdBookmarksDialog *) g_object_new (GNOME_CMD_TYPE_BOOKMARKS_DIALOG,
        "title", _("Bookmarks"),
        "transient-for", parent,
        // "modal", TRUE,
        "destroy-with-parent", TRUE,
        NULL);
    return dialog;
}


static void fill_tree (GtkTreeStore *store, GtkTreePath *&current_group)
{
    gtk_tree_store_clear (store);

    GnomeCmdCon *current_con = main_win->fs(ACTIVE)->get_connection();
    GtkTreeIter toplevel;

    map<string,set<string> > keys;

    for (GnomeCmdUserActions::const_iterator i=gcmd_user_actions.begin(); i!=gcmd_user_actions.end(); ++i)
        if (!ascii_isupper (*i))                                       // ignore lowercase keys as they duplicate uppercase ones
        {
            const gchar *options = gcmd_user_actions.options(i);

            if (options && strcmp(gcmd_user_actions.name(i),"bookmarks.goto")==0)
            {
                gchar *accelerator = egg_accelerator_get_label(i->first.keyval, (GdkModifierType) i->first.state);
                keys[options].insert(accelerator);
                g_free (accelerator);
            }
        }

    for (GList *all_cons = gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); all_cons; all_cons = all_cons->next)
    {
        GnomeCmdCon *con = (GnomeCmdCon *) all_cons->data;
        GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (con);

        if (!group || !group->bookmarks)
            continue;

        gtk_tree_store_append (store, &toplevel, NULL);
        gtk_tree_store_set (store, &toplevel,
                            COL_GROUP, gnome_cmd_con_get_alias (con),
                            -1);

        if (con==current_con)
            current_group = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &toplevel);

        GtkTreeIter child;

        for (GList *bookmarks = group->bookmarks; bookmarks; bookmarks = bookmarks->next)
        {
            gtk_tree_store_append (store, &child, &toplevel);
            GnomeCmdBookmark *bookmark = (GnomeCmdBookmark *) bookmarks->data;

            gtk_tree_store_set (store, &child,
                                COL_NAME, bookmark->name,
                                COL_PATH, bookmark->path,
                                COL_SHORTCUT, join(keys[bookmark->name], ", ").c_str(),
                                COL_BOOKMARK, bookmark,
                                -1);
        }
    }
}


static GtkTreeModel *create_and_fill_model (GtkTreePath *&current_group)
{
    GtkTreeStore *store = gtk_tree_store_new (NUM_COLUMNS,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_POINTER);
    fill_tree (store, current_group);


    return GTK_TREE_MODEL (store);
}


static GtkTreeView *create_view_and_model ()
{
    GtkTreeView *bm_view = GTK_TREE_VIEW (gtk_tree_view_new ());

    g_object_set (bm_view,
                  "rules-hint", TRUE,
                  "enable-search", TRUE,
                  "search-column", COL_NAME,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (bm_view), renderer, COL_GROUP, _("Group"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Bookmark group"));

    g_object_set (renderer,
                  "weight-set", TRUE,
                  "weight", PANGO_WEIGHT_BOLD,
                  NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (bm_view), renderer, COL_NAME, _("Name"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Bookmark name"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (bm_view), renderer, COL_SHORTCUT, _("Shortcut"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Keyboard shortcut for selected bookmark"));

    g_object_set (renderer,
                  "foreground-set", TRUE,
                  "foreground", "DarkGray",
                  NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (bm_view), renderer, COL_PATH, _("Path"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Bookmarked path"));

    g_object_set (renderer,
                  "foreground-set", TRUE,
                  "foreground", "DarkGray",
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  NULL);

    GtkTreePath *group = NULL;
    GtkTreeModel *model = create_and_fill_model (group);

    gtk_tree_view_set_model (GTK_TREE_VIEW (bm_view), model);

    g_object_unref (model);          // destroy model automatically with view

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (bm_view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

    if (group)
        gtk_tree_view_expand_row (GTK_TREE_VIEW (bm_view), group, TRUE);
    else
        group = gtk_tree_path_new_first ();

    gtk_tree_view_set_cursor (GTK_TREE_VIEW (bm_view), group, NULL, FALSE);
    gtk_tree_path_free (group);

    return bm_view;
}


void gnome_cmd_bookmarks_dialog_update (GnomeCmdBookmarksDialog *dialog)
{
    auto priv = static_cast<GnomeCmdBookmarksDialogPrivate*>(gnome_cmd_bookmarks_dialog_get_instance_private (dialog));

    GtkTreePath *group = NULL;
    fill_tree (GTK_TREE_STORE (gtk_tree_view_get_model (priv->view)), group);
    if (group)
        gtk_tree_view_expand_row (priv->view, group, TRUE);
}


static void cursor_changed_callback (GtkTreeView *bm_view, GnomeCmdBookmarksDialog *dialog)
{
    auto priv = static_cast<GnomeCmdBookmarksDialogPrivate*>(gnome_cmd_bookmarks_dialog_get_instance_private (dialog));

    GtkTreeModel *bm_model = gtk_tree_view_get_model (bm_view);
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (bm_view), NULL, &iter))
    {
        GnomeCmdBookmark *bookmark = NULL;

        gtk_tree_model_get (bm_model, &iter, COL_BOOKMARK, &bookmark, -1);

        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_JUMP_TO, bookmark!=NULL);
        gtk_widget_set_sensitive (priv->edit_button, bookmark!=NULL);
        gtk_widget_set_sensitive (priv->remove_button, bookmark!=NULL);
        gtk_widget_set_sensitive (priv->up_button, bookmark!=NULL && bookmark->group->bookmarks->data!=bookmark);
        gtk_widget_set_sensitive (priv->down_button, bookmark!=NULL && gtk_tree_model_iter_next (bm_model, &iter));
    }
}


static void row_activated_callback (GtkTreeView *bm_view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer)
{
    gtk_dialog_response ((GtkDialog *) gtk_widget_get_ancestor (GTK_WIDGET (bm_view), GTK_TYPE_DIALOG), RESPONSE_JUMP_TO);
}


static void edit_clicked_callback (GtkButton *button, GnomeCmdBookmarksDialog *dialog)
{
    auto priv = static_cast<GnomeCmdBookmarksDialogPrivate*>(gnome_cmd_bookmarks_dialog_get_instance_private (dialog));

    g_return_if_fail (dialog!=NULL);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->view);
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        GnomeCmdBookmark *bookmark = NULL;
        GtkTreeModel *model = gtk_tree_view_get_model (priv->view);
        gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

        if (gnome_cmd_edit_bookmark_dialog (GTK_WINDOW (dialog), _("Edit Bookmark"), bookmark->name, bookmark->path))
            gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
                                COL_NAME, bookmark->name,
                                COL_PATH, bookmark->path,
                                -1);
    }
}


static void remove_clicked_callback (GtkButton *button, GnomeCmdBookmarksDialog *dialog)
{
    auto priv = static_cast<GnomeCmdBookmarksDialogPrivate*>(gnome_cmd_bookmarks_dialog_get_instance_private (dialog));

    GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->view);
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        GnomeCmdBookmark *bookmark = NULL;
        GtkTreeModel *model = gtk_tree_view_get_model (priv->view);
        gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);
        gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

        if (bookmark)
        {
            bookmark->group->bookmarks = g_list_remove (bookmark->group->bookmarks, bookmark);
            g_free (bookmark->name);
            g_free (bookmark->path);
            g_free (bookmark);

            main_win->update_bookmarks ();

            gnome_cmd_data.save_bookmarks ();
        }
    }
}


static void up_clicked_callback (GtkButton *button, GnomeCmdBookmarksDialog *dialog)
{
    auto priv = static_cast<GnomeCmdBookmarksDialogPrivate*>(gnome_cmd_bookmarks_dialog_get_instance_private (dialog));

    GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->view);
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
        return;

    GnomeCmdBookmark *bookmark = NULL;
    GtkTreeModel *model = gtk_tree_view_get_model (priv->view);
    gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

    if (!bookmark)
        return;

    GList *elem = g_list_find (bookmark->group->bookmarks, bookmark);

    if (!elem)
        return;

    bookmark->group->bookmarks = g_list_insert_before (bookmark->group->bookmarks, g_list_previous (elem), bookmark);
    bookmark->group->bookmarks = g_list_remove_link (bookmark->group->bookmarks, elem);

    GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
    GtkTreeIter prev;
    gtk_tree_path_prev (path);
    gtk_tree_model_get_iter (model, &prev, path);
    gtk_tree_path_free (path);

    gtk_tree_store_swap (GTK_TREE_STORE (model), &prev, &iter);

    gtk_widget_set_sensitive (priv->up_button, bookmark->group->bookmarks->data!=bookmark);
    gtk_widget_set_sensitive (priv->down_button, gtk_tree_model_iter_next (model, &iter));
}


static void down_clicked_callback (GtkButton *button, GnomeCmdBookmarksDialog *dialog)
{
    auto priv = static_cast<GnomeCmdBookmarksDialogPrivate*>(gnome_cmd_bookmarks_dialog_get_instance_private (dialog));

    GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->view);
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
        return;

    GnomeCmdBookmark *bookmark = NULL;
    GtkTreeModel *model = gtk_tree_view_get_model (priv->view);
    gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

    if (!bookmark)
        return;

    GList *elem = g_list_find (bookmark->group->bookmarks, bookmark);

    if (!elem)
        return;

    elem = g_list_insert (elem, bookmark, 2);
    bookmark->group->bookmarks = g_list_remove_link (bookmark->group->bookmarks, elem);

    GtkTreeIter next = iter;

    gtk_tree_model_iter_next (model, &next);
    gtk_tree_store_swap (GTK_TREE_STORE (model), &iter, &next);
    gtk_widget_set_sensitive (priv->up_button, bookmark!=NULL && bookmark->group->bookmarks->data!=bookmark);
    gtk_widget_set_sensitive (priv->down_button, bookmark!=NULL && gtk_tree_model_iter_next (model, &iter));
}


static void size_allocate_callback (GtkWidget *widget, GtkAllocation *allocation, gpointer unused)
{
    gnome_cmd_data.bookmarks_defaults.width = allocation->width;
    gnome_cmd_data.bookmarks_defaults.height = allocation->height;
}


static void response_callback (GtkDialog *dlg, int response_id)
{
    auto dialog = GNOME_CMD_BOOKMARKS_DIALOG (dlg);
    auto priv = static_cast<GnomeCmdBookmarksDialogPrivate*>(gnome_cmd_bookmarks_dialog_get_instance_private (dialog));

    switch (response_id)
    {
        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-bookmarks");
            break;

        case RESPONSE_JUMP_TO:
            {
                GtkTreeModel *model = gtk_tree_view_get_model (priv->view);
                GtkTreeIter iter;

                if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (priv->view), NULL, &iter))
                {
                    GnomeCmdBookmark *bookmark = NULL;

                    gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

                    if (bookmark)
                    {
                        gnome_cmd_bookmark_goto (bookmark);
                        gtk_window_destroy (GTK_WINDOW (dialog));
                    }
                }
            }

        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            gtk_window_destroy (GTK_WINDOW (dialog));
            break;

        default :
            g_assert_not_reached ();
    }
}


void gnome_cmd_bookmark_goto (GnomeCmdBookmark *bookmark)
{
    g_return_if_fail (bookmark->group->con != NULL);

    GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdCon *con = bookmark->group->con;

    if (fs->get_connection() == con)
    {
        if (fs->file_list()->locked)
            fs->new_tab(gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, bookmark->path)));
        else
            fs->goto_directory(bookmark->path);
    }
    else
    {
        if (con->state == GnomeCmdCon::STATE_OPEN)
        {
            GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, bookmark->path));

            if (fs->file_list()->locked)
                fs->new_tab(dir);
            else
                fs->set_connection(con, dir);
        }
        else
        {
            delete con->base_path;

            con->base_path = gnome_cmd_con_create_path (con, bookmark->path);
            if (fs->file_list()->locked)
                fs->new_tab(gnome_cmd_con_get_default_dir (con));
            else
                fs->set_connection(con);
        }
    }
}


void gnome_cmd_bookmark_add_current (GnomeCmdDir *dir)
{
    gchar *path = gnome_cmd_dir_is_local (dir) ? GNOME_CMD_FILE (dir)->get_real_path () : GNOME_CMD_FILE (dir)->GetPathStringThroughParent();

    if (!g_utf8_validate (path, -1, NULL))
    {
        gnome_cmd_show_message (NULL, _("To bookmark a directory the whole search path to the directory must be in valid UTF-8 encoding"));
        g_free (path);
        return;
    }

    gchar *name = g_path_get_basename (path);

    if (gnome_cmd_edit_bookmark_dialog (NULL, _("New Bookmark"), name, path))
    {
        GnomeCmdCon *con = gnome_cmd_dir_is_local (dir) ? get_home_con () : gnome_cmd_dir_get_connection (dir);
        GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (con);
        GnomeCmdBookmark *bookmark = g_new0 (GnomeCmdBookmark, 1);

        bookmark->name = name;
        bookmark->path = path;
        bookmark->group = group;

        group->bookmarks = g_list_append (group->bookmarks, bookmark);

        main_win->update_bookmarks();

        gnome_cmd_data.save_bookmarks ();
    }
    else
    {
        g_free (name);
        g_free (path);
    }
}
