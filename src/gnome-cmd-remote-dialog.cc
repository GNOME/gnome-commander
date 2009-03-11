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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-remote-dialog.h"
#include "gnome-cmd-con-dialog.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-con-ftp.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-treeview.h"
#include "imageloader.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"

using namespace std;


static GnomeCmdDialogClass *parent_class = NULL;


struct GnomeCmdRemoteDialogPrivate
{
    GtkWidget         *connection_list;
    GtkWidget         *anonymous_pw_entry;
    GtkWidget         *connect_button;
};



/******************************************************
    The main ftp dialog
******************************************************/

enum
{
    COL_METHOD,
    COL_LOCK,
    COL_AUTH,
    COL_NAME,
    COL_CON,
    COL_FTP_CON,        // later: to be removed
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


inline GnomeCmdConFtp *get_selected_server (GnomeCmdRemoteDialog *dialog, GtkTreeIter *iter=NULL)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (dialog->priv->connection_list);
    GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
    GtkTreeIter priv_iter;

    if (!iter)
        iter = &priv_iter;

    GnomeCmdConFtp *server = NULL;

    if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), NULL, iter))
        return server;

    gtk_tree_model_get (model, iter, COL_FTP_CON, &server, -1);

    return server;
}


inline void set_server (GtkListStore *store, GtkTreeIter *iter, GnomeCmdConFtp *server)
{
    GnomeCmdCon *con = GNOME_CMD_CON (server);

    gtk_list_store_set (store, iter,
                        COL_METHOD, gnome_cmd_con_get_icon_name (con->method),
                        COL_LOCK, con->gnome_auth ? GTK_STOCK_DIALOG_AUTHENTICATION : NULL,
                        COL_AUTH, con->gnome_auth,
                        COL_NAME, gnome_cmd_con_get_alias (con),
                        COL_CON, con,
                        COL_FTP_CON, server,                // later: to be removed
                        -1);
}


static gboolean do_connect_real (GnomeCmdConFtp *server)
{
    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_fs (main_win, ACTIVE);
    GnomeCmdCon *con = GNOME_CMD_CON (server);

    fs->set_connection(con);
    // gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, G_DIR_SEPARATOR_S)));

    return FALSE;
}


inline void do_connect (GnomeCmdRemoteDialog *ftp_dialog, GnomeCmdConFtp *server=NULL)
{
    if (!server)
        server = get_selected_server (ftp_dialog);

    if (!server)        // exit as there is no server selected
        return;

    // store the anonymous ftp password as the user might have changed it
    const gchar *anon_pw = gtk_entry_get_text (GTK_ENTRY (ftp_dialog->priv->anonymous_pw_entry));
    gnome_cmd_data_set_ftp_anonymous_password (anon_pw);

    GnomeCmdCon *con = GNOME_CMD_CON (server);

    gtk_widget_destroy (GTK_WIDGET (ftp_dialog));

    g_timeout_add (1, (GtkFunction) do_connect_real, server);
}


static void on_connect_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *ftp_dialog)
{
    do_connect (ftp_dialog);
}


static void on_close_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *dialog)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_help_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *dialog)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-remote-connections");
}


static void on_new_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *ftp_dialog)
{
    GnomeCmdConFtp *server = gnome_cmd_connect_dialog_new ();

    if (!server)
        return;

    GnomeCmdCon *con = GNOME_CMD_CON (server);
    GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (ftp_dialog->priv->connection_list)));
    GtkTreeIter iter;

    gnome_cmd_con_list_add_ftp (gnome_cmd_con_list_get (), server);
    gtk_list_store_append (store, &iter);
    set_server (store, &iter, server);
}


static void on_edit_btn_clicked (GtkButton *button, GnomeCmdRemoteDialog *ftp_dialog)
{
    GtkTreeIter iter;
    GnomeCmdConFtp *server = get_selected_server (ftp_dialog, &iter);

    if (!server)        // exit as there is no server selected
        return;

    if (gnome_cmd_connect_dialog_edit (server))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (ftp_dialog->priv->connection_list));
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
        GnomeCmdConFtp *server = NULL;

        gtk_tree_model_get (model, &iter, COL_FTP_CON, &server, -1);
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        gnome_cmd_con_list_remove_ftp (gnome_cmd_con_list_get (), server);
    }
    else
        g_printerr (_("No server selected"));
}


enum
{
    SORTID_AUTH,
    SORTID_METHOD,
    SORTID_NAME
};


static void on_list_row_deleted (GtkTreeModel *tree_model, GtkTreePath *path, GnomeCmdRemoteDialog *dialog)
{
    if (model_is_empty (tree_model))
    {
        gtk_widget_set_sensitive (lookup_widget (GTK_WIDGET (dialog), "remove_button"), FALSE);
        gtk_widget_set_sensitive (lookup_widget (GTK_WIDGET (dialog), "edit_button"), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (dialog->priv->connect_button), FALSE);
    }
}


static void on_list_row_inserted (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, GnomeCmdRemoteDialog *dialog)
{
    gtk_widget_set_sensitive (lookup_widget (GTK_WIDGET (dialog), "remove_button"), TRUE);
    gtk_widget_set_sensitive (lookup_widget (GTK_WIDGET (dialog), "edit_button"), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (dialog->priv->connect_button), TRUE);
}


static void on_list_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, GnomeCmdRemoteDialog *dialog)
{
    do_connect (dialog);
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


static gint sort_by_auth (GtkTreeModel *model, GtkTreeIter *i1, GtkTreeIter *i2, gpointer user_data)
{
    gboolean a1;
    gboolean a2;

    gtk_tree_model_get (model, i1, COL_AUTH, &a1, -1);
    gtk_tree_model_get (model, i2, COL_AUTH, &a2, -1);

    return a1 == a2 ? sort_by_name (model, i1, i2, user_data) :
                      a1 ? -1 : 1;
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
        GnomeCmdCon *con = GNOME_CMD_CON (list->data);

        gtk_list_store_append (store, &iter);
        set_server (store, &iter, GNOME_CMD_CON_FTP (list->data));
    }

    GtkTreeSortable *sortable = GTK_TREE_SORTABLE (store);

    gtk_tree_sortable_set_sort_func (sortable, SORTID_AUTH, sort_by_auth, NULL, NULL);
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

    GtkTooltips *tips = gtk_tooltips_new ();

    // col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_AUTH);

    col = gnome_cmd_treeview_create_new_pixbuf_column (GTK_TREE_VIEW (view), renderer, COL_LOCK);
    gtk_tooltips_set_tip (tips, col->button, _("GNOME authentication manager usage"), NULL);
    gtk_tree_view_column_set_sort_column_id (col, SORTID_AUTH);

    col = gnome_cmd_treeview_create_new_pixbuf_column (GTK_TREE_VIEW (view), renderer, COL_METHOD);
    gtk_tooltips_set_tip (tips, col->button, _("Network protocol"), NULL);
    gtk_tree_view_column_set_sort_column_id (col, SORTID_METHOD);

    // col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_METHOD);
    // gtk_tree_view_column_set_sort_column_id (col, SORTID_METHOD);
    // g_object_set (renderer,
                  // "foreground-set", TRUE,
                  // "foreground", "DarkGray",
                  // NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_NAME, _("Name"));
    gtk_tooltips_set_tip (tips, col->button, _("Connection name"), NULL);
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

static void destroy (GtkObject *object)
{
    GnomeCmdRemoteDialog *dialog = GNOME_CMD_REMOTE_DIALOG (object);

    if (!dialog->priv)
        g_warning ("GnomeCmdRemoteDialog: dialog->priv != NULL test failed");

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdRemoteDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (gnome_cmd_dialog_get_type ());

    object_class->destroy = destroy;

    widget_class->map = ::map;
}


static void init (GnomeCmdRemoteDialog *ftp_dialog)
{
    GtkWidget *cat_box, *table, *cat, *sw, *label, *button, *bbox;

    GtkWidget *dialog = GTK_WIDGET (ftp_dialog);

    ftp_dialog->priv = g_new0 (GnomeCmdRemoteDialogPrivate, 1);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (dialog), _("Remote Connections"));

    gnome_cmd_dialog_set_transient_for (GNOME_CMD_DIALOG (dialog), GTK_WINDOW (main_win));
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

    cat_box = create_hbox (dialog, FALSE, 12);
    cat = create_category (dialog, cat_box, _("Connections"));
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), cat);

    sw = create_sw (dialog);
    gtk_box_pack_start (GTK_BOX (cat_box), sw, TRUE, TRUE, 0);

    ftp_dialog->priv->connection_list = create_view_and_model (get_ftp_cons ());
    gtk_widget_ref (ftp_dialog->priv->connection_list);
    gtk_object_set_data_full (GTK_OBJECT (dialog), "connection_list", ftp_dialog->priv->connection_list, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (ftp_dialog->priv->connection_list);
    gtk_container_add (GTK_CONTAINER (sw), ftp_dialog->priv->connection_list);
    gtk_widget_set_size_request (ftp_dialog->priv->connection_list, -1, 240);

    // check if there are any items in the connection list
    gboolean empty_view = tree_is_empty (GTK_TREE_VIEW (ftp_dialog->priv->connection_list));

    bbox = create_vbuttonbox (dialog);
    gtk_box_pack_start (GTK_BOX (cat_box), bbox, FALSE, FALSE, 0);
    button = create_stock_button (dialog, GTK_STOCK_ADD, GTK_SIGNAL_FUNC (on_new_btn_clicked));
    gtk_container_add (GTK_CONTAINER (bbox), button);
    button = create_named_stock_button (dialog, GTK_STOCK_EDIT, "edit_button", GTK_SIGNAL_FUNC (on_edit_btn_clicked));
    gtk_widget_set_sensitive (button, !empty_view);
    gtk_container_add (GTK_CONTAINER (bbox), button);
    button = create_named_stock_button (dialog, GTK_STOCK_REMOVE, "remove_button", GTK_SIGNAL_FUNC (on_remove_btn_clicked));
    gtk_widget_set_sensitive (button, !empty_view);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    table = create_table (dialog, 1, 2);
    cat = create_category (dialog, table, _("Options"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), cat);

    label = create_label (dialog, _("Anonymous FTP password:"));
    table_add (table, label, 0, 0, (GtkAttachOptions) 0);

    ftp_dialog->priv->anonymous_pw_entry = create_entry (dialog, "anonymous_pw_entry", gnome_cmd_data_get_ftp_anonymous_password ());
    table_add (table, ftp_dialog->priv->anonymous_pw_entry, 1, 0, GTK_FILL);

    button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_HELP, GTK_SIGNAL_FUNC (on_help_btn_clicked), dialog);
    button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_SIGNAL_FUNC (on_close_btn_clicked), dialog);
    button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CONNECT, GTK_SIGNAL_FUNC (on_connect_btn_clicked), dialog);

    ftp_dialog->priv->connect_button = button;
    gtk_widget_set_sensitive (button, !empty_view);

    gtk_signal_connect (GTK_OBJECT (ftp_dialog->priv->connection_list), "row-activated", GTK_SIGNAL_FUNC (on_list_row_activated), dialog);

    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (ftp_dialog->priv->connection_list));

    g_signal_connect (G_OBJECT (model), "row-inserted", G_CALLBACK (on_list_row_inserted), dialog);
    g_signal_connect (G_OBJECT (model), "row-deleted", G_CALLBACK (on_list_row_deleted), dialog);

    gtk_widget_grab_focus (ftp_dialog->priv->connection_list);
}


/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_remote_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdRemoteDialog",
            sizeof (GnomeCmdRemoteDialog),
            sizeof (GnomeCmdRemoteDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gnome_cmd_dialog_get_type (), &dlg_info);
    }
    return dlg_type;
}


GtkWidget *gnome_cmd_remote_dialog_new ()
{
    GnomeCmdRemoteDialog *dialog = (GnomeCmdRemoteDialog *) gtk_type_new (gnome_cmd_remote_dialog_get_type ());

    return GTK_WIDGET (dialog);
}


/***********************************************
 *
 * The quick connect dialog
 *
 ***********************************************/

void show_quick_connect_dialog ()
{
    GnomeCmdConFtp *con = gnome_cmd_data.get_quick_connect();

    if (gnome_cmd_connect_dialog_edit (con))
        do_connect_real (con);
}
