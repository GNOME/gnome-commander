/**
 * @file gnome-cmd-application.h
 * @copyright (C) 2024 Andrey Kutejko\n
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

#include <gtk/gtk.h>

#define GNOME_CMD_TYPE_APPLICATION (gnome_cmd_application_get_type ())
G_DECLARE_FINAL_TYPE (GnomeCmdApplication, gnome_cmd_application, GNOME_CMD_APPLICATION, WINDOW, GtkApplication)

GnomeCmdApplication *gnome_cmd_application_new ();
