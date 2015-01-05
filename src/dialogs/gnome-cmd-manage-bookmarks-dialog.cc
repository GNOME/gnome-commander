/** 
 * @file gnome-cmd-manage-bookmarks-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2015 Uwe Scholz\n
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
#include "gnome-cmd-menu-button.h"
#include "gnome-cmd-hintbox.h"
#include "eggcellrendererkeys.h"
#include "utils.h"

using namespace std;


static GtkTreeModel *create_and_fill_model (GtkTreePath *&current_group);
static GtkWidget *create_view_and_model ();

static void cursor_changed_callback (GtkTreeView *view, GtkWidget *dialog);
static void row_activated_callback (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer);
static void edit_clicked_callback (GtkButton *button, GtkWidget *view);
static void remove_clicked_callback (GtkButton *button, GtkWidget *view);
static void up_clicked_callback (GtkButton *button, GtkWidget *view);
static void down_clicked_callback (GtkButton *button, GtkWidget *view);
static void size_allocate_callback (GtkWidget *widget, GtkAllocation *allocation, gpointer unused);
static void response_callback (GtkDialog *dialog, int response_id, GtkTreeView *view);


enum
{
    COL_GROUP,
    COL_NAME,
    COL_PATH,
    COL_SHORTCUT,
    COL_BOOKMARK,
    NUM_COLUMNS
} ;


const int RESPONSE_JUMP_TO = 123;


void gnome_cmd_bookmark_dialog_new (const gchar *title, GtkWindow *parent)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons (title, parent,
                                                     GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                                     GTK_STOCK_JUMP_TO, RESPONSE_JUMP_TO,
                                                     NULL);

#if GTK_CHECK_VERSION (2, 14, 0)
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG (dialog));
#endif

    GtkWidget *vbox, *hbox, *scrolled_window, *view, *button;

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_box_set_spacing (GTK_BOX (content_area), 2);
#else
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
#endif
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

    vbox = gtk_vbox_new (FALSE, 12);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
 #if GTK_CHECK_VERSION (2, 14, 0)
   gtk_container_add (GTK_CONTAINER (content_area), vbox);
#else
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), vbox);
#endif

    hbox = gtk_hbox_new (FALSE, 12);
    gtk_container_add (GTK_CONTAINER (vbox), hbox);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);

    view = create_view_and_model ();
    gtk_window_set_default_size (GTK_WINDOW (dialog), gnome_cmd_data.bookmarks_defaults.width, gnome_cmd_data.bookmarks_defaults.height);
    gtk_widget_set_size_request (view, 400, 250);
    g_signal_connect (view, "cursor-changed", G_CALLBACK (cursor_changed_callback), dialog);
    g_signal_connect (view, "row-activated", G_CALLBACK (row_activated_callback), dialog);
    gtk_container_add (GTK_CONTAINER (scrolled_window), view);

    vbox = gtk_vbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

    button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (button, "clicked", G_CALLBACK (edit_clicked_callback), view);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    g_object_set_data (G_OBJECT (dialog), "edit-button", button);

    button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (button, "clicked", G_CALLBACK (remove_clicked_callback), view);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    g_object_set_data (G_OBJECT (dialog), "remove-button", button);

    button = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (button, "clicked", G_CALLBACK (up_clicked_callback), view);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    g_object_set_data (G_OBJECT (dialog), "up-button", button);

    button = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (button, "clicked", G_CALLBACK (down_clicked_callback), view);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    g_object_set_data (G_OBJECT (dialog), "down-button", button);

    gtk_widget_grab_focus (view);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), RESPONSE_JUMP_TO);
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_JUMP_TO, FALSE);

#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_widget_show_all (content_area);
#else
    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);
#endif

    g_signal_connect (dialog, "size-allocate", G_CALLBACK (size_allocate_callback), NULL);
    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), view);

    gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);
}


static GtkTreeModel *create_and_fill_model (GtkTreePath *&current_group)
{
    GtkTreeStore *store = gtk_tree_store_new (NUM_COLUMNS,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_POINTER);

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

    return GTK_TREE_MODEL (store);
}


static GtkWidget *create_view_and_model ()
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "enable-search", TRUE,
                  "search-column", COL_NAME,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    GtkTooltips *tips = gtk_tooltips_new ();

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_GROUP, _("Group"));
    gtk_tooltips_set_tip (tips, col->button, _("Bookmark group"), NULL);

    g_object_set (renderer,
                  "weight-set", TRUE,
                  "weight", PANGO_WEIGHT_BOLD,
                  NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_NAME, _("Name"));
    gtk_tooltips_set_tip (tips, col->button, _("Bookmark name"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_SHORTCUT, _("Shortcut"));
    gtk_tooltips_set_tip (tips, col->button, _("Keyboard shortcut for selected bookmark"), NULL);

    g_object_set (renderer,
                  "foreground-set", TRUE,
                  "foreground", "DarkGray",
                  NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_PATH, _("Path"));
    gtk_tooltips_set_tip (tips, col->button, _("Bookmarked path"), NULL);

    g_object_set (renderer,
                  "foreground-set", TRUE,
                  "foreground", "DarkGray",
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  NULL);

    GtkTreePath *group = NULL;
    GtkTreeModel *model = create_and_fill_model (group);

    gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

    g_object_unref (model);          // destroy model automatically with view

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

    if (group)
        gtk_tree_view_expand_row (GTK_TREE_VIEW (view), group, TRUE);
    else
        group = gtk_tree_path_new_first ();

    gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), group, NULL, FALSE);
    gtk_tree_path_free (group);

    return view;
}


static void cursor_changed_callback (GtkTreeView *view, GtkWidget *dialog)
{
    GtkTreeModel *model = gtk_tree_view_get_model (view);
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), NULL, &iter))
    {
        GnomeCmdBookmark *bookmark = NULL;

        gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_JUMP_TO, bookmark!=NULL);
        gtk_widget_set_sensitive ((GtkWidget *) g_object_get_data (G_OBJECT (dialog), "edit-button"), bookmark!=NULL);
        gtk_widget_set_sensitive ((GtkWidget *) g_object_get_data (G_OBJECT (dialog), "remove-button"), bookmark!=NULL);
        gtk_widget_set_sensitive ((GtkWidget *) g_object_get_data (G_OBJECT (dialog), "up-button"), bookmark!=NULL && bookmark->group->bookmarks->data!=bookmark);
        gtk_widget_set_sensitive ((GtkWidget *) g_object_get_data (G_OBJECT (dialog), "down-button"), bookmark!=NULL && gtk_tree_model_iter_next (model, &iter));
    }
}


static void row_activated_callback (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer)
{
    gtk_dialog_response ((GtkDialog *) gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_DIALOG), RESPONSE_JUMP_TO);
}


static void edit_clicked_callback (GtkButton *button, GtkWidget *view)
{
    GtkWidget *dialog = gtk_widget_get_ancestor (view, GTK_TYPE_DIALOG);

    g_return_if_fail (dialog!=NULL);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        GnomeCmdBookmark *bookmark = NULL;
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
        gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

        if (gnome_cmd_edit_bookmark_dialog (GTK_WINDOW (dialog), _("Edit Bookmark"), bookmark->name, bookmark->path))
            gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
                                COL_NAME, bookmark->name,
                                COL_PATH, bookmark->path,
                                -1);
    }
}


static void remove_clicked_callback (GtkButton *button, GtkWidget *view)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        GnomeCmdBookmark *bookmark = NULL;
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
        gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);
        gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

        if (bookmark)
        {
            bookmark->group->bookmarks = g_list_remove (bookmark->group->bookmarks, bookmark);
            g_free (bookmark->name);
            g_free (bookmark->path);
            g_free (bookmark);
        }
    }
}


static void up_clicked_callback (GtkButton *button, GtkWidget *view)
{
    GtkWidget *dialog = gtk_widget_get_ancestor (view, GTK_TYPE_DIALOG);
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
        return;

    GnomeCmdBookmark *bookmark = NULL;
    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
    gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

    if (!bookmark)
        return;

    GList *elem = g_list_find (bookmark->group->bookmarks, bookmark);

    if (!elem)
        return;

    bookmark->group->bookmarks = g_list_insert_before (bookmark->group->bookmarks, g_list_previous (elem), bookmark);
    g_list_remove_link (bookmark->group->bookmarks, elem);

    GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
    GtkTreeIter prev;
    gtk_tree_path_prev (path);
    gtk_tree_model_get_iter (model, &prev, path);
    gtk_tree_path_free (path);

    gtk_tree_store_swap (GTK_TREE_STORE (model), &prev, &iter);

    gtk_widget_set_sensitive ((GtkWidget *) g_object_get_data (G_OBJECT (dialog), "up-button"), bookmark!=NULL && bookmark->group->bookmarks->data!=bookmark);
    gtk_widget_set_sensitive ((GtkWidget *) g_object_get_data (G_OBJECT (dialog), "down-button"), bookmark!=NULL && gtk_tree_model_iter_next (model, &iter));
}


static void down_clicked_callback (GtkButton *button, GtkWidget *view)
{
    GtkWidget *dialog = gtk_widget_get_ancestor (view, GTK_TYPE_DIALOG);
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
        return;

    GnomeCmdBookmark *bookmark = NULL;
    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
    gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

    if (!bookmark)
        return;

    GList *elem = g_list_find (bookmark->group->bookmarks, bookmark);

    if (!elem)
        return;

    g_list_insert (elem, bookmark, 2);
    bookmark->group->bookmarks = g_list_remove_link (bookmark->group->bookmarks, elem);

    GtkTreeIter next = iter;

    gtk_tree_model_iter_next (model, &next);
    gtk_tree_store_swap (GTK_TREE_STORE (model), &iter, &next);
    gtk_widget_set_sensitive ((GtkWidget *) g_object_get_data (G_OBJECT (dialog), "up-button"), bookmark!=NULL && bookmark->group->bookmarks->data!=bookmark);
    gtk_widget_set_sensitive ((GtkWidget *) g_object_get_data (G_OBJECT (dialog), "down-button"), bookmark!=NULL && gtk_tree_model_iter_next (model, &iter));
}


static void size_allocate_callback (GtkWidget *widget, GtkAllocation *allocation, gpointer unused)
{
    gnome_cmd_data.bookmarks_defaults.width = allocation->width;
    gnome_cmd_data.bookmarks_defaults.height = allocation->height;
}


static void response_callback (GtkDialog *dialog, int response_id, GtkTreeView *view)
{
    switch (response_id)
    {
        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-bookmarks");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case RESPONSE_JUMP_TO:
            {
                GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
                GtkTreeIter iter;

                if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)), NULL, &iter))
                {
                    GnomeCmdBookmark *bookmark = NULL;

                    gtk_tree_model_get (model, &iter, COL_BOOKMARK, &bookmark, -1);

                    if (bookmark)
                        gnome_cmd_bookmark_goto (bookmark);
                    else
                        g_signal_stop_emission_by_name (dialog, "response");
                }
                else
                    g_signal_stop_emission_by_name (dialog, "response");
            }

        case GTK_RESPONSE_CLOSE:
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
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
    gchar *path = gnome_cmd_dir_is_local (dir) ? GNOME_CMD_FILE (dir)->get_real_path () : GNOME_CMD_FILE (dir)->get_path();

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
    }
    else
    {
        g_free (name);
        g_free (path);
    }
}
