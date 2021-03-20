/** 
 * @file gnome-cmd-plain-path.cc
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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-plain-path.h"

using namespace std;


GnomeCmdPath *GnomeCmdPlainPath::get_parent()
{
    GnomeVFSURI *t = gnome_vfs_uri_new (G_DIR_SEPARATOR_S);
    GnomeVFSURI *u1 = gnome_vfs_uri_append_path (t, path);
    gnome_vfs_uri_unref (t);

    GnomeVFSURI *u2 = gnome_vfs_uri_get_parent (u1);
    gnome_vfs_uri_unref (u1);

    if (!u2)  return NULL;

    gchar *s = gnome_vfs_uri_to_string (u2, GNOME_VFS_URI_HIDE_PASSWORD);
    gnome_vfs_uri_unref (u2);

    GnomeCmdPath *parent_path = new GnomeCmdPlainPath (gnome_vfs_get_local_path_from_uri (s));
    g_free (s);

    return parent_path;
}


GnomeCmdPath *GnomeCmdPlainPath::get_child(const gchar *child)
{
    auto fullPath = *path == '/'
                        ? strchr (child, '/') == nullptr
                            ? g_strconcat(path, G_DIR_SEPARATOR_S, child, nullptr)
                            : g_strconcat(path, child, nullptr)
                        : strchr (child, '/') == nullptr
                            ? g_strconcat(G_DIR_SEPARATOR_S, path, G_DIR_SEPARATOR_S, child, nullptr)
                            : g_strconcat(G_DIR_SEPARATOR_S, path, child, nullptr);

    auto childGFile = g_file_new_for_path(fullPath);

    // It could be that in the meantime the file is deleted again.
    // Therefore we have to check if it really exists.
    if (!g_file_query_exists (childGFile, nullptr))
    {
        g_object_unref(childGFile);
        return nullptr;
    }
    gchar *pathString = g_file_get_path(childGFile);
    g_object_unref(childGFile);

    GnomeCmdPath *child_path = new GnomeCmdPlainPath(pathString);
    g_free (pathString);

    return child_path;
}
