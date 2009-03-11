/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-smb-net.h"
#include "utils.h"

using namespace std;


static GHashTable *entities = NULL;
static gchar *current_wg_name;


static void add_host_to_list (GnomeVFSFileInfo *info, GList **list)
{
    SmbEntity *ent = g_new (SmbEntity, 1);

    ent->name = info->name;
    ent->type = SMB_HOST;
    ent->workgroup_name = current_wg_name;

    *list = g_list_append (*list, ent);
}


static void add_wg_to_list (GnomeVFSFileInfo *info, GList **list)
{
    SmbEntity *ent = g_new0 (SmbEntity, 1);

    ent->name = info->name;
    ent->type = SMB_WORKGROUP;
    // ent->workgroup_name = NULL;

    *list = g_list_append (*list, ent);
}


inline GnomeVFSResult blocking_list (const gchar *uri_str, GList **list)
{
    return gnome_vfs_directory_list_load (list, uri_str, GNOME_VFS_FILE_INFO_DEFAULT);
}


inline GList *get_hosts (const gchar *wg)
{
    GList *fileinfos;

    gchar *uri_str = g_strdup_printf ("smb://%s", wg);
    GnomeVFSResult result = blocking_list (uri_str, &fileinfos);
    g_free (uri_str);

    GList *list = NULL;

    if (result == GNOME_VFS_OK)
        g_list_foreach (fileinfos, (GFunc) add_host_to_list, &list);

    return list;
}


inline GList *get_wgs ()
{
    GList *fileinfos;

    GnomeVFSResult result = blocking_list ("smb://", &fileinfos);

    GList *list = NULL;

    if (result == GNOME_VFS_OK)
        g_list_foreach (fileinfos, (GFunc) add_wg_to_list, &list);

    return list;
}


static void add_host_to_map (SmbEntity *ent)
{
    DEBUG ('s', "Discovered host %s in workgroup %s\n", ent->name, ent->workgroup_name);
    g_hash_table_insert (entities, ent->name, ent);
}


static void add_wg_to_map (SmbEntity *ent)
{
    GList *hosts;

    DEBUG ('s', "Discovered workgroup %s\n", ent->name);
    g_hash_table_insert (entities, ent->name, ent);
    current_wg_name = ent->name;
    hosts = get_hosts (ent->name);
    g_list_foreach (hosts, (GFunc) add_host_to_map, NULL);
}


static gboolean str_ncase_equal (gchar *a, gchar *b)
{
    return g_ascii_strcasecmp(a,b) == 0;
}


guint str_hash (gchar *key)
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

    GList *wgs = get_wgs ();
    g_list_foreach (wgs, (GFunc) add_wg_to_map, NULL);
}


SmbEntity *gnome_cmd_smb_net_get_entity (const gchar *name)
{
    gboolean b = FALSE;

    if (!entities)
    {
        DEBUG ('s', "Building the SMB database for the first time.\n");
        rebuild_map ();
        b = TRUE;
    }

    SmbEntity *ent = (SmbEntity *) g_hash_table_lookup (entities, name);
    if (!ent && !b)
    {
        DEBUG ('s', "Entity not found, rebuilding the database\n");
        rebuild_map ();
        ent = (SmbEntity *) g_hash_table_lookup (entities, name);
    }

    if (ent)
        DEBUG ('s', "Found entity for %s\n", name);
    else
        DEBUG ('s', "No entity named %s found\n", name);

    return ent;
}
