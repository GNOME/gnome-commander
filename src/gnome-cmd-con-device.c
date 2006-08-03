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

#include <sys/types.h>
#include <sys/wait.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-volume.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-con-device.h"
#include "gnome-cmd-plain-path.h"
#include "imageloader.h"
#include "utils.h"

#include "gnome-cmd-main-win.h"


struct _GnomeCmdConDevicePrivate
{
    gchar *alias;
    gchar *device_fn;
    gchar *mountp;
    gchar *icon_path;
    gboolean autovolume;
    GnomeVFSVolume *vfsvol;
};


static GnomeCmdConClass *parent_class = NULL;



static gboolean
is_mounted (GnomeCmdCon *con)
{
    gchar *s;
    FILE *fd;
    gchar tmp[256];
    gboolean ret = FALSE;
    GnomeCmdConDevice *dev_con;

    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), FALSE);

    dev_con = GNOME_CMD_CON_DEVICE (con);

    fd = fopen ("/etc/mtab", "r");
    if (!fd) return FALSE;

    while ((s = fgets (tmp, sizeof(tmp), fd)))
    {
        char **v = g_strsplit (s, " ", 3);
        if (v[1] && strcmp (v[1], dev_con->priv->mountp) == 0)
            ret = TRUE;

        g_strfreev (v);

        if (ret) break;
    }

    return ret;
}


static void
do_mount_thread_func (GnomeCmdCon *con)
{
    gint ret,estatus;
    gchar *cmd, *uri_str;
    GnomeVFSURI *uri;
    GnomeVFSResult result;
    GnomeCmdConDevice *dev_con = GNOME_CMD_CON_DEVICE (con);
    GnomeVFSFileInfoOptions infoOpts = GNOME_VFS_FILE_INFO_FOLLOW_LINKS
        | GNOME_VFS_FILE_INFO_GET_MIME_TYPE
        | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE;

    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    if (!is_mounted (con)) {
        gchar *emsg = NULL;

        DEBUG ('m', "mounting %s\n", dev_con->priv->mountp);
        if (dev_con->priv->device_fn)
            cmd = g_strdup_printf (
                "mount %s %s", dev_con->priv->device_fn, dev_con->priv->mountp);
        else
            cmd = g_strdup_printf ("mount %s", dev_con->priv->mountp);
        DEBUG ('m', "Mount command: %s\n", cmd);
        ret = system (cmd);
        estatus = WEXITSTATUS (ret);
        g_free (cmd);
        DEBUG ('m', "mount returned %d and had the exitstatus %d\n", ret, estatus);

        if (ret == -1)
            emsg = g_strdup_printf (_("Failed to execute the mount command"));
        else {
            switch (estatus) {
                case 0:
                    emsg = NULL;
                    break;
                case 1:
                    emsg = g_strdup (_("Mount failed: Permission denied"));
                    break;
                case 32:
                    emsg = g_strdup (_("Mount failed: No medium found"));
                    break;
                default:
                    emsg = g_strdup_printf (
                        _("Mount failed: mount exited with existatus %d"),
                        estatus);
                    break;
            }
        }

        if (emsg != NULL) {
            con->open_result = CON_OPEN_FAILED;
            con->state = CON_STATE_CLOSED;
            con->open_failed_msg = emsg;
            return;
        }
    }
    else
        DEBUG('m', "The device was already mounted\n");

    uri = gnome_cmd_con_create_uri (con, con->base_path);
    if (!uri) return;

    uri_str = gnome_vfs_uri_to_string (uri, 0);
    con->base_info = gnome_vfs_file_info_new ();
    result = gnome_vfs_get_file_info_uri (
        uri, con->base_info, infoOpts);

    gnome_vfs_uri_unref (uri);
    g_free (uri_str);

    if (result == GNOME_VFS_OK) {
        con->state = CON_STATE_OPEN;
        con->open_result = CON_OPEN_OK;
    }
    else {
        gnome_vfs_file_info_unref (con->base_info);
        con->base_info = NULL;
        con->open_failed_reason = result;
        con->open_result = CON_OPEN_FAILED;
        con->state = CON_STATE_CLOSED;
    }
}


static void
dev_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));

    DEBUG ('m', "Mounting device\n");

    if (!con->base_path) {
        con->base_path = gnome_cmd_plain_path_new ("/");
        gtk_object_ref (GTK_OBJECT (con->base_path));
    }

    con->state = CON_STATE_OPENING;
    con->open_result = CON_OPEN_IN_PROGRESS;

    g_thread_create ((GThreadFunc)do_mount_thread_func, con, FALSE, NULL);
}


static void  dev_vfs_umount_callback    (gboolean succeeded,
                                             char *error,
                                             char *detailed_error,
                                             gpointer data)
{
    GtkWidget *msgbox;

    DEBUG('m',"VFS Umount Callback: %s %s %s\n", succeeded ? "Succeeded" : "Failed",
            error ? error : "",
            detailed_error ? detailed_error : "");

    if (succeeded) {
            msgbox = gtk_message_dialog_new(GTK_WINDOW(main_win),
                            GTK_DIALOG_MODAL,
                            GTK_MESSAGE_INFO,
                            GTK_BUTTONS_OK,
                            _("Volume unmounting succeeded.\nIt is safe to eject the media."));
    } else {
            msgbox = gtk_message_dialog_new(GTK_WINDOW(main_win),
                            GTK_DIALOG_MODAL,
                            GTK_MESSAGE_ERROR,
                            GTK_BUTTONS_OK,
                            _("Volume unmounting failed:\n%s %s"),
                            error ? error : _("Unknown Error"),
                            detailed_error ? detailed_error: ""
                            );
    }
    gtk_dialog_run (GTK_DIALOG (msgbox));
    gtk_widget_destroy (msgbox);
}

static gboolean
dev_close (GnomeCmdCon *con)
{
    gint ret;
    gchar *cmd;
    GnomeCmdConDevice *dev_con;

    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), FALSE);

    dev_con = GNOME_CMD_CON_DEVICE (con);

    gnome_cmd_con_set_default_dir (con, NULL);
    gnome_cmd_con_set_cwd (con, NULL);

    chdir (g_get_home_dir ());

    if (dev_con->priv->autovolume) {
        if (dev_con->priv->vfsvol) {
            gchar *name;
            name = gnome_vfs_volume_get_display_name (dev_con->priv->vfsvol);
            DEBUG ('m', "umounting VFS volume \"%s\"\n", name);
            g_free(name);

            gnome_vfs_volume_unmount(dev_con->priv->vfsvol,dev_vfs_umount_callback,NULL);
        }
        ret = 0;
    }
    else {
        DEBUG ('m', "umounting %s\n", dev_con->priv->mountp);
        cmd = g_strdup_printf ("umount %s", dev_con->priv->mountp);
        ret = system (cmd);
        DEBUG ('m', "umount returned %d\n", ret);
        g_free (cmd);
    }

    if (ret == 0) {
        con->state = CON_STATE_CLOSED;
    }

    return (ret == 0);
}


static void
dev_cancel_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_DEVICE (con));
}


static gboolean
dev_open_is_needed (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), FALSE);

    return TRUE;
}


static GnomeVFSURI *
dev_create_uri (GnomeCmdCon *con, GnomeCmdPath *path)
{
    gchar *p;
    const gchar *path_str;
    GnomeVFSURI *u1, *u2;
    GnomeCmdConDevice *dev_con;

    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), NULL);
    g_return_val_if_fail (GNOME_CMD_IS_PATH (path), NULL);

    dev_con = GNOME_CMD_CON_DEVICE (con);

    path_str = gnome_cmd_path_get_path (path);
    p = g_build_path ("/", dev_con->priv->mountp, path_str, NULL);
    u1 = gnome_vfs_uri_new ("file:");
    u2 = gnome_vfs_uri_append_path (u1, p);
    gnome_vfs_uri_unref (u1);

    return u2;
}


static GnomeCmdPath *
dev_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_DEVICE (con), NULL);
    g_return_val_if_fail (path_str != NULL, NULL);

    return gnome_cmd_plain_path_new (path_str);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdConDevice *con = GNOME_CMD_CON_DEVICE (object);

    if (con->priv->vfsvol) {
    gnome_vfs_volume_unref(con->priv->vfsvol);
    con->priv->vfsvol = NULL ;
    }
    
    g_free (con->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdConDeviceClass *klass)
{
    GtkObjectClass *object_class;
    GnomeCmdConClass *con_class;

    object_class = GTK_OBJECT_CLASS (klass);
    con_class = GNOME_CMD_CON_CLASS (klass);
    parent_class = gtk_type_class (gnome_cmd_con_get_type ());

    object_class->destroy = destroy;

    con_class->open = dev_open;
    con_class->close = dev_close;
    con_class->cancel_open = dev_cancel_open;
    con_class->open_is_needed = dev_open_is_needed;
    con_class->create_uri = dev_create_uri;
    con_class->create_path = dev_create_path;
}


static void
init (GnomeCmdConDevice *dev_con)
{
    GnomeCmdCon *con = GNOME_CMD_CON (dev_con);

    dev_con->priv = g_new0 (GnomeCmdConDevicePrivate, 1);

    con->should_remember_dir = TRUE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = TRUE;
    con->is_local = TRUE;
    con->is_closeable = TRUE;
    con->go_pixmap = NULL;
    con->open_pixmap = NULL;
    con->close_pixmap = NULL;
}


/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_con_device_get_type         (void)
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdConDevice",
            sizeof (GnomeCmdConDevice),
            sizeof (GnomeCmdConDeviceClass),
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


GnomeCmdConDevice*
gnome_cmd_con_device_new (const gchar *alias,
                          const gchar *device_fn,
                          const gchar *mountp,
                          const gchar *icon_path)
{
    GnomeCmdConDevice *dev = gtk_type_new (gnome_cmd_con_device_get_type ());

    gnome_cmd_con_device_set_device_fn (dev, device_fn);
    gnome_cmd_con_device_set_mountp (dev, mountp);
    gnome_cmd_con_device_set_icon_path (dev, icon_path);
    gnome_cmd_con_device_set_autovol(dev, FALSE);
    gnome_cmd_con_device_set_vfs_volume(dev, NULL);
    gnome_cmd_con_device_set_alias (dev, alias);


    GNOME_CMD_CON (dev)->open_msg = g_strdup_printf (_("Mounting %s"), alias);

    return dev;
}


void
gnome_cmd_con_device_set_alias (GnomeCmdConDevice *dev, const gchar *alias)
{
    g_return_if_fail (dev != NULL);
    g_return_if_fail (dev->priv != NULL);
    g_return_if_fail (alias != NULL);

    if (dev->priv->alias)
        g_free (dev->priv->alias);

    dev->priv->alias = g_strdup (alias);
    GNOME_CMD_CON (dev)->alias = g_strdup (alias);
    GNOME_CMD_CON (dev)->go_text = g_strdup_printf (_("Go to: %s (%s)"), alias,dev->priv->mountp);
    GNOME_CMD_CON (dev)->open_text = g_strdup_printf (_("Mount: %s"), alias);
    GNOME_CMD_CON (dev)->close_text = g_strdup_printf (_("Unmount: %s"), alias);
}


void
gnome_cmd_con_device_set_device_fn (GnomeCmdConDevice *dev, const gchar *device_fn)
{
    g_return_if_fail (dev != NULL);
    g_return_if_fail (dev->priv != NULL);

    if (dev->priv->device_fn)
        g_free (dev->priv->device_fn);

    if (!device_fn)
        dev->priv->device_fn = NULL;
    else
        dev->priv->device_fn = g_strdup (device_fn);
}


void
gnome_cmd_con_device_set_mountp (GnomeCmdConDevice *dev, const gchar *mountp)
{
    g_return_if_fail (dev != NULL);
    g_return_if_fail (dev->priv != NULL);
    if (!mountp) return;

    if (dev->priv->mountp)
        g_free (dev->priv->mountp);

    dev->priv->mountp = g_strdup (mountp);
}


void
gnome_cmd_con_device_set_icon_path (GnomeCmdConDevice *dev, const gchar *icon_path)
{
    GnomeCmdCon *con;

    g_return_if_fail (dev != NULL);
    g_return_if_fail (dev->priv != NULL);

    if (dev->priv->icon_path)
        g_free (dev->priv->icon_path);

    con = GNOME_CMD_CON (dev);
    if (con->go_pixmap)
        gnome_cmd_pixmap_free (con->go_pixmap);
    if (con->open_pixmap)
        gnome_cmd_pixmap_free (con->open_pixmap);
    if (con->close_pixmap) {
        gnome_cmd_pixmap_free (con->close_pixmap);
        con->close_pixmap = NULL;
    }

    if (!icon_path)
        dev->priv->icon_path = NULL;
    else {
        dev->priv->icon_path = g_strdup (icon_path);

        con->go_pixmap = gnome_cmd_pixmap_new_from_file (icon_path);
        con->open_pixmap = gnome_cmd_pixmap_new_from_file (icon_path);
        if (con->open_pixmap) {
            GdkPixbuf *tmp = IMAGE_get_pixbuf(PIXMAP_OVERLAY_UMOUNT);
            if (tmp) {
                int w, h;
                GdkPixbuf *overlay = gdk_pixbuf_copy (con->open_pixmap->pixbuf);

                w = gdk_pixbuf_get_width (tmp);
                h = gdk_pixbuf_get_height (tmp);
                if (w > 14) w=14;
                if (h > 14) h=14;

                gdk_pixbuf_copy_area (tmp, 0, 0, w, h, overlay, 0, 0);
                con->close_pixmap = gnome_cmd_pixmap_new_from_pixbuf (overlay);
            }
        }
    }
}


void
gnome_cmd_con_device_set_autovol (GnomeCmdConDevice *dev,    const gboolean autovol )
{
    g_return_if_fail (dev != NULL);
    g_return_if_fail (dev->priv != NULL);

    dev->priv->autovolume = autovol;
}


void
gnome_cmd_con_device_set_vfs_volume    (GnomeCmdConDevice *dev,
                                          GnomeVFSVolume *vfsvol )
{
    g_return_if_fail (dev != NULL);
    g_return_if_fail (dev->priv != NULL);

    if (dev->priv->vfsvol) {
        gnome_vfs_volume_unref(dev->priv->vfsvol);
        dev->priv->vfsvol = NULL;
    }

    dev->priv->vfsvol = vfsvol;
    if (dev->priv->vfsvol)
        gnome_vfs_volume_ref(dev->priv->vfsvol);
}


const gchar *
gnome_cmd_con_device_get_alias (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != NULL, NULL);
    g_return_val_if_fail (dev->priv != NULL, NULL);

    return dev->priv->alias;
}


const gchar *
gnome_cmd_con_device_get_device_fn (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != NULL, NULL);
    g_return_val_if_fail (dev->priv != NULL, NULL);

    return dev->priv->device_fn;
}


const gchar *
gnome_cmd_con_device_get_mountp (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != NULL, NULL);
    g_return_val_if_fail (dev->priv != NULL, NULL);

    return dev->priv->mountp;
}


const gchar *
gnome_cmd_con_device_get_icon_path (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != NULL, NULL);
    g_return_val_if_fail (dev->priv != NULL, NULL);

    return dev->priv->icon_path;
}


gboolean
gnome_cmd_con_device_get_autovol (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != NULL, FALSE);
    g_return_val_if_fail (dev->priv != NULL, FALSE);

    return dev->priv->autovolume;
}


GnomeVFSVolume *
gnome_cmd_con_device_get_vfs_volume    (GnomeCmdConDevice *dev)
{
    g_return_val_if_fail (dev != NULL, NULL);
    g_return_val_if_fail (dev->priv != NULL, NULL);

    return dev->priv->vfsvol;
}
