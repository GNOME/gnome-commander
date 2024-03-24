/**
 * @file gnome-cmd-con-device.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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
    gchar *alias {nullptr};
    gchar *device_fn {nullptr}; // The device identifier (either a linux device string or a uuid)
    gchar *mountp {nullptr};
    gchar *icon_path {nullptr};
    gboolean autovolume;
    GMount *gMount;
    GVolume *gVolume;
};


G_DEFINE_TYPE (GnomeCmdConDevice, gnome_cmd_con_device, GNOME_CMD_TYPE_CON)


static gboolean is_mounted (GnomeCmdConDevice *dev_con)
{
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
            if (strcmp (dir, dev_con->priv->mountp) == 0)
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

    gint ret, estatus;

    if (!is_mounted (dev_con))
    {
        gchar *cmd = nullptr;
        gchar *emsg = nullptr;

        if (strlen(dev_con->priv->device_fn) == 0)
            return;

        DEBUG ('m', "mounting %s\n", dev_con->priv->device_fn);
        if (dev_con->priv->device_fn[0] == G_DIR_SEPARATOR)
        {
            cmd = g_strdup_printf ("mount %s", dev_con->priv->device_fn);
        }
        else
        {
            cmd = g_strdup_printf ("mount -L %s", dev_con->priv->device_fn);
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
    if (con->base_gFileInfo)
        g_object_unref (con->base_gFileInfo);
    con->base_gFileInfo = nullptr;
    con->open_result = GnomeCmdCon::OPEN_FAILED;
    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_failed_msg = con->open_failed_error->message;
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
        con->open_failed_error = g_error_copy(error);
        set_con_mount_failed(con);
        g_error_free(error);
        return;
    }
    dev_con->priv->gMount = g_volume_get_mount (gVolume);
    if (!con->base_path)
    {
        set_con_base_path_for_gmount(con, dev_con->priv->gMount);
    }
    if (con->base_path)
    {
        gnome_cmd_con_set_root_path(con, con->base_path->get_path());
    }
    set_con_base_gfileinfo(con);
    set_con_mount_succeed(con);
}

static void do_legacy_mount_thread_func(GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    GError *error = nullptr;

    if (!con->base_path)
    {
        con->base_path = new GnomeCmdPlainPath(GNOME_CMD_CON_DEVICE(con)->priv->mountp);
    }

    do_legacy_mount(con);

    if (con->open_result == GnomeCmdCon::OPEN_FAILED)
    {
        con->open_failed_error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED, "Unable to mount %s", GNOME_CMD_CON_DEVICE (con)->priv->device_fn);
        set_con_mount_failed(con);
        return;
    }

    auto gFile = gnome_cmd_con_create_gfile(con, con->base_path);

    con->base_gFileInfo = g_file_query_info(gFile,
                              "*",
                              G_FILE_QUERY_INFO_NONE,
                              nullptr,
                              &error);
    g_object_unref(gFile);
    if (error)
    {
        con->open_failed_error = g_error_copy(error);
        set_con_mount_failed(con);
        g_warning("Unable to mount the volume via legacy mount, error: %s", error->message);
        g_error_free(error);
        return;
    }

    set_con_mount_succeed(con);
    return;
}


static void do_mount (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

    // This is a legacy-mount: If mount point is given, we mount the device with system calls ('mount')
    if (dev_con->priv->mountp)
    {
        auto gThread = g_thread_new (nullptr, (GThreadFunc) do_legacy_mount_thread_func, con);
        g_thread_unref(gThread);
    }

    // Check if the volume is already mounted:
    if (dev_con->priv->gVolume)
    {
        auto gMount = g_volume_get_mount (dev_con->priv->gVolume);
        if (gMount)
        {
            dev_con->priv->gMount = gMount;
            set_con_base_path_for_gmount(con, dev_con->priv->gMount);
            set_con_base_gfileinfo(con);
            set_con_mount_succeed(con);
            return;
        }

        auto gMountOperation = gtk_mount_operation_new ((GtkWindow*) main_win);

        g_volume_mount (dev_con->priv->gVolume,
            G_MOUNT_MOUNT_NONE,
            gMountOperation,
            nullptr,
            mount_finish_callback,
            con);
    }
}


static void dev_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    DEBUG ('m', "Mounting device\n");

    con->state = GnomeCmdCon::STATE_OPENING;
    con->open_result = GnomeCmdCon::OPEN_IN_PROGRESS;

    do_mount(con);
}


static void show_message_dialog_volume_unmounted()
{
    DEBUG('m', "unmount_callback: succeeded\n");
    GtkWidget *msgbox;
    msgbox = gtk_message_dialog_new (*main_win,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_INFO,
                                     GTK_BUTTONS_OK,
                                     _("Volume successfully unmounted"));
    gtk_dialog_run (GTK_DIALOG (msgbox));
    gtk_widget_destroy (msgbox);
}


static void unmount_callback(GObject *gMnt, GAsyncResult *result, gpointer user_data)
{
    GnomeCmdCon *con = GNOME_CMD_CON (user_data);
    auto gMount = G_MOUNT(gMnt);
    GError *error = nullptr;
    GtkWidget *msgbox;

    if (!g_mount_unmount_with_operation_finish (gMount, result, &error))
    {
        DEBUG('m', "unmount_callback: failed %s\n", error->message);

        msgbox = gtk_message_dialog_new (*main_win,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("Cannot unmount the volume:\n%s\nError code: %d"),
                                         error->message,
                                         error->code);
        gtk_dialog_run (GTK_DIALOG (msgbox));
        gtk_widget_destroy (msgbox);

        g_error_free(error);
        return;
    }

    con->state = GnomeCmdCon::STATE_CLOSED;

    show_message_dialog_volume_unmounted();
}


static gboolean dev_close (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), FALSE);

    gint ret = 0;

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

    gnome_cmd_con_set_default_dir (con, nullptr);

    if (chdir (g_get_home_dir ()) == -1)
    {
        DEBUG ('m', "Could not go back to home directory before unmounting\n");
    }

    if (dev_con->priv->autovolume)
    {
        if (!g_mount_can_unmount(dev_con->priv->gMount))
            return ret == 0;

        auto gMountName = g_mount_get_name(dev_con->priv->gMount);
        DEBUG ('m', "umounting GIO mount \"%s\"\n", gMountName);
        g_free(gMountName);
        g_mount_unmount_with_operation (dev_con->priv->gMount,
                            G_MOUNT_UNMOUNT_NONE,
                            nullptr,
                            nullptr,
                            unmount_callback,
                            con);
    }
    else
    {
        // Legacy unmount
        if(dev_con->priv->mountp)
        {
            DEBUG ('m', "umounting %s\n", dev_con->priv->mountp);
            gchar *cmd = g_strdup_printf ("umount %s", dev_con->priv->mountp);
            ret = system (cmd);
            DEBUG ('m', "umount returned %d\n", ret);
            g_free (cmd);

            if (ret == 0)
            {
                con->state = GnomeCmdCon::STATE_CLOSED;

                show_message_dialog_volume_unmounted();
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


static GFile *dev_create_gfile (GnomeCmdCon *con, GnomeCmdPath *gnomeCmdPath)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), nullptr);

    GFile *newGFile = nullptr;
    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

    if (dev_con->priv->gMount != nullptr)
    {
        if (gnomeCmdPath)
        {
            auto gMountGFile = g_mount_get_default_location (dev_con->priv->gMount);
            newGFile = g_file_resolve_relative_path(gMountGFile, gnomeCmdPath->get_path());
            g_object_unref(gMountGFile);
        }
        else
        {
            newGFile = g_mount_get_default_location (dev_con->priv->gMount);
        }
    }
    else
    {
        if (gnomeCmdPath != nullptr)
        {
            newGFile = g_file_new_for_path(gnomeCmdPath->get_path());
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


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdConDevice *con = GNOME_CMD_CON_DEVICE (object);

    if (con->priv->gMount)
    {
        g_object_unref(con->priv->gMount);
        con->priv->gMount = nullptr;
    }
    if (con->priv->gVolume)
    {
        g_object_unref(con->priv->gVolume);
        con->priv->gVolume = nullptr;
    }

    g_free (con->priv);

    GTK_OBJECT_CLASS (gnome_cmd_con_device_parent_class)->destroy (object);
}


static void gnome_cmd_con_device_class_init (GnomeCmdConDeviceClass *klass)
{
    GtkObjectClass *object_class;
    GnomeCmdConClass *con_class;

    object_class = GTK_OBJECT_CLASS (klass);
    con_class = GNOME_CMD_CON_CLASS (klass);

    object_class->destroy = destroy;

    con_class->open = dev_open;
    con_class->close = dev_close;
    con_class->cancel_open = dev_cancel_open;
    con_class->open_is_needed = dev_open_is_needed;
    con_class->create_gfile = dev_create_gfile;
    con_class->create_path = dev_create_path;
}


static void gnome_cmd_con_device_init (GnomeCmdConDevice *dev_con)
{
    GnomeCmdCon *con = GNOME_CMD_CON (dev_con);

    dev_con->priv = g_new0 (GnomeCmdConDevicePrivate, 1);

    con->method = CON_FILE;
    con->should_remember_dir = TRUE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = TRUE;
    con->is_local = TRUE;
    con->is_closeable = TRUE;
    con->go_pixmap = nullptr;
    con->open_pixmap = nullptr;
    con->close_pixmap = nullptr;
}

/***********************************
 * Public functions
 ***********************************/

GnomeCmdConDevice *gnome_cmd_con_device_new (const gchar *alias, const gchar *device_fn, const gchar *mountp, const gchar *icon_path)
{
    auto dev = static_cast<GnomeCmdConDevice*> (g_object_new (GNOME_CMD_TYPE_CON_DEVICE, nullptr));
    GnomeCmdCon *con = GNOME_CMD_CON (dev);

    gnome_cmd_con_device_set_device_fn (dev, device_fn);
    gnome_cmd_con_device_set_mountp (dev, mountp);
    gnome_cmd_con_device_set_icon_path (dev, icon_path);
    gnome_cmd_con_device_set_autovol(dev, FALSE);
    gnome_cmd_con_device_set_gmount(dev, nullptr);
    gnome_cmd_con_device_set_gvolume(dev, nullptr);
    gnome_cmd_con_device_set_alias (dev, alias);

    if (mountp)
        gnome_cmd_con_set_root_path (con, mountp);

    con->open_msg = g_strdup_printf (_("Mounting %s"), alias);

    return dev;
}


void gnome_cmd_con_device_set_alias (GnomeCmdConDevice *dev, const gchar *alias)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);
    g_return_if_fail (alias != nullptr);

    g_free (dev->priv->alias);

    dev->priv->alias = g_strdup (alias);
    GNOME_CMD_CON (dev)->alias = g_strdup (alias);
    if (dev->priv->mountp)
        GNOME_CMD_CON (dev)->go_text = g_strdup_printf (_("Go to: %s (%s)"), alias, dev->priv->mountp);
    else
        GNOME_CMD_CON (dev)->go_text = g_strdup_printf (_("Go to: %s"), alias);
    GNOME_CMD_CON (dev)->open_text = g_strdup_printf (_("Mount: %s"), alias);
    GNOME_CMD_CON (dev)->close_text = g_strdup_printf (_("Unmount: %s"), alias);
}


void gnome_cmd_con_device_set_device_fn (GnomeCmdConDevice *dev, const gchar *device_fn)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);

    g_free (dev->priv->device_fn);

    dev->priv->device_fn = g_strdup (device_fn ? device_fn : "");
}


void gnome_cmd_con_device_set_mountp (GnomeCmdConDevice *dev, const gchar *mountp)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);

    if (!mountp) return;

    g_free (dev->priv->mountp);

    dev->priv->mountp = g_strdup (mountp);
}


void gnome_cmd_con_device_set_icon_path (GnomeCmdConDevice *dev, const gchar *icon_path)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);

    g_free (dev->priv->icon_path);

    GnomeCmdCon *con = GNOME_CMD_CON (dev);

    gnome_cmd_pixmap_free (con->go_pixmap);
    gnome_cmd_pixmap_free (con->open_pixmap);
    gnome_cmd_pixmap_free (con->close_pixmap);

    con->go_pixmap = nullptr;
    con->open_pixmap = nullptr;
    con->close_pixmap = nullptr;

    dev->priv->icon_path = g_strdup (icon_path);

    if (icon_path)
    {
        guint dev_icon_size = gnome_cmd_data.dev_icon_size;

        con->go_pixmap = gnome_cmd_pixmap_new_from_file (icon_path, dev_icon_size, dev_icon_size);
        con->open_pixmap = gnome_cmd_pixmap_new_from_file (icon_path, dev_icon_size, dev_icon_size);

        if (con->open_pixmap)
        {
            GdkPixbuf *overlay = gdk_pixbuf_copy (con->open_pixmap->pixbuf);

            if (overlay)
            {
                GdkPixbuf *umount = IMAGE_get_pixbuf (PIXMAP_OVERLAY_UMOUNT);
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
                if (umount)
                {
                    gdk_pixbuf_copy_area (umount, 0, 0,
                                          MIN (gdk_pixbuf_get_width (umount), dev_icon_size),
                                          MIN (gdk_pixbuf_get_height (umount), dev_icon_size),
                                          overlay, 0, 0);

                    con->close_pixmap = gnome_cmd_pixmap_new_from_pixbuf (overlay);
                }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
                g_object_unref (overlay);
            }
        }
    }
}


void gnome_cmd_con_device_set_autovol (GnomeCmdConDevice *dev, const gboolean autovol)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);

    dev->priv->autovolume = autovol;
}


void gnome_cmd_con_device_set_gmount (GnomeCmdConDevice *dev, GMount *gMount)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);

    if (dev->priv->gMount)
    {
        g_object_unref (dev->priv->gMount);
        dev->priv->gMount = nullptr;
    }

    dev->priv->gMount = gMount;
    if (dev->priv->gMount)
        g_object_ref(dev->priv->gMount);
}


void gnome_cmd_con_device_set_gvolume (GnomeCmdConDevice *dev, GVolume *gVolume)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);

    if (dev->priv->gVolume)
    {
        g_object_unref (dev->priv->gVolume);
        dev->priv->gVolume = nullptr;
    }

    dev->priv->gVolume = gVolume;
    if (dev->priv->gVolume)
        g_object_ref(dev->priv->gVolume);
}


const gchar *gnome_cmd_con_device_get_alias (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != nullptr, nullptr);
    g_return_val_if_fail (dev->priv != nullptr, nullptr);

    return dev->priv->alias;
}


const gchar *gnome_cmd_con_device_get_device_fn (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != nullptr, nullptr);
    g_return_val_if_fail (dev->priv != nullptr, nullptr);

    return dev->priv->device_fn != nullptr ? dev->priv->device_fn : "";
}


const gchar *gnome_cmd_con_device_get_mountp_string (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != nullptr, nullptr);
    g_return_val_if_fail (dev->priv != nullptr, nullptr);

    return dev->priv->mountp;
}


const gchar *gnome_cmd_con_device_get_icon_path (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != nullptr, nullptr);
    g_return_val_if_fail (dev->priv != nullptr, nullptr);

    return dev->priv->icon_path;
}


gboolean gnome_cmd_con_device_get_autovol (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != nullptr, FALSE);
    g_return_val_if_fail (dev->priv != nullptr, FALSE);

    return dev->priv->autovolume;
}


GMount *gnome_cmd_con_device_get_gmount (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != nullptr, nullptr);
    g_return_val_if_fail (dev->priv != nullptr, nullptr);

    return dev->priv->gMount;
}

GVolume *gnome_cmd_con_device_get_gvolume (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != nullptr, nullptr);
    g_return_val_if_fail (dev->priv != nullptr, nullptr);

    return dev->priv->gVolume;
}
