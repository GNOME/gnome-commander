/** 
 * @file gnome-cmd-remote-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2020 Uwe Scholz\n
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-remote-dialog.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-treeview.h"
#include "imageloader.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"
#include "dialogs/gnome-cmd-con-dialog.h"

using namespace std;


struct GnomeCmdRemoteDialog::Private
{
    GtkWidget *connection_list;
    GtkWidget *anonymous_pw_entry;
    GtkWidget *connect_button;
};


G_DEFINE_TYPE (GnomeCmdRemoteDialog, gnome_cmd_remote_dialog, GNOME_CMD_TYPE_DIALOG)


/******************************************************
    The main remote dialog
******************************************************/

enum
{
    COL_METHOD,
    COL_LOCK,
    COL_AUTH,
    COL_NAME,
    COL_CON,
    COL_FTP_CON,        // FIXME: to be removed
    NUM_COLS
} ;


inline gboolean model_is_empty (GtkTreeModel *tree_model)
{
    GtkTreeIter iter;

    return !gtk_tree_model_get_iter_first (tree_model, &iter);
}


inline gboolean tree_is_empty (GtkTreeView *tree_view)
{
    return model_is_empty (gtk_tree_view_get_model (tree_view));
}


inline GnomeCmdConRemote *get_selected_server (GnomeCmdRemoteDialog *dialog, GtkTreeIter *iter=NULL)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (dialog->priv->connection_list);
    GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
    GtkTreeIter priv_iter;

    if (!iter)
        iter = &priv_iter;

    GnomeCmdConRemote *server = NULL;

    if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), NULL, iter))
        return server;

    gtk_tree_model_get (model, iter, COL_FTP_CON, &server, -1);

    return server;
}


inline void set_server (GtkListStore *store, GtkTreeIter *iter, GnomeCmdConRemote *server)
{
    GnomeCmdCon *con = GNOME_CMD_CON (server);

    gtk_list_store_set (store, iter,
                        COL_METHOD, gnome_cmd_con_get_icon_name (con->method),
                        COL_LOCK, con->auth==GnomeCmdCon::SAVE_PERMANENTLY ? GTK_STOCK_DIALOG_AUTHENTICATION : NULL,
                        COL_AUTH, con->auth,
                        COL_NAME, gnome_cmd_con_get_alias (con),
                        COL_CON, con,
                        COL_FTP_CON, server,                // FIXME: to be removed
                        -1);
}


static gboolean do_connect_real (GnomeCmdConRemote *server)
{
    GnomeCmdCon *con = GNOME_CMD_CON (server);
    GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);

    if (fs->file_list()->locked)
        fs->new_tab();

    fs->set_connection(con);

    return FALSE;
}


inline void GnomeCmdRemoteDialog::do_connect(GnomeCmdConRemote *server)
{
    if (!server)
        server = get_selected_server (this);

    if (!server)        // exit as there is no server selected
        return;

    // store the anonymous ftp password as the user might have changed it
    const gchar *anon_pw = gtk_entry_get_text (GTK_ENTRY (priv->anonymous_pw_entry));
    gnome_cmd_data_set_ftp_anonymous_password (anon_pw);

    gtk_widget_destroy (*this);

    g_timeout_add (1, (GtkFunction) do_connect_real, server);
}


static void on_connect_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *remote_dialog)
{
    remote_dialog->do_connect();
}


static void on_close_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *dialog)
{
    gtk_widget_destroy (*dialog);
}


static void on_help_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *dialog)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-remote-connections");
}


static void on_new_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *remote_dialog)
{
    GnomeCmdConRemote *server = gnome_cmd_connect_dialog_new ();

    if (!server)
        return;

    GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (remote_dialog->priv->connection_list)));
    GtkTreeIter iter;

    gnome_cmd_con_list_get()->add(server);
    gtk_list_store_append (store, &iter);
    set_server (store, &iter, server);
}


static void on_edit_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *remote_dialog)
{
    GtkTreeIter iter;
    GnomeCmdConRemote *server = get_selected_server (remote_dialog, &iter);

    if (!server)        // exit as there is no server selected
        return;

    if (gnome_cmd_connect_dialog_edit (server))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (remote_dialog->priv->connection_list));
        set_server (GTK_LIST_STORE (model), &iter, server);
    }
}


static void on_remove_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *dialog)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (dialog->priv->connection_list);
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
        GnomeCmdConRemote *server = NULL;

        gtk_tree_model_get (model, &iter, COL_FTP_CON, &server, -1);
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        gnome_cmd_con_list_get()->remove(server);
    }
    else
        g_printerr (_("No server selected"));
}


enum
{
    SORTID_METHOD,
    SORTID_NAME
};


static void on_list_row_deleted (GtkTreeModel *tree_model, GtkTreePath *path, GnomeCmdRemoteDialog *dialog)
{
    if (model_is_empty (tree_model))
    {
        gtk_widget_set_sensitive (lookup_widget (*dialog, "remove_button"), FALSE);
        gtk_widget_set_sensitive (lookup_widget (*dialog, "edit_button"), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (dialog->priv->connect_button), FALSE);
    }
}


static void on_list_row_inserted (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, GnomeCmdRemoteDialog *dialog)
{
    gtk_widget_set_sensitive (lookup_widget (*dialog, "remove_button"), TRUE);
    gtk_widget_set_sensitive (lookup_widget (*dialog, "edit_button"), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (dialog->priv->connect_button), TRUE);
}


static void on_list_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, GnomeCmdRemoteDialog *dialog)
{
    dialog->do_connect();
}


static gint sort_by_name (GtkTreeModel *model, GtkTreeIter *i1, GtkTreeIter *i2, gpointer user_data)
{
    gchar *s1;
    gchar *s2;

    gtk_tree_model_get (model, i1, COL_NAME, &s1, -1);
    gtk_tree_model_get (model, i2, COL_NAME, &s2, -1);

    gint retval = 0;

    if (!s1 && !s2)
        return retval;

    if (!s1)
        retval = 1;
    else
        if (!s2)
            retval = -1;
        else
        {
            // compare s1 and s2 in UTF8 aware way, case insensitive

            gchar *is1 = g_utf8_casefold (s1, -1);
            gchar *is2 = g_utf8_casefold (s2, -1);

            retval = g_utf8_collate (is1, is2);

            g_free (is1);
            g_free (is2);
        }

    g_free (s1);
    g_free (s2);

    return retval;
}


static gint sort_by_method (GtkTreeModel *model, GtkTreeIter *i1, GtkTreeIter *i2, gpointer user_data)
{
    GnomeCmdCon *c1;
    GnomeCmdCon *c2;

    gtk_tree_model_get (model, i1, COL_CON, &c1, -1);
    gtk_tree_model_get (model, i2, COL_CON, &c2, -1);

    if (c1->method==c2->method)
        return sort_by_name (model, i1, i2, user_data);

    return int(c1->method) - int(c2->method);
}


inline GtkTreeModel *create_and_fill_model (GList *list)
{
    GtkListStore *store = gtk_list_store_new (NUM_COLS,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_BOOLEAN,
                                              G_TYPE_STRING,
                                              G_TYPE_POINTER,
                                              G_TYPE_POINTER);

    for (GtkTreeIter iter; list; list=list->next)
    {
        gtk_list_store_append (store, &iter);
        set_server (store, &iter, GNOME_CMD_CON_REMOTE (list->data));
    }

    GtkTreeSortable *sortable = GTK_TREE_SORTABLE (store);

    gtk_tree_sortable_set_sort_func (sortable, SORTID_METHOD, sort_by_method, NULL, NULL);
    gtk_tree_sortable_set_sort_func (sortable, SORTID_NAME, sort_by_name, NULL, NULL);

    gtk_tree_sortable_set_sort_column_id (sortable, SORTID_NAME, GTK_SORT_ASCENDING);   // set initial sort order

    return GTK_TREE_MODEL (store);
}


inline GtkWidget *create_view_and_model (GList *list)
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "enable-search", TRUE,
                  "search-column", COL_NAME,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    // col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_AUTH);

    col = gnome_cmd_treeview_create_new_pixbuf_column (GTK_TREE_VIEW (view), renderer, COL_METHOD);
    gtk_widget_set_tooltip_text (col->button, _("Network protocol"));
    gtk_tree_view_column_set_sort_column_id (col, SORTID_METHOD);

    // col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_METHOD);
    // gtk_tree_view_column_set_sort_column_id (col, SORTID_METHOD);
    // g_object_set (renderer,
                  // "foreground-set", TRUE,
                  // "foreground", "DarkGray",
                  // NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_NAME, _("Name"));
    gtk_widget_set_tooltip_text (col->button, _("Connection name"));
    gtk_tree_view_column_set_sort_column_id (col, SORTID_NAME);
    g_object_set (renderer,
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  NULL);

    GtkTreeModel *model = create_and_fill_model (list);

    gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

    g_object_unref (model);          // destroy model automatically with view

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_first (gtk_tree_view_get_model (GTK_TREE_VIEW (view)), &iter))      // select the first row here...
        gtk_tree_selection_select_iter (selection, &iter);

    return view;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_remote_dialog_finalize (GObject *object)
{
    GnomeCmdRemoteDialog *dialog = GNOME_CMD_REMOTE_DIALOG (object);

    g_free (dialog->priv);

    G_OBJECT_CLASS (gnome_cmd_remote_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_remote_dialog_class_init (GnomeCmdRemoteDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_remote_dialog_finalize;
}


static void gnome_cmd_remote_dialog_init (GnomeCmdRemoteDialog *dialog)
{
    GtkWidget *cat_box, *table, *cat, *sw, *label, *button, *bbox;

    dialog->priv = g_new0 (GnomeCmdRemoteDialog::Private, 1);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (dialog), _("Remote Connections"));

    gtk_window_set_transient_for (GTK_WINDOW (dialog), *main_win);
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

    cat_box = create_hbox (*dialog, FALSE, 12);
    cat = create_category (*dialog, cat_box, _("Connections"));
    gnome_cmd_dialog_add_expanding_category (*dialog, cat);

    sw = create_sw (*dialog);
    gtk_box_pack_start (GTK_BOX (cat_box), sw, TRUE, TRUE, 0);

    dialog->priv->connection_list = create_view_and_model (get_remote_cons ());
    g_object_ref (dialog->priv->connection_list);
    g_object_set_data_full (*dialog, "connection_list", dialog->priv->connection_list, g_object_unref);
    gtk_widget_show (dialog->priv->connection_list);
    gtk_container_add (GTK_CONTAINER (sw), dialog->priv->connection_list);
    gtk_widget_set_size_request (dialog->priv->connection_list, -1, 240);

    // check if there are any items in the connection list
    gboolean empty_view = tree_is_empty (GTK_TREE_VIEW (dialog->priv->connection_list));

    bbox = create_vbuttonbox (*dialog);
    gtk_box_pack_start (GTK_BOX (cat_box), bbox, FALSE, FALSE, 0);
    button = create_stock_button (*dialog, GTK_STOCK_ADD, GTK_SIGNAL_FUNC (on_new_btn_clicked));
    gtk_container_add (GTK_CONTAINER (bbox), button);
    button = create_named_stock_button (*dialog, GTK_STOCK_EDIT, "edit_button", GTK_SIGNAL_FUNC (on_edit_btn_clicked));
    gtk_widget_set_sensitive (button, !empty_view);
    gtk_container_add (GTK_CONTAINER (bbox), button);
    button = create_named_stock_button (*dialog, GTK_STOCK_REMOVE, "remove_button", GTK_SIGNAL_FUNC (on_remove_btn_clicked));
    gtk_widget_set_sensitive (button, !empty_view);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    table = create_table (*dialog, 1, 2);
    cat = create_category (*dialog, table, _("Options"));
    gnome_cmd_dialog_add_category (*dialog, cat);

    label = create_label (*dialog, _("Anonymous FTP password:"));
    table_add (table, label, 0, 0, (GtkAttachOptions) 0);

    dialog->priv->anonymous_pw_entry = create_entry (*dialog, "anonymous_pw_entry", gnome_cmd_data_get_ftp_anonymous_password ());
    table_add (table, dialog->priv->anonymous_pw_entry, 1, 0, GTK_FILL);

    gnome_cmd_dialog_add_button (*dialog, GTK_STOCK_HELP, GTK_SIGNAL_FUNC (on_help_btn_clicked), dialog);
    gnome_cmd_dialog_add_button (*dialog, GTK_STOCK_CLOSE, GTK_SIGNAL_FUNC (on_close_btn_clicked), dialog);
    button = gnome_cmd_dialog_add_button (*dialog, GTK_STOCK_CONNECT, GTK_SIGNAL_FUNC (on_connect_btn_clicked), dialog);

    dialog->priv->connect_button = button;
    gtk_widget_set_sensitive (button, !empty_view);

    g_signal_connect (dialog->priv->connection_list, "row-activated", G_CALLBACK (on_list_row_activated), dialog);

    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->connection_list));

    g_signal_connect (model, "row-inserted", G_CALLBACK (on_list_row_inserted), dialog);
    g_signal_connect (model, "row-deleted", G_CALLBACK (on_list_row_deleted), dialog);

    gtk_widget_grab_focus (dialog->priv->connection_list);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_remote_dialog_new ()
{
    return GTK_WIDGET (g_object_new (GNOME_CMD_TYPE_REMOTE_DIALOG, NULL));
}


/***********************************************
 *
 * The quick connect dialog
 *
 ***********************************************/

void show_quick_connect_dialog ()
{
    GnomeCmdConRemote *con = gnome_cmd_data.get_quick_connect();

    if (gnome_cmd_connect_dialog_edit (con))
        do_connect_real (con);
}
