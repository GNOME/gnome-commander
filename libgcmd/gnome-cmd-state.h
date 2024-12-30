/**
 * @copyright (C) 2024-2025 Andrey Kutejko\n
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

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gnome-cmd-file-descriptor.h>

G_BEGIN_DECLS

#define GNOME_CMD_TYPE_STATE (gnome_cmd_state_get_type ())
G_DECLARE_DERIVABLE_TYPE (GnomeCmdState, gnome_cmd_state, GNOME_CMD, STATE, GObject)

struct _GnomeCmdStateClass
{
    GObjectClass parent_class;
};

GnomeCmdState *gnome_cmd_state_new();

GnomeCmdFileDescriptor *gnome_cmd_state_get_active_dir(GnomeCmdState *state);
void gnome_cmd_state_set_active_dir(GnomeCmdState *state, GnomeCmdFileDescriptor *active_dir);

GnomeCmdFileDescriptor *gnome_cmd_state_get_inactive_dir(GnomeCmdState *state);
void gnome_cmd_state_set_inactive_dir(GnomeCmdState *state, GnomeCmdFileDescriptor *inactive_dir);

GList *gnome_cmd_state_get_active_dir_files(GnomeCmdState *state);
void gnome_cmd_state_set_active_dir_files(GnomeCmdState *state, GList *active_dir_files);

GList *gnome_cmd_state_get_inactive_dir_files(GnomeCmdState *state);
void gnome_cmd_state_set_inactive_dir_files(GnomeCmdState *state, GList *inactive_dir_files);

GList *gnome_cmd_state_get_active_dir_selected_files(GnomeCmdState *state);
void gnome_cmd_state_set_active_dir_selected_files(GnomeCmdState *state, GList *active_dir_selected_files);

GList *gnome_cmd_state_get_inactive_dir_selected_files(GnomeCmdState *state);
void gnome_cmd_state_set_inactive_dir_selected_files(GnomeCmdState *state, GList *inactive_dir_selected_files);

G_END_DECLS
