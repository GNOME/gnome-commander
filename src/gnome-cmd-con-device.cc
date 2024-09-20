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

#include <config.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-con-device.h"
#include "gnome-cmd-plain-path.h"
#include "imageloader.h"
#include "utils.h"

#include "gnome-cmd-main-win.h"

using namespace std;


struct GnomeCmdConDevicePrivate
{
    gchar *device_fn {nullptr}; // The device identifier (either a linux device string or a uuid)
    gchar *mountp {nullptr};
    GIcon *icon {nullptr};
    gboolean autovolume;
    GMount *gMount;
    GVolume *gVolume;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdConDevice, gnome_cmd_con_device, GNOME_CMD_TYPE_CON)


static void set_con_base_path_for_gmount (GnomeCmdConDevice *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (con));

    g_return_if_fail (G_IS_MOUNT (priv->gMount));

    auto gFile = g_mount_get_default_location (priv->gMount);
    auto pathString = g_file_get_path(gFile);
    g_object_unref(gFile);

    gnome_cmd_con_set_base_path (GNOME_CMD_CON (con), new GnomeCmdPlainPath(pathString));

    g_free(pathString);
}


static gboolean is_mounted (GnomeCmdConDevice *dev_con)
{
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev_con));

    FILE *fd = fopen ("/etc/mtab", "r");

    if (!fd)
        return FALSE;

    gchar tmp[512];
    gboolean ret = FALSE;

    gchar *s;
    while ((s=fgets (tmp, sizeof(tmp), fd))!=nullptr)
    {
        char **v = g_strsplit (s, " ", 3);

        if (v[1])
        {
            gchar *dir = g_strcompress (v[1]);
            if (strcmp (dir, priv->mountp) == 0)
                ret = TRUE;
            g_free (dir);
        }

        g_strfreev (v);

        if (ret)
            break;
    }

    fclose (fd);

    return ret;
}


static void do_legacy_mount(GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev_con));

    gint ret, estatus;

    if (!is_mounted (dev_con))
    {
        gchar *cmd = nullptr;
        gchar *emsg = nullptr;

        if (strlen(priv->device_fn) == 0)
            return;

        DEBUG ('m', "mounting %s\n", priv->device_fn);
        if (priv->device_fn[0] == G_DIR_SEPARATOR)
        {
            cmd = g_strdup_printf ("mount %s", priv->device_fn);
        }
        else
        {
            cmd = g_strdup_printf ("mount -L %s", priv->device_fn);
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
            con->open_result = GnomeCmdCon::OPEN_FAILED;
            con->state = GnomeCmdCon::STATE_CLOSED;
            con->open_failed_msg = emsg;
            return;
        }
    }
    else
        DEBUG('m', "The device was already mounted\n");
}


static void set_con_mount_succeed(GnomeCmdCon *con)
{
    con->state = GnomeCmdCon::STATE_OPEN;
    con->open_result = GnomeCmdCon::OPEN_OK;
}


static void set_con_mount_failed(GnomeCmdCon *con)
{
    gnome_cmd_con_set_base_file_info(con, nullptr);
    con->open_result = GnomeCmdCon::OPEN_FAILED;
    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_failed_msg = con->open_failed_error->message;
}


static gboolean set_con_base_gfileinfo(GnomeCmdCon *con)
{
    GError *error = nullptr;
    auto gFile = gnome_cmd_con_create_gfile (con, gnome_cmd_con_get_base_path (con)->get_path());
    auto gFileInfo = g_file_query_info(gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &error);
    gnome_cmd_con_set_base_file_info (con, gFileInfo);
    g_object_unref(gFile);
    if (error)
    {
        con->open_failed_error = g_error_copy(error);
        set_con_mount_failed(con);
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
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev_con));
    auto gVolume = G_VOLUME(gVol);
    GError *error = nullptr;

    if(!g_volume_mount_finish(gVolume, result, &error))
    {
        g_critical("Unable to mount the volume, error: %s", error->message);
        con->open_failed_error = g_error_copy(error);
        set_con_mount_failed(con);
        g_error_free(error);
        return;
    }
    priv->gMount = g_volume_get_mount (gVolume);
    if (gnome_cmd_con_get_base_path (con) == nullptr)
    {
        set_con_base_path_for_gmount (dev_con);
    }
    if (GnomeCmdPath *base_path = gnome_cmd_con_get_base_path (con))
    {
        gchar *uri_string = g_filename_to_uri (base_path->get_path(), nullptr, nullptr);
        gnome_cmd_con_set_uri_string (con, uri_string);
        g_free (uri_string);
    }
    set_con_base_gfileinfo(con);
}

static void do_legacy_mount_thread_func(GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (GNOME_CMD_CON_DEVICE (con)));

    if (gnome_cmd_con_get_base_path (con) == nullptr)
    {
        gnome_cmd_con_set_base_path (con, new GnomeCmdPlainPath(priv->mountp));
    }

    do_legacy_mount(con);

    if (con->open_result == GnomeCmdCon::OPEN_FAILED)
    {
        con->open_failed_error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED, "Unable to mount %s", priv->device_fn);
        set_con_mount_failed(con);
        return;
    }

    set_con_base_gfileinfo(con);
}


static void do_mount (GnomeCmdCon *con, GtkWindow *parent_window)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev_con));

    // This is a legacy-mount: If mount point is given, we mount the device with system calls ('mount')
    if (priv->mountp)
    {
        auto gThread = g_thread_new (nullptr, (GThreadFunc) do_legacy_mount_thread_func, con);
        g_thread_unref(gThread);
    }

    // Check if the volume is already mounted:
    if (priv->gVolume)
    {
        auto gMount = g_volume_get_mount (priv->gVolume);
        if (gMount)
        {
            priv->gMount = gMount;
            set_con_base_path_for_gmount(dev_con);
            set_con_base_gfileinfo(con);
            return;
        }

        auto gMountOperation = gtk_mount_operation_new (parent_window);

        g_volume_mount (priv->gVolume,
            G_MOUNT_MOUNT_NONE,
            gMountOperation,
            nullptr,
            mount_finish_callback,
            con);
    }
}


static void dev_open (GnomeCmdCon *con, GtkWindow *parent_window)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    DEBUG ('m', "Mounting device\n");

    con->state = GnomeCmdCon::STATE_OPENING;
    con->open_result = GnomeCmdCon::OPEN_IN_PROGRESS;

    do_mount(con, parent_window);
}


static void show_message_dialog_volume_unmounted (GtkWindow *parent_window)
{
    DEBUG('m', "unmount_callback: succeeded\n");
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

    con->state = GnomeCmdCon::STATE_CLOSED;

    show_message_dialog_volume_unmounted (parent_window);
}


static gboolean dev_close (GnomeCmdCon *con, GtkWindow *parent_window)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), FALSE);

    gint ret = 0;

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev_con));

    gnome_cmd_con_set_default_dir (con, nullptr);

    if (chdir (g_get_home_dir ()) == -1)
    {
        DEBUG ('m', "Could not go back to home directory before unmounting\n");
    }

    if (priv->autovolume)
    {
        if (!g_mount_can_unmount(priv->gMount))
            return ret == 0;

        auto gMountName = g_mount_get_name(priv->gMount);
        DEBUG ('m', "umounting GIO mount \"%s\"\n", gMountName);
        g_free(gMountName);

        UnmountClosure *cls = g_new0 (UnmountClosure, 1);
        cls->con = con;
        cls->parent_window = parent_window;

        g_mount_unmount_with_operation (priv->gMount,
                            G_MOUNT_UNMOUNT_NONE,
                            nullptr,
                            nullptr,
                            unmount_callback,
                            cls);
    }
    else
    {
        // Legacy unmount
        if(priv->mountp)
        {
            DEBUG ('m', "umounting %s\n", priv->mountp);
            gchar *cmd = g_strdup_printf ("umount %s", priv->mountp);
            ret = system (cmd);
            DEBUG ('m', "umount returned %d\n", ret);
            g_free (cmd);

            if (ret == 0)
            {
                con->state = GnomeCmdCon::STATE_CLOSED;

                show_message_dialog_volume_unmounted (parent_window);
            }
        }
    }

    return ret == 0;
}


static void dev_cancel_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));
}


static gboolean dev_open_is_needed (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), FALSE);

    return TRUE;
}


static GFile *dev_create_gfile (GnomeCmdCon *con, const gchar *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), nullptr);

    GFile *newGFile = nullptr;
    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev_con));

    if (priv->gMount != nullptr)
    {
        if (path != nullptr)
        {
            auto gMountGFile = g_mount_get_default_location (priv->gMount);
            newGFile = g_file_resolve_relative_path(gMountGFile, path);
            g_object_unref(gMountGFile);
        }
        else
        {
            newGFile = g_mount_get_default_location (priv->gMount);
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

    return new GnomeCmdPlainPath(path_str);
}


static gchar *dev_get_go_text (GnomeCmdCon *con)
{
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (GNOME_CMD_CON_DEVICE (con)));
    const gchar *alias = con->alias;
    if (priv->mountp)
        return g_strdup_printf (_("Go to: %s (%s)"), alias, priv->mountp);
    else
        return g_strdup_printf (_("Go to: %s"), alias);
}


static gchar *dev_get_open_text (GnomeCmdCon *con)
{
    const gchar *alias = con->alias;
    return g_strdup_printf (_("Mount: %s"), alias);
}


static gchar *dev_get_close_text (GnomeCmdCon *con)
{
    const gchar *alias = con->alias;
    return g_strdup_printf (_("Unmount: %s"), alias);
}


static GIcon *dev_get_icon (GnomeCmdCon *con)
{
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (GNOME_CMD_CON_DEVICE (con)));
    return priv->icon ? g_object_ref (priv->icon) : nullptr;
}


static GIcon *dev_get_close_icon (GnomeCmdCon *con)
{
    GIcon *icon = dev_get_icon (con);
    if (!icon)
        return nullptr;

    GIcon *unmount = g_themed_icon_new (OVERLAY_UMOUNT_ICON);
    GEmblem *emblem = g_emblem_new (unmount);

    return g_emblemed_icon_new (icon, emblem);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    GnomeCmdConDevice *dev = GNOME_CMD_CON_DEVICE (object);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    g_clear_object (&priv->gMount);
    g_clear_object (&priv->gVolume);

    G_OBJECT_CLASS (gnome_cmd_con_device_parent_class)->dispose (object);
}


static void gnome_cmd_con_device_class_init (GnomeCmdConDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    object_class->dispose = dispose;

    con_class->open = dev_open;
    con_class->close = dev_close;
    con_class->cancel_open = dev_cancel_open;
    con_class->open_is_needed = dev_open_is_needed;
    con_class->create_gfile = dev_create_gfile;
    con_class->create_path = dev_create_path;

    con_class->get_go_text = dev_get_go_text;
    con_class->get_open_text = dev_get_open_text;
    con_class->get_close_text = dev_get_close_text;

    con_class->get_go_icon = dev_get_icon;
    con_class->get_open_icon = dev_get_icon;
    con_class->get_close_icon = dev_get_close_icon;
}


static void gnome_cmd_con_device_init (GnomeCmdConDevice *dev_con)
{
    GnomeCmdCon *con = GNOME_CMD_CON (dev_con);

    con->should_remember_dir = TRUE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = TRUE;
    con->is_local = TRUE;
    con->is_closeable = TRUE;
}

/***********************************
 * Public functions
 ***********************************/

GnomeCmdConDevice *gnome_cmd_con_device_new (const gchar *alias, const gchar *device_fn, const gchar *mountp, GIcon *icon)
{
    auto dev = static_cast<GnomeCmdConDevice*> (g_object_new (GNOME_CMD_TYPE_CON_DEVICE, nullptr));
    GnomeCmdCon *con = GNOME_CMD_CON (dev);

    gnome_cmd_con_device_set_device_fn (dev, device_fn);
    gnome_cmd_con_device_set_mountp (dev, mountp);
    gnome_cmd_con_device_set_icon (dev, icon);
    gnome_cmd_con_device_set_autovol(dev, FALSE);
    gnome_cmd_con_device_set_gmount(dev, nullptr);
    gnome_cmd_con_device_set_gvolume(dev, nullptr);
    gnome_cmd_con_set_alias (con, alias);

    if (mountp)
    {
        gchar *uri_string = g_filename_to_uri (mountp, nullptr, nullptr);
        gnome_cmd_con_set_uri_string (con, uri_string);
        g_free (uri_string);
    }

    con->open_msg = g_strdup_printf (_("Mounting %s"), alias);

    return dev;
}


void gnome_cmd_con_device_set_device_fn (GnomeCmdConDevice *dev, const gchar *device_fn)
{
    g_return_if_fail (dev != nullptr);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    g_free (priv->device_fn);

    priv->device_fn = g_strdup (device_fn ? device_fn : "");
}


void gnome_cmd_con_device_set_mountp (GnomeCmdConDevice *dev, const gchar *mountp)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (dev));
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    if (!mountp) return;

    g_free (priv->mountp);

    priv->mountp = g_strdup (mountp);
}


void gnome_cmd_con_device_set_icon (GnomeCmdConDevice *dev, GIcon *icon)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (dev));
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    g_clear_object (&priv->icon);
    priv->icon = icon;
}


void gnome_cmd_con_device_set_autovol (GnomeCmdConDevice *dev, const gboolean autovol)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (dev));
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    priv->autovolume = autovol;
}


void gnome_cmd_con_device_set_gmount (GnomeCmdConDevice *dev, GMount *gMount)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (dev));
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    if (priv->gMount)
    {
        g_object_unref (priv->gMount);
        priv->gMount = nullptr;
    }

    priv->gMount = gMount;
    if (priv->gMount)
        g_object_ref(priv->gMount);
}


void gnome_cmd_con_device_set_gvolume (GnomeCmdConDevice *dev, GVolume *gVolume)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (dev));
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    if (priv->gVolume)
    {
        g_object_unref (priv->gVolume);
        priv->gVolume = nullptr;
    }

    priv->gVolume = gVolume;
    if (priv->gVolume)
        g_object_ref(priv->gVolume);
}


const gchar *gnome_cmd_con_device_get_device_fn (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (dev), nullptr);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    return priv->device_fn != nullptr ? priv->device_fn : "";
}


const gchar *gnome_cmd_con_device_get_mountp_string (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (dev), nullptr);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    return priv->mountp;
}


GIcon *gnome_cmd_con_device_get_icon (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (dev), nullptr);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    return priv->icon;
}


gboolean gnome_cmd_con_device_get_autovol (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (dev), FALSE);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    return priv->autovolume;
}


GMount *gnome_cmd_con_device_get_gmount (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (dev), nullptr);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    return priv->gMount;
}

GVolume *gnome_cmd_con_device_get_gvolume (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (dev), nullptr);
    auto priv = static_cast<GnomeCmdConDevicePrivate *> (gnome_cmd_con_device_get_instance_private (dev));

    return priv->gVolume;
}
