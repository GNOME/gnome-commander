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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-chmod-component.h"
#include "utils.h"

using namespace std;


GnomeVFSFilePermissions check_perm[3][3] = {
    {GNOME_VFS_PERM_USER_READ, GNOME_VFS_PERM_USER_WRITE, GNOME_VFS_PERM_USER_EXEC},
    {GNOME_VFS_PERM_GROUP_READ, GNOME_VFS_PERM_GROUP_WRITE, GNOME_VFS_PERM_GROUP_EXEC},
    {GNOME_VFS_PERM_OTHER_READ, GNOME_VFS_PERM_OTHER_WRITE, GNOME_VFS_PERM_OTHER_EXEC}
};


struct GnomeCmdChmodComponentPrivate
{
    GtkWidget *check_boxes[3][3];
    GtkWidget *textview_label;
    GtkWidget *numberview_label;
};

enum {PERMS_CHANGED, LAST_SIGNAL};


static GtkVBoxClass *parent_class = NULL;
static guint chmod_component_signals[LAST_SIGNAL] = { 0 };


static void on_perms_changed (GnomeCmdChmodComponent *comp)
{
    static gchar text_view[10];
    static gchar number_view[4];

    GnomeVFSFilePermissions perms = gnome_cmd_chmod_component_get_perms (comp);

    perm2textstring (perms, text_view, 10);
    perm2numstring (perms, number_view, 4);
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

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdChmodComponentClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkVBoxClass *) gtk_type_class (gtk_vbox_get_type ());
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


static void init (GnomeCmdChmodComponent *comp)
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
            gtk_signal_connect (GTK_OBJECT (comp->priv->check_boxes[y][x]), "toggled",
                                GTK_SIGNAL_FUNC (on_check_toggled), comp);
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

GtkWidget *gnome_cmd_chmod_component_new (GnomeVFSFilePermissions perms)
{
    GnomeCmdChmodComponent *comp = (GnomeCmdChmodComponent *) gtk_type_new (gnome_cmd_chmod_component_get_type ());

    gnome_cmd_chmod_component_set_perms (comp, perms);

    return GTK_WIDGET (comp);
}


GtkType gnome_cmd_chmod_component_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdChmodComponent",
            sizeof (GnomeCmdChmodComponent),
            sizeof (GnomeCmdChmodComponentClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_vbox_get_type (), &info);
    }
    return type;
}


GnomeVFSFilePermissions gnome_cmd_chmod_component_get_perms (GnomeCmdChmodComponent *comp)
{
    guint perms = 0;

    for (gint y=0; y<3; y++)
        for (gint x=0; x<3; x++)
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (comp->priv->check_boxes[y][x])))
                perms |= check_perm[y][x];

    return (GnomeVFSFilePermissions) perms;
}


void gnome_cmd_chmod_component_set_perms (GnomeCmdChmodComponent *component, GnomeVFSFilePermissions perms)
{
    for (gint y=0; y<3; y++)
        for (gint x=0; x<3; x++)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (component->priv->check_boxes[y][x]), perms & check_perm[y][x]);
}
