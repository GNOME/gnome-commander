/**
 * @file gnome-cmd-quicksearch-popup.cc
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
#include "gnome-cmd-quicksearch-popup.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-main-win.h"

using namespace std;


struct GnomeCmdQuicksearchPopupPrivate
{
    GnomeCmdFileList *fl;

    GList *matches;
    GList *pos;
    GnomeCmdFile *last_focused_file;

    GtkWidget *entry;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdQuicksearchPopup, gnome_cmd_quicksearch_popup, GTK_TYPE_POPOVER)


inline void focus_file (GnomeCmdQuicksearchPopup *popup, GnomeCmdFile *f)
{
    if (f->is_dotdot)
        return;

    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    priv->last_focused_file = f;
    auto row = priv->fl->get_row_from_file(f);
    priv->fl->focus_file_at_row(row.get());
}


static void set_filter (GnomeCmdQuicksearchPopup *popup, const gchar *text)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (priv->fl));
    g_return_if_fail (text != nullptr);

    gboolean first = TRUE;

    if (priv->matches)
    {
        g_list_free (priv->matches);
        priv->matches = nullptr;
    }

    gchar *pattern;

    if (gnome_cmd_data.options.quick_search_exact_match_begin)
        pattern = gnome_cmd_data.options.quick_search_exact_match_end ? g_strdup (text) : g_strconcat (text, "*", nullptr);
    else
        pattern = gnome_cmd_data.options.quick_search_exact_match_end ? g_strconcat ("*", text, nullptr) : g_strconcat ("*", text, "*", nullptr);

    auto files = priv->fl->get_all_files();
    for (auto i = files.begin(); i != files.end(); ++i)
    {
        GnomeCmdFile *f = *i;

        if (gnome_cmd_filter_fnmatch (pattern,
                g_file_info_get_display_name(f->get_file_info()),
                gnome_cmd_data.options.case_sens_sort))
        {
            if (first)
            {
                focus_file (popup, f);
                first = FALSE;
            }

            priv->matches = g_list_append (priv->matches, f);
        }
    }

    g_free (pattern);

    // If no file matches the new filter, focus on the last file that matched a previous filter
    if (priv->matches == nullptr && priv->last_focused_file != nullptr)
        priv->matches = g_list_append (priv->matches, priv->last_focused_file);

    priv->pos = priv->matches;
}


inline void hide_popup (GnomeCmdQuicksearchPopup *popup)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    gtk_grab_remove (priv->entry);
    gtk_widget_grab_focus (GTK_WIDGET (priv->fl));
    if (priv->matches)
        g_list_free (priv->matches);
    priv->last_focused_file = nullptr;
    gtk_widget_hide (GTK_WIDGET (popup));
}


static void on_text_changed (GtkEntry *entry, GnomeCmdQuicksearchPopup *popup)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    set_filter (popup, gtk_entry_get_text (GTK_ENTRY (entry)));

    if (priv->pos)
        focus_file (popup, GNOME_CMD_FILE (priv->pos->data));
}


static gboolean on_key_pressed (GtkWidget *entry, GdkEventKey *event, GnomeCmdQuicksearchPopup *popup)
{
    GnomeCmdKeyPress key_press_event = { .keyval = event->keyval, .state = event->state };

    if (event->type == GDK_KEY_PRESS && gtk_editable_get_editable (GTK_EDITABLE (entry)))
        if (gtk_entry_im_context_filter_keypress (GTK_ENTRY (entry), event))
            return TRUE;

    // While in quicksearch, treat "ALT/CTRL + key" as a simple "key"
    event->state &= ~(GDK_CONTROL_MASK | GDK_MOD1_MASK);

    switch (event->keyval)
    {
        case GDK_KEY_Escape:
            // popup->priv->fl->select_row(GNOME_CMD_FILE_LIST (popup->priv->fl)->drag_motion_row);
            hide_popup (popup);
            return TRUE;

        // for more convenience do ENTER action directly on the current quicksearch item
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:

        // for more convenience jump direct to Fx function on the current quicksearch item
        case GDK_KEY_F3:
        case GDK_KEY_F4:
        case GDK_KEY_F5:
        case GDK_KEY_F6:
        case GDK_KEY_F8:
            // popup->priv->fl->select_row(GNOME_CMD_FILE_LIST (popup->priv->fl)->drag_motion_row);
            hide_popup (popup);
            main_win->key_pressed(&key_press_event);
            return TRUE;

        default:
            break;
    }

    return FALSE;
}


static gboolean on_key_pressed_after (GtkWidget *entry, GdkEventKey *event, GnomeCmdQuicksearchPopup *popup)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    switch (event->keyval)
    {
        case GDK_KEY_Up:
            {
                if (priv->pos)
                {
                    if (priv->pos->prev)
                        priv->pos = priv->pos->prev;
                    else
                        priv->pos = g_list_last (priv->matches);
                }
            }
            break;

        case GDK_KEY_Down:
            {
                if (priv->pos)
                {
                    if (priv->pos->next)
                        priv->pos = priv->pos->next;
                    else
                        priv->pos = priv->matches;
                }
            }
            break;

        default:
            break;
    }

    if (priv->pos)
        focus_file (popup, GNOME_CMD_FILE (priv->pos->data));

    return TRUE;
}


static void on_button_press (GtkWidget *entry, GdkEventButton *event, GnomeCmdQuicksearchPopup *popup)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    gboolean ret;

    hide_popup (popup);

    g_signal_emit_by_name (priv->fl, "button-press-event", event, &ret);
}


static GObject *constructor (GType gtype, guint n_properties, GObjectConstructParam *properties)
{
    // change construct only properties
    for (guint i = 0; i < n_properties; ++i)
    {
        gchar const *name = g_param_spec_get_name (properties[i].pspec);
        if (!strcmp (name, "type"))
        {
            g_value_set_enum (properties[i].value, GTK_WINDOW_POPUP);
            break;
        }
    }

    return G_OBJECT_CLASS (gnome_cmd_quicksearch_popup_parent_class)->constructor (gtype, n_properties, properties);
}

static void map (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (gnome_cmd_quicksearch_popup_parent_class)->map (widget);
}


static void gnome_cmd_quicksearch_popup_class_init (GnomeCmdQuicksearchPopupClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructor = constructor;
    widget_class->map = ::map;
}


static void gnome_cmd_quicksearch_popup_init (GnomeCmdQuicksearchPopup *popup)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget *lbl = gtk_label_new (_("Search"));
    priv->entry = gtk_entry_new ();

    gtk_box_append (GTK_BOX (box), lbl);
    gtk_box_append (GTK_BOX (box), priv->entry);
    gtk_container_add (GTK_CONTAINER (popup), box);

    g_signal_connect (priv->entry, "key-press-event", G_CALLBACK (on_key_pressed), popup);
    g_signal_connect_after (priv->entry, "key-press-event", G_CALLBACK (on_key_pressed_after), popup);
    g_signal_connect_after (priv->entry, "changed", G_CALLBACK (on_text_changed), popup);
    g_signal_connect (priv->entry, "button-press-event", G_CALLBACK (on_button_press), popup);

    gtk_widget_show_all (GTK_WIDGET (popup));
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_quicksearch_popup_new (GnomeCmdFileList *fl)
{
    GnomeCmdQuicksearchPopup *popup;

    popup = static_cast<GnomeCmdQuicksearchPopup*> (g_object_new (GNOME_CMD_TYPE_QUICKSEARCH_POPUP, nullptr));
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));
    priv->fl = fl;
    priv->last_focused_file = nullptr;

    gtk_popover_set_relative_to (GTK_POPOVER (popup), *fl);
    gtk_popover_set_position (GTK_POPOVER (popup), GTK_POS_BOTTOM);

    return GTK_WIDGET (popup);
}


void gnome_cmd_quicksearch_popup_set_char (GnomeCmdQuicksearchPopup *popup, gchar ch)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    gchar text[2];
    text[0] = ch;
    text[1] = '\0';

    if (ch != 0)
    {
        gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
        gtk_editable_set_position (GTK_EDITABLE (priv->entry), 1);
    }

    gtk_widget_grab_focus (priv->entry);
}
