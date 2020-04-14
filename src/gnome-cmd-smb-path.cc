/**
 * @file gnome-cmd-smb-path.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2020 Uwe Scholz\n
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
#include "gnome-cmd-smb-path.h"
#include "gnome-cmd-smb-net.h"
#include "utils.h"

using namespace std;


inline void GnomeCmdSmbPath::set_resources(const gchar *set_res_workgroup, const gchar *set_res_resource, const gchar *set_res_path)
{
    this->workgroup = g_strdup (set_res_workgroup);

    if (set_res_workgroup)
    {
        if (set_res_resource)
        {
            this->resource = g_strdup (set_res_resource);
            this->resource_path = g_strdup (set_res_path);
            this->path = g_strconcat (G_DIR_SEPARATOR_S, set_res_resource, set_res_path, nullptr);
        }
        else
            this->path = g_strconcat (G_DIR_SEPARATOR_S, set_res_workgroup, nullptr);
    }
    else
        this->path = g_strdup (G_DIR_SEPARATOR_S);

    this->display_path = unix_to_unc (this->path);
}


GnomeCmdPath *GnomeCmdSmbPath::get_parent()
{
    if (!workgroup)
        return nullptr;

    gchar *a = nullptr,
          *b = nullptr,
          *c = nullptr;

    if (resource)
    {
        if (resource_path)
        {
            GnomeVFSURI *t = gnome_vfs_uri_new (G_DIR_SEPARATOR_S);
            GnomeVFSURI *u1 = gnome_vfs_uri_append_path (t, resource_path);
            gnome_vfs_uri_unref (t);

            if (u1 && gnome_vfs_uri_has_parent (u1))
            {
                GnomeVFSURI *u2 = gnome_vfs_uri_get_parent (u1);
                g_return_val_if_fail (u2 != nullptr, nullptr);

                gchar *s = gnome_vfs_uri_to_string (u2, GNOME_VFS_URI_HIDE_PASSWORD);
                gnome_vfs_uri_unref (u2);

                c = gnome_vfs_get_local_path_from_uri (s);
                g_free (s);
            }

            b = resource;
            gnome_vfs_uri_unref (u1);
        }

        a = workgroup;
    }

    return new GnomeCmdSmbPath(a, b, c);
}


 GnomeCmdPath *GnomeCmdSmbPath::get_child(const gchar *child)
{
    g_return_val_if_fail (child != nullptr, nullptr);
    g_return_val_if_fail (child[0] != '/', nullptr);

    gchar *a = nullptr,
          *b = nullptr,
          *c = nullptr;

    if (workgroup)
    {
        if (resource)
        {
            if (resource_path)
            {
                GnomeVFSURI *u1, *u2;

                GnomeVFSURI *t = gnome_vfs_uri_new (G_DIR_SEPARATOR_S);
                u1 = gnome_vfs_uri_append_path (t, resource_path);
                gnome_vfs_uri_unref (t);
                if (!strchr (child, '/'))
                    u2 = gnome_vfs_uri_append_file_name (u1, child);
                else
                    u2 = gnome_vfs_uri_append_path (u1, child);
                gnome_vfs_uri_unref (u1);
                g_return_val_if_fail (u2 != nullptr, nullptr);

                c = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (u2), 0);
                gnome_vfs_uri_unref (u2);
            }
            else
                c = g_strdup_printf ("/%s", child);

            b = g_strdup (resource);
        }
        else
            b = g_strdup (child);

        a = g_strdup (workgroup);
    }
    else
        a = g_strdup (child);

    GnomeCmdPath *out = new GnomeCmdSmbPath(a, b, c);
    g_free (a);
    g_free (b);
    g_free (c);

    return out;
}


GnomeCmdSmbPath::GnomeCmdSmbPath(const gchar *constr_workgroup, const gchar *constr_resource, const gchar *constr_resource_path): resource(0), resource_path(0)
{
    set_resources(constr_workgroup,constr_resource,constr_resource_path);
}


GnomeCmdSmbPath::GnomeCmdSmbPath(const gchar *path_str): workgroup(0), resource(0), resource_path(0), path(0), display_path(0)
{
    g_return_if_fail (path_str != nullptr);

    gchar *s, *t;
    gchar *c = nullptr;

    DEBUG('s', "Creating smb-path for %s\n", path_str);

    s = t = g_strdup (path_str);

    // Replace '\' with '/'
    g_strdelimit (s, "\\", '/');

    // Eat up all leading slashes
    for (; *s && *s=='/'; ++s);

    if (!*s)
    {
        g_free (t);
        return;
    }

    gchar **v = g_strsplit (s, G_DIR_SEPARATOR_S, 0);
    g_free (t);

    if (v[0] != nullptr)
    {
        gchar *a = nullptr;
        gchar *b = nullptr;

        a = g_strdup (v[0]);
        if (v[1] != nullptr)
        {
            b = g_strdup (v[1]);
            if (v[2] != nullptr)
            {
                c = g_strconcat (G_DIR_SEPARATOR_S, v[2], nullptr);
                if (v[3] != nullptr)
                {
                    gchar *t1 = c;
                    gchar *t2 = g_strjoinv (G_DIR_SEPARATOR_S, &v[3]);
                    c = g_strjoin (G_DIR_SEPARATOR_S, t1, t2, nullptr);
                    g_free (t1);
                    g_free (t2);
                }
            }
        }

        SmbEntity *ent = gnome_cmd_smb_net_get_entity (a);

        if (ent)
        {
            if (ent->type == SMB_WORKGROUP)
                set_resources(a, b, c);
            else
            {
                if (!b)
                    b = (char*) "/";
                b = c ? g_strconcat (G_DIR_SEPARATOR_S, b, c, nullptr) : g_strdup (b);
                g_free (c);
                set_resources(ent->workgroup_name, a, b);
            }
        }
        else
            g_warning ("Can't find a host or workgroup named %s", a);
    }
    else
        set_resources(nullptr, nullptr, nullptr);
}
