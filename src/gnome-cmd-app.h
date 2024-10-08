/** 
 * @file gnome-cmd-app.h
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

enum AppTarget
{
    APP_TARGET_ALL_FILES,
    APP_TARGET_ALL_DIRS,
    APP_TARGET_ALL_DIRS_AND_FILES,
    APP_TARGET_SOME_FILES
};


struct GnomeCmdApp;


extern "C" GnomeCmdApp *gnome_cmd_app_new_from_app_info (GAppInfo *gAppInfo);

extern "C" GnomeCmdApp *gnome_cmd_app_new_with_values (const gchar *name,
                                            const gchar *cmd,
                                            const gchar *icon_path,
                                            AppTarget target,
                                            const gchar *pattern_string,
                                            gboolean handles_uris,
                                            gboolean handles_multiple,
                                            gboolean requires_terminal,
                                            GAppInfo *gAppInfo);

extern "C" void gnome_cmd_app_free (GnomeCmdApp *app);

extern "C" gchar *gnome_cmd_app_get_name (GnomeCmdApp *app);
extern "C" void gnome_cmd_app_set_name (GnomeCmdApp *app, const gchar *name);

extern "C" gchar *gnome_cmd_app_get_command (GnomeCmdApp *app);
extern "C" void gnome_cmd_app_set_command (GnomeCmdApp *app, const gchar *cmd);

extern "C" GIcon *gnome_cmd_app_get_icon (GnomeCmdApp *app);

extern "C" gchar *gnome_cmd_app_get_icon_path (GnomeCmdApp *app);
extern "C" void gnome_cmd_app_set_icon_path (GnomeCmdApp *app, const gchar *icon_path);

extern "C" AppTarget gnome_cmd_app_get_target (GnomeCmdApp *app);
extern "C" void gnome_cmd_app_set_target (GnomeCmdApp *app, AppTarget target);

extern "C" gchar *gnome_cmd_app_get_pattern_string (GnomeCmdApp *app);
extern "C" void gnome_cmd_app_set_pattern_string (GnomeCmdApp *app, const gchar *extensions);

extern "C" GList *gnome_cmd_app_get_pattern_list (GnomeCmdApp *app);

extern "C" gboolean gnome_cmd_app_get_handles_uris (GnomeCmdApp *app);
extern "C" void gnome_cmd_app_set_handles_uris (GnomeCmdApp *app, gboolean handles_uris);

extern "C" gboolean gnome_cmd_app_get_handles_multiple (GnomeCmdApp *app);
extern "C" void gnome_cmd_app_set_handles_multiple (GnomeCmdApp *app, gboolean handles_multiple);

extern "C" gboolean gnome_cmd_app_get_requires_terminal (GnomeCmdApp *app);
extern "C" void gnome_cmd_app_set_requires_terminal (GnomeCmdApp *app, gboolean requires_terminal);

extern "C" GAppInfo *gnome_cmd_app_get_app_info (GnomeCmdApp *app);

extern "C" GVariant *gnome_cmd_app_save_to_variant (GnomeCmdApp *app);
extern "C" GnomeCmdApp *gnome_cmd_app_load_from_variant (GVariant *variant);
