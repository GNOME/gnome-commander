/**
 * @file scroll-box.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
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
 *
 */

#include <config.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "scroll-box.h"

using namespace std;


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

static void scroll_box_destroy (GtkWidget *widget);
static gboolean scroll_box_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data);

G_DEFINE_TYPE_WITH_PRIVATE (ScrollBox, scroll_box, GTK_TYPE_GRID)

/*****************************************
    public functions
    (defined in the header file)
*****************************************/
GtkWidget* scroll_box_new ()
{
    auto w = static_cast<ScrollBox*> (g_object_new (scroll_box_get_type (), nullptr));

    return GTK_WIDGET (w);
}


static void scroll_box_class_init (ScrollBoxClass *klass)
{
    GTK_WIDGET_CLASS (klass)->destroy = scroll_box_destroy;
}


static void scroll_box_init (ScrollBox *w)
{
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (w));

    priv->vscroll = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, nullptr);
    gtk_widget_show (priv->vscroll);
    gtk_grid_attach (GTK_GRID (w), priv->vscroll, 1, 0, 1, 1);

    priv->hscroll = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, nullptr);
    gtk_widget_show (priv->hscroll);
    gtk_grid_attach (GTK_GRID (w), priv->hscroll, 0, 1, 1, 1);
    priv->client = nullptr;

    g_signal_connect (w, "button-press-event", G_CALLBACK (scroll_box_button_press), w);
    g_signal_connect (w, "destroy-event", G_CALLBACK (scroll_box_destroy), w);
}


static void scroll_box_destroy (GtkWidget *widget)
{
    g_return_if_fail (IS_SCROLL_BOX (widget));
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (SCROLL_BOX (widget)));

    g_clear_object (&priv->client);

    GTK_WIDGET_CLASS (scroll_box_parent_class)->destroy (widget);
}


GtkRange *scroll_box_get_v_range(ScrollBox *obj)
{
    g_return_val_if_fail (IS_SCROLL_BOX (obj), FALSE);
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (obj));

    return GTK_RANGE (priv->vscroll);
}


static gboolean scroll_box_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    g_return_val_if_fail (IS_SCROLL_BOX (widget), FALSE);
    g_return_val_if_fail (event != nullptr, FALSE);

    // ScrollBox *w = SCROLL_BOX (widget);

    return FALSE;
}


void scroll_box_set_client (ScrollBox *obj, GtkWidget *client)
{
    g_return_if_fail (IS_SCROLL_BOX (obj));
    g_return_if_fail (client != nullptr);
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (obj));

    g_object_ref (client);
    g_clear_object (&priv->client);
    priv->client = client;

    gtk_widget_show (client);

    gtk_widget_set_hexpand (client, TRUE);
    gtk_widget_set_vexpand (client, TRUE);
    gtk_grid_attach (GTK_GRID (obj), client, 0, 0, 1, 1);
}


GtkWidget *scroll_box_get_client (ScrollBox *obj)
{
    g_return_val_if_fail (IS_SCROLL_BOX (obj), nullptr);
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (obj));

    return priv->client;
}


void scroll_box_set_h_adjustment (ScrollBox *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (IS_SCROLL_BOX (obj));
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (obj));

    gtk_range_set_adjustment (GTK_RANGE (priv->hscroll), adjustment);
}


void scroll_box_set_v_adjustment (ScrollBox *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (IS_SCROLL_BOX (obj));
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (obj));

    gtk_range_set_adjustment (GTK_RANGE (priv->vscroll), adjustment);
}


GtkAdjustment *scroll_box_get_h_adjustment (ScrollBox *obj)
{
    g_return_val_if_fail (IS_SCROLL_BOX (obj), nullptr);
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (obj));

    return gtk_range_get_adjustment (GTK_RANGE (priv->hscroll));
}


GtkAdjustment *scroll_box_get_v_adjustment (ScrollBox *obj)
{
    g_return_val_if_fail (IS_SCROLL_BOX (obj), nullptr);
    auto priv = static_cast<ScrollBoxPrivate*>(scroll_box_get_instance_private (obj));

    return gtk_range_get_adjustment (GTK_RANGE (priv->vscroll));
}
