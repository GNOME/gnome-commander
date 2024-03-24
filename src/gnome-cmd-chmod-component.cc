/**
 * @file gnome-cmd-chmod-component.cc
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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-chmod-component.h"
#include "utils.h"

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


G_DEFINE_TYPE (GnomeCmdChmodComponent, gnome_cmd_chmod_component, GTK_TYPE_VBOX)


static void on_perms_changed (GnomeCmdChmodComponent *comp)
{
    static gchar text_view[10];
    static gchar number_view[4];

    guint32 permissions = gnome_cmd_chmod_component_get_perms (comp);

    perm2textstring (permissions, text_view, sizeof(text_view));
    perm2numstring (permissions, number_view, sizeof(number_view));
    gtk_label_set_text (GTK_LABEL (comp->priv->textview_label), text_view);
    gtk_label_set_text (GTK_LABEL (comp->priv->numberview_label), number_view);
}


static void on_check_toggled (GtkToggleButton *togglebutton, GnomeCmdChmodComponent *component)
{
    gtk_signal_emit (GTK_OBJECT (component), chmod_component_signals[PERMS_CHANGED]);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdChmodComponent *comp = GNOME_CMD_CHMOD_COMPONENT (object);

    g_free (comp->priv);

    GTK_OBJECT_CLASS (gnome_cmd_chmod_component_parent_class)->destroy (object);
}


static void map (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (gnome_cmd_chmod_component_parent_class)->map (widget);
}


static void gnome_cmd_chmod_component_class_init (GnomeCmdChmodComponentClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->destroy = destroy;
    widget_class->map = ::map;

    klass->perms_changed = on_perms_changed;

    chmod_component_signals[PERMS_CHANGED] =
        gtk_signal_new ("perms-changed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdChmodComponentClass, perms_changed),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);
}


static void gnome_cmd_chmod_component_init (GnomeCmdChmodComponent *comp)
{
    GtkWidget *label;
    GtkWidget *hsep;

    gchar *check_text[3] = {_("Read"), _("Write"), _("Execute")};
    gchar *check_categories[3] = {_("Owner:"), _("Group:"), _("Others:")};

    comp->priv = g_new (GnomeCmdChmodComponentPrivate, 1);

    gtk_box_set_spacing (GTK_BOX (comp), 5);

    GtkWidget *table = create_table (GTK_WIDGET (comp), 3, 4);
    gtk_box_pack_start (GTK_BOX (comp), table, FALSE, FALSE, 0);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 6);

    for (gint y=0; y<3; y++)
    {
        GtkWidget *lbl = create_label (GTK_WIDGET (comp), check_categories[y]);
        table_add (GTK_WIDGET (table), lbl, 0, y, GTK_FILL);

        for (gint x=0; x<3; x++)
        {
            comp->priv->check_boxes[y][x] = create_check (GTK_WIDGET (comp), check_text[x], "check");
            g_signal_connect (comp->priv->check_boxes[y][x], "toggled", G_CALLBACK (on_check_toggled), comp);
            table_add (GTK_WIDGET (table), comp->priv->check_boxes[y][x], x+1, y, GTK_FILL);
        }
    }

    hsep = create_hsep (GTK_WIDGET (comp));
    gtk_box_pack_start (GTK_BOX (comp), hsep, TRUE, TRUE, 0);

    table = create_table (GTK_WIDGET (comp), 2, 2);
    gtk_box_pack_start (GTK_BOX (comp), table, TRUE, TRUE, 0);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 6);

    label = create_label (GTK_WIDGET (comp), _("Text view:"));
    table_add (table, label, 0, 0, GTK_FILL);

    label = create_label (GTK_WIDGET (comp), _("Number view:"));
    table_add (table, label, 0, 1, GTK_FILL);

    comp->priv->textview_label = create_label (GTK_WIDGET (comp), "");
    table_add (table, comp->priv->textview_label, 1, 0, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    comp->priv->numberview_label = create_label (GTK_WIDGET (comp), "");
    table_add (table, comp->priv->numberview_label, 1, 1, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));
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
    guint32 perms = 0;

    for (gint y=0; y<3; y++)
        for (gint x=0; x<3; x++)
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (comp->priv->check_boxes[y][x])))
                perms |= check_perm[y][x];

    return perms;
}


void gnome_cmd_chmod_component_set_perms (GnomeCmdChmodComponent *component, guint32 permissions)
{
    for (gint y=0; y<3; y++)
        for (gint x=0; x<3; x++)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (component->priv->check_boxes[y][x]), permissions & check_perm[y][x]);
}
