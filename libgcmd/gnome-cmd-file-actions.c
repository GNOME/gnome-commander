/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gnome-cmd-file-actions.h"

G_DEFINE_INTERFACE (GnomeCmdFileActions, gnome_cmd_file_actions, G_TYPE_OBJECT);

static void gnome_cmd_file_actions_default_init (GnomeCmdFileActionsInterface *iface)
{
}

GMenuModel *gnome_cmd_file_actions_create_main_menu (GnomeCmdFileActions *fa)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_ACTIONS (fa), NULL);
    GnomeCmdFileActionsInterface *iface = GNOME_CMD_FILE_ACTIONS_GET_IFACE (fa);
    return (* iface->create_main_menu) (fa);
}

GMenuModel *gnome_cmd_file_actions_create_popup_menu_items (GnomeCmdFileActions *fa, GnomeCmdState *state)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_ACTIONS (fa), NULL);
    GnomeCmdFileActionsInterface *iface = GNOME_CMD_FILE_ACTIONS_GET_IFACE (fa);
    return (* iface->create_popup_menu_items) (fa, state);
}

void gnome_cmd_file_actions_execute (GnomeCmdFileActions *fa,
                                     const gchar *action,
                                     GVariant *parameter,
                                     GtkWindow *parent_window,
                                     GnomeCmdState *state)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_ACTIONS (fa));
    GnomeCmdFileActionsInterface *iface = GNOME_CMD_FILE_ACTIONS_GET_IFACE (fa);
    (* iface->execute) (fa, action, parameter, parent_window, state);
}
