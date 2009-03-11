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
#include <gtk/gtk.h>

#include "gnome-cmd-menu-button.h"

using namespace std;


static void on_menu_button_clicked (GtkButton *widget, GtkWidget *menu)
{
    GdkEventButton *event = (GdkEventButton *) gtk_get_current_event();

    if (event == NULL)
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 1, gtk_get_current_event_time());
    else
        if (event->button == 1)
            gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
}


inline GtkWidget *create_menu_button (const gchar *stock_id, const gchar *label_text, GtkWidget *menu)
{
    GtkWidget *button = gtk_button_new ();
    GtkWidget *hbox = gtk_hbox_new (FALSE, 3);
    GtkWidget *label = gtk_label_new_with_mnemonic (label_text ? label_text : stock_id);

    gtk_container_add (GTK_CONTAINER (button), hbox);

    if (stock_id)
        gtk_box_pack_start (GTK_BOX (hbox), gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE), FALSE, FALSE, 0);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
    gtk_widget_set_events (button, GDK_BUTTON_PRESS_MASK);
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (on_menu_button_clicked), menu);

    return button;
}


GtkWidget *gnome_cmd_button_menu_new (const gchar *label, GtkWidget *menu)
{
    return create_menu_button (NULL, label, menu);
}


GtkWidget *gnome_cmd_button_menu_new_from_stock (const gchar *stock_id, GtkWidget *menu)
{
    return create_menu_button (stock_id, NULL, menu);
}


GtkWidget *gnome_cmd_button_menu_new_from_stock (const gchar *stock_id, const gchar *label, GtkWidget *menu)
{
    return create_menu_button (stock_id, label, menu);
}


gulong gnome_cmd_button_menu_connect_handler (GtkWidget *button, GtkWidget *menu)
{
    return g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (on_menu_button_clicked), menu);
}

void gnome_cmd_button_menu_disconnect_handler (GtkWidget *button, GtkWidget *menu)
{
    g_signal_handlers_disconnect_by_func (G_OBJECT (button), gpointer (on_menu_button_clicked), menu);
}
