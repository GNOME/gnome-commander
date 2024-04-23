/**
 * @file gnome-cmd-smb-path.cc
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

    gchar *workgroupString = nullptr,
          *resourceString = nullptr,
          *resourceParentString = nullptr;

    if (resource)
    {
        if (resource_path)
        {
            auto *gFileTmp = g_file_new_for_uri (G_DIR_SEPARATOR_S);
            auto resourcePathGFile = g_file_resolve_relative_path (gFileTmp, resource_path);
            g_object_unref(gFileTmp);

            if (g_file_has_parent (resourcePathGFile, nullptr))
            {
                auto resourceParentGFile = g_file_get_parent (resourcePathGFile);
                g_return_val_if_fail (resourceParentGFile != nullptr, nullptr);

                resourceParentString = g_file_get_path (resourceParentGFile);
                g_object_unref (resourceParentGFile);
            }

            resourceString = resource;
            g_object_unref (resourcePathGFile);
        }

        workgroupString = workgroup;
    }

    return new GnomeCmdSmbPath(workgroupString, resourceString, resourceParentString);
}


 GnomeCmdPath *GnomeCmdSmbPath::get_child(const gchar *child)
{
    g_return_val_if_fail (child != nullptr, nullptr);
    g_return_val_if_fail (child[0] != '/', nullptr);

    gchar *workgroupTmp = nullptr,
          *resourceTmp = nullptr,
          *resourcePathTmp = nullptr;

    if (workgroupTmp)
    {
        if (resource)
        {
            if (resource_path)
            {
                GFile *gFileTmp = g_file_new_for_uri (G_DIR_SEPARATOR_S);
                auto resourcePathGFile = g_file_resolve_relative_path (gFileTmp, resource_path);
                g_object_unref(gFileTmp);

                GFile *childGFileTmp;

                if (!strchr (child, '/'))
                    childGFileTmp = g_file_resolve_relative_path (resourcePathGFile, child+1);
                else
                    childGFileTmp = g_file_resolve_relative_path (resourcePathGFile, child);
                g_object_unref (resourcePathGFile);
                g_return_val_if_fail (childGFileTmp != nullptr, nullptr);

                auto childGFilePath = g_file_get_path (childGFileTmp);
                resourcePathTmp = g_uri_unescape_string (childGFilePath, 0);
                g_free(childGFilePath);
                g_object_unref (childGFileTmp);
            }
            else
                resourcePathTmp = g_strdup_printf ("/%s", child);

            resourceTmp = g_strdup (resource);
        }
        else
            resourceTmp = g_strdup (child);

        workgroupTmp = g_strdup (workgroupTmp);
    }
    else
        workgroupTmp = g_strdup (child);

    GnomeCmdPath *childGnomeCmdPath = new GnomeCmdSmbPath(workgroupTmp, resourceTmp, resourcePathTmp);
    g_free (workgroupTmp);
    g_free (resourceTmp);
    g_free (resourcePathTmp);

    return childGnomeCmdPath;
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
