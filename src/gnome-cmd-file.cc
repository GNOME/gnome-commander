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
extern "C" GnomeCmdCon *file_get_connection (GnomeCmdFile *f);


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
    klass->get_connection = file_get_connection;
}


/***********************************
 * Public functions
 ***********************************/

GnomeCmdFile *gnome_cmd_file_new (GFileInfo *gFileInfo, GnomeCmdDir *dir)
{
    g_return_val_if_fail (gFileInfo != nullptr, nullptr);
    g_return_val_if_fail (dir != nullptr, nullptr);

    GFile *gFile = g_file_get_child (
        gnome_cmd_file_descriptor_get_file (GNOME_CMD_FILE_DESCRIPTOR (dir)),
        g_file_info_get_name (gFileInfo));

    g_return_val_if_fail (gFile != nullptr, nullptr);

    return gnome_cmd_file_new_full (gFileInfo, gFile, dir);
}


//ToDo: Try to remove usage of this method.
gchar *GnomeCmdFile::get_real_path()
{
    auto gFileTmp = get_file();

    if (!gFileTmp)
        return nullptr;

    gchar *path = g_file_get_path (gFileTmp);

    return path;
}


gchar *GnomeCmdFile::get_uri_str()
{
    return g_file_get_uri(this->get_file());
}

gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_real_path();
}

gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_uri_str();
}

GnomeCmdCon *gnome_cmd_file_get_connection (GnomeCmdFile *f)
{
    GnomeCmdFileClass *klass = GNOME_CMD_FILE_GET_CLASS (f);
    return klass->get_connection (f);
}

gboolean gnome_cmd_file_is_local (GnomeCmdFile *f)
{
    GnomeCmdCon *con = gnome_cmd_file_get_connection (f);
    return gnome_cmd_con_is_local (con);
}
