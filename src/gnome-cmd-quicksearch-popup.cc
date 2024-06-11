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

    gtk_widget_grab_focus (GTK_WIDGET (priv->fl));
    if (priv->matches)
        g_list_free (priv->matches);
    priv->last_focused_file = nullptr;
    gtk_popover_popdown (GTK_POPOVER (popup));
}


static void on_text_changed (GtkEntry *entry, GnomeCmdQuicksearchPopup *popup)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    set_filter (popup, gtk_editable_get_text (GTK_EDITABLE (entry)));

    if (priv->pos)
        focus_file (popup, GNOME_CMD_FILE (priv->pos->data));
}


static gboolean on_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    auto popup = static_cast<GnomeCmdQuicksearchPopup*> (user_data);
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    // While in quicksearch, treat "ALT/CTRL + key" as a simple "key"
    GnomeCmdKeyPress event = { .keyval = keyval, .state = state };

    switch (event.keyval)
    {
        case GDK_KEY_Escape:
            hide_popup (popup);
            return TRUE;

        case GDK_KEY_Up:
            if (priv->pos)
            {
                if (priv->pos->prev)
                    priv->pos = priv->pos->prev;
                else
                    priv->pos = g_list_last (priv->matches);
            }
            if (priv->pos)
                focus_file (popup, GNOME_CMD_FILE (priv->pos->data));
            return TRUE;

        case GDK_KEY_Down:
            if (priv->pos)
            {
                if (priv->pos->next)
                    priv->pos = priv->pos->next;
                else
                    priv->pos = priv->matches;
            }
            if (priv->pos)
                focus_file (popup, GNOME_CMD_FILE (priv->pos->data));
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
            // hide popup and let an event to bubble up and be processed by a parent widget
            hide_popup (popup);
            return FALSE;

        default:
            break;
    }

    return FALSE;
}


static void gnome_cmd_quicksearch_popup_class_init (GnomeCmdQuicksearchPopupClass *klass)
{
}


static void gnome_cmd_quicksearch_popup_init (GnomeCmdQuicksearchPopup *popup)
{
    auto priv = static_cast<GnomeCmdQuicksearchPopupPrivate *> (gnome_cmd_quicksearch_popup_get_instance_private (popup));

    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lbl = gtk_label_new (_("Search"));
    priv->entry = gtk_entry_new ();

    gtk_box_append (GTK_BOX (box), lbl);
    gtk_box_append (GTK_BOX (box), priv->entry);
    gtk_popover_set_child (GTK_POPOVER (popup), box);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (priv->entry, GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_key_pressed), popup);

    g_signal_connect_after (priv->entry, "changed", G_CALLBACK (on_text_changed), popup);
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

    gtk_widget_set_parent (GTK_WIDGET (popup), *fl);
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
        gtk_editable_set_text (GTK_EDITABLE (priv->entry), text);
        gtk_editable_set_position (GTK_EDITABLE (priv->entry), -1);
    }
}
