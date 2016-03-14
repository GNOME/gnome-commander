/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2016 Uwe Scholz

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
#ifndef __GNOME_CMD_SELECTION_PROFILE_COMPONENT_H__
#define __GNOME_CMD_SELECTION_PROFILE_COMPONENT_H__

#include "gnome-cmd-data.h"

#define GNOME_CMD_TYPE_SELECTION_PROFILE_COMPONENT              (gnome_cmd_selection_profile_component_get_type ())
#define GNOME_CMD_SELECTION_PROFILE_COMPONENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_SELECTION_PROFILE_COMPONENT, GnomeCmdSelectionProfileComponent))
#define GNOME_CMD_SELECTION_PROFILE_COMPONENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_SELECTION_PROFILE_COMPONENT, GnomeCmdSelectionProfileComponentClass))
#define GNOME_CMD_IS_SELECTION_PROFILE_COMPONENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_SELECTION_PROFILE_COMPONENT))
#define GNOME_CMD_IS_SELECTION_PROFILE_COMPONENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_SELECTION_PROFILE_COMPONENT))
#define GNOME_CMD_SELECTION_PROFILE_COMPONENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_SELECTION_PROFILE_COMPONENT, GnomeCmdSelectionProfileComponentClass))


GType gnome_cmd_selection_profile_component_get_type ();


struct GnomeCmdSelectionProfileComponent
{
    GtkVBox parent;

    class Private;

    Private *priv;

    operator GtkWidget * () const       {  return GTK_WIDGET (this);  }

    void *operator new (size_t size)    {  return g_object_new (GNOME_CMD_TYPE_SELECTION_PROFILE_COMPONENT, NULL);  }
    void operator delete (void *p)      {  g_object_unref (p);  }

    GnomeCmdData::Selection &profile;

    GnomeCmdSelectionProfileComponent(GnomeCmdData::Selection &profile, GtkWidget *widget=NULL, gchar *label=NULL);
    ~GnomeCmdSelectionProfileComponent()    {}

    void update();
    void copy();                                        //  copies component to associated profile
    void copy(GnomeCmdData::Selection &profile);        //  copies component to specified profile
    void set_focus();

    void set_name_patterns_history(GList *history);
    void set_content_patterns_history(GList *history);

    void set_default_activation(GtkWindow *w);
};

#endif // __GNOME_CMD_SELECTION_PROFILE_COMPONENT_H__
