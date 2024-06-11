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

    priv->user_combo = gtk_combo_box_text_new ();
    gtk_widget_set_hexpand (priv->user_combo, TRUE);
    gtk_grid_attach (GTK_GRID (comp), priv->user_combo, 1, 0, 1, 1);

    priv->group_combo = gtk_combo_box_text_new ();
    gtk_widget_set_hexpand (priv->group_combo, TRUE);
    gtk_grid_attach (GTK_GRID (comp), priv->group_combo, 1, 1, 1, 1);
}


/***********************************
 * Public functions
 ***********************************/

inline void load_users_and_groups (GnomeCmdChownComponent *comp)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (comp));

    GtkListStore *users_model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_LONG);
    for (auto list = gcmd_owner.users.get_names(); list; list = list->next)
    {
        const char *name = (const char *) list->data;
        uid_t uid = gcmd_owner.users[name];

        GtkTreeIter iter;
        gtk_list_store_append (users_model, &iter);
        gtk_list_store_set (users_model, &iter, 0, name, 1, (glong) uid, -1);
    }
    gtk_combo_box_set_model (GTK_COMBO_BOX (priv->user_combo), GTK_TREE_MODEL (users_model));

    // disable user combo if user is not root, else fill the combo with all users in the system
    if (!gcmd_owner.is_root())
        gtk_widget_set_sensitive (priv->user_combo, FALSE);

    GtkListStore *groups_model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_LONG);
    gtk_combo_box_set_model (GTK_COMBO_BOX (priv->group_combo), GTK_TREE_MODEL (groups_model));

    if (gcmd_owner.get_group_names())
        for (auto list = gcmd_owner.get_group_names(); list; list = list->next)                                     // fill the groups combo with all groups that the user is part of
        {                                                                                                           // if ordinary user or all groups if root
            const char *name = (const char *) list->data;
            gid_t gid = gcmd_owner.groups[name];

            GtkTreeIter iter;
            gtk_list_store_append (groups_model, &iter);
            gtk_list_store_set (groups_model, &iter, 0, name, 1, (glong) gid, -1);
        }
    else
        gtk_widget_set_sensitive (priv->group_combo, FALSE);                                                        // disable group combo if yet not loaded
}


GtkWidget *gnome_cmd_chown_component_new ()
{
    auto comp = static_cast<GnomeCmdChownComponent*> (g_object_new (GNOME_CMD_TYPE_CHOWN_COMPONENT, nullptr));

    load_users_and_groups (comp);

    return GTK_WIDGET (comp);
}


static bool find_iter (GtkTreeModel *model, glong id, GtkTreeIter *iter)
{
    if (gtk_tree_model_get_iter_first (model, iter))
    {
        do
        {
            glong item_id;
            gtk_tree_model_get (model, iter, 1, &item_id, -1);

            if (item_id == id)
                return true;
        }
        while (gtk_tree_model_iter_next (model, iter));
    }
    return false;
}


void gnome_cmd_chown_component_set (GnomeCmdChownComponent *comp, uid_t uid, gid_t gid)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (comp));

    GtkTreeIter iter;

    auto user_combo_model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->user_combo));
    if (!find_iter (user_combo_model, uid, &iter))
    {
        gtk_list_store_append (GTK_LIST_STORE (user_combo_model), &iter);
        gchar *uidString = g_strdup_printf("%u", uid);
        gtk_list_store_set (GTK_LIST_STORE (user_combo_model), &iter, 0, uidString, 1, (glong) uid, -1);
        g_free(uidString);
    }
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->user_combo), &iter);

    auto group_combo_model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->group_combo));
    if (!find_iter (group_combo_model, gid, &iter))
    {
        gtk_list_store_append (GTK_LIST_STORE (group_combo_model), &iter);
        gchar *gidString = g_strdup_printf("%u", gid);
        gtk_list_store_set (GTK_LIST_STORE (group_combo_model), &iter, 0, gidString, 1, (glong) gid, -1);
        g_free(gidString);
    }
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->group_combo), &iter);
}


uid_t gnome_cmd_chown_component_get_owner (GnomeCmdChownComponent *component)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (component));

    GtkTreeIter iter;
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->user_combo), &iter))
        return -1;

    auto model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->user_combo));
    glong uid;
    gtk_tree_model_get (model, &iter, 1, &uid, -1);

    return uid;
}


gid_t gnome_cmd_chown_component_get_group (GnomeCmdChownComponent *component)
{
    auto priv = static_cast<GnomeCmdChownComponentPrivate*>(gnome_cmd_chown_component_get_instance_private (component));

    GtkTreeIter iter;
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->group_combo), &iter))
        return -1;

    auto model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->group_combo));
    glong gid;
    gtk_tree_model_get (model, &iter, 1, &gid, -1);

    return gid;
}
