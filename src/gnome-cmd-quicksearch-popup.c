/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#include "gnome-cmd-quicksearch-popup.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-data.h"


static GtkWindowClass *parent_class = NULL;

struct _GnomeCmdQuicksearchPopupPrivate {
    GnomeCmdFileList *fl;

    GList *matches;
    GList *pos;
    GnomeCmdFile *last_focused_file;
};


static void
focus_file (GnomeCmdQuicksearchPopup *popup, GnomeCmdFile *finfo)
{
    gint row = gnome_cmd_file_list_get_row_from_file (popup->priv->fl, finfo);
    popup->priv->last_focused_file = finfo;
    if (row > 0)
    {
        gtk_clist_moveto (GTK_CLIST (popup->priv->fl), row, 0, 1, 0);
        gtk_clist_freeze (GTK_CLIST (popup->priv->fl));
        GNOME_CMD_CLIST (popup->priv->fl)->drag_motion_row = row;
        gtk_clist_thaw (GTK_CLIST (popup->priv->fl));
    }
}


static void
set_filter (GnomeCmdQuicksearchPopup *popup, const gchar *text)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (popup->priv->fl));
    g_return_if_fail (text != NULL);

    gboolean first = TRUE;

    gtk_clist_freeze (GTK_CLIST (popup->priv->fl));
    GNOME_CMD_CLIST (popup->priv->fl)->drag_motion_row = -1;
    gtk_clist_thaw (GTK_CLIST (popup->priv->fl));

    if (popup->priv->matches)
    {
        g_list_free (popup->priv->matches);
        popup->priv->matches = NULL;
    }

    GList *files = gnome_cmd_file_list_get_all_files (popup->priv->fl);
    for (; files; files = files->next)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) files->data;
        gint res;

        if (gnome_cmd_data_get_case_sens_sort ())
            res = strncmp (finfo->info->name, text, strlen(text));
        else
            res = strncasecmp (finfo->info->name, text, strlen(text));

        if (res == 0)
        {
            if (first)
            {
                focus_file (popup, finfo);
                first = FALSE;
            }

            popup->priv->matches = g_list_append (popup->priv->matches, finfo);
        }
    }

    // If no file matches the new filter, focus on the last file that matched a previous filter
    if (popup->priv->matches==NULL && popup->priv->last_focused_file!=NULL)
        popup->priv->matches = g_list_append (popup->priv->matches, popup->priv->last_focused_file);

    popup->priv->pos = popup->priv->matches;
}


static void
hide_popup (GnomeCmdQuicksearchPopup *popup)
{
    gtk_grab_remove (popup->entry);
    gtk_clist_freeze (GTK_CLIST (popup->priv->fl));
    GNOME_CMD_CLIST (popup->priv->fl)->drag_motion_row = -1;
    gtk_clist_thaw (GTK_CLIST (popup->priv->fl));
    gtk_widget_grab_focus (GTK_WIDGET (popup->priv->fl));
    if (popup->priv->matches)
        g_list_free (popup->priv->matches);
    popup->priv->last_focused_file = NULL;
    gtk_widget_hide (GTK_WIDGET (popup));
}


static void
on_text_changed                     (GtkEntry *entry,
                                     GnomeCmdQuicksearchPopup *popup)
{
    set_filter (popup, gtk_entry_get_text (GTK_ENTRY (entry)));

    if (popup->priv->pos)
        focus_file (popup, GNOME_CMD_FILE (popup->priv->pos->data));
}


static gboolean
on_key_pressed                      (GtkWidget *entry,
                                     GdkEventKey *event,
                                     GnomeCmdQuicksearchPopup *popup)
{
    // While in quicksearch, treat "ALT/CTRL + key" as a simple "key"
    if (event->state & GDK_CONTROL_MASK || event->state & GDK_MOD1_MASK) {
        event->state &= ~(GDK_CONTROL_MASK|GDK_MOD1_MASK);
    }

    switch (event->keyval)
    {
        case GDK_Escape:
        case GDK_Return:
            gnome_cmd_file_list_select_row (
                popup->priv->fl, GNOME_CMD_CLIST (popup->priv->fl)->drag_motion_row);
            hide_popup (popup);
            return TRUE;
    }

    return FALSE;
}


static gboolean
on_key_pressed_after                (GtkWidget *entry,
                                     GdkEventKey *event,
                                     GnomeCmdQuicksearchPopup *popup)
{
    switch (event->keyval)
    {
        case GDK_Up:
            {
                if (popup->priv->pos)
                {
                    if (popup->priv->pos->prev)
                        popup->priv->pos = popup->priv->pos->prev;
                    else
                        popup->priv->pos = g_list_last (popup->priv->matches);
                }
            }
            break;

        case GDK_Down:
            {
                if (popup->priv->pos)
                {
                    if (popup->priv->pos->next)
                        popup->priv->pos = popup->priv->pos->next;
                    else
                        popup->priv->pos = popup->priv->matches;
                }
            }
            break;
    }

    if (popup->priv->pos)
        focus_file (popup, GNOME_CMD_FILE (popup->priv->pos->data));

    return TRUE;
}


static void
on_button_press (GtkWidget *entry, GdkEventButton *event, GnomeCmdQuicksearchPopup *popup)
{
    gboolean ret;

    hide_popup (popup);

    gtk_signal_emit_by_name (GTK_OBJECT (popup->priv->fl), "button-press-event",
                             event, &ret);
}


static void
set_popup_position (GnomeCmdQuicksearchPopup *popup)
{
    gint x, y, w, h;
    GtkWidget *wid;

    wid = GTK_WIDGET (popup->priv->fl);

    gdk_window_get_origin (wid->window, &x, &y);
    gdk_window_get_size (wid->window, &w, &h);

    y = y + h;

    gtk_widget_set_uposition (GTK_WIDGET (popup), x, y);
}


static void
destroy (GtkObject *object)
{
    GnomeCmdQuicksearchPopup *popup = GNOME_CMD_QUICKSEARCH_POPUP (object);

    g_free (popup->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdQuicksearchPopupClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = gtk_type_class (gtk_window_get_type ());

    object_class->destroy = destroy;
    widget_class->map = map;
}


static void
init (GnomeCmdQuicksearchPopup *popup)
{
    popup->priv = g_new0 (GnomeCmdQuicksearchPopupPrivate, 1);

    popup->frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (popup->frame), GTK_SHADOW_OUT);
    popup->box = gtk_hbox_new (FALSE, 2);
    popup->lbl = gtk_label_new (_("Search"));
    popup->entry = gtk_entry_new ();

    gtk_widget_ref (popup->frame);
    gtk_widget_ref (popup->box);
    gtk_widget_ref (popup->lbl);
    gtk_widget_ref (popup->entry);

    gtk_box_pack_start (GTK_BOX (popup->box), popup->lbl, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (popup->box), popup->entry, FALSE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (popup->frame), popup->box);
    gtk_container_add (GTK_CONTAINER (popup), popup->frame);

    gtk_signal_connect (GTK_OBJECT (popup->entry), "key-press-event",
                        GTK_SIGNAL_FUNC (on_key_pressed), popup);
    gtk_signal_connect_after (GTK_OBJECT (popup->entry), "key-press-event",
                              GTK_SIGNAL_FUNC (on_key_pressed_after), popup);
    gtk_signal_connect_after (GTK_OBJECT (popup->entry), "changed",
                              GTK_SIGNAL_FUNC (on_text_changed), popup);
    gtk_signal_connect (GTK_OBJECT (popup->entry), "button-press-event",
                        GTK_SIGNAL_FUNC (on_button_press), popup);

    gtk_widget_show (popup->lbl);
    gtk_widget_show (popup->entry);
    gtk_widget_show (popup->box);
    gtk_widget_show (popup->frame);

    gtk_grab_add (popup->entry);
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_quicksearch_popup_get_type         (void)
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdQuicksearchPopup",
            sizeof (GnomeCmdQuicksearchPopup),
            sizeof (GnomeCmdQuicksearchPopupClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_window_get_type (), &info);
    }
    return type;
}


GtkWidget *
gnome_cmd_quicksearch_popup_new              (GnomeCmdFileList *fl)
{
    GnomeCmdQuicksearchPopup *popup;

    popup = gtk_type_new (gnome_cmd_quicksearch_popup_get_type ());
    GTK_WINDOW (popup)->type = GTK_WINDOW_POPUP;
    popup->priv->fl = fl;
    popup->priv->last_focused_file = NULL;
    set_popup_position (popup);

    return GTK_WIDGET (popup);
}
