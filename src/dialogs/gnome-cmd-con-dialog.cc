/**
 * @file gnome-cmd-con-dialog.cc
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "utils.h"
#include "dialogs/gnome-cmd-con-dialog.h"
#include "gnome-cmd-plain-path.h"

using namespace std;


#define GNOME_CMD_TYPE_CONNECT_DIALOG         (gnome_cmd_connect_dialog_get_type())
#define GNOME_CMD_CONNECT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_CONNECT_DIALOG, GnomeCmdConnectDialog))
#define GNOME_CMD_CONNECT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_CMD_TYPE_CONNECT_DIALOG, GnomeCmdConnectDialogClass))
#define GNOME_CMD_IS_CONNECT_DIALOG(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_CMD_TYPE_CONNECT_DIALOG)


GType gnome_cmd_connect_dialog_get_type ();


struct GnomeCmdConnectDialog
{
    GtkDialog parent;

    struct Private;

    Private *priv;

    operator GtkWidget * () const         {  return GTK_WIDGET (this);    }
    operator GtkWindow * () const         {  return GTK_WINDOW (this);    }
    operator GtkDialog * () const         {  return GTK_DIALOG (this);    }

    gboolean verify_uri();
};


struct GnomeCmdConnectDialogClass
{
    GtkDialogClass parent_class;
};


struct GnomeCmdConnectDialog::Private
{
    string *alias {nullptr};
    string uri_str;

    GtkGrid *required_grid {nullptr};
    GtkGrid *optional_grid {nullptr};

    GtkWidget *type_combo {nullptr};

    GtkWidget *alias_entry;
    GtkWidget *uri_entry;
    GtkWidget *server_entry;
    GtkWidget *port_entry;
    GtkWidget *folder_entry;
    GtkWidget *domain_entry;

    Private();
    ~Private();

    void setup_for_type();
    void show_entry(GtkGrid *grid, GtkWidget *entry, const gchar *text, gint &i);
};


inline GnomeCmdConnectDialog::Private::Private()
{
    alias_entry = gtk_entry_new ();
    uri_entry = gtk_entry_new ();
    server_entry = gtk_entry_new ();
    port_entry = gtk_entry_new ();
    folder_entry = gtk_entry_new ();
    domain_entry = gtk_entry_new ();

    gtk_entry_set_activates_default (GTK_ENTRY (alias_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (uri_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (server_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (port_entry), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (folder_entry), TRUE);

    // We need an extra ref so we can remove them from the table
    g_object_ref (alias_entry);
    g_object_ref (uri_entry);
    g_object_ref (server_entry);
    g_object_ref (port_entry);
    g_object_ref (folder_entry);
    g_object_ref (domain_entry);
}


inline GnomeCmdConnectDialog::Private::~Private()
{
    g_object_unref (alias_entry);
    g_object_unref (uri_entry);
    g_object_unref (server_entry);
    g_object_unref (port_entry);
    g_object_unref (folder_entry);
    g_object_unref (domain_entry);

    delete alias;
}


static void remove_grid_child (GtkWidget *child, gpointer user_data)
{
    gtk_container_remove (GTK_CONTAINER (user_data), child);
}


void GnomeCmdConnectDialog::Private::setup_for_type()
{
    gint type = gtk_combo_box_get_active (GTK_COMBO_BOX (type_combo));

    if (gtk_widget_get_parent (alias_entry))
        gtk_container_remove (GTK_CONTAINER (required_grid), alias_entry);

    if (gtk_widget_get_parent (uri_entry))
        gtk_container_remove (GTK_CONTAINER (required_grid), uri_entry);

    if (gtk_widget_get_parent (server_entry))
        gtk_container_remove (GTK_CONTAINER (required_grid), server_entry);

    if (gtk_widget_get_parent (port_entry))
        gtk_container_remove (GTK_CONTAINER (optional_grid), port_entry);

    if (gtk_widget_get_parent (folder_entry))
        gtk_container_remove (GTK_CONTAINER (optional_grid), folder_entry);

    if (gtk_widget_get_parent (domain_entry))
        gtk_container_remove (GTK_CONTAINER (optional_grid), domain_entry);

    // Destroy all labels
    gtk_container_foreach (GTK_CONTAINER (required_grid), (GtkCallback) remove_grid_child, required_grid);

    gint i = 1;

    gboolean show_port, show_domain;

    show_entry (required_grid, alias_entry, _("_Alias:"), i);

    switch (type)
    {
        case CON_URI:
            show_entry (required_grid, uri_entry, _("_Location (URI):"), i);
            return;

        default:
        case CON_SSH:
        case CON_FTP:
        case CON_DAV:
        case CON_DAVS:
            show_port = TRUE;
            show_domain = FALSE;
            break;

       case CON_ANON_FTP:
            show_port = TRUE;
            show_domain = FALSE;
            break;

        case CON_SMB:
            show_port = FALSE;
            show_domain = TRUE;
            break;
    }

    show_entry (required_grid, server_entry, _("_Server:"), i);

    gchar *str = g_strdup_printf ("<b>%s</b>", _("Optional information"));
    GtkWidget *label = gtk_label_new (str);
    g_free (str);

    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_show (label);
    gtk_widget_set_margin_top (label, 12);
    gtk_grid_attach (required_grid, label, 0, i, 2, 1);
    i++;

    optional_grid = GTK_GRID (gtk_grid_new ());
    gtk_grid_set_row_spacing (optional_grid, 6);
    gtk_grid_set_column_spacing (optional_grid, 12);
    gtk_widget_show (GTK_WIDGET (optional_grid));
    gtk_widget_set_margin_start (GTK_WIDGET (optional_grid), 12);
    gtk_grid_attach (required_grid, GTK_WIDGET (optional_grid), 0, i, 2, 1);

    i = 0;

    if (show_port)
        show_entry (optional_grid, port_entry, _("_Port:"), i);

    show_entry (optional_grid, folder_entry, _("_Folder:"), i);

    if (show_domain)
        show_entry (optional_grid, domain_entry, _("_Domain name:"), i);
}


inline void GnomeCmdConnectDialog::Private::show_entry(GtkGrid *grid, GtkWidget *entry, const gchar *text, gint &i)
{
    GtkWidget *label = gtk_label_new_with_mnemonic (text);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_show (label);
    gtk_grid_attach (GTK_GRID (grid), label, 0, i, 1, 1);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_widget_show (entry);
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, i, 1, 1);

    ++i;
}

/**
 * This method sets priv->uri_str to the URI created from values in the dialog if the URI is valid.
 */
gboolean GnomeCmdConnectDialog::verify_uri()
{
    string uri;
    string server;
    string port;
    string folder;
    string domain;

    if (gtk_widget_get_parent (priv->uri_entry))
        stringify (uri, gtk_editable_get_chars (GTK_EDITABLE (priv->uri_entry), 0, -1));

    if (gtk_widget_get_parent (priv->server_entry))
        stringify (server, gtk_editable_get_chars (GTK_EDITABLE (priv->server_entry), 0, -1));

    if (gtk_widget_get_parent (priv->port_entry))
        stringify (port, gtk_editable_get_chars (GTK_EDITABLE (priv->port_entry), 0, -1));

    if (gtk_widget_get_parent (priv->folder_entry))
        stringify (folder, gtk_editable_get_chars (GTK_EDITABLE (priv->folder_entry), 0, -1));

    if (gtk_widget_get_parent (priv->domain_entry))
        stringify (domain, gtk_editable_get_chars (GTK_EDITABLE (priv->domain_entry), 0, -1));

    int type = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->type_combo));

    if (type!=CON_URI && server.empty())
    {
        gnome_cmd_show_message (*this, _("You must enter a name for the server"), _("Please enter a name and try again."));

        return FALSE;
    }

    gnome_cmd_con_make_uri (uri, (ConnectionMethodID) type, uri, server, port, folder, domain);

    if (type==CON_URI && !uri_is_valid(uri.c_str()))
    {
        gnome_cmd_show_message (*this,
                                stringify(g_strdup_printf (_("“%s” is not a valid location"), uri.c_str())),
                                _("Please check spelling and try again."));
        return FALSE;
    }

    if (priv->alias)
        stringify (*priv->alias, gtk_editable_get_chars (GTK_EDITABLE (priv->alias_entry), 0, -1));

    priv->uri_str = uri;

    return TRUE;
}


G_DEFINE_TYPE (GnomeCmdConnectDialog, gnome_cmd_connect_dialog, GTK_TYPE_DIALOG)


static void gnome_cmd_connect_dialog_finalize (GObject *object)
{
    GnomeCmdConnectDialog *dialog = GNOME_CMD_CONNECT_DIALOG (object);

    delete dialog->priv;

    G_OBJECT_CLASS (gnome_cmd_connect_dialog_parent_class)->finalize (object);
}


static void response_callback (GnomeCmdConnectDialog *dialog, int response_id, gpointer data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            if (!dialog->verify_uri())
                g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-config-remote-connections");
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


static void dlg_changed_callback (GtkComboBox *combo_box, GnomeCmdConnectDialog *dialog)
{
    dialog->priv->setup_for_type();
}


static void port_insert_text (GtkEditable *editable, const gchar *new_text, gint new_text_length, gint *position)
{
    if (new_text_length < 0)
        new_text_length = strlen (new_text);

    if (new_text_length!=1 || g_ascii_isdigit (new_text[0]))
        return;

    gdk_display_beep (gtk_widget_get_display (GTK_WIDGET (editable)));
    g_signal_stop_emission_by_name (editable, "insert-text");
}


static void gnome_cmd_connect_dialog_init (GnomeCmdConnectDialog *dialog)
{
    GtkWidget *label;
    GtkWidget *combo;
    GtkWidget *hbox;
    GtkWidget *vbox;

    dialog->priv = new GnomeCmdConnectDialog::Private;

    g_return_if_fail (dialog->priv != NULL);

    gtk_window_set_title (*dialog, _("Remote Server"));
    gtk_window_set_resizable (*dialog, FALSE);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_widget_set_margin_top (content_area, 10);
    gtk_widget_set_margin_bottom (content_area, 10);
    gtk_widget_set_margin_start (content_area, 10);
    gtk_widget_set_margin_end (content_area, 10);
    gtk_box_set_spacing (GTK_BOX (content_area), 6);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append (GTK_BOX (content_area), vbox);
    gtk_widget_show (vbox);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append (GTK_BOX (vbox), hbox);
    gtk_widget_show (hbox);

    gchar *str = g_strdup_printf ("<b>%s</b>", _("Service _type:"));
    label = gtk_label_new_with_mnemonic (str);
    g_free (str);

    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_show (label);
    gtk_box_append (GTK_BOX (hbox), label);

    dialog->priv->type_combo = combo = gtk_combo_box_text_new ();

    // Keep this in sync with enum ConnectionMethodID in gnome-cmd-con.h
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("SSH"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("FTP (with login)"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Public FTP"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Windows share"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("WebDAV (HTTP)"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Secure WebDAV (HTTPS)"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Custom location"));
    gtk_widget_show (combo);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
    gtk_box_append (GTK_BOX (hbox), combo);
    g_signal_connect (combo, "changed", G_CALLBACK (dlg_changed_callback), dialog);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append (GTK_BOX (vbox), hbox);
    gtk_widget_show (hbox);

    dialog->priv->required_grid = GTK_GRID (gtk_grid_new ());
    gtk_grid_set_row_spacing (dialog->priv->required_grid, 6);
    gtk_grid_set_column_spacing (dialog->priv->required_grid, 12);
    gtk_widget_show (GTK_WIDGET (dialog->priv->required_grid));
    gtk_widget_set_margin_start (GTK_WIDGET (dialog->priv->required_grid), 12);
    gtk_box_append (GTK_BOX (hbox), GTK_WIDGET (dialog->priv->required_grid));

    g_signal_connect (dialog->priv->port_entry, "insert-text", G_CALLBACK (port_insert_text), NULL);

    dialog->priv->setup_for_type();

    gtk_dialog_add_buttons (*dialog,
                            _("_Help"), GTK_RESPONSE_HELP,
                            _("_Cancel"), GTK_RESPONSE_CANCEL,
                            _("_OK"), GTK_RESPONSE_OK,
                            NULL);

    gtk_dialog_set_default_response (*dialog, GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), dialog);
}


/**
 * Dialog for setting up a new remote server connection.
 */
GnomeCmdConRemote *gnome_cmd_connect_dialog_new (gboolean has_alias)
{
    auto *dialog = static_cast<GnomeCmdConnectDialog*> (g_object_new (GNOME_CMD_TYPE_CONNECT_DIALOG, nullptr));

    g_return_val_if_fail (dialog != nullptr, nullptr);

    if (has_alias)
        dialog->priv->alias = new string;
    else
        gtk_widget_set_sensitive (dialog->priv->alias_entry, FALSE);

    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->type_combo), CON_SSH);

    gint response = gtk_dialog_run (*dialog);

    GnomeCmdConRemote *server = nullptr;

    if (response==GTK_RESPONSE_OK)
    {
        const gchar *alias = dialog->priv->alias && !dialog->priv->alias->empty() ? dialog->priv->alias->c_str() : nullptr;

        server = gnome_cmd_con_remote_new (alias, dialog->priv->uri_str.c_str());

        GnomeCmdCon *con = GNOME_CMD_CON (server);

        con->method = (ConnectionMethodID) gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->type_combo));
    }

    gtk_window_destroy (GTK_WINDOW (dialog));

    return server;
}


gboolean gnome_cmd_connect_dialog_edit (GnomeCmdConRemote *server)
{
    g_return_val_if_fail (server != nullptr, FALSE);

    auto *dialog = reinterpret_cast<GnomeCmdConnectDialog*> (g_object_new (GNOME_CMD_TYPE_CONNECT_DIALOG, nullptr));
    g_return_val_if_fail (dialog != nullptr, FALSE);

    auto *con = GNOME_CMD_CON (server);

    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->type_combo), con->method);

    if (con->alias)
    {
        dialog->priv->alias = new string(con->alias);
        gtk_editable_set_text (GTK_EDITABLE (dialog->priv->alias_entry), con->alias);
    }
    else
        gtk_widget_set_sensitive (dialog->priv->alias_entry, FALSE);

    GUri *uri = gnome_cmd_con_get_uri (con);
    if (uri)
    {
        gchar *uri_str = g_uri_to_string (uri);
        dialog->priv->uri_str = uri_str;
        g_free (uri_str);

        gtk_editable_set_text (GTK_EDITABLE (dialog->priv->uri_entry), dialog->priv->uri_str.c_str());
        gtk_editable_set_text (GTK_EDITABLE (dialog->priv->server_entry), g_uri_get_host (uri));
        gtk_editable_set_text (GTK_EDITABLE (dialog->priv->folder_entry), g_uri_get_path (uri));

        auto port = g_uri_get_port (uri);
        if (port != -1)
            gtk_editable_set_text (GTK_EDITABLE (dialog->priv->port_entry), stringify(port).c_str());
    }

    gint response = gtk_dialog_run (*dialog);

    if (response == GTK_RESPONSE_OK)
    {
        GError *error = nullptr;

        if (dialog->priv->uri_str.c_str())
        {
            auto uri = g_uri_parse(dialog->priv->uri_str.c_str(), G_URI_FLAGS_NONE, &error);
            if (error)
            {
                g_warning("gnome_cmd_connect_dialog_edit - g_uri_parse error: %s", error->message);
                g_error_free(error);
                return FALSE;
            }
            auto uriPath = g_uri_get_path(uri);
            auto uriString = g_uri_to_string(uri);
            gnome_cmd_con_set_uri_string (con, uriString);
            g_free(uriString);

            gnome_cmd_con_set_base_path(con, uriPath && strlen(uriPath) > 0
                ? new GnomeCmdPlainPath(uriPath)
                : new GnomeCmdPlainPath(G_DIR_SEPARATOR_S));
        }
        else
        {
            const gchar *path = gtk_editable_get_text (GTK_EDITABLE (dialog->priv->folder_entry));

            gnome_cmd_con_set_base_path(con, path && strlen(path) > 0
                ? new GnomeCmdPlainPath(path)
                : new GnomeCmdPlainPath(G_DIR_SEPARATOR_S));
        }

        auto alias = dialog->priv->alias ? dialog->priv->alias->c_str() : nullptr;
        gnome_cmd_con_set_alias (con, alias);
        con->method = (ConnectionMethodID) gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->type_combo));
    }

    gtk_window_destroy (GTK_WINDOW (dialog));

    return response==GTK_RESPONSE_OK;
}
