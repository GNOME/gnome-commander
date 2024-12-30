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

G_BEGIN_DECLS

typedef struct GnomeCmdState
{
    GFile *activeDirGfile;
    GFile *inactiveDirGfile;
    GList *active_dir_files;
    GList *inactive_dir_files;
    GList *active_dir_selected_files;
    GList *inactive_dir_selected_files;
} GnomeCmdState;


#define GNOME_CMD_TYPE_FILE_ACTIONS (gnome_cmd_file_actions_get_type ())

G_DECLARE_INTERFACE (GnomeCmdFileActions,
                     gnome_cmd_file_actions,
                     GNOME_CMD,
                     FILE_ACTIONS,
                     GObject)

struct _GnomeCmdFileActionsInterface
{
    GTypeInterface g_iface;

    GSimpleActionGroup *(* create_actions) (GnomeCmdFileActions *fa, const gchar *name);
    GMenuModel *(* create_main_menu) (GnomeCmdFileActions *fa);
    GMenuModel *(* create_popup_menu_items) (GnomeCmdFileActions *fa, GnomeCmdState *state);
};

GSimpleActionGroup *gnome_cmd_file_actions_create_actions (GnomeCmdFileActions *fa, const gchar *name);
GMenuModel *gnome_cmd_file_actions_create_main_menu (GnomeCmdFileActions *fa);
GMenuModel *gnome_cmd_file_actions_create_popup_menu_items (GnomeCmdFileActions *fa, GnomeCmdState *state);

G_END_DECLS
