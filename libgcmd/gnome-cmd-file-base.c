/**
 * @file gnome-cmd-file-base.cc
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

#include "gnome-cmd-file-base.h"


G_DEFINE_TYPE (GnomeCmdFileBase, gnome_cmd_file_base, G_TYPE_OBJECT)


static void gnome_cmd_file_base_init (GnomeCmdFileBase *self)
{
}


static void gnome_cmd_file_base_finalize (GObject *object)
{
    GnomeCmdFileBase *self = GNOME_CMD_FILE_BASE (object);

    g_clear_object (&self->gFile);
    g_clear_object (&self->gFileInfo);

    G_OBJECT_CLASS (gnome_cmd_file_base_parent_class)->finalize (object);
}


static void gnome_cmd_file_base_class_init (GnomeCmdFileBaseClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = gnome_cmd_file_base_finalize;
}

GFile *gnome_cmd_file_base_get_file(GnomeCmdFileBase *file_base)
{
    return file_base->gFile;
}

GFileInfo *gnome_cmd_file_base_get_file_info(GnomeCmdFileBase *file_base)
{
    return file_base->gFileInfo;
}
