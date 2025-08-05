/**
 * @file gnome-cmd-con-smb.cc
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
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-path.h"
#include "utils.h"

using namespace std;


extern "C" GnomeCmdPath *gnome_cmd_smb_path_new(const gchar *path, GnomeCmdConSmb *con);


struct GnomeCmdConSmbClass
{
    GnomeCmdConClass parent_class;
};


G_DEFINE_TYPE (GnomeCmdConSmb, gnome_cmd_con_smb, GNOME_CMD_TYPE_CON)


static void mount_func (GnomeCmdCon *con)
{
    g_return_if_fail(GNOME_CMD_IS_CON(con));

    // ToDo: Check if the error block below is executed if samba is not available on the system.
    // ToDo: Check if password is visible in the logs below!
    gchar *path = gnome_cmd_path_get_path (gnome_cmd_con_get_base_path (con));
    auto gFile = gnome_cmd_con_create_gfile (con, path);
    if (!gFile)
    {
        DEBUG('s', "gnome_cmd_con_create_gfile returned NULL\n");
        con->state = GnomeCmdCon::STATE_CLOSED;
        con->open_result = GnomeCmdCon::OPEN_FAILED;
        con->open_failed_error = g_error_new(G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Could not create a GFile object for \"smb:%s\"", path);
        con->open_failed_msg = g_strdup (_("Failed to browse the network. Is Samba supported on the system?"));
        g_free (path);
        return;
    }
    g_free (path);

    GError *error = nullptr;

    auto uriString = g_file_get_uri (gFile);
    if (!gnome_cmd_con_get_uri (con))
    {
        auto uri = g_uri_parse (uriString, G_URI_FLAGS_NON_DNS, &error);
        if (error)
            DEBUG('s', "g_uri_parse error: %s\n", error->message);
        else
            gnome_cmd_con_set_uri (con, uri);
    }
    DEBUG('s', "Connecting to %s\n", uriString);
    g_free(uriString);

    error = nullptr;
    auto base_gFileInfo = g_file_query_info(gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &error);
    if (error)
    {
        DEBUG('s', "g_file_query_info error: %s\n", error->message);
    }
    g_object_unref (gFile);

    if (con->state == GnomeCmdCon::STATE_OPENING)
    {
        DEBUG('s', "State was OPENING, setting flags\n");

        if (!error)
        {
            con->state = GnomeCmdCon::STATE_OPEN;
            gnome_cmd_con_set_base_file_info(con, base_gFileInfo);
            con->open_result = GnomeCmdCon::OPEN_OK;
        }
        else
        {
            con->state = GnomeCmdCon::STATE_CLOSED;
            con->open_result = GnomeCmdCon::OPEN_FAILED;
            con->open_failed_error = error;
        }
    }
    else
    {
        if (con->state == GnomeCmdCon::STATE_CANCELLING)
            DEBUG('s', "The open operation was cancelled, doing nothing\n");
        else
            DEBUG('s', "Strange ConState %d\n", con->state);
        con->state = GnomeCmdCon::STATE_CLOSED;
    }
}


static gboolean
start_mount_func (GnomeCmdCon *con)
{
    g_thread_new (nullptr, (GThreadFunc) mount_func, con);

    return FALSE;
}


static void smb_open (GnomeCmdCon *con, GtkWindow *parent_window)
{
    if (gnome_cmd_con_get_base_path (con) == nullptr)
        gnome_cmd_con_set_base_path (con, gnome_cmd_smb_path_new (nullptr, nullptr));

    con->state = GnomeCmdCon::STATE_OPENING;
    con->open_result = GnomeCmdCon::OPEN_IN_PROGRESS;

    g_timeout_add (1, (GSourceFunc) start_mount_func, con);

}


static void smb_close (GnomeCmdCon *con, GtkWindow *parent_window)
{
    // Copied from gnome-cmd-con-remote.cc:
    gnome_cmd_con_set_default_dir (con, nullptr);
    gnome_cmd_con_set_base_path (con, nullptr);
    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_result = GnomeCmdCon::OPEN_NOT_STARTED;
}


static void smb_cancel_open (GnomeCmdCon *con)
{
    DEBUG('s', "Setting state CANCELLING\n");
    con->state = GnomeCmdCon::STATE_CANCELLING;
}


static GFile *smb_create_gfile (GnomeCmdCon *con, const gchar *path)
{
    auto *gFileTmp = g_file_new_for_uri ("smb:");
    GFile *gFile;
    if (path)
    {
        gFile = g_file_resolve_relative_path (gFileTmp, path);
        g_object_unref(gFileTmp);
    }
    else
    {
        gFile = gFileTmp;
    }

    if (!gFile)
    {
        return nullptr;
    }

    return gFile;
}


static GnomeCmdPath *smb_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    return gnome_cmd_smb_path_new (path_str, GNOME_CMD_CON_SMB (con));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_con_smb_class_init (GnomeCmdConSmbClass *klass)
{
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    con_class->open = smb_open;
    con_class->close = smb_close;
    con_class->cancel_open = smb_cancel_open;
    con_class->create_gfile = smb_create_gfile;
    con_class->create_path = smb_create_path;
}


static void gnome_cmd_con_smb_init (GnomeCmdConSmb *smb_con)
{
    GnomeCmdCon *con = GNOME_CMD_CON (smb_con);

    con->alias = g_strdup (_("SMB"));
}
