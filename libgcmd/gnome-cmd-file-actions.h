/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
