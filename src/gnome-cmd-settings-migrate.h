/**
 * @file gnome-cmd-settings-migrate.h
 * @brief Header file for functions and other stuff for migrating xml
 * settings to Gnomes GSettings
 * @copyright (C) 2013-2016 Uwe Scholz\n
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

#ifndef __GNOME_CMD_SETTINGS_MIGRATE_H__
#define __GNOME_CMD_SETTINGS_MIGRATE_H__

#include <glib-object.h>
#include <glib.h>
#include "gnome-cmd-settings.h"

G_BEGIN_DECLS

extern gchar *config_dir;

gboolean load_xml_into_gsettings (const gchar *fname);

G_END_DECLS

#endif // __GNOME_CMD_SETTINGS_H__
