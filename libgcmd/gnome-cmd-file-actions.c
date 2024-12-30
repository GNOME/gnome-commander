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
