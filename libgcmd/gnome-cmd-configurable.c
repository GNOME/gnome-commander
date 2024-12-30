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
