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
#include "gnome-cmd-data.h"
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-smb-path.h"
#include "imageloader.h"
#include "utils.h"

using namespace std;


static GnomeCmdConClass *parent_class = NULL;


static void
get_file_info_callback (GnomeVFSAsyncHandle *handle,
                        GList *results, /* GnomeVFSGetFileInfoResult *items */
                        GnomeCmdCon *con)
{
    g_return_if_fail (results != NULL);

    GnomeCmdConSmb *smb_con = GNOME_CMD_CON_SMB (con);

    if (con->state == CON_STATE_OPENING)
    {
        GnomeVFSGetFileInfoResult *r = (GnomeVFSGetFileInfoResult *) results->data;

        if (r && r->result == GNOME_VFS_OK)
        {
            gnome_vfs_file_info_ref (r->file_info);
            con->state = CON_STATE_OPEN;
            con->base_info = r->file_info;
            con->open_result = CON_OPEN_OK;
        }
        else if (r)
        {
            con->state = CON_STATE_CLOSED;
            con->open_result = CON_OPEN_FAILED;
            con->open_failed_reason = r->result;
        }
        else
        {
            g_warning ("No result at all");
            con->state = CON_STATE_CLOSED;
            con->open_result = CON_OPEN_FAILED;
        }
    }
    else
        if (con->state == CON_STATE_CANCELLING)
            DEBUG('m', "The open operation was cancelled, doing nothing\n");
    else
        DEBUG('m', "Strange ConState %d\n", con->state);

    con->state = CON_STATE_CLOSED;
}


static void smb_open (GnomeCmdCon *con)
{
    if (!con->base_path)
    {
        con->base_path = gnome_cmd_smb_path_new (NULL, NULL, NULL);
        gtk_object_ref (GTK_OBJECT (con->base_path));
    }

    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, con->base_path);
    if (!uri)
    {
        DEBUG('m', "gnome_cmd_con_create_uri returned NULL\n");
        con->state = CON_STATE_CLOSED;
        con->open_result = CON_OPEN_FAILED;
        con->open_failed_msg = g_strdup (_("Failed to browse the network. Is the SMB module installed?"));
        return;
    }

    DEBUG('l', "Connecting to %s\n", gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE));
    GList *uri_list = g_list_append (NULL, uri);

    con->state = CON_STATE_OPENING;
    con->open_result = CON_OPEN_IN_PROGRESS;

    GnomeVFSAsyncHandle *handle;
    GnomeVFSFileInfoOptions infoOpts = (GnomeVFSFileInfoOptions) (GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
                                                                  GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
                                                                  GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);

    gnome_vfs_async_get_file_info (
        &handle,
        uri_list,
        infoOpts,
        0,
        (GnomeVFSAsyncGetFileInfoCallback) get_file_info_callback,
        con);
}


static gboolean smb_close (GnomeCmdCon *con)
{
    return FALSE;
}


static void smb_cancel_open (GnomeCmdCon *con)
{
    con->state = CON_STATE_CANCELLING;
}


static gboolean smb_open_is_needed (GnomeCmdCon *con)
{
    return TRUE;
}


static GnomeVFSURI *smb_create_uri (GnomeCmdCon *con, GnomeCmdPath *path)
{
    GnomeVFSURI *u1, *u2;

    u1 = gnome_vfs_uri_new ("smb:");
    if (!u1) return NULL;

    u2 = gnome_vfs_uri_append_path (u1, gnome_cmd_path_get_path (path));
    gnome_vfs_uri_unref (u1);
    if (!u2) return NULL;

    const gchar *p = gnome_vfs_uri_get_path (u2);
    gchar *s = g_strdup_printf ("smb:/%s", p);
    u1 = gnome_vfs_uri_new (s);
    gnome_vfs_uri_unref (u2);
    g_free (s);

    return u1;
}


static GnomeCmdPath *smb_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    return gnome_cmd_smb_path_new_from_str (path_str);
}



/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdConSmb *con = GNOME_CMD_CON_SMB (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdConSmbClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    parent_class = (GnomeCmdConClass *) gtk_type_class (gnome_cmd_con_get_type ());

    object_class->destroy = destroy;

    con_class->open = smb_open;
    con_class->close = smb_close;
    con_class->cancel_open = smb_cancel_open;
    con_class->open_is_needed = smb_open_is_needed;
    con_class->create_uri = smb_create_uri;
    con_class->create_path = smb_create_path;
}


static void init (GnomeCmdConSmb *smb_con)
{
    guint dev_icon_size = gnome_cmd_data.dev_icon_size;

    GnomeCmdCon *con = GNOME_CMD_CON (smb_con);

    con->alias = g_strdup (_("SMB"));
    con->method = CON_SMB;
    con->open_msg = g_strdup (_("Searching for workgroups and hosts"));
    con->should_remember_dir = TRUE;
    con->needs_open_visprog = TRUE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = FALSE;
    con->is_local = FALSE;
    con->is_closeable = FALSE;
    con->go_text = g_strdup (_("Go to: Samba Network"));
    con->go_pixmap = gnome_cmd_pixmap_new_from_icon ("gnome-fs-network", dev_icon_size);
    con->open_pixmap = gnome_cmd_pixmap_new_from_icon ("gnome-fs-network", dev_icon_size);
    con->close_pixmap = gnome_cmd_pixmap_new_from_icon ("gnome-fs-network", dev_icon_size);
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_con_smb_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdConSmb",
            sizeof (GnomeCmdConSmb),
            sizeof (GnomeCmdConSmbClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gnome_cmd_con_get_type (), &info);
    }
    return type;
}


GnomeCmdCon *gnome_cmd_con_smb_new ()
{
    return GNOME_CMD_CON (gtk_type_new (gnome_cmd_con_smb_get_type ()));
}
