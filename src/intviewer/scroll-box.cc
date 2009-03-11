/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
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
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gtk/gtktable.h>

#include "scroll-box.h"

using namespace std;


static GtkTableClass *parent_class = NULL;

// Class Private Data
struct ScrollBoxPrivate
{
    GtkWidget *hscroll;
    GtkWidget *vscroll;
    GtkWidget *client;
};

// Gtk class related static functions
static void scroll_box_init (ScrollBox *w);
static void scroll_box_class_init (ScrollBoxClass *klass);

static void scroll_box_destroy (GtkObject *widget);
static gboolean scroll_box_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data);

/*****************************************
    public functions
    (defined in the header file)
*****************************************/
GtkType scroll_box_get_type ()
{
    static GtkType type = 0;
    if (type == 0)
    {
        GTypeInfo info =
        {
            sizeof (ScrollBoxClass),
            NULL,
            NULL,
            (GClassInitFunc) scroll_box_class_init,
            NULL,
            NULL,
            sizeof(ScrollBox),
            0,
            (GInstanceInitFunc) scroll_box_init
        };
        type = g_type_register_static (GTK_TYPE_TABLE, "scrollbox", &info, (GTypeFlags) 0);
    }
    return type;
}


GtkWidget* scroll_box_new ()
{
    ScrollBox *w = (ScrollBox *) gtk_type_new (scroll_box_get_type ());

    return GTK_WIDGET (w);
}


static void scroll_box_class_init (ScrollBoxClass *klass)
{
    GtkObjectClass *object_class;

    object_class = GTK_OBJECT_CLASS (klass);
    parent_class = (GtkTableClass *) gtk_type_class(gtk_table_get_type());

    object_class->destroy = scroll_box_destroy;
}


static void scroll_box_init (ScrollBox *w)
{
    w->priv = g_new0(ScrollBoxPrivate, 1);

    gtk_table_resize(GTK_TABLE(w), 2, 2);
    gtk_table_set_homogeneous(GTK_TABLE(w), FALSE);

    w->priv->vscroll = gtk_vscrollbar_new (NULL);
    gtk_widget_show (w->priv->vscroll);
    gtk_table_attach (GTK_TABLE(w), w->priv->vscroll, 1, 2, 0, 1,
        (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (GTK_FILL), 0, 0);

    w->priv->hscroll = gtk_hscrollbar_new (NULL);
    gtk_widget_show (w->priv->hscroll);
    gtk_table_attach (GTK_TABLE (w), w->priv->hscroll, 0, 1, 1, 2,
            (GtkAttachOptions) (GTK_FILL),
            (GtkAttachOptions) (GTK_FILL), 0, 0);
    w->priv->client = NULL;

    g_signal_connect(G_OBJECT (w), "button-press-event", G_CALLBACK(scroll_box_button_press), (gpointer) w);
    g_signal_connect(G_OBJECT (w), "destroy-event", G_CALLBACK(scroll_box_destroy), (gpointer) w);
}


static void scroll_box_destroy (GtkObject *widget)
{
    g_return_if_fail (widget!= NULL);
    g_return_if_fail (IS_SCROLL_BOX (widget));

    ScrollBox *w = SCROLL_BOX (widget);

    if (w->priv)
    {
        if (w->priv->client)
            g_object_unref (G_OBJECT (w->priv->client));
        w->priv->client=NULL;

        g_free(w->priv);
        w->priv = NULL;
    }

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy)(widget);
}


GtkRange *scroll_box_get_v_range(ScrollBox *obj)
{
    g_return_val_if_fail (obj!= NULL, FALSE);
    g_return_val_if_fail (IS_SCROLL_BOX (obj), FALSE);

    return GTK_RANGE(obj->priv->vscroll);
}


static gboolean scroll_box_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_SCROLL_BOX (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    // ScrollBox *w = SCROLL_BOX (widget);

    return FALSE;
}


void scroll_box_set_client (ScrollBox *obj, GtkWidget *client)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_SCROLL_BOX (obj));
    g_return_if_fail (client!=NULL);

    if (obj->priv->client)
    {
        if (obj->priv->client)
            g_object_unref (G_OBJECT (obj->priv->client));
        obj->priv->client = NULL;
    }

    g_object_ref (G_OBJECT (client));
    gtk_widget_show (client);
    obj->priv->client = client;
    gtk_table_attach (GTK_TABLE (obj), client , 0, 1, 0, 1,
                      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
}


GtkWidget *scroll_box_get_client (ScrollBox *obj)
{
    g_return_val_if_fail (obj!= NULL, NULL);
    g_return_val_if_fail (IS_SCROLL_BOX (obj), NULL);

    return obj->priv->client;
}


void scroll_box_set_h_adjustment (ScrollBox *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_SCROLL_BOX(obj));

    gtk_range_set_adjustment(GTK_RANGE(obj->priv->hscroll), adjustment);
}


void scroll_box_set_v_adjustment (ScrollBox *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_SCROLL_BOX(obj));

    gtk_range_set_adjustment(GTK_RANGE(obj->priv->vscroll), adjustment);
}


GtkAdjustment *scroll_box_get_h_adjustment (ScrollBox *obj)
{
    g_return_val_if_fail (obj != NULL, NULL);
    g_return_val_if_fail (IS_SCROLL_BOX(obj), NULL);

    return gtk_range_get_adjustment (GTK_RANGE(obj->priv->hscroll));
}


GtkAdjustment *scroll_box_get_v_adjustment (ScrollBox *obj)
{
    g_return_val_if_fail (obj != NULL, NULL);
    g_return_val_if_fail (IS_SCROLL_BOX(obj), NULL);

    return gtk_range_get_adjustment (GTK_RANGE(obj->priv->vscroll));
}
