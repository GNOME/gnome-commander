/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

/**
 * gnome_cmd_state_get_active_dir:
 *
 * Returns: (transfer none): a file descriptor of an active directory
 */
GnomeCmdFileDescriptor *gnome_cmd_state_get_active_dir(GnomeCmdState *state);

/**
 * gnome_cmd_state_set_active_dir:
 * @active_dir: (transfer none): a file descriptor
 */
void gnome_cmd_state_set_active_dir(GnomeCmdState *state, GnomeCmdFileDescriptor *active_dir);

/**
 * gnome_cmd_state_get_inactive_dir:
 *
 * Returns: (transfer none): a file descriptor of an inactive directory
 */
GnomeCmdFileDescriptor *gnome_cmd_state_get_inactive_dir(GnomeCmdState *state);

/**
 * gnome_cmd_state_set_inactive_dir:
 * @inactive_dir: (transfer none): a file descriptor
 */
void gnome_cmd_state_set_inactive_dir(GnomeCmdState *state, GnomeCmdFileDescriptor *inactive_dir);

/**
 * gnome_cmd_state_get_active_dir_files:
 *
 * Returns: (element-type GnomeCmdFileDescriptor) (transfer none): a list of file descriptors in an active directory
 */
GList *gnome_cmd_state_get_active_dir_files(GnomeCmdState *state);

/**
 * gnome_cmd_state_set_active_dir_files:
 * @active_dir_files: (element-type GnomeCmdFileDescriptor) (transfer none): a list of file descriptors
 */
void gnome_cmd_state_set_active_dir_files(GnomeCmdState *state, GList *active_dir_files);

/**
 * gnome_cmd_state_get_inactive_dir_files:
 *
 * Returns: (element-type GnomeCmdFileDescriptor) (transfer none): a list of file descriptors in an inactive directory
 */
GList *gnome_cmd_state_get_inactive_dir_files(GnomeCmdState *state);

/**
 * gnome_cmd_state_set_inactive_dir_files:
 * @inactive_dir_files: (element-type GnomeCmdFileDescriptor) (transfer none): a list of file descriptors
 */
void gnome_cmd_state_set_inactive_dir_files(GnomeCmdState *state, GList *inactive_dir_files);

/**
 * gnome_cmd_state_get_active_dir_selected_files:
 *
 * Returns: (element-type GnomeCmdFileDescriptor) (transfer none): a list of selected file descriptors in an active directory
 */
GList *gnome_cmd_state_get_active_dir_selected_files(GnomeCmdState *state);

/**
 * gnome_cmd_state_set_active_dir_selected_files:
 * @active_dir_selected_files: (element-type GnomeCmdFileDescriptor) (transfer none): a list of file descriptors
 */
void gnome_cmd_state_set_active_dir_selected_files(GnomeCmdState *state, GList *active_dir_selected_files);

/**
 * gnome_cmd_state_get_inactive_dir_selected_files:
 *
 * Returns: (element-type GnomeCmdFileDescriptor) (transfer none): a list of selected file descriptors in an inactive directory
 */
GList *gnome_cmd_state_get_inactive_dir_selected_files(GnomeCmdState *state);

/**
 * gnome_cmd_state_set_inactive_dir_selected_files:
 * @inactive_dir_selected_files: (element-type GnomeCmdFileDescriptor) (transfer none): a list of file descriptors
 */
void gnome_cmd_state_set_inactive_dir_selected_files(GnomeCmdState *state, GList *inactive_dir_selected_files);

G_END_DECLS
