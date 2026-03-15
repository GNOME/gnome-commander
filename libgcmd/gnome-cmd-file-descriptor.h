/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GNOME_CMD_TYPE_FILE_DESCRIPTOR (gnome_cmd_file_descriptor_get_type ())

G_DECLARE_INTERFACE (GnomeCmdFileDescriptor,
                     gnome_cmd_file_descriptor,
                     GNOME_CMD,
                     FILE_DESCRIPTOR,
                     GObject)

struct _GnomeCmdFileDescriptorInterface
{
    GTypeInterface g_iface;

    GFile       *(*get_file)        (GnomeCmdFileDescriptor *fd);
    GFileInfo   *(*get_file_info)   (GnomeCmdFileDescriptor *fd);
};

/**
 * gnome_cmd_file_descriptor_get_file:
 *
 * Returns: (transfer none): the file
 */
GFile *gnome_cmd_file_descriptor_get_file (GnomeCmdFileDescriptor *fd);

/**
 * gnome_cmd_file_descriptor_get_file_info:
 *
 * Returns: (transfer none): the file info
 */
GFileInfo *gnome_cmd_file_descriptor_get_file_info (GnomeCmdFileDescriptor *fd);

G_END_DECLS
