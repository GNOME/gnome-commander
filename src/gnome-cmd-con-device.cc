/**
 * @file gnome-cmd-con-device.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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
#include <libgnomevfs/gnome-vfs-volume.h>

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
    gchar *device_fn {nullptr};
    gchar *mountp {nullptr};
    gchar *icon_path {nullptr};
    gboolean autovolume;
    GnomeVFSVolume *vfsvol;
};


static GnomeCmdConClass *parent_class = nullptr;


static gboolean is_mounted (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), FALSE);

    FILE *fd = fopen ("/etc/mtab", "r");

    if (!fd)
        return FALSE;

    gchar tmp[512];
    gboolean ret = FALSE;
    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

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


static void do_mount_thread_func (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    gint ret, estatus;

    if (!is_mounted (con))
    {
        gchar *cmd;
        gchar *emsg = nullptr;

        GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

        DEBUG ('m', "mounting %s\n", dev_con->priv->mountp);
        if (dev_con->priv->device_fn)
            cmd = g_strdup_printf ("mount %s %s", dev_con->priv->device_fn, dev_con->priv->mountp);
        else
            cmd = g_strdup_printf ("mount %s", dev_con->priv->mountp);
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

    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, con->base_path);
    if (!uri)
        return;

    gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
    con->base_info = gnome_vfs_file_info_new ();
    GnomeVFSFileInfoOptions infoOpts = (GnomeVFSFileInfoOptions) (GNOME_VFS_FILE_INFO_FOLLOW_LINKS | GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);
    GnomeVFSResult result = gnome_vfs_get_file_info_uri (uri, con->base_info, infoOpts);

    gnome_vfs_uri_unref (uri);
    g_free (uri_str);

    if (result == GNOME_VFS_OK)
    {
        con->state = GnomeCmdCon::STATE_OPEN;
        con->open_result = GnomeCmdCon::OPEN_OK;
    }
    else
    {
        gnome_vfs_file_info_unref (con->base_info);
        con->base_info = nullptr;
        con->open_failed_reason = result;
        con->open_result = GnomeCmdCon::OPEN_FAILED;
        con->state = GnomeCmdCon::STATE_CLOSED;
    }
}


static void dev_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    DEBUG ('m', "Mounting device\n");

    if (!con->base_path)
        con->base_path = new GnomeCmdPlainPath(G_DIR_SEPARATOR_S);

    con->state = GnomeCmdCon::STATE_OPENING;
    con->open_result = GnomeCmdCon::OPEN_IN_PROGRESS;

    g_thread_new (nullptr, (GThreadFunc) do_mount_thread_func, con);
}


static void dev_vfs_umount_callback (gboolean succeeded, char *error, char *detailed_error, gpointer data)
{
    GtkWidget *msgbox;

    DEBUG('m', "VFS umount callback: %s %s %s\n", succeeded ? "succeeded" : "failed",
               error ? error : "",
               detailed_error ? detailed_error : "");

    if (succeeded)
        msgbox = gtk_message_dialog_new (*main_win,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_OK,
                                         _("Device is now safe to remove"));
    else
        msgbox = gtk_message_dialog_new (*main_win,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("Cannot unmount the volume:\n%s %s"),
                                         error ? error : _("Unknown error"),
                                         detailed_error ? detailed_error: "");

    gtk_dialog_run (GTK_DIALOG (msgbox));
    gtk_widget_destroy (msgbox);
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
        if (dev_con->priv->vfsvol)
        {
            gchar *name;
            name = gnome_vfs_volume_get_display_name (dev_con->priv->vfsvol);
            DEBUG ('m', "umounting VFS volume \"%s\"\n", name);
            g_free (name);

            gnome_vfs_volume_unmount (dev_con->priv->vfsvol, dev_vfs_umount_callback, nullptr);
        }
    }
    else
    {
        DEBUG ('m', "umounting %s\n", dev_con->priv->mountp);
        gchar *cmd = g_strdup_printf ("umount %s", dev_con->priv->mountp);
        ret = system (cmd);
        DEBUG ('m', "umount returned %d\n", ret);
        g_free (cmd);
    }

    if (ret == 0)
        con->state = GnomeCmdCon::STATE_CLOSED;

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


static GnomeVFSURI *dev_create_uri (GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), nullptr);
    g_return_val_if_fail (path!=nullptr, nullptr);

    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);

    gchar *p = g_build_filename (dev_con->priv->mountp, path->get_path(), nullptr);
    GnomeVFSURI *u1 = gnome_vfs_uri_new ("file:");
    GnomeVFSURI *u2 = gnome_vfs_uri_append_path (u1, p);
    gnome_vfs_uri_unref (u1);

    return u2;
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

    if (con->priv->vfsvol)
    {
        gnome_vfs_volume_unref (con->priv->vfsvol);
        con->priv->vfsvol = nullptr;
    }

    g_free (con->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdConDeviceClass *klass)
{
    GtkObjectClass *object_class;
    GnomeCmdConClass *con_class;

    object_class = GTK_OBJECT_CLASS (klass);
    con_class = GNOME_CMD_CON_CLASS (klass);
    parent_class = static_cast<GnomeCmdConClass*> (gtk_type_class (GNOME_CMD_TYPE_CON));

    object_class->destroy = destroy;

    con_class->open = dev_open;
    con_class->close = dev_close;
    con_class->cancel_open = dev_cancel_open;
    con_class->open_is_needed = dev_open_is_needed;
    con_class->create_uri = dev_create_uri;
    con_class->create_path = dev_create_path;
}


static void init (GnomeCmdConDevice *dev_con)
{
    GnomeCmdCon *con = GNOME_CMD_CON (dev_con);

    dev_con->priv = g_new0 (GnomeCmdConDevicePrivate, 1);

    con->method = CON_LOCAL;
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

GtkType gnome_cmd_con_device_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            (gchar *) "GnomeCmdConDevice",
            sizeof (GnomeCmdConDevice),
            sizeof (GnomeCmdConDeviceClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ nullptr,
            /* reserved_2 */ nullptr,
            (GtkClassInitFunc) nullptr
        };

        type = gtk_type_unique (GNOME_CMD_TYPE_CON, &info);
    }
    return type;
}


GnomeCmdConDevice *gnome_cmd_con_device_new (const gchar *alias, const gchar *device_fn, const gchar *mountp, const gchar *icon_path)
{
    auto dev = static_cast<GnomeCmdConDevice*> (g_object_new (GNOME_CMD_TYPE_CON_DEVICE, nullptr));
    GnomeCmdCon *con = GNOME_CMD_CON (dev);

    gnome_cmd_con_device_set_device_fn (dev, device_fn);
    gnome_cmd_con_device_set_mountp (dev, mountp);
    gnome_cmd_con_device_set_icon_path (dev, icon_path);
    gnome_cmd_con_device_set_autovol(dev, FALSE);
    gnome_cmd_con_device_set_vfs_volume(dev, nullptr);
    gnome_cmd_con_device_set_alias (dev, alias);

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
    GNOME_CMD_CON (dev)->go_text = g_strdup_printf (_("Go to: %s (%s)"), alias, dev->priv->mountp);
    GNOME_CMD_CON (dev)->open_text = g_strdup_printf (_("Mount: %s"), alias);
    GNOME_CMD_CON (dev)->close_text = g_strdup_printf (_("Unmount: %s"), alias);
}


void gnome_cmd_con_device_set_device_fn (GnomeCmdConDevice *dev, const gchar *device_fn)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);

    g_free (dev->priv->device_fn);

    dev->priv->device_fn = g_strdup (device_fn);
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


void gnome_cmd_con_device_set_vfs_volume (GnomeCmdConDevice *dev, GnomeVFSVolume *vfsvol)
{
    g_return_if_fail (dev != nullptr);
    g_return_if_fail (dev->priv != nullptr);

    if (dev->priv->vfsvol)
    {
        gnome_vfs_volume_unref (dev->priv->vfsvol);
        dev->priv->vfsvol = nullptr;
    }

    dev->priv->vfsvol = vfsvol;
    if (dev->priv->vfsvol)
        gnome_vfs_volume_ref(dev->priv->vfsvol);
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

    return dev->priv->device_fn;
}


const gchar *gnome_cmd_con_device_get_mountp (GnomeCmdConDevice *dev)
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


GnomeVFSVolume *gnome_cmd_con_device_get_vfs_volume (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != nullptr, nullptr);
    g_return_val_if_fail (dev->priv != nullptr, nullptr);

    return dev->priv->vfsvol;
}
