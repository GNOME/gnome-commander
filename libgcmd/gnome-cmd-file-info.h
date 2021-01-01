/** 
 * @file gnome-cmd-file-info.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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

#define GNOME_CMD_TYPE_FILE_INFO              (gnome_cmd_file_info_get_type ())
#define GNOME_CMD_FILE_INFO(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE_INFO, GnomeCmdFileInfo))
#define GNOME_CMD_FILE_INFO_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE_INFO, GnomeCmdFileInfoClass))
#define GNOME_CMD_IS_FILE_INFO(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE_INFO))
#define GNOME_CMD_IS_FILE_INFO_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE_INFO))
#define GNOME_CMD_FILE_INFO_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE_INFO, GnomeCmdFileInfoClass))


GType gnome_cmd_file_info_get_type ();


struct GnomeCmdFileInfo
{
    GObject parent;

    GnomeVFSURI *uri;
    GnomeVFSFileInfo *info;
    
    void setup(GnomeVFSURI *uri, GnomeVFSFileInfo *info);
};

struct GnomeCmdFileInfoClass
{
    GObjectClass parent_class;
};

inline void GnomeCmdFileInfo::setup(GnomeVFSURI *uri_setup, GnomeVFSFileInfo *info_setup)
{
    this->info = info_setup;
    this->uri = uri_setup;
}
