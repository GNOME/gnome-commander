/** 
 * @file gnome-cmd-chown-component.cc
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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-chown-component.h"
#include "gnome-cmd-owner.h"

using namespace std;


struct GnomeCmdChownComponentPrivate
{
    GtkWidget *user_combo, *group_combo;
};


static GtkTableClass *parent_class = NULL;


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdChownComponent *comp = GNOME_CMD_CHOWN_COMPONENT (object);

    g_free (comp->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdChownComponentClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkTableClass *) gtk_type_class (gtk_table_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdChownComponent *comp)
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

inline void load_users_and_groups (GnomeCmdChownComponent *comp)
{
    // disable user combo if user is not root, else fill the combo with all users in the system
    if (gcmd_owner.is_root())
        gtk_combo_set_popdown_strings (GTK_COMBO (comp->priv->user_combo), gcmd_owner.users.get_names());
    else
        gtk_widget_set_sensitive (comp->priv->user_combo, FALSE);

    if (gcmd_owner.get_group_names())
        gtk_combo_set_popdown_strings (GTK_COMBO (comp->priv->group_combo), gcmd_owner.get_group_names());      // fill the groups combo with all groups that the user is part of
                                                                                                                // if ordinary user or all groups if root
    else
        gtk_widget_set_sensitive (comp->priv->group_combo, FALSE);                                              // disable group combo if yet not loaded
}


GtkWidget *gnome_cmd_chown_component_new ()
{
    auto comp = static_cast<GnomeCmdChownComponent*> (g_object_new (GNOME_CMD_TYPE_CHOWN_COMPONENT, nullptr));

    load_users_and_groups (comp);

    return GTK_WIDGET (comp);
}


GtkType gnome_cmd_chown_component_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            (gchar*) "GnomeCmdChownComponent",
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


void gnome_cmd_chown_component_set (GnomeCmdChownComponent *comp, uid_t uid, gid_t gid)
{
    const gchar *uid_name = gcmd_owner.users[uid];
    const gchar *gid_name = gcmd_owner.groups[gid];

    if (uid_name)
        gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (comp->priv->user_combo)->entry), uid_name);
    if (gid_name)
        gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (comp->priv->group_combo)->entry), gid_name);
}


uid_t gnome_cmd_chown_component_get_owner (GnomeCmdChownComponent *component)
{
    const gchar *owner = get_combo_text (component->priv->user_combo);

    return gcmd_owner.users[owner];
}


gid_t gnome_cmd_chown_component_get_group (GnomeCmdChownComponent *component)
{
    const gchar *group = get_combo_text (component->priv->group_combo);

    return gcmd_owner.groups[group];
}
