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
