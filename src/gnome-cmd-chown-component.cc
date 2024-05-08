/**
 * @file gnome-cmd-chown-component.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdChownComponent, gnome_cmd_chown_component, GTK_TYPE_GRID)


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_chown_component_class_init (GnomeCmdChownComponentClass *klass)
{
}


static void gnome_cmd_chown_component_init (GnomeCmdChownComponent *comp)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (comp));

    GtkWidget *label;

    gtk_grid_set_row_spacing (GTK_GRID (comp), 6);
    gtk_grid_set_column_spacing (GTK_GRID (comp), 6);

    label = create_label (GTK_WIDGET (comp), _("Owner:"));
    gtk_grid_attach (GTK_GRID (comp), label, 0, 0, 1, 1);

    label = create_label (GTK_WIDGET (comp), _("Group:"));
    gtk_grid_attach (GTK_GRID (comp), label, 0, 1, 1, 1);

    priv->user_combo = create_combo_box_text_with_entry (GTK_WIDGET (comp));
    gtk_widget_set_hexpand (priv->user_combo, TRUE);
    gtk_grid_attach (GTK_GRID (comp), priv->user_combo, 1, 0, 1, 1);

    priv->group_combo = create_combo_box_text_with_entry (GTK_WIDGET (comp));
    gtk_widget_set_hexpand (priv->group_combo, TRUE);
    gtk_grid_attach (GTK_GRID (comp), priv->group_combo, 1, 1, 1, 1);
}


/***********************************
 * Public functions
 ***********************************/

inline void load_users_and_groups (GnomeCmdChownComponent *comp)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (comp));

    // disable user combo if user is not root, else fill the combo with all users in the system
    if (gcmd_owner.is_root())
        for (auto list = gcmd_owner.users.get_names(); list; list = list->next)
            gtk_combo_box_text_append_text ((GtkComboBoxText*) priv->user_combo, (const gchar*) list->data);
    else
        gtk_widget_set_sensitive (priv->user_combo, FALSE);

    if (gcmd_owner.get_group_names())
        for (auto list = gcmd_owner.get_group_names(); list; list = list->next)                                     // fill the groups combo with all groups that the user is part of
            gtk_combo_box_text_append_text ((GtkComboBoxText*) priv->group_combo, (const gchar*) list->data);       // if ordinary user or all groups if root
    else
        gtk_widget_set_sensitive (priv->group_combo, FALSE);                                                        // disable group combo if yet not loaded
}


GtkWidget *gnome_cmd_chown_component_new ()
{
    auto comp = static_cast<GnomeCmdChownComponent*> (g_object_new (GNOME_CMD_TYPE_CHOWN_COMPONENT, nullptr));

    load_users_and_groups (comp);

    return GTK_WIDGET (comp);
}


void gnome_cmd_chown_component_set (GnomeCmdChownComponent *comp, uid_t uid, gid_t gid)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (comp));

    const gchar *uid_name = gcmd_owner.users[uid];
    const gchar *gid_name = gcmd_owner.groups[gid];

    if (uid_name)
    {
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->user_combo))), uid_name);
    }
    else
    {
        auto uidString = g_strdup_printf("%u", uid);
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->user_combo))), uidString);
        g_free(uidString);
    }

    if (gid_name)
    {
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->group_combo))), gid_name);
    }
    else
    {
        auto gidString = g_strdup_printf("%u", gid);
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->group_combo))), gidString);
        g_free(gidString);
    }
}


uid_t gnome_cmd_chown_component_get_owner (GnomeCmdChownComponent *component)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (component));

    const gchar *owner = get_combo_box_entry_text (priv->user_combo);

    return gcmd_owner.users[owner];
}


gid_t gnome_cmd_chown_component_get_group (GnomeCmdChownComponent *component)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (component));

    const gchar *group = get_combo_box_entry_text (priv->group_combo);

    return gcmd_owner.groups[group];
}
