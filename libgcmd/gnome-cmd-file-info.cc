/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2015 Uwe Scholz

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

#include <config.h>

#include "libgcmd-deps.h"
#include "gnome-cmd-file-info.h"

using namespace std;


G_DEFINE_TYPE (GnomeCmdFileInfo, gnome_cmd_file_info, G_TYPE_OBJECT)


static void gnome_cmd_file_info_init (GnomeCmdFileInfo *self)
{
}


static void gnome_cmd_file_info_finalize (GObject *object)
{
    GnomeCmdFileInfo *self = GNOME_CMD_FILE_INFO (object);

    gnome_vfs_file_info_unref (self->info);
    if (self->uri)
        gnome_vfs_uri_unref (self->uri);

    G_OBJECT_CLASS (gnome_cmd_file_info_parent_class)->finalize (object);
}


static void gnome_cmd_file_info_class_init (GnomeCmdFileInfoClass *klass)
{
    gnome_cmd_file_info_parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

    GObjectClass *object_class = (GObjectClass *) klass;

    object_class->finalize = gnome_cmd_file_info_finalize;
}
