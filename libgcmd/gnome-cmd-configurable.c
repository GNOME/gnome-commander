/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gnome-cmd-configurable.h"

G_DEFINE_INTERFACE (GnomeCmdConfigurable, gnome_cmd_configurable, G_TYPE_OBJECT);

static void gnome_cmd_configurable_default_init (GnomeCmdConfigurableInterface *iface)
{
}

void gnome_cmd_configurable_configure (GnomeCmdConfigurable *cfg, GtkWindow *parent_window)
{
    g_return_if_fail (GNOME_CMD_IS_CONFIGURABLE (cfg));
    GnomeCmdConfigurableInterface *iface = GNOME_CMD_CONFIGURABLE_GET_IFACE (cfg);
    (* iface->configure) (cfg, parent_window);
}
