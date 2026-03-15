// SPDX-FileCopyrightText: 2003 Marcus Bjurman
// SPDX-FileCopyrightText: 2005 Piotr Eljasiak
// SPDX-FileCopyrightText: 2014 Uwe Scholz <u.scholz83@gmx.de>
// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#define GNOME_CMD_TYPE_TEST_PLUGIN (gnome_cmd_test_plugin_get_type ())
G_DECLARE_FINAL_TYPE (GnomeCmdTestPlugin, gnome_cmd_test_plugin, GNOME_CMD, TEST_PLUGIN, GObject)

extern "C"
{
    GObject        *create_plugin   ();
    GnomeCmdPluginInfo     *get_plugin_info ();
}
