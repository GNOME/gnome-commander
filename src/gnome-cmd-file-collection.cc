/** 
 * @file gnome-cmd-file-collection.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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
#include "gnome-cmd-file-collection.h"

using namespace std;


void GnomeCmdFileCollection::add(GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));

    list = g_list_append (list, f);

    gchar *uri_str = f->get_uri_str();
    g_hash_table_insert (map, uri_str, f);
    f->ref();
}


gboolean GnomeCmdFileCollection::remove(GnomeCmdFile *f)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), FALSE);

    list = g_list_remove (list, f);

    gchar *uri_str = f->get_uri_str();
    gboolean retval = g_hash_table_remove (map, uri_str);
    g_free (uri_str);

    return retval;
}


gboolean GnomeCmdFileCollection::remove(const gchar *uri_str)
{
    g_return_val_if_fail (uri_str != NULL, FALSE);

    GnomeCmdFile *file = find(uri_str);

    if (!file)
        return FALSE;

    list = g_list_remove (list, file);
    return g_hash_table_remove (map, uri_str);
}


GnomeCmdFile *GnomeCmdFileCollection::find(const gchar *uri_str)
{
    g_return_val_if_fail (uri_str != NULL, NULL);

    return GNOME_CMD_FILE (g_hash_table_lookup (map, uri_str));
}


void GnomeCmdFileCollection::clear()
{
    g_list_free (list);
    list = NULL;
    g_hash_table_destroy (map);
    map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gnome_cmd_file_unref);
}


GList *GnomeCmdFileCollection::sort(GCompareDataFunc compare_func, gpointer user_data)
{
    list = g_list_sort_with_data (list, compare_func, user_data);

    return list;
}
