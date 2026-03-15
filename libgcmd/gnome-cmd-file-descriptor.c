/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gnome-cmd-file-descriptor.h"

G_DEFINE_INTERFACE (GnomeCmdFileDescriptor, gnome_cmd_file_descriptor, G_TYPE_OBJECT);

static void gnome_cmd_file_descriptor_default_init (GnomeCmdFileDescriptorInterface *iface)
{
}

GFile *gnome_cmd_file_descriptor_get_file (GnomeCmdFileDescriptor *fd)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_DESCRIPTOR (fd), NULL);
    GnomeCmdFileDescriptorInterface *iface = GNOME_CMD_FILE_DESCRIPTOR_GET_IFACE (fd);
    return (* iface->get_file) (fd);
}

GFileInfo *gnome_cmd_file_descriptor_get_file_info (GnomeCmdFileDescriptor *fd)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_DESCRIPTOR (fd), NULL);
    GnomeCmdFileDescriptorInterface *iface = GNOME_CMD_FILE_DESCRIPTOR_GET_IFACE (fd);
    return (* iface->get_file_info) (fd);
}
