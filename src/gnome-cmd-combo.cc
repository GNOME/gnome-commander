/* gtkcombo - combo widget for gtk+
 * Copyright 1997 Paolo Molaro
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Modified by Marcus Bjurman <marbj499@student.liu.se> 2001-2006
 * The orginal comments are left intact above
 */

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-data.h"

using namespace std;


enum
{
  ITEM_SELECTED,
  POPWIN_HIDDEN,
  LAST_SIGNAL
};

struct GnomeCmdComboClass
{
    GtkHBoxClass parent_class;

    void (* item_selected)     (GnomeCmdCombo *combo, gpointer data);
    void (* popwin_hidden)     (GnomeCmdCombo *combo);
};

const gchar *gnome_cmd_combo_string_key = "gnome-cmd-combo-string-value";

#define COMBO_LIST_MAX_HEIGHT    (400)
#define EMPTY_LIST_HEIGHT         (15)

static GtkHBoxClass *parent_class = NULL;

static guint signals[LAST_SIGNAL] = { 0 };


static void gnome_cmd_combo_item_selected (GnomeCmdCombo *combo, gpointer data);


/*******************************
 * Utility functions
 *******************************/

static void size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GNOME_CMD_IS_COMBO (widget));
    g_return_if_fail (allocation != NULL);

    GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

    GnomeCmdCombo *combo = GNOME_CMD_COMBO (widget);

    if (combo->entry->allocation.height > combo->entry->requisition.height)
    {
        GtkAllocation button_allocation;

        button_allocation = combo->button->allocation;
        button_allocation.height = combo->entry->requisition.height;
        button_allocation.y = combo->entry->allocation.y +
            (combo->entry->allocation.height - combo->entry->requisition.height) / 2;
        gtk_widget_size_allocate (combo->button, &button_allocation);
    }
}


inline void get_pos (GnomeCmdCombo *combo, gint *x, gint *y, gint *height, gint *width)
{
    GtkWidget *widget = GTK_WIDGET(combo);
    GtkScrolledWindow *popup = GTK_SCROLLED_WINDOW (combo->popup);
    GtkBin *popwin = GTK_BIN (combo->popwin);

    gint real_height;
    GtkRequisition list_requisition;
    gboolean show_hscroll = FALSE;
    gboolean show_vscroll = FALSE;
    gint avail_height;
    gint min_height;
    gint alloc_width;
    gint work_height;
    gint old_height;
    gint old_width;

    gdk_window_get_origin (combo->entry->window, x, y);
    real_height = MIN (combo->entry->requisition.height, combo->entry->allocation.height);
    *y += real_height;
    avail_height = gdk_screen_height () - *y;

    gtk_widget_size_request (combo->list, &list_requisition);
    min_height = MIN (list_requisition.height, popup->vscrollbar->requisition.height);
    if (!GTK_CLIST (combo->list)->rows)
        list_requisition.height += EMPTY_LIST_HEIGHT;

    alloc_width = (widget->allocation.width -
                   2 * popwin->child->style->xthickness -
                   2 * GTK_CONTAINER (popwin->child)->border_width -
                   2 * GTK_CONTAINER (combo->popup)->border_width -
                   2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width -
                   2 * GTK_BIN (popup)->child->style->xthickness) + 100;

    work_height = (2 * popwin->child->style->ythickness +
                   2 * GTK_CONTAINER (popwin->child)->border_width +
                   2 * GTK_CONTAINER (combo->popup)->border_width +
                   2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width +
                   2 * GTK_BIN (popup)->child->style->xthickness)+20;

    do
    {
        old_width = alloc_width;
        old_height = work_height;

        if (!show_hscroll && alloc_width < list_requisition.width)
        {
            work_height += popup->hscrollbar->requisition.height +
                GTK_SCROLLED_WINDOW_GET_CLASS
                (combo->popup)->scrollbar_spacing;
            show_hscroll = TRUE;
        }
        if (!show_vscroll && work_height + list_requisition.height > avail_height)
        {
            if (work_height + min_height > avail_height && *y - real_height > avail_height)
            {
                *y -= (work_height + list_requisition.height + real_height);
                break;
            }
            alloc_width -=
                popup->vscrollbar->requisition.width +
                GTK_SCROLLED_WINDOW_GET_CLASS
                (combo->popup)->scrollbar_spacing;
            show_vscroll = TRUE;
        }
    } while (old_width != alloc_width || old_height != work_height);

    *width = widget->allocation.width;
    if (*width < 200)
        *width = 200;

    *height = show_vscroll ? avail_height : work_height + list_requisition.height;

    if (*x < 0)
        *x = 0;
}


void GnomeCmdCombo::popup_list()
{
    gint height, width, x, y;

    get_pos (this, &x, &y, &height, &width);

    gtk_widget_set_uposition (popwin, x, y);
    gtk_widget_set_size_request (popwin, width, height);
    gtk_widget_realize (popwin);
    gdk_window_resize (popwin->window, width, height);
    gtk_widget_show (popwin);
}


/*******************************
 * Callbacks
 *******************************/

static gboolean on_popup_button_release (GtkWidget *button,  GnomeCmdCombo *combo)
{
    if (combo->is_popped)
        gtk_widget_hide (combo->popwin);
    else
        combo->popup_list();

    combo->is_popped = !combo->is_popped;

    return TRUE;
}


static int on_list_key_press (GtkWidget *widget, GdkEventKey *event, GnomeCmdCombo *combo)
{
    if (event->keyval == GDK_Escape)
    {
        gtk_widget_hide (combo->popwin);
        combo->is_popped = FALSE;
        return TRUE;
    }

    return FALSE;
}


static gboolean on_popwin_button_released (GtkWidget *button, GdkEventButton *event, GnomeCmdCombo *combo)
{
    if (!event || event->button != 1) return FALSE;

    // Check to see if we clicked inside the popwin
    GtkWidget *child = gtk_get_event_widget ((GdkEvent *) event);
    while (child && child != (combo->popwin))
        child = child->parent;

    if (child != combo->popwin)
    {
        // We clicked outside the popwin
        gtk_widget_hide (combo->popwin);
        combo->is_popped = FALSE;
        return TRUE;
    }

    return FALSE;
}


static int on_popwin_keypress (GtkWidget *widget, GdkEventKey *event, GnomeCmdCombo *combo)
{
    switch (event->keyval)
    {
        case GDK_Return:
        case GDK_KP_Enter:
            {
                gint row = GTK_CLIST (combo->list)->focus_row;

                if (row < 0) return TRUE;

                gpointer data = gtk_clist_get_row_data (GTK_CLIST (combo->list), row);
                gtk_signal_emit (*combo, signals[ITEM_SELECTED], data);
            }

            return TRUE;

        default:
            return FALSE;
    }
}


static gboolean on_list_button_press (GtkCList *clist, GdkEventButton *event, GnomeCmdCombo *combo)
{
    if (clist->clist_window != event->window)
        return FALSE;

    gint row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (clist), event->x, event->y);
    if (row < 0)
        return FALSE;

    if (row != clist->focus_row)
    {
        gtk_clist_select_row (clist, row, 0);
        clist->focus_row = row;
    }
    else
        gtk_signal_emit_stop_by_name (GTK_OBJECT (clist), "button-press-event");

    return TRUE;
}


static gboolean on_list_button_release (GtkCList *clist, GdkEventButton *event, GnomeCmdCombo *combo)
{
    gint row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (clist), event->x, event->y);

    if (clist->focus_row >= 0 && clist->focus_row == row)
    {
        gpointer data = gtk_clist_get_row_data (clist, clist->focus_row);
        gtk_signal_emit (*combo, signals[ITEM_SELECTED], data);

        return TRUE;
    }

    return FALSE;
}


static void on_popwin_show (GtkWidget *widget, GnomeCmdCombo *combo)
{
    gtk_grab_add (combo->popwin);
    gtk_widget_grab_focus (combo->list);
}


static void on_popwin_hide (GtkWidget *widget, GnomeCmdCombo *combo)
{
    gtk_grab_remove (combo->popwin);
    gtk_signal_emit (*combo, signals[POPWIN_HIDDEN]);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *combo)
{
    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (combo);
}


static void class_init (GnomeCmdComboClass *klass)
{
    parent_class = (GtkHBoxClass *) gtk_type_class (gtk_hbox_get_type ());

    GtkObjectClass *object_class = (GtkObjectClass *) klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

    signals[ITEM_SELECTED] =
        gtk_signal_new ("item-selected",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdComboClass, item_selected),
            gtk_marshal_NONE__POINTER,
            GTK_TYPE_NONE,
            1, GTK_TYPE_POINTER);

    signals[POPWIN_HIDDEN] =
        gtk_signal_new ("popwin-hidden",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdComboClass, popwin_hidden),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    object_class->destroy = destroy;
    widget_class->size_allocate = size_allocate;
    klass->item_selected = gnome_cmd_combo_item_selected;
    klass->popwin_hidden = NULL;
}


static void init (GnomeCmdCombo *combo)
{
    GtkWidget *arrow;
    GtkWidget *frame;
    GtkWidget *event_box;
    GdkCursor *cursor;

    combo->value_in_list = 0;
    combo->ok_if_empty = 1;
    combo->highest_pixmap = 20;
    combo->widest_pixmap = 20;
    combo->is_popped = FALSE;

    combo->entry = gtk_entry_new ();
    gtk_widget_ref (combo->entry);
    g_object_set_data_full (*combo, "entry", combo->entry, g_object_unref);
    gtk_widget_show (combo->entry);
    gtk_widget_set_size_request (combo->entry, 60, -1);
    gtk_entry_set_editable (GTK_ENTRY (combo->entry), FALSE);
    GTK_WIDGET_UNSET_FLAGS (combo->entry, GTK_CAN_FOCUS);

    combo->button = gtk_button_new ();
    gtk_widget_ref (combo->button);
    gtk_button_set_relief (GTK_BUTTON (combo->button), gnome_cmd_data.button_relief);
    g_object_set_data_full (*combo, "button", combo->button, g_object_unref);
    gtk_widget_show (combo->button);

    arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
    gtk_widget_ref (arrow);
    g_object_set_data_full (*combo, "arrow", arrow, g_object_unref);
    gtk_widget_show (arrow);

    gtk_container_add (GTK_CONTAINER (combo->button), arrow);
    gtk_box_pack_start (GTK_BOX (combo), combo->entry, TRUE, TRUE, 0);
    gtk_box_pack_end (GTK_BOX (combo), combo->button, FALSE, FALSE, 0);

    // connect button signals
    g_signal_connect (combo->button, "clicked", G_CALLBACK (on_popup_button_release), combo);

    combo->popwin = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_widget_ref (combo->popwin);
    g_object_set_data_full (*combo, "popwin", combo->popwin, g_object_unref);
    gtk_window_set_policy (GTK_WINDOW (combo->popwin), 1, 1, 0);

    gtk_widget_set_events (combo->popwin, GDK_KEY_PRESS_MASK);
    gtk_widget_set_events (combo->popwin, GDK_BUTTON_PRESS_MASK);

    // connect popupwin signals
    g_signal_connect (combo->popwin, "button-release-event", G_CALLBACK (on_popwin_button_released), combo);
    g_signal_connect (combo->popwin, "key-press-event", G_CALLBACK (on_popwin_keypress), combo);
    g_signal_connect (combo->popwin, "show", G_CALLBACK (on_popwin_show), combo);
    g_signal_connect (combo->popwin, "hide", G_CALLBACK (on_popwin_hide), combo);

    event_box = gtk_event_box_new ();
    gtk_widget_ref (event_box);
    g_object_set_data_full (*combo, "event_box", event_box, g_object_unref);
    gtk_container_add (GTK_CONTAINER (combo->popwin), event_box);
    gtk_widget_show (event_box);
    gtk_widget_realize (event_box);

    cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
    gdk_window_set_cursor (event_box->window, cursor);
    gdk_cursor_destroy (cursor);

    frame = gtk_frame_new (NULL);
    gtk_widget_ref (frame);
    g_object_set_data_full (*combo, "frame", frame, g_object_unref);
    gtk_container_add (GTK_CONTAINER (event_box), frame);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
    gtk_widget_show (frame);

    combo->popup = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (combo->popup);
    g_object_set_data_full (*combo, "combo->popup", combo->popup, g_object_unref);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (combo->popup), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GTK_WIDGET_UNSET_FLAGS (GTK_SCROLLED_WINDOW (combo->popup)->hscrollbar, GTK_CAN_FOCUS);
    GTK_WIDGET_UNSET_FLAGS (GTK_SCROLLED_WINDOW (combo->popup)->vscrollbar, GTK_CAN_FOCUS);
    gtk_container_add (GTK_CONTAINER (frame), combo->popup);
    gtk_widget_show (combo->popup);
}


/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_combo_get_type ()
{
    static GtkType combo_type = 0;

    if (!combo_type)
    {
        static const GtkTypeInfo combo_info =
        {
            "GnomeCmdCombo",
            sizeof (GnomeCmdCombo),
            sizeof (GnomeCmdComboClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL,
        };
        combo_type = gtk_type_unique (gtk_hbox_get_type (), &combo_info);
    }
    return combo_type;
}


GnomeCmdCombo::GnomeCmdCombo(gint num_cols, gint _text_col, gchar **col_titles): sel_data(NULL), sel_text(NULL), text_col(_text_col)
{

    list = col_titles ? gnome_cmd_clist_new_with_titles (num_cols, col_titles) : gnome_cmd_clist_new (num_cols);

    gtk_widget_ref (list);
    g_object_set_data_full (*this, "list", list, g_object_unref);

    // We'll use enter notify events to figure out when to transfer the grab to the list
    gtk_container_add (GTK_CONTAINER (popup), list);
    gtk_widget_show (list);

    // connect list signals
    g_signal_connect (list, "button-press-event", G_CALLBACK (on_list_button_press), this);
    g_signal_connect (list, "button-release-event", G_CALLBACK (on_list_button_release), this);
    g_signal_connect (list, "key-press-event", G_CALLBACK (on_list_key_press), this);
}


gint GnomeCmdCombo::append(gchar **text, gpointer data)
{
    g_return_val_if_fail (text != NULL, -1);

    gint row = gtk_clist_append (GTK_CLIST (list), text);
    gtk_clist_set_row_data (GTK_CLIST (list), row, data);

    return row;
}


gint GnomeCmdCombo::insert(gchar **text, gpointer data)
{
    g_return_val_if_fail (text != NULL, -1);

    gint row = gtk_clist_insert (GTK_CLIST (list), 0, text);
    gtk_clist_set_row_data (GTK_CLIST (list), row, data);

    return row;
}


void GnomeCmdCombo::set_pixmap(gint row, gint col, GnomeCmdPixmap *pixmap)
{
    g_return_if_fail (pixmap != NULL);

    gtk_clist_set_pixmap (GTK_CLIST (list), row, col, pixmap->pixmap, pixmap->mask);
    if (pixmap->height > highest_pixmap)
    {
        gtk_clist_set_row_height (GTK_CLIST (list), pixmap->height);
        highest_pixmap = pixmap->height;
    }
    if (pixmap->width > widest_pixmap)
    {
        gtk_clist_set_column_width (GTK_CLIST (list), 0, pixmap->width);
        widest_pixmap = pixmap->width;
    }
}


void GnomeCmdCombo::select_text(const gchar *text)
{
    if (text)
        gtk_entry_set_text (GTK_ENTRY (entry), text);
}


void GnomeCmdCombo::select_data(gpointer data)
{
    GtkCList *clist = GTK_CLIST (list);
    gint row = gtk_clist_find_row_from_data (clist, data);

    if (gtk_clist_get_text (clist, row, text_col, &sel_text))
    {
        sel_data = data;
        gtk_entry_set_text (GTK_ENTRY (entry), sel_text);
        gtk_clist_select_row (GTK_CLIST (list), row, 0);
    }
}


void GnomeCmdCombo::update_style()
{
    gtk_widget_set_style (entry, list_style);
    gnome_cmd_clist_update_style (GNOME_CMD_CLIST (list));
}


static void gnome_cmd_combo_item_selected (GnomeCmdCombo *combo, gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_COMBO (combo));

    gtk_widget_hide (combo->popwin);
    combo->is_popped = FALSE;

    gint row = gtk_clist_find_row_from_data (GTK_CLIST (combo->list), data);

    if (gtk_clist_get_text (GTK_CLIST (combo->list), row, combo->text_col, &combo->sel_text))
    {
        combo->sel_data = data;
        gtk_entry_set_text (GTK_ENTRY (combo->entry), combo->sel_text);
    }
}
