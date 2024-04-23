/**
 * @file gnome-cmd-smb-net.cc
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
#include "gnome-cmd-smb-net.h"
#include "utils.h"

using namespace std;


static GHashTable *entities = nullptr;
static gchar *current_wg_name;


static void add_host_to_list (GFileInfo *info, GList **list)
{
    SmbEntity *ent = g_new (SmbEntity, 1);

    ent->name = g_strdup(g_file_info_get_name(info));
    ent->type = SMB_HOST;
    ent->workgroup_name = current_wg_name;

    *list = g_list_append (*list, ent);
}


static void add_workgroup_to_list (GFileInfo *gFileInfo, GList **list)
{
    SmbEntity *ent = g_new0 (SmbEntity, 1);

    ent->name = g_strdup(g_file_info_get_name(gFileInfo));
    ent->type = SMB_WORKGROUP;

    *list = g_list_append (*list, ent);
}


inline gboolean enumerate_smb_uri (const gchar *uriString, GList **list)
{
    GError *error = nullptr;
    auto gFileTmp = g_file_new_for_uri(uriString);

    auto gFileEnumerator = g_file_enumerate_children (gFileTmp,
                        "*",
                        G_FILE_QUERY_INFO_NONE,
                        nullptr,
                        &error);
    if(error)
    {
        g_warning("enumerate_smb_uri: Unable to enumerate %s children, error: %s", uriString, error->message);
        g_error_free(error);
        return false;
    }

    GFileInfo *gFileInfoTmp = nullptr;
    do
    {
        gFileInfoTmp = g_file_enumerator_next_file(gFileEnumerator, nullptr, &error);
        if(error)
        {
            g_critical("sync_list: Unable to enumerate next file, error: %s", error->message);
            break;
        }
        if (gFileInfoTmp)
        {
            *list = g_list_append(*list, gFileInfoTmp);
        }
    }
    while (gFileInfoTmp && !error);

    g_file_enumerator_close (gFileEnumerator, nullptr, nullptr);

    return true;
}


inline GList *get_hosts (const gchar *wg)
{
    GList *gFileInfoList = nullptr;

    gchar *uri_str = g_strdup_printf ("smb://%s", wg);
    auto result = enumerate_smb_uri (uri_str, &gFileInfoList);
    g_free (uri_str);

    GList *list = nullptr;

    if (result)
        g_list_foreach (gFileInfoList, (GFunc) add_host_to_list, &list);

    return list;
}


inline GList *get_workgroups ()
{
    GList *gFileInfoList = nullptr;

    auto result = enumerate_smb_uri ("smb:", &gFileInfoList);

    GList *smbEntitiesList = nullptr;

    if (result && gFileInfoList)
        g_list_foreach (gFileInfoList, (GFunc) add_workgroup_to_list, &smbEntitiesList);

    g_list_free(gFileInfoList);

    return smbEntitiesList;
}


static void add_host_to_map (SmbEntity *ent)
{
    DEBUG ('s', "Discovered host %s in workgroup %s\n", ent->name, ent->workgroup_name);
    g_hash_table_insert (entities, ent->name, ent);
}


static void add_workgroup_to_map (SmbEntity *ent)
{
    GList *hosts;

    DEBUG ('s', "Discovered workgroup %s\n", ent->name);
    g_hash_table_insert (entities, ent->name, ent);
    current_wg_name = ent->name;
    hosts = get_hosts (ent->name);
    g_list_foreach (hosts, (GFunc) add_host_to_map, nullptr);
}


static gboolean str_ncase_equal (gchar *a, gchar *b)
{
    return g_ascii_strcasecmp(a,b) == 0;
}


static guint str_hash (gchar *key)
{
    gchar *s = g_ascii_strup (key, strlen (key));
    gint i = g_str_hash (s);
    g_free (s);
    return i;
}


inline void rebuild_map ()
{
    if (entities)
        g_hash_table_destroy (entities);

    entities = g_hash_table_new_full (
        (GHashFunc) str_hash, (GEqualFunc) str_ncase_equal, (GDestroyNotify) g_free, (GDestroyNotify) g_free);

    g_list_foreach (get_workgroups (), (GFunc) add_workgroup_to_map, nullptr);
}


SmbEntity *gnome_cmd_smb_net_get_entity (const gchar *name)
{
    gboolean rebuilt = FALSE;

    if (!entities)
    {
        DEBUG ('s', "Building the SMB database for the first time.\n");
        rebuild_map ();
        rebuilt = TRUE;
    }

    auto entity = static_cast<SmbEntity*> (g_hash_table_lookup (entities, name));
    if (!entity && !rebuilt)
    {
        DEBUG ('s', "Entity not found, rebuilding the database\n");
        rebuild_map ();
        entity = static_cast<SmbEntity*> (g_hash_table_lookup (entities, name));
    }

    if (entity)
        DEBUG ('s', "Found entity for %s\n", name);
    else
        DEBUG ('s', "No entity named %s found\n", name);

    return entity;
}
