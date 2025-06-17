/**
 * @file gnome-cmd-dir-indicator.cc
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
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "widget-factory.h"
#include "text-utils.h"

using namespace std;


struct GnomeCmdDirIndicatorPrivate
{
    GtkWidget *directory_indicator;
    GnomeCmdFileSelector *fs;
};


enum {
    NAVIGATE,
    LAST_SIGNAL
};


static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdDirIndicator, gnome_cmd_dir_indicator, GTK_TYPE_WIDGET)


extern "C" GType gnome_cmd_file_directory_indicator_get_type ();


/*******************************
 * Gtk class implementation
 *******************************/
static void dispose (GObject *object)
{
    while (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (object)))
        gtk_widget_unparent (child);

    G_OBJECT_CLASS (gnome_cmd_dir_indicator_parent_class)->dispose (object);
}


static void gnome_cmd_dir_indicator_class_init (GnomeCmdDirIndicatorClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = dispose;

    signals[NAVIGATE] =
        g_signal_new ("navigate",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirIndicatorClass, navigate),
            NULL, NULL, NULL,
            G_TYPE_NONE,
            2, G_TYPE_STRING, G_TYPE_BOOLEAN);
}


/*******************************
 * Event handlers
 *******************************/
static void on_directory_indicator_navigate (GtkWidget *directory_indicator, const gchar *path, gboolean new_tab, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (user_data));
    auto indicator = GNOME_CMD_DIR_INDICATOR (user_data);
    g_signal_emit (indicator, signals[NAVIGATE], 0, path, new_tab);
}


void gnome_cmd_dir_indicator_show_history (GnomeCmdDirIndicator *indicator)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    GnomeCmdCon *con = priv->fs->get_connection();
    GStrv dir_history = gnome_cmd_con_export_dir_history (con);

    GMenu *menu = g_menu_new ();
    for (GStrv l = dir_history; *l; ++l)
    {
        gchar *path = *l;

        GMenuItem *item = g_menu_item_new (path, nullptr);
        g_menu_item_set_action_and_target (item, "fs.select-path", "s", path);
        g_menu_append_item (menu, item);
    }
    g_strfreev (dir_history);

    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (popover, GTK_WIDGET (indicator));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

    GtkAllocation indicator_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (indicator), &indicator_allocation);

    if (indicator_allocation.width > 100)
        gtk_widget_set_size_request (popover, indicator_allocation.width, -1);

    gtk_popover_popup (GTK_POPOVER (popover));
}


void gnome_cmd_dir_indicator_show_bookmarks (GnomeCmdDirIndicator *indicator)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    GnomeCmdCon *con = priv->fs->get_connection();
    GListModel *bookmarks = gnome_cmd_con_get_bookmarks (con);

    GMenu *bookmarks_section = g_menu_new ();
    for (guint i = 0; i < g_list_model_get_n_items (bookmarks); ++i)
    {
        auto bookmark = static_cast<GnomeCmdBookmark*> (g_list_model_get_item(bookmarks, i));

        gchar *name = gnome_cmd_bookmark_get_name (bookmark);
        gchar *path = gnome_cmd_bookmark_get_path (bookmark);

        GMenuItem *item = g_menu_item_new (name, nullptr);
        g_menu_item_set_action_and_target (item, "fs.select-path", "s", path);
        g_menu_append_item (bookmarks_section, item);

        g_free (name);
        g_free (path);
    }

    GMenu *manage_section = g_menu_new ();
    g_menu_append (manage_section, _("Add current dir"), "fs.add-bookmark");
    g_menu_append (manage_section, _("Manage bookmarks…"), "win.bookmarks-edit");

    GMenu *menu = g_menu_new ();
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (bookmarks_section));
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (manage_section));

    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (popover, GTK_WIDGET (indicator));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

    GtkAllocation indicator_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (indicator), &indicator_allocation);

    if (indicator_allocation.width > 100)
        gtk_widget_set_size_request (popover, indicator_allocation.width, -1);

    gtk_popover_popup (GTK_POPOVER (popover));
}


static void gnome_cmd_dir_indicator_init (GnomeCmdDirIndicator *indicator)
{
    auto layout_manager = gtk_box_layout_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_layout_manager (GTK_WIDGET (indicator), layout_manager);

    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    priv->directory_indicator = GTK_WIDGET (g_object_new (gnome_cmd_file_directory_indicator_get_type (), nullptr));
    g_signal_connect (priv->directory_indicator, "navigate", G_CALLBACK (on_directory_indicator_navigate), indicator);

    GtkWidget *sw = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), priv->directory_indicator);
    gtk_widget_set_hexpand (sw, TRUE);
    gtk_widget_set_margin_start (sw, 6);
    gtk_widget_set_margin_end (sw, 6);
    gtk_widget_set_parent (sw, GTK_WIDGET (indicator));

    // create the bookmark popup button
    auto bookmark_button = gtk_button_new_from_icon_name ("gnome-commander-bookmark-outline");
    gtk_widget_set_can_focus (bookmark_button, FALSE);
    gtk_button_set_has_frame (GTK_BUTTON (bookmark_button), FALSE);
    gtk_widget_set_parent (bookmark_button, GTK_WIDGET (indicator));

    // create the history popup button
    auto history_button = gtk_button_new_from_icon_name ("gnome-commander-down");
    gtk_widget_set_can_focus (history_button, FALSE);
    gtk_button_set_has_frame (GTK_BUTTON (history_button), FALSE);
    gtk_widget_set_parent (history_button, GTK_WIDGET (indicator));

    g_signal_connect_swapped (history_button, "clicked", G_CALLBACK (gnome_cmd_dir_indicator_show_history), indicator);
    g_signal_connect_swapped (bookmark_button, "clicked", G_CALLBACK (gnome_cmd_dir_indicator_show_bookmarks), indicator);
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_dir_indicator_new (GnomeCmdFileSelector *fs)
{
    auto dir_indicator = static_cast<GnomeCmdDirIndicator*> (g_object_new (GNOME_CMD_TYPE_DIR_INDICATOR, nullptr));
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (dir_indicator));

    priv->fs = fs;

    return GTK_WIDGET (dir_indicator);
}


void gnome_cmd_dir_indicator_set_dir (GnomeCmdDirIndicator *indicator, GnomeCmdDir *dir)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));
    g_object_set (priv->directory_indicator, "directory", dir, nullptr);
}


void gnome_cmd_dir_indicator_set_active (GnomeCmdDirIndicator *indicator, gboolean active)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));
    g_object_set (priv->directory_indicator, "active", active, nullptr);
}
