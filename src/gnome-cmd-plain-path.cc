/** 
 * @file gnome-cmd-plain-path.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2014 Uwe Scholz\n
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

    gchar *s = gnome_vfs_uri_to_string (u2, GNOME_VFS_URI_HIDE_NONE);
    gnome_vfs_uri_unref (u2);

    GnomeCmdPath *parent_path = new GnomeCmdPlainPath (gnome_vfs_get_local_path_from_uri (s));
    g_free (s);

    return parent_path;
}


GnomeCmdPath *GnomeCmdPlainPath::get_child(const gchar *child)
{
    GnomeVFSURI *t = gnome_vfs_uri_new (G_DIR_SEPARATOR_S);
    GnomeVFSURI *u1 = gnome_vfs_uri_append_path (t, path);
    gnome_vfs_uri_unref (t);

    GnomeVFSURI *u2 = strchr (child, '/')==NULL ?
                      gnome_vfs_uri_append_file_name (u1, child) :
                      gnome_vfs_uri_append_path (u1, child);
    gnome_vfs_uri_unref (u1);

    if (!u2)  return NULL;

    gchar *path_str = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (u2), 0);
    gnome_vfs_uri_unref (u2);

    GnomeCmdPath *child_path = new GnomeCmdPlainPath(path_str);
    g_free (path_str);

    return child_path;
}
