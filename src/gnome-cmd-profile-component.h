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
#ifndef __GNOME_CMD_PROFILE_COMPONENT_H__
#define __GNOME_CMD_PROFILE_COMPONENT_H__

#include "gnome-cmd-data.h"

#define GNOME_CMD_TYPE_PROFILE_COMPONENT          (gnome_cmd_profile_component_get_type())
#define GNOME_CMD_PROFILE_COMPONENT(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_PROFILE_COMPONENT, GnomeCmdProfileComponent))
#define GNOME_CMD_IS_PROFILE_COMPONENT(obj)       (G_TYPE_INSTANCE_CHECK_TYPE ((obj), GNOME_CMD_TYPE_PROFILE_COMPONENT)


GType gnome_cmd_profile_component_get_type ();


struct GnomeCmdProfileComponent
{
    GtkVBox parent;

    class Private;

    Private *priv;

    operator GtkWidget * ()             {  return GTK_WIDGET (this);  }

    void *operator new (size_t size)    {  return g_object_new (GNOME_CMD_TYPE_PROFILE_COMPONENT, NULL);  }
    void operator delete (void *p)      {  g_object_unref (p);  }

    enum {COL_REGEX, COL_MALFORMED_REGEX, COL_PATTERN, COL_REPLACE, COL_MATCH_CASE, NUM_REGEX_COLS};

    GnomeCmdData::AdvrenameConfig::Profile &profile;

    GnomeCmdProfileComponent(GnomeCmdData::AdvrenameConfig::Profile &profile);
    ~GnomeCmdProfileComponent()     {}

    void update();
    void copy();                                                    //  copies component to associated profile
    void copy(GnomeCmdData::AdvrenameConfig::Profile &profile);     //  copies component to specified profile

    gchar *convert_case(gchar *string);
    gchar *trim_blanks(gchar *string);

    const gchar *get_template_entry() const;
    void set_template_history(GList *history);
    GtkTreeModel *get_regex_model() const;
};

#endif // __GNOME_CMD_PROFILE_COMPONENT_H__
