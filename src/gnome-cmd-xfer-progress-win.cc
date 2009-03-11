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
#include "gnome-cmd-xfer-progress-win.h"
#include "gnome-cmd-data.h"
#include "utils.h"

using namespace std;


static GtkWindowClass *parent_class = NULL;


/******************************
 * Callbacks
 ******************************/

static void on_cancel (GtkButton *btn, GnomeCmdXferProgressWin *win)
{
    win->cancel_pressed = TRUE;
    gnome_cmd_xfer_progress_win_set_action (win, _("stopping..."));
    gtk_widget_set_sensitive (GTK_WIDGET (win), FALSE);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdXferProgressWinClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkWindowClass *) gtk_type_class (gtk_window_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdXferProgressWin *win)
{
    GtkWidget *vbox;
    GtkWidget *bbox;
    GtkWidget *button;
    GtkWidget *w = GTK_WIDGET (win);

    win->cancel_pressed = FALSE;

    gtk_window_set_title (GTK_WINDOW (win), _("Progress"));
    gtk_window_set_policy (GTK_WINDOW (win), FALSE, FALSE, FALSE);
    gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request (GTK_WIDGET (win), 300, -1);

    vbox = create_vbox (w, FALSE, 6);
    gtk_container_add (GTK_CONTAINER (win), vbox);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

    win->msg_label = create_label (w, "");
    gtk_container_add (GTK_CONTAINER (vbox), win->msg_label);

    win->fileprog_label = create_label (w, "");
    gtk_container_add (GTK_CONTAINER (vbox), win->fileprog_label);

    win->totalprog = create_progress_bar (w);
    gtk_container_add (GTK_CONTAINER (vbox), win->totalprog);

    win->fileprog = create_progress_bar (w);
    gtk_container_add (GTK_CONTAINER (vbox), win->fileprog);

    bbox = create_hbuttonbox (w);
    gtk_container_add (GTK_CONTAINER (vbox), bbox);

    button = create_stock_button (w, GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_cancel));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_xfer_progress_win_new (guint no_of_files)
{
    GnomeCmdXferProgressWin *win = (GnomeCmdXferProgressWin *) gtk_type_new (gnome_cmd_xfer_progress_win_get_type ());

    if (no_of_files<2)
    {
        GtkWidget *vbox = gtk_bin_get_child (GTK_BIN (win));
        gtk_container_remove (GTK_CONTAINER (vbox), win->fileprog);
        win->fileprog = NULL;
    }

    return GTK_WIDGET (win);
}


GtkType gnome_cmd_xfer_progress_win_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdXferProgressWin",
            sizeof (GnomeCmdXferProgressWin),
            sizeof (GnomeCmdXferProgressWinClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gtk_window_get_type (), &dlg_info);
    }

    return dlg_type;
}


void gnome_cmd_xfer_progress_win_set_total_progress (GnomeCmdXferProgressWin *win,
                                                     GnomeVFSFileSize file_bytes_copied,
                                                     GnomeVFSFileSize file_size,
                                                     GnomeVFSFileSize bytes_copied,
                                                     GnomeVFSFileSize bytes_total)
{
    gfloat total_prog = bytes_total>0 ? (gdouble) bytes_copied/(gdouble) bytes_total : -1.0f;
    gtk_progress_set_percentage (GTK_PROGRESS (win->totalprog), total_prog);

    if (win->fileprog)
    {
        gfloat file_prog = file_size>0 ? (gdouble) file_bytes_copied/(gdouble) file_size : -1.0f;
        gtk_progress_set_percentage (GTK_PROGRESS (win->fileprog), file_prog);
    }

    gchar *bytes_total_str = g_strdup (size2string (bytes_total, gnome_cmd_data.size_disp_mode));
    const gchar *bytes_copied_str = size2string (bytes_copied, gnome_cmd_data.size_disp_mode);

    gchar text[128];

    g_snprintf (text, sizeof (text), _("%s of %s copied"), bytes_copied_str, bytes_total_str);

    gtk_label_set_text (GTK_LABEL (win->fileprog_label), text);

    g_snprintf (text, sizeof (text), _("%.0f%% copied"), total_prog*100.0f);
    gtk_window_set_title (GTK_WINDOW (win), text);

    g_free (bytes_total_str);
}


void gnome_cmd_xfer_progress_win_set_msg (GnomeCmdXferProgressWin *win, const gchar *string)
{
    gtk_label_set_text (GTK_LABEL (win->msg_label), string);
}


void gnome_cmd_xfer_progress_win_set_action (GnomeCmdXferProgressWin *win, const gchar *string)
{
    gtk_window_set_title (GTK_WINDOW (win), string);
}
