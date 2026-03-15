// SPDX-FileCopyrightText: 2003 Marcus Bjurman
// SPDX-FileCopyrightText: 2005 Piotr Eljasiak
// SPDX-FileCopyrightText: 2014 Uwe Scholz <u.scholz83@gmx.de>
// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#define PLUGIN_TYPE_SETTINGS (plugin_settings_get_type ())
G_DECLARE_FINAL_TYPE (PluginSettings, plugin_settings, GCMD, SETTINGS, GObject)

PluginSettings *plugin_settings_new (void);


#define GNOME_CMD_TYPE_FILE_ROLLER_PLUGIN (gnome_cmd_file_roller_plugin_get_type ())
G_DECLARE_FINAL_TYPE (GnomeCmdFileRollerPlugin, gnome_cmd_file_roller_plugin, GNOME_CMD, FILE_ROLLER_PLUGIN, GObject)


extern "C"
{
    GObject        *create_plugin ();
    GnomeCmdPluginInfo     *get_plugin_info ();
}
