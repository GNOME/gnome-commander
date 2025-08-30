/**
 * @file utils.cc
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

#include "gnome-cmd-includes.h"
#include "utils.h"


static GtkWidget *create_error_message_dialog (GtkWindow *parent, const gchar *message, const gchar *secondary_text)
{
    GtkWidget *dlg = gtk_message_dialog_new (parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", message);
    if (secondary_text)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg), "%s", secondary_text);
    g_signal_connect (dlg, "response", G_CALLBACK (gtk_window_destroy), dlg);
    return dlg;
}


void gnome_cmd_error_message (GtkWindow *parent, const gchar *message, GError *error)
{
    GtkWidget *dlg = create_error_message_dialog (parent, message, error->message);
    g_error_free (error);
    gtk_window_present (GTK_WINDOW (dlg));
}
