/** 
 * @file gnome-cmd-con-device.h
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

#ifndef __GNOME_CMD_CON_DEVICE_H__
#define __GNOME_CMD_CON_DEVICE_H__

#include "gnome-cmd-con.h"

#define GNOME_CMD_TYPE_CON_DEVICE              (gnome_cmd_con_device_get_type ())
#define GNOME_CMD_CON_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CON_DEVICE, GnomeCmdConDevice))
#define GNOME_CMD_CON_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CON_DEVICE, GnomeCmdConDeviceClass))
#define GNOME_CMD_IS_CON_DEVICE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CON_DEVICE))
#define GNOME_CMD_IS_CON_DEVICE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CON_DEVICE))
#define GNOME_CMD_CON_DEVICE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CON_DEVICE, GnomeCmdConDeviceClass))


struct GnomeCmdConDevicePrivate;


struct GnomeCmdConDevice
{
    GnomeCmdCon parent;

    GnomeCmdConDevicePrivate *priv;
};

struct GnomeCmdConDeviceClass
{
    GnomeCmdConClass parent_class;
};


GtkType gnome_cmd_con_device_get_type ();

GnomeCmdConDevice *gnome_cmd_con_device_new (const gchar *alias, const gchar *device_fn, const gchar *mountp, const gchar *icon_path);

void gnome_cmd_con_device_free (GnomeCmdConDevice *dev);

const gchar *gnome_cmd_con_device_get_alias (GnomeCmdConDevice *dev);
void gnome_cmd_con_device_set_alias (GnomeCmdConDevice *dev, const gchar *alias);

const gchar *gnome_cmd_con_device_get_device_fn (GnomeCmdConDevice *dev);
void gnome_cmd_con_device_set_device_fn (GnomeCmdConDevice *dev, const gchar *device_fn);

const gchar *gnome_cmd_con_device_get_mountp (GnomeCmdConDevice *dev);
void gnome_cmd_con_device_set_mountp (GnomeCmdConDevice *dev, const gchar *mountp);

const gchar *gnome_cmd_con_device_get_icon_path (GnomeCmdConDevice *dev);
void gnome_cmd_con_device_set_icon_path (GnomeCmdConDevice *dev, const gchar *icon_path);

gboolean gnome_cmd_con_device_get_autovol (GnomeCmdConDevice *dev);
void gnome_cmd_con_device_set_autovol (GnomeCmdConDevice *dev, const gboolean autovol);

GnomeVFSVolume *gnome_cmd_con_device_get_vfs_volume (GnomeCmdConDevice *dev);
void gnome_cmd_con_device_set_vfs_volume (GnomeCmdConDevice *dev, GnomeVFSVolume *vfsvol);

#endif // __GNOME_CMD_CON_DEVICE_H__
