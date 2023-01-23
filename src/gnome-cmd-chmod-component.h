/** 
 * @file gnome-cmd-chmod-component.h
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

#pragma once

#include "gnome-cmd-file.h"

#define GNOME_CMD_TYPE_CHMOD_COMPONENT              (gnome_cmd_chmod_component_get_type ())
#define GNOME_CMD_CHMOD_COMPONENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CHMOD_COMPONENT, GnomeCmdChmodComponent))
#define GNOME_CMD_CHMOD_COMPONENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CHMOD_COMPONENT, GnomeCmdChmodComponentClass))
#define GNOME_CMD_IS_CHMOD_COMPONENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CHMOD_COMPONENT))
#define GNOME_CMD_IS_CHMOD_COMPONENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CHMOD_COMPONENT))
#define GNOME_CMD_CHMOD_COMPONENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CHMOD_COMPONENT, GnomeCmdChmodComponentClass))


struct GnomeCmdChmodComponentPrivate;


struct GnomeCmdChmodComponent
{
    GtkVBox parent;
    GnomeCmdChmodComponentPrivate *priv;
};


struct GnomeCmdChmodComponentClass
{
    GtkVBoxClass parent_class;

    void (* perms_changed)      (GnomeCmdChmodComponent *component);
};


GtkWidget *gnome_cmd_chmod_component_new (guint32 perms);

GtkType gnome_cmd_chmod_component_get_type ();

guint32 gnome_cmd_chmod_component_get_perms (GnomeCmdChmodComponent *component);
void gnome_cmd_chmod_component_set_perms (GnomeCmdChmodComponent *component, guint32 permissions);
