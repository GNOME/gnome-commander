/*
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * For more details see the file COPYING.
 */


#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gnome-cmd-state.h>

G_BEGIN_DECLS

#define GNOME_CMD_TYPE_FILE_ACTIONS (gnome_cmd_file_actions_get_type ())

G_DECLARE_INTERFACE (GnomeCmdFileActions,
                     gnome_cmd_file_actions,
                     GNOME_CMD,
                     FILE_ACTIONS,
                     GObject)

struct _GnomeCmdFileActionsInterface
{
    GTypeInterface g_iface;

    GMenuModel *(* create_main_menu) (GnomeCmdFileActions *fa);
    GMenuModel *(* create_popup_menu_items) (GnomeCmdFileActions *fa, GnomeCmdState *state);

    void (* execute) (GnomeCmdFileActions *fa,
                      const gchar *action,
                      GVariant *parameter,
                      GtkWindow *parent_window,
                      GnomeCmdState *state);
};

/**
 * gnome_cmd_file_actions_create_main_menu:
 *
 * Creates a menu model to be shown in an application's main menu.
 *
 * Returns: (transfer full): a menu model
 */
GMenuModel *gnome_cmd_file_actions_create_main_menu (GnomeCmdFileActions *fa);

/**
 * gnome_cmd_file_actions_create_popup_menu_items:
 * @state: (transfer none): current state of both application's panels
 *
 * Creates a menu model to be shown in a context popup menu
 *
 * Returns: (transfer full): a menu model
 */
GMenuModel *gnome_cmd_file_actions_create_popup_menu_items (GnomeCmdFileActions *fa, GnomeCmdState *state);

/**
 * gnome_cmd_file_actions_execute:
 * @action: (transfer none): name of the action
 * @parameter: (transfer none): parameter of the action
 * @parent_window: (transfer none): a window, plug-in may use as a parent (`transient-for`)
 * @state: (transfer none): current state of both application's panels
 *
 * Executes a custom action defined by a plug-in
 */
void gnome_cmd_file_actions_execute (GnomeCmdFileActions *fa,
                                     const gchar *action,
                                     GVariant *parameter,
                                     GtkWindow *parent_window,
                                     GnomeCmdState *state);

G_END_DECLS
