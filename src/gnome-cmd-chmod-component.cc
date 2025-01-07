/**
 * @file gnome-cmd-chmod-component.cc
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-chmod-component.h"
#include "utils.h"
#include "widget-factory.h"

using namespace std;


guint check_perm[3][3] = {
    {GNOME_CMD_PERM_USER_READ, GNOME_CMD_PERM_USER_WRITE, GNOME_CMD_PERM_USER_EXEC},
    {GNOME_CMD_PERM_GROUP_READ, GNOME_CMD_PERM_GROUP_WRITE, GNOME_CMD_PERM_GROUP_EXEC},
    {GNOME_CMD_PERM_OTHER_READ, GNOME_CMD_PERM_OTHER_WRITE, GNOME_CMD_PERM_OTHER_EXEC}
};


struct GnomeCmdChmodComponentPrivate
{
    GtkWidget *check_boxes[3][3];
    GtkWidget *textview_label;
    GtkWidget *numberview_label;
};

enum {PERMS_CHANGED, LAST_SIGNAL};


static guint chmod_component_signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdChmodComponent, gnome_cmd_chmod_component, GTK_TYPE_BOX)


static void on_perms_changed (GnomeCmdChmodComponent *comp)
{
    auto priv = static_cast<GnomeCmdChmodComponentPrivate*>(gnome_cmd_chmod_component_get_instance_private (comp));

    static gchar text_view[10];
    static gchar number_view[4];

    guint32 permissions = gnome_cmd_chmod_component_get_perms (comp);

    perm2textstring (permissions, text_view, sizeof(text_view));
    perm2numstring (permissions, number_view, sizeof(number_view));
    gtk_label_set_text (GTK_LABEL (priv->textview_label), text_view);
    gtk_label_set_text (GTK_LABEL (priv->numberview_label), number_view);
}


static void on_check_toggled (GtkCheckButton *checkbutton, GnomeCmdChmodComponent *component)
{
    g_signal_emit (component, chmod_component_signals[PERMS_CHANGED], 0);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_chmod_component_class_init (GnomeCmdChmodComponentClass *klass)
{
    klass->perms_changed = on_perms_changed;

    chmod_component_signals[PERMS_CHANGED] =
        g_signal_new ("perms-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdChmodComponentClass, perms_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);
}


static void gnome_cmd_chmod_component_init (GnomeCmdChmodComponent *comp)
{
    auto priv = static_cast<GnomeCmdChmodComponentPrivate*>(gnome_cmd_chmod_component_get_instance_private (comp));

    GtkWidget *label;
    GtkWidget *hsep;

    gchar *check_text[3] = {_("Read"), _("Write"), _("Execute")};
    gchar *check_categories[3] = {_("Owner:"), _("Group:"), _("Others:")};

    g_object_set (comp, "orientation", GTK_ORIENTATION_VERTICAL, NULL);
    gtk_box_set_spacing (GTK_BOX (comp), 5);

    GtkGrid *grid = GTK_GRID (gtk_grid_new ());
    gtk_widget_show (GTK_WIDGET (grid));
    gtk_box_append (GTK_BOX (comp), GTK_WIDGET (grid));
    gtk_grid_set_row_spacing (grid, 6);
    gtk_grid_set_column_spacing (grid, 6);

    for (gint y=0; y<3; y++)
    {
        GtkWidget *lbl = create_label (GTK_WIDGET (comp), check_categories[y]);
        gtk_grid_attach (grid, lbl, 0, y, 1, 1);

        for (gint x=0; x<3; x++)
        {
            priv->check_boxes[y][x] = create_check (GTK_WIDGET (comp), check_text[x], "check");
            g_signal_connect (priv->check_boxes[y][x], "toggled", G_CALLBACK (on_check_toggled), comp);
            gtk_grid_attach (grid, priv->check_boxes[y][x], x+1, y, 1, 1);
        }
    }

    hsep = create_hsep (GTK_WIDGET (comp));
    gtk_box_append (GTK_BOX (comp), hsep);

    grid = GTK_GRID (gtk_grid_new ());
    gtk_widget_show (GTK_WIDGET (grid));
    gtk_box_append (GTK_BOX (comp), GTK_WIDGET (grid));
    gtk_grid_set_row_spacing (grid, 6);
    gtk_grid_set_column_spacing (grid, 6);

    label = create_label (GTK_WIDGET (comp), _("Text view:"));
    gtk_grid_attach (grid, label, 0, 0, 1, 1);

    label = create_label (GTK_WIDGET (comp), _("Number view:"));
    gtk_grid_attach (grid, label, 0, 1, 1, 1);

    priv->textview_label = create_label (GTK_WIDGET (comp), "");
    gtk_grid_attach (grid, priv->textview_label, 1, 0, 1, 1);

    priv->numberview_label = create_label (GTK_WIDGET (comp), "");
    gtk_grid_attach (grid, priv->numberview_label, 1, 1, 1, 1);
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_chmod_component_new (guint32 perms)
{
    auto comp = static_cast <GnomeCmdChmodComponent*> (g_object_new (GNOME_CMD_TYPE_CHMOD_COMPONENT, nullptr));

    gnome_cmd_chmod_component_set_perms (comp, perms);

    return GTK_WIDGET (comp);
}


guint32 gnome_cmd_chmod_component_get_perms (GnomeCmdChmodComponent *comp)
{
    auto priv = static_cast<GnomeCmdChmodComponentPrivate*>(gnome_cmd_chmod_component_get_instance_private (comp));

    guint32 perms = 0;

    for (gint y=0; y<3; y++)
        for (gint x=0; x<3; x++)
            if (gtk_check_button_get_active (GTK_CHECK_BUTTON (priv->check_boxes[y][x])))
                perms |= check_perm[y][x];

    return perms;
}


void gnome_cmd_chmod_component_set_perms (GnomeCmdChmodComponent *component, guint32 permissions)
{
    auto priv = static_cast<GnomeCmdChmodComponentPrivate*>(gnome_cmd_chmod_component_get_instance_private (component));

    for (gint y=0; y<3; y++)
        for (gint x=0; x<3; x++)
            gtk_check_button_set_active (GTK_CHECK_BUTTON (priv->check_boxes[y][x]), permissions & check_perm[y][x]);
}
