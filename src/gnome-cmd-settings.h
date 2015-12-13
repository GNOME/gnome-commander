/**
 * @file gnome-cmd-settings.h
 * @brief Functions for storing settings of GCMD using GSettings
 * @copyright (C) 2013-2015 Uwe Scholz\n
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

#ifndef __GNOME_CMD_SETTINGS_H__
#define __GNOME_CMD_SETTINGS_H__

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define GCMD_TYPE_SETTINGS (gcmd_settings_get_type ())

G_DECLARE_FINAL_TYPE (GcmdSettings, gcmd_settings, GCMD, SETTINGS, GObject)

GcmdSettings *gcmd_settings_new (void);

/* key constants */
#define GCMD_SETTINGS_USE_DEFAULT_FONT                "use-default-font"
#define GCMD_SETTINGS_PANEL_FONT                      "panel-font"
#define GCMD_SETTINGS_SYSTEM_FONT                     "monospace-font-name"

G_END_DECLS

#endif // __GNOME_CMD_SETTINGS_H__
