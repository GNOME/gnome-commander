/**
 * @file gnome-cmd-user-actions.h
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

#pragma once

#include <string>
#include <map>

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-main-win.h"
#include "dict.h"

#define USER_ACTION_SETTINGS (gcmd_user_action_settings_get_type ())
G_DECLARE_FINAL_TYPE (GcmdUserActionSettings, gcmd_user_action_settings, GCMD, USER_ACTIONS, GObject)
GcmdUserActionSettings *gcmd_user_action_settings_new (void);

extern GcmdUserActionSettings *settings;


struct GnomeCmdShortcuts;

extern "C" GnomeCmdShortcuts *gnome_cmd_shortcuts_new ();
extern "C" GnomeCmdShortcuts *gnome_cmd_shortcuts_load_from_settings();
extern "C" void gnome_cmd_shortcuts_free(GnomeCmdShortcuts *a);
extern "C" void gnome_cmd_shortcuts_save_to_settings(GnomeCmdShortcuts *a);
extern "C" gboolean gnome_cmd_shortcuts_handle_key_event(GnomeCmdShortcuts *a, GnomeCmdMainWin *mw, guint keyval, guint mask);
extern "C" gchar *gnome_cmd_shortcuts_bookmark_shortcuts(GnomeCmdShortcuts *a, const gchar *bookmark_name);

extern GnomeCmdShortcuts *gcmd_shortcuts;

extern "C" GtkTreeModel *gnome_cmd_user_actions_create_model ();


extern "C" int spawn_async_r(const char *cwd, GList *files_list, const char *command_template, GError **error);
