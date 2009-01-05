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

#include <config.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-chown-component.h"
#include "owner.h"

using namespace std;


struct _GnomeCmdChownComponentPrivate
{
    GtkWidget *user_combo, *group_combo;
    GList *user_strings, *group_strings;
};


static GtkTableClass *parent_class = NULL;


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdChownComponent *comp = GNOME_CMD_CHOWN_COMPONENT (object);

    g_free (comp->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdChownComponentClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkTableClass *) gtk_type_class (gtk_table_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void
init (GnomeCmdChownComponent *comp)
{
    GtkWidget *label;

    comp->priv = g_new0 (GnomeCmdChownComponentPrivate, 1);

    gtk_table_resize (GTK_TABLE (comp), 2, 2);
    gtk_table_set_row_spacings (GTK_TABLE (comp), 6);
    gtk_table_set_col_spacings (GTK_TABLE (comp), 6);

    label = create_label (GTK_WIDGET (comp), _("Owner:"));
    table_add (GTK_WIDGET (comp), label, 0, 0, GTK_FILL);

    label = create_label (GTK_WIDGET (comp), _("Group:"));
    table_add (GTK_WIDGET (comp), label, 0, 1, GTK_FILL);

    comp->priv->user_combo = create_combo (GTK_WIDGET (comp));
    table_add (GTK_WIDGET (comp), comp->priv->user_combo, 1, 0, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    comp->priv->group_combo = create_combo (GTK_WIDGET (comp));
    table_add (GTK_WIDGET (comp), comp->priv->group_combo, 1, 1, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));
}


/***********************************
 * Public functions
 ***********************************/

static void load_users_and_groups (GnomeCmdChownComponent *comp)
{
    user_t *prog_user = OWNER_get_program_user ();

    g_return_if_fail (prog_user != NULL);

    // disable user combo if user is not root, else fill the combo with all users in the system
    if (prog_user->uid != 0)
        gtk_widget_set_sensitive (comp->priv->user_combo, FALSE);
    else
    {
        for (GList *tmp = OWNER_get_all_users (); tmp; tmp = tmp->next)
        {
            user_t *user = (user_t *) tmp->data;

            comp->priv->user_strings = g_list_append (comp->priv->user_strings, g_strdup (user->name));
        }

        gtk_combo_set_popdown_strings (GTK_COMBO (comp->priv->user_combo), comp->priv->user_strings);
    }

    // fill the groups combo with all groups that the user is part of if ordinary user or all groups if root

    for (GList *tmp = prog_user->uid != 0 ? prog_user->groups : OWNER_get_all_groups (); tmp; tmp = tmp->next)
    {
        group_t *group = (group_t *) tmp->data;
        comp->priv->group_strings = g_list_append (comp->priv->group_strings, g_strdup (group->name));
    }

    gtk_combo_set_popdown_strings (GTK_COMBO (comp->priv->group_combo), comp->priv->group_strings);
}


GtkWidget*
gnome_cmd_chown_component_new ()
{
    GnomeCmdChownComponent *comp = (GnomeCmdChownComponent *) gtk_type_new (gnome_cmd_chown_component_get_type ());

    load_users_and_groups (comp);

    return GTK_WIDGET (comp);
}



GtkType
gnome_cmd_chown_component_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdChownComponent",
            sizeof (GnomeCmdChownComponent),
            sizeof (GnomeCmdChownComponentClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_table_get_type (), &info);
    }
    return type;
}


void
gnome_cmd_chown_component_set (GnomeCmdChownComponent *comp, uid_t owner, gid_t group)
{
    const gchar *s_owner = OWNER_get_name_by_uid (owner);
    const gchar *s_group = OWNER_get_name_by_gid (group);

    gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (comp->priv->user_combo)->entry), s_owner);
    gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (comp->priv->group_combo)->entry), s_group);
}


uid_t
gnome_cmd_chown_component_get_owner (GnomeCmdChownComponent *component)
{
    const gchar *s_owner = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (component->priv->user_combo)->entry));
    uid_t owner = OWNER_get_uid_by_name (s_owner);

    return owner;
}


gid_t
gnome_cmd_chown_component_get_group (GnomeCmdChownComponent *component)
{
    const gchar *s_group = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (component->priv->group_combo)->entry));
    gid_t group = OWNER_get_gid_by_name (s_group);

    return group;
}
