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

#include <config.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-ftp-dialog.h"
#include "gnome-cmd-con-ftp.h"
#include "gnome-cmd-data.h"
#include "imageloader.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"


static GnomeCmdDialogClass *parent_class = NULL;


struct _GnomeCmdFtpDialogPrivate {
    GnomeCmdConFtp *selected_server;

    GtkWidget         *server_list;
    GtkWidget         *anonymous_pw_entry;
};



/******************************************************
    The main ftp dialog
******************************************************/

static GList *
get_ftp_connections ()
{
    return gnome_cmd_con_list_get_all_ftp (gnome_cmd_data_get_con_list ());
}


static void
load_ftp_connections (GnomeCmdFtpDialog *dialog)
{
    GList *tmp = get_ftp_connections ();
    GtkCList *server_list = GTK_CLIST (dialog->priv->server_list);

    gtk_clist_clear (server_list);

    if (tmp)
        dialog->priv->selected_server = GNOME_CMD_CON_FTP (tmp->data);

    for (; tmp; tmp = tmp->next)
    {
        GnomeCmdConFtp *server = GNOME_CMD_CON_FTP (tmp->data);
        if (server)
        {
            int row;
            gchar *text[4];

            text[0] = NULL;
            text[1] = (gchar *) gnome_cmd_con_ftp_get_alias (server);
            text[2] = (gchar *) gnome_cmd_con_ftp_get_host_name (server);
            text[3] = NULL;
            row = gtk_clist_append (server_list, text);
            if (!row)
                gtk_clist_select_row (server_list, 0, 0);

            gtk_clist_set_row_data (server_list, row, server);
            gtk_clist_set_pixmap (GTK_CLIST (server_list), row, 0,
                                  IMAGE_get_pixmap (PIXMAP_SERVER_SMALL),
                                  IMAGE_get_mask (PIXMAP_SERVER_SMALL));
        }
        else
            g_warning ("NULL entry in the ftp-server list");
     }
}


static gboolean
do_connect_real (GnomeCmdConFtp *server)
{
    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_active_fs (main_win);
    GnomeCmdCon *con = GNOME_CMD_CON (server);

    gnome_cmd_file_selector_set_connection (fs, con, NULL);
//        gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, G_DIR_SEPARATOR_S)));

    return FALSE;
}


static void
do_connect (GtkWidget *dialog,
            GnomeCmdConFtp *server,
            const gchar *password)
{
    if (!server) return;

    gtk_widget_destroy (GTK_WIDGET (dialog));

    gnome_cmd_con_ftp_set_pw (server, password);

    g_timeout_add (1, (GtkFunction)do_connect_real, server);
}


static gboolean
on_password_ok (GnomeCmdStringDialog *string_dialog,
                const gchar **values,
                GnomeCmdFtpDialog *dialog)
{
    const gchar *password = values[0];

    do_connect (GTK_WIDGET (dialog), dialog->priv->selected_server, password);

    return TRUE;
}


static void
on_connect_btn_clicked (GtkButton         *button,
                        GnomeCmdFtpDialog *ftp_dialog)
{
    GnomeCmdConFtp *server = ftp_dialog->priv->selected_server;

    if (server)
    {
        const gchar *anon_pw = gtk_entry_get_text (GTK_ENTRY (ftp_dialog->priv->anonymous_pw_entry));
        const gchar *uname = gnome_cmd_con_ftp_get_user_name (server);
        const gchar *pw = gnome_cmd_con_ftp_get_pw (server);

        // store the anonymous password as the user might have changed it
        gnome_cmd_data_set_ftp_anonymous_password (anon_pw);


        if (strcmp (uname, "anonymous") != 0 && pw != NULL) {
            do_connect (GTK_WIDGET (ftp_dialog), ftp_dialog->priv->selected_server, pw);
        }
        else if (strcmp (uname, "anonymous") != 0 && pw == NULL) {
            gchar *labels[1];
            GtkWidget *dialog;

            labels[0] = g_strdup_printf (
                _("Enter password for %s@%s"),
                gnome_cmd_con_ftp_get_user_name (server),
                gnome_cmd_con_ftp_get_host_name (server));

            dialog = gnome_cmd_string_dialog_new (
                _("Enter Password"),
                (const gchar **)labels,
                1,
                (GnomeCmdStringDialogCallback)on_password_ok,
                ftp_dialog);

            gnome_cmd_string_dialog_set_hidden (GNOME_CMD_STRING_DIALOG (dialog), 0, TRUE);
            gtk_widget_ref (dialog);
            gtk_object_set_data_full (GTK_OBJECT (ftp_dialog),
                                      "ftp-password-dialog", dialog,
                                      (GtkDestroyNotify)gtk_widget_unref);
            gtk_widget_show (dialog);
        }
        else
            do_connect (GTK_WIDGET (ftp_dialog), ftp_dialog->priv->selected_server, anon_pw);
    }
}


static void
on_cancel_btn_clicked (GtkButton       *button,
                       GnomeCmdFtpDialog *dialog)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static gchar*
update_server_from_strings (GnomeCmdConFtp **server,
                            const gchar **values,
                            gboolean with_alias)
{
    gint i=0;
    const gchar *alias = NULL;
    const gchar *host;
    const gchar *port;
    const gchar *user;
    const gchar *pw;
    const gchar *remote_dir;
    gushort iport;

    if (with_alias) alias = values[i++];
    host  = values[i++];
    port  = values[i++];
    user  = values[i++];
    pw    = values[i++];
    remote_dir = values[i++];

    if (with_alias && !alias)
        return g_strdup (_("No alias specified"));

    if (!host)
        return g_strdup (_("No host specified"));

    if (!string2ushort (port, &iport))
        return g_strdup_printf (_("Invalid port number: %s"), port);

    if (!(*server)) {
        *server = gnome_cmd_con_ftp_new (alias?alias:"tmp", host, iport, user, pw, remote_dir);
    }
    else {
        if (with_alias)
            gnome_cmd_con_ftp_set_alias (*server, alias);
        gnome_cmd_con_ftp_set_host_name (*server, host);
        gnome_cmd_con_ftp_set_user_name (*server, user);
        gnome_cmd_con_ftp_set_host_port (*server, iport);
        gnome_cmd_con_ftp_set_pw (*server, pw);
        gnome_cmd_con_ftp_set_remote_dir (*server, remote_dir);
    }

    return NULL;
}


static gboolean
on_new_ftp_server_dialog_ok (GnomeCmdStringDialog *string_dialog,
                             const gchar          **values,
                             GnomeCmdFtpDialog    *ftp_dialog)
{
    GnomeCmdConFtp *server = NULL;
    gchar *error_desc = update_server_from_strings (&server, values, TRUE);

    if (error_desc != NULL) {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, error_desc);
        gtk_object_unref (GTK_OBJECT (server));
    }
    else {
        gnome_cmd_con_list_add_ftp (gnome_cmd_data_get_con_list (), server);
        load_ftp_connections (ftp_dialog);
    }

    return error_desc == NULL;
}


static gboolean
on_edit_ftp_server_dialog_ok (GnomeCmdStringDialog *string_dialog,
                              const gchar **values,
                              GnomeCmdFtpDialog *ftp_dialog)
{
    GnomeCmdConFtp *server = ftp_dialog->priv->selected_server;
    gchar *error_desc = update_server_from_strings (&server, values, TRUE);

    if (error_desc != NULL)
        gnome_cmd_string_dialog_set_error_desc (string_dialog, error_desc);
    else
        load_ftp_connections (ftp_dialog);

    return error_desc == NULL;
}


static void
on_pw_entry_changed (GtkEditable *editable, GnomeCmdStringDialog *string_dialog)
{
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (editable));
    gtk_widget_set_sensitive (string_dialog->entries[string_dialog->rows-2],
                              !(text && strcmp (text, "anonymous") == 0));
}


static GtkWidget*
create_ftp_server_dialog (const gchar *title,
                          GnomeCmdStringDialogCallback on_ok_func,
                          GnomeCmdFtpDialog *ftp_dialog,
                          gboolean with_alias)
{
    const gchar *labels1[] = {_("Alias:"), _("Host:"), _("Port:"), _("User:"), _("Password:"), _("Remote Dir:")};
    const gchar *labels2[] = {_("Host:"), _("Port:"), _("User:"), _("Password:"), _("Remote Dir:")};

    const gchar **labels = with_alias ? labels1 : labels2;
    const guint labels_size = with_alias ? ARRAY_ELEMENTS(labels1) : ARRAY_ELEMENTS(labels2);

    GtkWidget *dialog;
    GtkWidget *pw_entry;

    dialog = gnome_cmd_string_dialog_new (title, labels, labels_size, on_ok_func, ftp_dialog);
    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);

    gnome_cmd_string_dialog_set_hidden (GNOME_CMD_STRING_DIALOG (dialog), with_alias?4:3, TRUE);

    pw_entry = GNOME_CMD_STRING_DIALOG (dialog)->entries[with_alias?3:2];
    gtk_signal_connect (GTK_OBJECT (pw_entry), "changed", GTK_SIGNAL_FUNC (on_pw_entry_changed), dialog);

    return dialog;
}


static void
on_new_btn_clicked (GtkButton         *button,
                    GnomeCmdFtpDialog *ftp_dialog)
{
    GtkWidget *dialog;

    dialog = create_ftp_server_dialog (
        _("New FTP Connection"), (GnomeCmdStringDialogCallback)on_new_ftp_server_dialog_ok,
        ftp_dialog, TRUE);

    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 2, "21");
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 3, "anonymous");
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 4, gnome_cmd_data_get_ftp_anonymous_password ());
}


static void
on_edit_btn_clicked (GtkButton         *button,
                     GnomeCmdFtpDialog *ftp_dialog)
{
    const gchar *alias;
    const gchar *host;
    gchar *port;
    const gchar *remote_dir;
    const gchar *user;
    const gchar *pw;
    GtkWidget *dialog;
    GnomeCmdConFtp *server = ftp_dialog->priv->selected_server;

    g_return_if_fail (server != NULL);

    dialog = create_ftp_server_dialog (_("Edit FTP Connection"), (GnomeCmdStringDialogCallback)on_edit_ftp_server_dialog_ok, ftp_dialog, TRUE);

    alias = gnome_cmd_con_ftp_get_alias (server);
    host  = gnome_cmd_con_ftp_get_host_name (server);
    port  = g_strdup_printf ("%d", gnome_cmd_con_ftp_get_host_port (server));
    remote_dir = gnome_cmd_con_ftp_get_remote_dir (server);
    user  = gnome_cmd_con_ftp_get_user_name (server);
    pw    = gnome_cmd_con_ftp_get_pw (server);

    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, alias);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 1, host);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 2, port);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 3, user);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 4, pw);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 5, remote_dir);
}


static void
on_remove_btn_clicked                  (GtkButton       *button,
                                        GnomeCmdFtpDialog *dialog)
{
    GnomeCmdConFtp *server = dialog->priv->selected_server;

    if (server)
    {
        gnome_cmd_con_list_remove_ftp (gnome_cmd_data_get_con_list (),
                                       server);
        load_ftp_connections (dialog);
        dialog->priv->selected_server = NULL;
    }
    else
        g_printerr (_("No server selected"));
}


static void
on_server_list_select_row                (GtkCList *clist, gint row, gint column,
                                          GdkEvent *event, gpointer user_data)
{
    GnomeCmdFtpDialog *dialog = GNOME_CMD_FTP_DIALOG (user_data);
    GtkWidget *remove_button = lookup_widget (GTK_WIDGET (dialog), "remove_button");
    GtkWidget *edit_button = lookup_widget (GTK_WIDGET (dialog), "edit_button");

    gtk_widget_set_sensitive (remove_button, TRUE);
    gtk_widget_set_sensitive (edit_button, TRUE);

    if (!event)
        return;

    if (event->type == GDK_2BUTTON_PRESS)
    {
        on_connect_btn_clicked (NULL, dialog);
    }
    else
    {
        dialog->priv->selected_server = GNOME_CMD_CON_FTP (gtk_clist_get_row_data (clist, row));
    }
}


static void
on_server_list_unselect_row              (GtkCList *clist, gint row, gint column,
                                          GdkEvent *event, gpointer user_data)
{
    GnomeCmdFtpDialog *dialog = GNOME_CMD_FTP_DIALOG (user_data);
    GtkWidget *remove_button = lookup_widget (GTK_WIDGET (dialog), "remove_button");
    GtkWidget *edit_button = lookup_widget (GTK_WIDGET (dialog), "edit_button");

    gtk_widget_set_sensitive (remove_button, FALSE);
    gtk_widget_set_sensitive (edit_button, FALSE);
}


static void
on_server_list_row_move                  (GtkCList        *clist,
                                          gint             arg1,
                                          gint             arg2,
                                          gpointer         user_data)
{
    GList *servers = get_ftp_connections ();
    gpointer s = g_list_nth_data (servers, arg1);

    g_return_if_fail (s != NULL);

    servers = g_list_remove (servers, s);
    servers = g_list_insert (servers, s, arg2);

    gnome_cmd_con_list_set_all_ftp (gnome_cmd_data_get_con_list (), servers);
}


static void
on_server_list_scroll_vertical                  (GtkCList          *clist,
                                                 GtkScrollType      scroll_type,
                                                 gfloat             position,
                                                 GnomeCmdFtpDialog *dialog)
{
    dialog->priv->selected_server = GNOME_CMD_CON_FTP (gtk_clist_get_row_data (clist, clist->focus_row));

    gtk_clist_select_row (clist, clist->focus_row, 0);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdFtpDialog *dialog = GNOME_CMD_FTP_DIALOG (object);

    if (!dialog->priv)
        g_warning ("GnomeCmdFtpDialog: dialog->priv != NULL test failed");
    else
        g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdFtpDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = gtk_type_class (gnome_cmd_dialog_get_type ());

    object_class->destroy = destroy;

    widget_class->map = map;
}

static void
init (GnomeCmdFtpDialog *ftp_dialog)
{
    GtkWidget *cat_box, *table, *cat, *sw, *label, *button, *bbox;

    GtkWidget *dialog = GTK_WIDGET (ftp_dialog);

    ftp_dialog->priv = g_new0 (GnomeCmdFtpDialogPrivate, 1);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (dialog), _("FTP Connect"));

    gnome_cmd_dialog_set_transient_for (GNOME_CMD_DIALOG (dialog), GTK_WINDOW (main_win));

    cat_box = create_hbox (dialog, FALSE, 12);
    cat = create_category (dialog, cat_box, _("Connections"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), cat);

    sw = create_sw (dialog);
    gtk_box_pack_start (GTK_BOX (cat_box), sw, TRUE, TRUE, 0);

    ftp_dialog->priv->server_list = gtk_clist_new (2);
    gtk_widget_ref (ftp_dialog->priv->server_list);
    gtk_object_set_data_full (GTK_OBJECT (dialog), "server_list", ftp_dialog->priv->server_list,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_clist_set_row_height (GTK_CLIST (ftp_dialog->priv->server_list), 16);
    gtk_widget_show (ftp_dialog->priv->server_list);
    gtk_container_add (GTK_CONTAINER (sw), ftp_dialog->priv->server_list);
    gtk_widget_set_size_request (ftp_dialog->priv->server_list, -1, 200);
    gtk_clist_set_column_width (GTK_CLIST (ftp_dialog->priv->server_list), 0, 16);
    gtk_clist_set_column_width (GTK_CLIST (ftp_dialog->priv->server_list), 1, 80);
    gtk_clist_column_titles_show (GTK_CLIST (ftp_dialog->priv->server_list));

    label = create_label (dialog, "");
    gtk_clist_set_column_widget (GTK_CLIST (ftp_dialog->priv->server_list), 0, label);
    label = create_label (dialog, _("Alias"));
    gtk_clist_set_column_widget (GTK_CLIST (ftp_dialog->priv->server_list), 1, label);

    bbox = create_vbuttonbox (dialog);
    gtk_box_pack_start (GTK_BOX (cat_box), bbox, FALSE, FALSE, 0);
    button = create_button (dialog, _("_New..."), GTK_SIGNAL_FUNC (on_new_btn_clicked));
    gtk_container_add (GTK_CONTAINER (bbox), button);
    button = create_named_button (dialog, _("_Edit..."), "edit_button", GTK_SIGNAL_FUNC (on_edit_btn_clicked));
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);
    button = create_named_button (dialog, _("_Remove"), "remove_button", GTK_SIGNAL_FUNC (on_remove_btn_clicked));
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);


    table = create_table (dialog, 1, 2);
    cat = create_category (dialog, table, _("Options"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), cat);

    label = create_label (dialog, _("Anonymous password:"));
    table_add (table, label, 0, 0, 0);

    ftp_dialog->priv->anonymous_pw_entry = create_entry (dialog, "anonymous_pw_entry", gnome_cmd_data_get_ftp_anonymous_password ());
    table_add (table, ftp_dialog->priv->anonymous_pw_entry, 1, 0, GTK_FILL);


    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_CANCEL,
                                 GTK_SIGNAL_FUNC (on_cancel_btn_clicked), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("Connect"),
                                 GTK_SIGNAL_FUNC (on_connect_btn_clicked), dialog);

    gtk_signal_connect (GTK_OBJECT (ftp_dialog->priv->server_list), "select_row",
                        GTK_SIGNAL_FUNC (on_server_list_select_row), dialog);
    gtk_signal_connect (GTK_OBJECT (ftp_dialog->priv->server_list), "unselect_row",
                        GTK_SIGNAL_FUNC (on_server_list_unselect_row), dialog);
    gtk_signal_connect (GTK_OBJECT (ftp_dialog->priv->server_list), "row_move",
                        GTK_SIGNAL_FUNC (on_server_list_row_move), dialog);
    gtk_signal_connect_after (GTK_OBJECT (ftp_dialog->priv->server_list),
                              "scroll-vertical",
                              GTK_SIGNAL_FUNC (on_server_list_scroll_vertical), dialog);

    gtk_widget_grab_focus (ftp_dialog->priv->server_list);
}


/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_ftp_dialog_get_type         (void)
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdFtpDialog",
            sizeof (GnomeCmdFtpDialog),
            sizeof (GnomeCmdFtpDialogClass),
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


GtkWidget*
gnome_cmd_ftp_dialog_new (void)
{
    GnomeCmdFtpDialog *dialog = gtk_type_new (gnome_cmd_ftp_dialog_get_type ());

    dialog->priv->selected_server = NULL;
    load_ftp_connections (dialog);

    return GTK_WIDGET (dialog);
}


/***********************************************
 *
 * The quick connect dialog
 *
 ***********************************************/

static gboolean
on_quick_connect_ok (GnomeCmdStringDialog *string_dialog,
                     const gchar **values,
                     gpointer not_used)
{
    GnomeCmdConFtp *server = NULL;
    gchar *error_desc = update_server_from_strings (&server, values, FALSE);

    if (error_desc != NULL)
        gnome_cmd_string_dialog_set_error_desc (string_dialog, error_desc);
    else {
        gchar *tmp_alias = g_strdup_printf ("[Q]%s", gnome_cmd_con_ftp_get_host_name (server));

        gnome_cmd_con_ftp_set_alias (server, tmp_alias);
        gnome_cmd_con_list_add_quick_ftp (gnome_cmd_data_get_con_list (), server);
        gnome_cmd_data_set_quick_connect_host (gnome_cmd_con_ftp_get_host_name (server));
        gnome_cmd_data_set_quick_connect_port (gnome_cmd_con_ftp_get_host_port (server));
        gnome_cmd_data_set_quick_connect_user (gnome_cmd_con_ftp_get_user_name (server));
        do_connect (GTK_WIDGET (string_dialog), server, gnome_cmd_con_ftp_get_pw (server));
        g_free (tmp_alias);
    }

    return error_desc == NULL;
}


void
show_ftp_quick_connect_dialog (void)
{
    gchar *port;
    GtkWidget *dialog;

    dialog = create_ftp_server_dialog (_("FTP Quick Connect"),
                                       (GnomeCmdStringDialogCallback)on_quick_connect_ok,
                                       NULL,
                                       FALSE);

    port = g_strdup_printf ("%d", gnome_cmd_data_get_quick_connect_port ());

    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, gnome_cmd_data_get_quick_connect_host ());
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 1, port);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 2, gnome_cmd_data_get_quick_connect_user ());
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 3, gnome_cmd_data_get_ftp_anonymous_password ());

    g_free (port);
}
