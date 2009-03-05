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

#ifndef __GNOME_CMD_CON_DEVICE_H__
#define __GNOME_CMD_CON_DEVICE_H__

#include "gnome-cmd-con.h"

#define GNOME_CMD_CON_DEVICE(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_con_device_get_type (), GnomeCmdConDevice)
#define GNOME_CMD_CON_DEVICE_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_con_device_get_type (), GnomeCmdConDeviceClass)
#define GNOME_CMD_IS_CON_DEVICE(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_con_device_get_type ())


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
