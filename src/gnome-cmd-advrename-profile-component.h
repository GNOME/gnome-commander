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
#ifndef __GNOME_CMD_ADVRENAME_PROFILE_COMPONENT_H__
#define __GNOME_CMD_ADVRENAME_PROFILE_COMPONENT_H__

#include "gnome-cmd-data.h"

#define GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT              (gnome_cmd_advrename_profile_component_get_type ())
#define GNOME_CMD_ADVRENAME_PROFILE_COMPONENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT, GnomeCmdAdvrenameProfileComponent))
#define GNOME_CMD_ADVRENAME_PROFILE_COMPONENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT, GnomeCmdAdvrenameProfileComponentClass))
#define GNOME_CMD_IS_ADVRENAME_PROFILE_COMPONENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT))
#define GNOME_CMD_IS_ADVRENAME_PROFILE_COMPONENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT))
#define GNOME_CMD_ADVRENAME_PROFILE_COMPONENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT, GnomeCmdAdvrenameProfileComponentClass))


GType gnome_cmd_advrename_profile_component_get_type ();


struct GnomeCmdAdvrenameProfileComponent
{
    GtkVBox parent;

    class Private;

    Private *priv;

    operator GtkWidget * () const       {  return GTK_WIDGET (this);  }

    void *operator new (size_t size)    {  return g_object_new (GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT, NULL);  }
    void operator delete (void *p)      {  g_object_unref (p);  }

    enum {COL_REGEX, COL_MALFORMED_REGEX, COL_PATTERN, COL_REPLACE, COL_MATCH_CASE, NUM_REGEX_COLS};

    GnomeCmdData::AdvrenameConfig::Profile &profile;

    GnomeCmdAdvrenameProfileComponent(GnomeCmdData::AdvrenameConfig::Profile &profile);
    ~GnomeCmdAdvrenameProfileComponent()     {}

    void update();
    void copy();                                                    //  copies component to associated profile
    void copy(GnomeCmdData::AdvrenameConfig::Profile &profile);     //  copies component to specified profile

    gchar *convert_case(gchar *string);
    gchar *trim_blanks(gchar *string);

    const gchar *get_template_entry() const;
    void set_template_history(GList *history);
    void set_sample_fname(const gchar *fname);
    GtkTreeModel *get_regex_model() const;
};

#endif // __GNOME_CMD_ADVRENAME_PROFILE_COMPONENT_H__
