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

/* Baobab - (C) 2005 Fabio Marzocca

    baobab-remote-connect-dialog.c

   Modified module from nautilus-connect-server-dialog.c
   Released under same licence
 */
/*
 * Nautilus
 *
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-con-dialog.h"
#include "utils.h"

using namespace std;


#define GNOME_CMD_TYPE_CONNECT_DIALOG         (gnome_cmd_connect_dialog_get_type())
#define GNOME_CMD_CONNECT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_CONNECT_DIALOG, GnomeCmdConnectDialog))
#define GNOME_CMD_CONNECT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_CMD_TYPE_CONNECT_DIALOG, GnomeCmdConnectDialogClass))
#define GNOME_CMD_IS_CONNECT_DIALOG(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), GNOME_CMD_TYPE_CONNECT_DIALOG)

typedef struct _GnomeCmdConnectDialog        GnomeCmdConnectDialog;
typedef struct _GnomeCmdConnectDialogClass   GnomeCmdConnectDialogClass;
typedef struct _GnomeCmdConnectDialogPrivate GnomeCmdConnectDialogPrivate;

struct _GnomeCmdConnectDialog
{
    GtkDialog parent;
    GnomeCmdConnectDialogPrivate *priv;
};

struct _GnomeCmdConnectDialogClass
{
    GtkDialogClass parent_class;
};


struct _GnomeCmdConnectDialogPrivate
{
    string *alias;
    string uri_str;

    gboolean use_auth;

    GtkWidget *required_table;
    GtkWidget *optional_table;

    GtkWidget *type_combo;
    GtkWidget *auth_check;

    GtkWidget *alias_entry;
    GtkWidget *uri_entry;
    GtkWidget *server_entry;
    GtkWidget *share_entry;
    GtkWidget *port_entry;
    GtkWidget *folder_entry;
    GtkWidget *domain_entry;
    GtkWidget *user_entry;
    GtkWidget *password_entry;

    _GnomeCmdConnectDialogPrivate();
    ~_GnomeCmdConnectDialogPrivate();
};


inline _GnomeCmdConnectDialogPrivate::_GnomeCmdConnectDialogPrivate()
{
    alias = NULL;

    use_auth = FALSE;

    required_table = NULL;
    optional_table = NULL;

    type_combo = NULL;
    auth_check = NULL;

    alias_entry = gtk_entry_new ();
    uri_entry = gtk_entry_new();
    server_entry = gtk_entry_new ();
    share_entry = gtk_entry_new ();
    port_entry = gtk_entry_new ();
    folder_entry = gtk_entry_new ();
    domain_entry = gtk_entry_new ();
    user_entry = gtk_entry_new ();
    password_entry = gtk_entry_new ();

    gtk_entry_set_activates_default (GTK_ENTRY (alias_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (uri_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (server_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (share_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (port_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (folder_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (user_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (password_entry), TRUE);

    gtk_entry_set_visibility (GTK_ENTRY (password_entry), FALSE);

    // We need an extra ref so we can remove them from the table
    g_object_ref (alias_entry);
    g_object_ref (uri_entry);
    g_object_ref (server_entry);
    g_object_ref (share_entry);
    g_object_ref (port_entry);
    g_object_ref (folder_entry);
    g_object_ref (domain_entry);
    g_object_ref (user_entry);
    g_object_ref (password_entry);
}


inline _GnomeCmdConnectDialogPrivate::~_GnomeCmdConnectDialogPrivate()
{
    g_object_unref (alias_entry);
    g_object_unref (uri_entry);
    g_object_unref (server_entry);
    g_object_unref (share_entry);
    g_object_unref (port_entry);
    g_object_unref (folder_entry);
    g_object_unref (domain_entry);
    g_object_unref (user_entry);
    g_object_unref (password_entry);

    delete alias;
}


G_DEFINE_TYPE (GnomeCmdConnectDialog, gnome_cmd_connect_dialog, GTK_TYPE_DIALOG)


static void gnome_cmd_connect_dialog_finalize (GObject *object)
{
    GnomeCmdConnectDialog *dialog = GNOME_CMD_CONNECT_DIALOG (object);

    delete dialog->priv;

    G_OBJECT_CLASS (gnome_cmd_connect_dialog_parent_class)->finalize (object);
}


inline gboolean verify_uri (GnomeCmdConnectDialog *dialog)
{
    string uri;
    string server;
    string share;
    string port;
    string folder;
    string domain;
    string user;
    string password;

    if (dialog->priv->uri_entry->parent)
        stringify (uri, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->uri_entry), 0, -1));

    if (dialog->priv->server_entry->parent)
        stringify (server, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->server_entry), 0, -1));

    if (dialog->priv->share_entry->parent)
        stringify (share, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->share_entry), 0, -1));

    if (dialog->priv->port_entry->parent)
        stringify (port, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->port_entry), 0, -1));

    if (dialog->priv->folder_entry->parent)
        stringify (folder, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->folder_entry), 0, -1));

    if (dialog->priv->domain_entry->parent)
        stringify (domain, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->domain_entry), 0, -1));

    if (dialog->priv->user_entry->parent)
        stringify (user, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->user_entry), 0, -1));

    if (dialog->priv->password_entry->parent)
        stringify (password, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->password_entry), 0, -1));

    int type = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->type_combo));

    if (type!=CON_URI && server.empty())
    {
        gnome_cmd_show_message (GTK_WINDOW (dialog), _("You must enter a name for the server"), _("Please enter a name and try again."));

        return FALSE;
    }

    dialog->priv->use_auth = type!=CON_ANON_FTP && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->auth_check));

    if (type==CON_ANON_FTP)
        user = "anonymous";

    gnome_cmd_con_make_uri (uri, (ConnectionMethodID) type, dialog->priv->use_auth, uri, server, share, port, folder, domain, user, password);

    if (type==CON_URI && uri.empty())
    {
            gnome_cmd_show_message (GTK_WINDOW (dialog),
                                    stringify(g_strdup_printf (_("\"%s\" is not a valid location"), uri.c_str())),
                                    _("Please check the spelling and try again."));
            return FALSE;
    }

    if (dialog->priv->alias)
        stringify (*dialog->priv->alias, gtk_editable_get_chars (GTK_EDITABLE (dialog->priv->alias_entry), 0, -1));

    dialog->priv->uri_str = uri;

    return TRUE;
}


static void response_callback (GnomeCmdConnectDialog *dialog, int response_id, gpointer data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            if (!verify_uri (dialog))
                g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-file-properties");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        default :
            g_assert_not_reached ();
    }
}


static void gnome_cmd_connect_dialog_class_init (GnomeCmdConnectDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_connect_dialog_finalize;
}


inline void show_entry (GtkWidget *table, GtkWidget *entry, const gchar *text, gint &i)
{
    GtkWidget *label = gtk_label_new_with_mnemonic (text);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, i, i+1, GTK_FILL, GTK_FILL, 0, 0);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_widget_show (entry);
    gtk_table_attach (GTK_TABLE (table), entry, 1, 2, i, i+1, GtkAttachOptions (GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);

    i++;
}


static void setup_for_type (GnomeCmdConnectDialog *dialog)
{
    gint type = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->type_combo));

    if (type==CON_ANON_FTP)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->auth_check), FALSE);

    gtk_widget_set_sensitive (dialog->priv->auth_check, type!=CON_ANON_FTP);

    if (dialog->priv->alias_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->required_table), dialog->priv->alias_entry);

    if (dialog->priv->uri_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->required_table), dialog->priv->uri_entry);

    if (dialog->priv->server_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->required_table), dialog->priv->server_entry);

    if (dialog->priv->share_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->optional_table), dialog->priv->share_entry);

    if (dialog->priv->port_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->optional_table), dialog->priv->port_entry);

    if (dialog->priv->folder_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->optional_table), dialog->priv->folder_entry);

    if (dialog->priv->user_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->optional_table), dialog->priv->user_entry);

    if (dialog->priv->password_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->optional_table), dialog->priv->password_entry);

    if (dialog->priv->domain_entry->parent)
        gtk_container_remove (GTK_CONTAINER (dialog->priv->optional_table), dialog->priv->domain_entry);

    // Destroy all labels
    gtk_container_foreach (GTK_CONTAINER (dialog->priv->required_table), (GtkCallback) gtk_widget_destroy, NULL);

    gint i = 1;
    GtkWidget *table = dialog->priv->required_table;

    gboolean show_share, show_port, show_user, show_domain;

    show_entry (table, dialog->priv->alias_entry, _("_Alias:"), i);

    if (type == CON_URI)
    {
        show_entry (table, dialog->priv->uri_entry, _("_Location (URI):"), i);

        return;
    }

    switch (type)
    {
        default:
        case CON_SSH:
        case CON_FTP:
        case CON_DAV:
        case CON_DAVS:
            show_share = FALSE;
            show_port = TRUE;
            show_user = TRUE;
            show_domain = FALSE;
            break;

        case CON_ANON_FTP:
            show_share = FALSE;
            show_port = TRUE;
            show_user = FALSE;
            show_domain = FALSE;
            break;

        case CON_SMB:
            show_share = TRUE;
            show_port = FALSE;
            show_user = TRUE;
            show_domain = TRUE;
            break;
    }

    show_entry (table, dialog->priv->server_entry, _("_Server:"), i);

    GtkWidget *align;

    align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 12, 0, 0, 0);
    gtk_table_attach (GTK_TABLE (table), align, 0, 2, i, i+1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show (align);

    i++;

    gchar *str = g_strdup_printf ("<b>%s</b>", _("Optional information"));
    GtkWidget *label = gtk_label_new (str);
    g_free (str);

    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (align), label);

    align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 0);
    gtk_table_attach (GTK_TABLE (table), align, 0, 2, i, i+1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show (align);


    dialog->priv->optional_table = table = gtk_table_new (1, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
    gtk_widget_show (table);
    gtk_container_add (GTK_CONTAINER (align), table);

    i = 0;

    if (show_share)
        show_entry (table, dialog->priv->share_entry, _("S_hare:"), i);

    if (show_port)
        show_entry (table, dialog->priv->port_entry, _("_Port:"), i);

    /* Translators: 'Dir' in the sense of 'Directory' */
    show_entry (table, dialog->priv->folder_entry, _("_Remote dir:"), i);

    if (show_user)
        show_entry (table, dialog->priv->user_entry, _("_User name:"), i);

    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->auth_check)) && type!=CON_ANON_FTP)
        show_entry (table, dialog->priv->password_entry, _("_Password:"), i);

    if (show_domain)
        show_entry (table, dialog->priv->domain_entry, _("_Domain name:"), i);
}


static void dlg_changed_callback (GtkComboBox *combo_box, GnomeCmdConnectDialog *dialog)
{
    setup_for_type (dialog);
}


static void port_insert_text (GtkEditable *editable, const gchar *new_text, gint new_text_length, gint *position)
{
    if (new_text_length < 0)
        new_text_length = strlen (new_text);

    if (new_text_length != 1 || !g_ascii_isdigit (new_text[0]))
    {
        gdk_display_beep (gtk_widget_get_display (GTK_WIDGET (editable)));
        g_signal_stop_emission_by_name (editable, "insert-text");
    }
}


static void gnome_cmd_connect_dialog_init (GnomeCmdConnectDialog *dialog)
{
    GtkWidget *align;
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *combo;
    GtkWidget *check;
    GtkWidget *hbox;
    GtkWidget *vbox;

    dialog->priv = new GnomeCmdConnectDialogPrivate;

    g_return_if_fail (dialog->priv != NULL);

    gtk_window_set_title (GTK_WINDOW (dialog), _("Remote Server"));
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, FALSE, TRUE, 0);
    gtk_widget_show (vbox);

    hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show (hbox);

    gchar *str = g_strdup_printf ("<b>%s</b>", _("Service _type:"));
    label = gtk_label_new_with_mnemonic (str);
    g_free (str);

    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    dialog->priv->type_combo = combo = gtk_combo_box_new_text ();

    // Keep this in sync with enum ConnectionMethodID in gnome-cmd-con.h
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("SSH"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("FTP (with login)"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Public FTP"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Windows share"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("WebDAV (HTTP)"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Secure WebDAV (HTTPS)"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Custom location"));
    gtk_widget_show (combo);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
    gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
    g_signal_connect (combo, "changed", G_CALLBACK (dlg_changed_callback), dialog);

    align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 0);
    gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
    gtk_widget_show (align);

    dialog->priv->auth_check = check = gtk_check_button_new_with_mnemonic (_("Use _GNOME Keyring Manager for authentication"));
    gtk_widget_show (check);
    gtk_container_add (GTK_CONTAINER (align), check);
    g_signal_connect (check, "toggled", G_CALLBACK (dlg_changed_callback), dialog);

    hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show (hbox);

    align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 0);
    gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
    gtk_widget_show (align);


    dialog->priv->required_table = table = gtk_table_new (1, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
    gtk_widget_show (table);
    gtk_container_add (GTK_CONTAINER (align), table);

    g_signal_connect (dialog->priv->port_entry, "insert-text", G_CALLBACK (port_insert_text), NULL);

    setup_for_type (dialog);

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), dialog);
}


GnomeCmdConFtp *gnome_cmd_connect_dialog_new (gboolean has_alias)
{
    GtkWidget *dialog = (GtkWidget *) gtk_type_new (gnome_cmd_connect_dialog_get_type ());

    g_return_val_if_fail (dialog != NULL, NULL);

    GnomeCmdConnectDialog *conndlg = GNOME_CMD_CONNECT_DIALOG (dialog);

    if (has_alias)
        conndlg->priv->alias = new string;
    else
        gtk_widget_set_sensitive (conndlg->priv->alias_entry, FALSE);

    gtk_combo_box_set_active (GTK_COMBO_BOX (conndlg->priv->type_combo), CON_SSH);

    conndlg->priv->use_auth = gnome_cmd_data.use_gnome_auth_manager;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (conndlg->priv->auth_check), conndlg->priv->use_auth);

    gint response = gtk_dialog_run (GTK_DIALOG (dialog));

    GnomeCmdConFtp *server = NULL;

    if (response==GTK_RESPONSE_OK)
    {
        const gchar *alias = conndlg->priv->alias && !conndlg->priv->alias->empty() ? conndlg->priv->alias->c_str() : NULL;

        server = gnome_cmd_con_ftp_new (alias, conndlg->priv->uri_str);

        GnomeCmdCon *con = GNOME_CMD_CON (server);

        con->method = (ConnectionMethodID) gtk_combo_box_get_active (GTK_COMBO_BOX (conndlg->priv->type_combo));
        con->gnome_auth = conndlg->priv->use_auth;
    }

    gtk_widget_destroy (dialog);

    return server;
}


gboolean gnome_cmd_connect_dialog_edit (GnomeCmdConFtp *server)
{
    g_return_val_if_fail (server != NULL, FALSE);

    GtkWidget *dialog = gtk_widget_new (GNOME_CMD_TYPE_CONNECT_DIALOG, NULL);

    g_return_val_if_fail (dialog != NULL, FALSE);

    GnomeCmdConnectDialog *conndlg = GNOME_CMD_CONNECT_DIALOG (dialog);

    GnomeCmdCon *con = GNOME_CMD_CON (server);

    // Service type
    gtk_combo_box_set_active (GTK_COMBO_BOX (conndlg->priv->type_combo), con->method);

    // Use GNOME Keyring Manager for authentication
    conndlg->priv->use_auth = con->gnome_auth;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (conndlg->priv->auth_check), con->gnome_auth);

    // Alias
    if (con->alias)
    {
        conndlg->priv->alias = new string(con->alias);
        gtk_entry_set_text (GTK_ENTRY (conndlg->priv->alias_entry), con->alias);
    }
    else
        gtk_widget_set_sensitive (conndlg->priv->alias_entry, FALSE);

    if (con->uri)
    {
        conndlg->priv->uri_str = con->uri;

        GnomeVFSURI *uri = gnome_vfs_uri_new (con->uri);

        if (uri)
        {
            gtk_entry_set_text (GTK_ENTRY (conndlg->priv->uri_entry), con->uri);

            gtk_entry_set_text (GTK_ENTRY (conndlg->priv->server_entry), gnome_vfs_uri_get_host_name (uri));
            // gtk_entry_set_text (GTK_ENTRY (conndlg->priv->share_entry), ???);
            gtk_entry_set_text (GTK_ENTRY (conndlg->priv->folder_entry), gnome_vfs_uri_get_path (uri));
            // gtk_entry_set_text (GTK_ENTRY (conndlg->priv->domain_entry), ???);
            gtk_entry_set_text (GTK_ENTRY (conndlg->priv->user_entry), gnome_vfs_uri_get_user_name (uri));
            gtk_entry_set_text (GTK_ENTRY (conndlg->priv->password_entry), gnome_vfs_uri_get_password (uri));

            guint port = gnome_vfs_uri_get_host_port (uri);

            if (port)
                gtk_entry_set_text (GTK_ENTRY (conndlg->priv->port_entry), stringify(port).c_str());

            gnome_vfs_uri_unref (uri);
        }
    }

    gint response = gtk_dialog_run (GTK_DIALOG (dialog));

    if (response==GTK_RESPONSE_OK)
    {
        GnomeVFSURI *uri = gnome_vfs_uri_new (conndlg->priv->uri_str.c_str());

        const gchar *alias = conndlg->priv->alias ? conndlg->priv->alias->c_str() : NULL;
        const gchar *host = gnome_vfs_uri_get_host_name (uri);      // do not g_free !!

        gnome_cmd_con_set_alias (con, alias);
        gnome_cmd_con_set_uri (con, conndlg->priv->uri_str);
        gnome_cmd_con_set_host_name (con, host);
        con->method = (ConnectionMethodID) gtk_combo_box_get_active (GTK_COMBO_BOX (conndlg->priv->type_combo));
        con->gnome_auth = conndlg->priv->use_auth;

        gnome_cmd_con_ftp_set_host_name (server, host);

        gnome_vfs_uri_unref (uri);
    }

    gtk_widget_destroy (dialog);

    return response==GTK_RESPONSE_OK;
}
