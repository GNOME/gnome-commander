/**
 * @file gnome-cmd-file.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con.h"


static void gnome_cmd_file_descriptor_init (GnomeCmdFileDescriptorInterface *iface);
extern "C" GFile *gnome_cmd_file_get_file (GnomeCmdFileDescriptor *fd);
extern "C" GFileInfo *gnome_cmd_file_get_file_info (GnomeCmdFileDescriptor *fd);


G_DEFINE_TYPE_WITH_CODE (GnomeCmdFile, gnome_cmd_file, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (GNOME_CMD_TYPE_FILE_DESCRIPTOR, gnome_cmd_file_descriptor_init))


static void gnome_cmd_file_descriptor_init (GnomeCmdFileDescriptorInterface *iface)
{
    iface->get_file = gnome_cmd_file_get_file;
    iface->get_file_info = gnome_cmd_file_get_file_info;
}


static void gnome_cmd_file_init (GnomeCmdFile *f)
{
}


static void gnome_cmd_file_class_init (GnomeCmdFileClass *klass)
{
}
