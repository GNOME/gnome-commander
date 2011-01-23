/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifndef __GNOME_CMD_FILE_INFO_H__
#define __GNOME_CMD_FILE_INFO_H__

#define GNOME_CMD_TYPE_FILE_INFO         (gnome_cmd_file_info_get_type ())
#define GNOME_CMD_FILE_INFO(obj)          GTK_CHECK_CAST (obj, GNOME_CMD_TYPE_FILE_INFO, GnomeCmdFileInfo)
#define GNOME_CMD_FILE_INFO_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, GNOME_CMD_TYPE_FILE_INFO, GnomeCmdFileInfoClass)
#define GNOME_CMD_IS_FILE_INFO(obj)       GTK_CHECK_TYPE (obj, GNOME_CMD_TYPE_FILE_INFO)


struct GnomeCmdFileInfo
{
    GtkObject parent;

    GnomeVFSURI *uri;
    GnomeVFSFileInfo *info;
    
    void setup(GnomeVFSURI *uri, GnomeVFSFileInfo *info);
};

struct GnomeCmdFileInfoClass
{
    GtkObjectClass parent_class;
};


GtkType gnome_cmd_file_info_get_type ();


inline void GnomeCmdFileInfo::setup(GnomeVFSURI *uri, GnomeVFSFileInfo *info)
{
    this->info = info;
    this->uri = uri;
}

#endif //__GNOME_CMD_FILE_INFO_H__
