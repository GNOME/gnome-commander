/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
