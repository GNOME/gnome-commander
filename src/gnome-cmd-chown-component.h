/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2015 Uwe Scholz

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

#ifndef __GNOME_CMD_CHOWN_COMPONENT_H__
#define __GNOME_CMD_CHOWN_COMPONENT_H__

#include "gnome-cmd-file.h"

#define GNOME_CMD_TYPE_CHOWN_COMPONENT              (gnome_cmd_chown_component_get_type ())
#define GNOME_CMD_CHOWN_COMPONENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CHOWN_COMPONENT, GnomeCmdChownComponent))
#define GNOME_CMD_CHOWN_COMPONENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CHOWN_COMPONENT, GnomeCmdChownComponentClass))
#define GNOME_CMD_IS_CHOWN_COMPONENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CHOWN_COMPONENT))
#define GNOME_CMD_IS_CHOWN_COMPONENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CHOWN_COMPONENT))
#define GNOME_CMD_CHOWN_COMPONENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CHOWN_COMPONENT, GnomeCmdChownComponentClass))


struct GnomeCmdChownComponentPrivate;


struct GnomeCmdChownComponent
{
    GtkTable parent;

    GnomeCmdChownComponentPrivate *priv;
};


struct GnomeCmdChownComponentClass
{
    GtkTableClass parent_class;
};


GtkWidget *gnome_cmd_chown_component_new ();
GtkType gnome_cmd_chown_component_get_type ();

void gnome_cmd_chown_component_set (GnomeCmdChownComponent *comp, uid_t owner, gid_t group);
uid_t gnome_cmd_chown_component_get_owner (GnomeCmdChownComponent *component);
gid_t gnome_cmd_chown_component_get_group (GnomeCmdChownComponent *component);

#endif // __GNOME_CMD_CHOWN_COMPONENT_H__
