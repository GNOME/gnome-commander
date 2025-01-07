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
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GNOME_CMD_TYPE_CONFIGURABLE (gnome_cmd_configurable_get_type ())

G_DECLARE_INTERFACE (GnomeCmdConfigurable,
                     gnome_cmd_configurable,
                     GNOME_CMD,
                     CONFIGURABLE,
                     GObject)

struct _GnomeCmdConfigurableInterface
{
    GTypeInterface g_iface;

    void (* configure) (GnomeCmdConfigurable *cfg, GtkWindow *parent_window);
};

void gnome_cmd_configurable_configure (GnomeCmdConfigurable *cfg, GtkWindow *parent_window);

G_END_DECLS
