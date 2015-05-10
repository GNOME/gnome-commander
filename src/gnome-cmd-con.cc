/** 
 * @file gnome-cmd-con.cc
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
#include <libgnomeui/libgnomeui.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con.h"

using namespace std;


struct GnomeCmdConPrivate
{
    GnomeCmdDir    *default_dir;   // the start dir of this connection
    History        *dir_history;
    GnomeCmdBookmarkGroup *bookmarks;
    GList          *all_dirs;
    GHashTable     *all_dirs_map;
};

enum
{
    UPDATED,
    CLOSE,
    OPEN_DONE,
    OPEN_FAILED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *parent_class = NULL;


// Keep this in sync with enum ConnectionMethodID in gnome-cmd-con.h
const gchar *icon_name[] = {"gnome-fs-ssh",            // CON_SSH
                            "gnome-fs-ftp",            // CON_FTP
                            "gnome-fs-ftp",            // CON_ANON_FTP
#ifdef HAVE_SAMBA
                            "gnome-fs-smb",            // CON_SMB
#endif
                            "gnome-fs-web",            // CON_DAV
                            "gnome-fs-web",            // CON_DAVS
                            "gnome-fs-network",        // CON_URI
                            "gnome-fs-directory"};     // CON_LOCAL


static void on_open_done (GnomeCmdCon *con)
{
    gnome_cmd_con_updated (con);
}


static void on_open_failed (GnomeCmdCon *con, const gchar *msg, GnomeVFSResult result)
{
    // gnome_cmd_con_updated (con);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdCon *con = GNOME_CMD_CON (object);

    g_free (con->alias);
    g_free (con->uri);

    delete con->base_path;
    g_string_free (con->root_path, TRUE);
    g_free (con->open_text);
    g_free (con->open_tooltip);
    gnome_cmd_pixmap_free (con->open_pixmap);
    g_free (con->close_text);
    g_free (con->close_tooltip);
    gnome_cmd_pixmap_free (con->close_pixmap);

    if (con->priv->default_dir)
        gnome_cmd_dir_unref (con->priv->default_dir);

    delete con->priv->dir_history;

    g_free (con->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdConClass *klass)
{
    GtkObjectClass *object_class;

    object_class = GTK_OBJECT_CLASS (klass);
    parent_class = (GtkObjectClass *) gtk_type_class (gtk_object_get_type ());

    signals[UPDATED] =
        gtk_signal_new ("updated",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, updated),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    signals[CLOSE] =
        gtk_signal_new ("close",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, close),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    signals[OPEN_DONE] =
        gtk_signal_new ("open-done",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, open_done),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    signals[OPEN_FAILED] =
        gtk_signal_new ("open-failed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, open_failed),
            gtk_marshal_NONE__POINTER_INT,
            GTK_TYPE_NONE,
            2, GTK_TYPE_POINTER, GTK_TYPE_INT);

    object_class->destroy = destroy;

    klass->updated = NULL;
    klass->open_done = on_open_done;
    klass->open_failed = on_open_failed;

    klass->open = NULL;
    klass->close = NULL;
    klass->cancel_open = NULL;
    klass->open_is_needed = NULL;
    klass->create_uri = NULL;
    klass->create_path = NULL;
}


static void init (GnomeCmdCon *con)
{
    con->alias = NULL;
    con->uri = NULL;
    con->method = CON_URI;
    con->auth = GnomeCmdCon::NOT_REQUIRED;

    con->base_path = NULL;
    con->base_info = NULL;
    con->root_path = g_string_sized_new (128);
    con->open_msg = NULL;
    con->should_remember_dir = FALSE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = FALSE;
    con->is_local = FALSE;
    con->is_closeable = FALSE;
    con->go_text = NULL;
    con->go_tooltip = NULL;
    con->go_pixmap = NULL;
    con->open_text = NULL;
    con->open_tooltip = NULL;
    con->open_pixmap = NULL;
    con->close_text = NULL;
    con->close_tooltip = NULL;
    con->close_pixmap = NULL;

    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_result = GnomeCmdCon::OPEN_NOT_STARTED;
    con->open_failed_reason = GNOME_VFS_OK;
    con->open_failed_msg = NULL;

    con->priv = g_new0 (GnomeCmdConPrivate, 1);
    con->priv->default_dir = NULL;
    con->priv->dir_history = new History(20);
    con->priv->bookmarks = g_new0 (GnomeCmdBookmarkGroup, 1);
    con->priv->bookmarks->con = con;
    // con->priv->bookmarks->bookmarks = NULL;
    // con->priv->bookmarks->data = NULL;
    con->priv->all_dirs = NULL;
    con->priv->all_dirs_map = NULL;
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_con_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdCon",
            sizeof (GnomeCmdCon),
            sizeof (GnomeCmdConClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_object_get_type (), &info);
    }
    return type;
}


static gboolean check_con_open_progress (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    g_return_val_if_fail (con->open_result != GnomeCmdCon::OPEN_NOT_STARTED, FALSE);

    switch (con->open_result)
    {
        case GnomeCmdCon::OPEN_IN_PROGRESS:
            return TRUE;

        case GnomeCmdCon::OPEN_OK:
            {
                DEBUG('m', "GnomeCmdCon::OPEN_OK detected\n");

                GnomeCmdDir *dir = gnome_cmd_dir_new_with_con (con);

                gnome_cmd_con_set_default_dir (con, dir);

                DEBUG ('m', "Emitting 'open-done' signal\n");
                gtk_signal_emit (GTK_OBJECT (con), signals[OPEN_DONE]);
            }
            return FALSE;

        case GnomeCmdCon::OPEN_FAILED:
            {
                DEBUG ('m', "GnomeCmdCon::OPEN_FAILED detected\n");
                DEBUG ('m', "Emitting 'open-failed' signal\n");
                gtk_signal_emit (GTK_OBJECT (con), signals[OPEN_FAILED], con->open_failed_msg, con->open_failed_reason);
            }
            return FALSE;

        default:
            return FALSE;
    }
}


void gnome_cmd_con_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    DEBUG ('m', "Opening connection\n");

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    if (con->state != GnomeCmdCon::STATE_OPEN)
        klass->open (con);

    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) check_con_open_progress, con);
}


void gnome_cmd_con_cancel_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (con->state == GnomeCmdCon::STATE_OPENING)
    {
        GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
        klass->cancel_open (con);
    }
}


gboolean gnome_cmd_con_close (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    if (gnome_cmd_con_is_closeable (con))
    {
        gtk_signal_emit (GTK_OBJECT (con), signals[CLOSE]);
        gtk_signal_emit (GTK_OBJECT (con), signals[UPDATED]);
    }

    return TRUE;
}


GnomeVFSURI *gnome_cmd_con_create_uri (GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    return klass->create_uri (con, path);
}


GnomeCmdPath *gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    return klass->create_path (con, path_str);
}


void gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (dir == con->priv->default_dir)
        return;

    if (dir)
        gnome_cmd_dir_ref (dir);
    if (con->priv->default_dir)
        gnome_cmd_dir_unref (con->priv->default_dir);
    con->priv->default_dir = dir;
}


GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->priv->default_dir;
}


History *gnome_cmd_con_get_dir_history (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->priv->dir_history;
}


GnomeCmdBookmarkGroup *gnome_cmd_con_get_bookmarks (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->priv->bookmarks;
}


void gnome_cmd_con_set_bookmarks (GnomeCmdCon *con, GnomeCmdBookmarkGroup *bookmarks)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    con->priv->bookmarks = bookmarks;
}


void gnome_cmd_con_updated (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gtk_signal_emit (GTK_OBJECT (con), signals[UPDATED]);
}


// Get the type of the file at the specified path.
// If the operation succeeds GNOME_VFS_OK is returned and type is set
GnomeVFSResult gnome_cmd_con_get_path_target_type (GnomeCmdCon *con, const gchar *path_str, GnomeVFSFileType *type)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), GNOME_VFS_ERROR_BAD_PARAMETERS);
    g_return_val_if_fail (path_str != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);

    GnomeCmdPath *path = gnome_cmd_con_create_path (con, path_str);
    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, path);
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
    GnomeVFSResult res = gnome_vfs_get_file_info_uri (uri, info, GNOME_VFS_FILE_INFO_DEFAULT);

    if (res == GNOME_VFS_OK && info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK)         // resolve the symlink to get the real type of it
        res = gnome_vfs_get_file_info_uri (uri, info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

    if (res == GNOME_VFS_OK)
        *type = info->type;

    gnome_vfs_uri_unref (uri);
    delete path;
    gnome_vfs_file_info_unref (info);

    return res;
}


GnomeVFSResult gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str)
{
    GnomeVFSResult result;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), GNOME_VFS_ERROR_BAD_PARAMETERS);
    g_return_val_if_fail (path_str != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);

    GnomeCmdPath *path = gnome_cmd_con_create_path (con, path_str);
    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, path);

    result = gnome_vfs_make_directory_for_uri (
        uri,
        GNOME_VFS_PERM_USER_READ|GNOME_VFS_PERM_USER_WRITE|GNOME_VFS_PERM_USER_EXEC|
        GNOME_VFS_PERM_GROUP_READ|GNOME_VFS_PERM_GROUP_EXEC|
        GNOME_VFS_PERM_OTHER_READ|GNOME_VFS_PERM_OTHER_EXEC);

    gnome_vfs_uri_unref (uri);
    delete path;

    return result;
}


void gnome_cmd_con_add_to_cache (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

    if (!con->priv->all_dirs_map)
        con->priv->all_dirs_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    DEBUG ('k', "ADDING 0x%p %s to the cache\n", dir, uri_str);
    g_hash_table_insert (con->priv->all_dirs_map, uri_str, dir);
}


void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

    DEBUG ('k', "REMOVING 0x%p %s from the cache\n", dir, uri_str);
    g_hash_table_remove (con->priv->all_dirs_map, uri_str);
    g_free (uri_str);
}


void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, const gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (uri_str != NULL);

    DEBUG ('k', "REMOVING %s from the cache\n", uri_str);
    g_hash_table_remove (con->priv->all_dirs_map, uri_str);
}


GnomeCmdDir *gnome_cmd_con_cache_lookup (GnomeCmdCon *con, const gchar *uri_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    g_return_val_if_fail (uri_str != NULL, NULL);

    GnomeCmdDir *dir = NULL;

    if (con->priv->all_dirs_map)
        dir = (GnomeCmdDir *) g_hash_table_lookup (con->priv->all_dirs_map, uri_str);

    if (dir)
        DEBUG ('k', "FOUND 0x%p %s in the hash-table, reusing it!\n", dir, uri_str);
    else
        DEBUG ('k', "FAILED to find %s in the hash-table\n", uri_str);

    return dir;
}


const gchar *gnome_cmd_con_get_icon_name (ConnectionMethodID method)
{
    return icon_name[method];
}


string &__gnome_cmd_con_make_uri (string &s, const gchar *method, gboolean use_auth, string &server, string &port, string &folder, string &user, string &password)
{
    user = stringify (gnome_vfs_escape_string (user.c_str()));
    password = stringify (gnome_vfs_escape_string (password.c_str()));

    if (!password.empty() && !use_auth)
    {
        user += ':';
        user += password;
    }

    folder = stringify (gnome_vfs_escape_path_string (folder.c_str()));

    s = method;

    if (!user.empty())
        s += user + '@';

    s += server;

    if (!port.empty())
        s += ':' + port;

    if (!folder.empty())
        s += '/' + folder;

    return s;
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdCon &con)
{
    if (!con.alias || !*con.alias || !con.uri || !*con.uri)
        return xml;

    xml << XML::tag("Connection");

    xml << XML::attr("name") << XML::escape(con.alias);
    xml << XML::attr("uri") << XML::escape(con.uri);
    xml << XML::attr("auth") << con.auth;

    return xml << XML::endtag();
}


GnomeKeyringAttributeList *gnome_cmd_con_create_keyring_attributes (const gchar *uri_str, const gchar *alias, ConnectionMethodID method)
{
    GnomeVFSURI *uri = gnome_vfs_uri_new (uri_str);

    const gchar *user = gnome_vfs_uri_get_user_name (uri);
    const gchar *domain = NULL;
    const gchar *server = gnome_vfs_uri_get_host_name (uri);
    const gchar *path = gnome_vfs_uri_get_path (uri);
    const gchar *protocol = gnome_vfs_uri_get_scheme (uri);
    guint port = gnome_vfs_uri_get_host_port (uri);

    gchar **a = NULL;

#ifdef HAVE_SAMBA
    if (method==CON_SMB)
    {
        a = g_strsplit (user, ";", 2);

        if (g_strv_length (a) > 1)
        {
            domain = a[0];
            user = a[1];
        }
    }
#endif

    GnomeKeyringAttributeList *attributes = gnome_keyring_attribute_list_new ();

    gnome_keyring_attribute_list_append_string (attributes, "alias", alias);

    if (protocol)
        gnome_keyring_attribute_list_append_string (attributes, "protocol", protocol);

    if (user)
        gnome_keyring_attribute_list_append_string (attributes, "user", user);

    if (server)
        gnome_keyring_attribute_list_append_string (attributes, "server", server);

    if (port)
        gnome_keyring_attribute_list_append_uint32 (attributes, "port", port);

    if (domain)
        gnome_keyring_attribute_list_append_string (attributes, "domain", domain);

    if (path)
        gnome_keyring_attribute_list_append_string (attributes, "object", path);

    gnome_keyring_attribute_list_append_string (attributes, "authtype", "password");

    gnome_keyring_attribute_list_append_string (attributes, "application", "GNOME Commander");

    {
        gchar date[11];
        GDate *now = g_date_new ();

        g_date_set_time_t (now, time (NULL));
        g_date_strftime (date, sizeof(date), "%Y-%m-%d", now);

        gnome_keyring_attribute_list_append_string (attributes, "date-modified", date);

        g_date_free (now);
    }

    gnome_vfs_uri_unref (uri);
    g_strfreev (a);

    return attributes;
}

/**
 * This callback function takes a const char* pointer and points it to a
 * newly created char array of the password the user has typed in the
 * associated dialog entry. The memory where @c password points to has
 * to be freed elsewhere.
 */
void set_password_callback (GtkDialog *dialog, int response_id, const char **password)
{
    switch (response_id)
    {
        case GTK_RESPONSE_OK:
	{
	    const char *passwd = NULL; /* local pointer pointing to const-defined memory area */

	    const gchar *entry = gtk_entry_get_text (GTK_ENTRY (lookup_widget (GTK_WIDGET (dialog), "password")));
	    int length = strlen(entry);
	    if (( passwd = (const char *) malloc(length * sizeof(char) + 1)) == NULL)
	    {
                DEBUG ('m', "Not enough memory for temporary storing the password!\n");
		break;
	    }
	
	    strcpy((char*) passwd, entry);
	    *password = passwd;
	}
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            break;

        default :
            g_assert_not_reached ();
    }
}

/**
 * A small dialog for setting the password. The const char pointer to
 * the password has to be freed outside of this function!
 */
const char* GnomeCmdCon::gnome_cmd_con_set_password()
{
    const char *password = NULL;
    GtkWidget *table;
    GtkWidget *entry;
    GtkWidget *label;
    GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Password for..."), GTK_WINDOW(window),
                                                     GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                     NULL);
#if GTK_CHECK_VERSION (2, 14, 0)
    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
#endif

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

    // HIG defaults
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_box_set_spacing (GTK_BOX (content_area), 2);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    gtk_box_set_spacing (GTK_BOX (content_area),6);
#else
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area),6);
#endif
    table = gtk_table_new (3, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (table), 5);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_container_add (GTK_CONTAINER (content_area), table);
#else
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), table);
#endif

    GnomeVFSURI *urivfs = gnome_vfs_uri_new (uri);

    label = gtk_label_new (gnome_vfs_uri_to_string (urivfs, GNOME_VFS_URI_HIDE_NONE));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 2, 0, 1);
    
    entry = gtk_entry_new ();
    g_object_set_data (G_OBJECT (dialog), "password", entry);
    gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_table_attach_defaults (GTK_TABLE (table), entry, 0, 2, 1, 2);

#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_widget_show_all (content_area);
#else
    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);
#endif

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (set_password_callback), (gpointer) &password);
    
    gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return password;
}
