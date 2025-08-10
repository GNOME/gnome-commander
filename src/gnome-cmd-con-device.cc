/**
 * @file gnome-cmd-con-device.cc
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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con-device.h"
#include "gnome-cmd-path.h"
#include "utils.h"


G_DEFINE_TYPE (GnomeCmdConDevice, gnome_cmd_con_device, GNOME_CMD_TYPE_CON)


static void set_con_base_path_for_gmount (GnomeCmdConDevice *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));
    auto gMount = gnome_cmd_con_device_get_gmount (GNOME_CMD_CON_DEVICE (con));
    g_return_if_fail (G_IS_MOUNT (gMount));

    auto gFile = g_mount_get_default_location (gMount);
    auto pathString = g_file_get_path(gFile);
    g_object_unref(gFile);

    gnome_cmd_con_set_base_path (GNOME_CMD_CON (con), gnome_cmd_plain_path_new (pathString));

    g_free(pathString);
}


static gboolean is_mounted (GnomeCmdConDevice *dev_con)
{
    FILE *fd = fopen ("/etc/mtab", "r");

    if (!fd)
        return FALSE;

    gchar tmp[512];
    gboolean ret = FALSE;
    gchar *mountp = gnome_cmd_con_device_get_mountp_string (dev_con);

    gchar *s;
    while ((s=fgets (tmp, sizeof(tmp), fd))!=nullptr)
    {
        char **v = g_strsplit (s, " ", 3);

        if (v[1])
        {
            gchar *dir = g_strcompress (v[1]);
            if (strcmp (dir, mountp) == 0)
                ret = TRUE;
            g_free (dir);
        }

        g_strfreev (v);

        if (ret)
            break;
    }

    fclose (fd);
    g_free (mountp);

    return ret;
}


static bool do_legacy_mount(GnomeCmdCon *con, const gchar *device_fn)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), false);

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

    gint ret, estatus;

    if (!is_mounted (dev_con))
    {
        gchar *cmd = nullptr;
        gchar *emsg = nullptr;

        if (!device_fn || strlen(device_fn) == 0)
            return false;

        DEBUG ('m', "mounting %s\n", device_fn);
        if (device_fn[0] == G_DIR_SEPARATOR)
        {
            cmd = g_strdup_printf ("mount %s", device_fn);
        }
        else
        {
            cmd = g_strdup_printf ("mount -L %s", device_fn);
        }

        DEBUG ('m', "Mount command: %s\n", cmd);
        ret = system (cmd);
        estatus = WEXITSTATUS (ret);
        g_free (cmd);
        DEBUG ('m', "mount returned %d and had the exitstatus %d\n", ret, estatus);

        if (ret == -1)
            emsg = g_strdup (_("Failed to execute the mount command"));
        else
            switch (estatus)
            {
                case 0:
                    emsg = nullptr;
                    break;
                case 1:
                    emsg = g_strdup (_("Mount failed: permission denied"));
                    break;
                case 32:
                    emsg = g_strdup (_("Mount failed: no medium found"));
                    break;
                default:
                    emsg = g_strdup_printf (_("Mount failed: mount exited with exitstatus %d"), estatus);
                    break;
            }

        if (emsg != nullptr)
        {
            GError *error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED, "Unable to mount %s", device_fn);
            gnome_cmd_con_set_open_state (con, GnomeCmdCon::OPEN_FAILED, error, emsg);
            g_error_free (error);
            g_free (emsg);
            return false;
        }
        return true;
    }
    else
    {
        DEBUG('m', "The device was already mounted\n");
        return true;
    }
}


static void set_con_mount_succeed(GnomeCmdCon *con)
{
    gnome_cmd_con_set_open_state (con, GnomeCmdCon::OPEN_OK, nullptr, nullptr);
}


static void set_con_mount_failed(GnomeCmdCon *con, GError *error)
{
    gnome_cmd_con_set_base_file_info(con, nullptr);
    gnome_cmd_con_set_open_state (con, GnomeCmdCon::OPEN_FAILED, error, error->message);
}


static gboolean set_con_base_gfileinfo(GnomeCmdCon *con)
{
    GError *error = nullptr;
    gchar *path = gnome_cmd_path_get_path (gnome_cmd_con_get_base_path (con));
    auto gFile = gnome_cmd_con_create_gfile (con, path);
    g_free (path);
    auto gFileInfo = g_file_query_info(gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &error);
    gnome_cmd_con_set_base_file_info (con, gFileInfo);
    g_object_unref(gFile);
    if (error)
    {
        set_con_mount_failed(con, error);
        g_critical("Unable to mount the volume: error: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    set_con_mount_succeed(con);
    return TRUE;
}


static void mount_finish_callback(GObject *gVol, GAsyncResult *result, gpointer user_data)
{
    GnomeCmdCon *con = GNOME_CMD_CON (user_data);
    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);
    auto gVolume = G_VOLUME(gVol);
    GError *error = nullptr;

    if(!g_volume_mount_finish(gVolume, result, &error))
    {
        g_critical("Unable to mount the volume, error: %s", error->message);
        set_con_mount_failed(con, error);
        g_error_free(error);
        return;
    }
    gnome_cmd_con_device_set_gmount (dev_con, g_volume_get_mount (gVolume));
    if (gnome_cmd_con_get_base_path (con) == nullptr)
    {
        set_con_base_path_for_gmount (dev_con);
    }
    if (GnomeCmdPath *base_path = gnome_cmd_con_get_base_path (con))
    {
        gchar *path = gnome_cmd_path_get_path (base_path);
        gchar *uri_string = g_filename_to_uri (path, nullptr, nullptr);
        g_free (path);
        gnome_cmd_con_set_uri_string (con, uri_string);
        g_free (uri_string);
    }
    set_con_base_gfileinfo(con);
}

static void do_legacy_mount_thread_func(GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    gchar *mountp = gnome_cmd_con_device_get_mountp_string (GNOME_CMD_CON_DEVICE (con));

    if (gnome_cmd_con_get_base_path (con) == nullptr)
    {
        gnome_cmd_con_set_base_path (con, gnome_cmd_plain_path_new (mountp));
    }

    gchar *device_fn = gnome_cmd_con_device_get_device_fn (GNOME_CMD_CON_DEVICE (con));

    if (do_legacy_mount(con, device_fn))
        set_con_base_gfileinfo(con);
    else
        gnome_cmd_con_set_base_file_info(con, nullptr);

    g_free (mountp);
    g_free (device_fn);
}


static void do_mount (GnomeCmdCon *con, GtkWindow *parent_window)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

    gchar *mountp = gnome_cmd_con_device_get_mountp_string (dev_con);
    bool is_legacy = mountp != nullptr;
    g_free (mountp);

    // This is a legacy-mount: If mount point is given, we mount the device with system calls ('mount')
    if (is_legacy)
    {
        auto gThread = g_thread_new (nullptr, (GThreadFunc) do_legacy_mount_thread_func, con);
        g_thread_unref(gThread);
        return;
    }

    // Check if the volume is already mounted:
    auto gVolume = gnome_cmd_con_device_get_gvolume (dev_con);
    if (gVolume)
    {
        auto gMount = g_volume_get_mount (gVolume);
        if (gMount)
        {
            gnome_cmd_con_device_set_gmount (dev_con, gMount);
            set_con_base_path_for_gmount(dev_con);
            set_con_base_gfileinfo(con);
            return;
        }

        auto gMountOperation = gtk_mount_operation_new (parent_window);

        g_volume_mount (gVolume,
            G_MOUNT_MOUNT_NONE,
            gMountOperation,
            nullptr,
            mount_finish_callback,
            con);
    }
}


static void dev_open (GnomeCmdCon *con, GtkWindow *parent_window, GCancellable *cancellable)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));
    DEBUG ('m', "Mounting device\n");
    do_mount(con, parent_window);
}


static void show_message_dialog_volume_unmounted (GtkWindow *parent_window)
{
    DEBUG('m', "unmount_callback: succeeded\n");

    if (!parent_window)
    {
        g_message ("%s", _("Volume successfully unmounted"));
        return;
    }

    GtkWidget *msgbox;
    msgbox = gtk_message_dialog_new (parent_window,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_INFO,
                                     GTK_BUTTONS_OK,
                                     _("Volume successfully unmounted"));
    g_signal_connect_swapped (msgbox, "response", G_CALLBACK (gtk_window_destroy), msgbox);
    gtk_window_present (GTK_WINDOW (msgbox));
}


struct UnmountClosure
{
    GnomeCmdCon *con;
    GtkWindow *parent_window;
};


static void unmount_callback(GObject *gMnt, GAsyncResult *result, gpointer user_data)
{
    UnmountClosure *cls = (UnmountClosure *) user_data;
    GnomeCmdCon *con = GNOME_CMD_CON (cls->con);
    GtkWindow *parent_window = cls->parent_window;
    g_free (cls);

    auto gMount = G_MOUNT(gMnt);
    GError *error = nullptr;
    GtkWidget *msgbox;

    if (!g_mount_unmount_with_operation_finish (gMount, result, &error))
    {
        DEBUG('m', "unmount_callback: failed %s\n", error->message);

        if (!parent_window)
        {
            g_warning (_("Cannot unmount the volume:\n%s\nError code: %d"), error->message, error->code);
            g_error_free(error);
            return;
        }

        msgbox = gtk_message_dialog_new (parent_window,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("Cannot unmount the volume:\n%s\nError code: %d"),
                                         error->message,
                                         error->code);
        g_signal_connect_swapped (msgbox, "response", G_CALLBACK (gtk_window_destroy), msgbox);
        gtk_window_present (GTK_WINDOW (msgbox));

        g_error_free(error);
        return;
    }

    gnome_cmd_con_set_state (con, GnomeCmdCon::STATE_CLOSED);

    show_message_dialog_volume_unmounted (parent_window);
}


static void dev_close (GnomeCmdCon *con, GtkWindow *parent_window)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    gint ret = 0;

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

    gnome_cmd_con_set_default_dir (con, nullptr);

    if (chdir (g_get_home_dir ()) == -1)
    {
        DEBUG ('m', "Could not go back to home directory before unmounting\n");
    }

    if (gnome_cmd_con_device_get_autovol (dev_con))
    {
        auto gMount = gnome_cmd_con_device_get_gmount (dev_con);
        if (!g_mount_can_unmount(gMount))
            return;

        auto gMountName = g_mount_get_name(gMount);
        DEBUG ('m', "umounting GIO mount \"%s\"\n", gMountName);
        g_free(gMountName);

        UnmountClosure *cls = g_new0 (UnmountClosure, 1);
        cls->con = con;
        cls->parent_window = parent_window;

        g_mount_unmount_with_operation (gMount,
                            G_MOUNT_UNMOUNT_NONE,
                            nullptr,
                            nullptr,
                            unmount_callback,
                            cls);
    }
    else
    {
        gchar *mountp = gnome_cmd_con_device_get_mountp_string (dev_con);

        // Legacy unmount
        if(mountp)
        {
            DEBUG ('m', "umounting %s\n", mountp);
            gchar *cmd = g_strdup_printf ("umount %s", mountp);
            ret = system (cmd);
            DEBUG ('m', "umount returned %d\n", ret);
            g_free (cmd);

            if (ret == 0)
            {
                gnome_cmd_con_set_state (con, GnomeCmdCon::STATE_CLOSED);

                show_message_dialog_volume_unmounted (parent_window);
            }
        }

        g_free (mountp);
    }
}


static GFile *dev_create_gfile (GnomeCmdCon *con, const gchar *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), nullptr);

    GFile *newGFile = nullptr;
    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

    auto gMount = gnome_cmd_con_device_get_gmount (dev_con);
    if (gMount != nullptr)
    {
        if (path != nullptr)
        {
            auto gMountGFile = g_mount_get_default_location (gMount);
            newGFile = g_file_resolve_relative_path(gMountGFile, path);
            g_object_unref(gMountGFile);
        }
        else
        {
            newGFile = g_mount_get_default_location (gMount);
        }
    }
    else
    {
        if (path != nullptr)
        {
            newGFile = g_file_new_for_path(path);
        }
    }
    return newGFile;
}


static GnomeCmdPath *dev_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), nullptr);
    g_return_val_if_fail (path_str != nullptr, nullptr);

    return gnome_cmd_plain_path_new(path_str);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_con_device_class_init (GnomeCmdConDeviceClass *klass)
{
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    con_class->open = dev_open;
    con_class->close = dev_close;
    con_class->create_gfile = dev_create_gfile;
    con_class->create_path = dev_create_path;
}


static void gnome_cmd_con_device_init (GnomeCmdConDevice *dev_con)
{
}
