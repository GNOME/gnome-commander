/**
 * @file dirlist.cc
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
#include "dirlist.h"
#include "gnome-cmd-con.h"
#include "utils.h"

using namespace std;


GList* sync_dir_list (const gchar *absDirPath)
{
    g_return_val_if_fail (absDirPath != nullptr, nullptr);

    auto gFile = g_file_new_for_path(absDirPath);

    GList *gFileInfoList = sync_dir_list (gFile, nullptr);

    g_object_unref(gFile);

    return gFileInfoList;
}

GList* sync_dir_list (GFile *gFile, GCancellable* cancellable)
{
    g_return_val_if_fail (gFile != nullptr, nullptr);

    gchar *pathOrUri = g_file_get_path (gFile);
    if (!pathOrUri)
        pathOrUri = g_file_get_uri (gFile);

    DEBUG('l', "sync_dir_list: %s\n", pathOrUri);

    if (!g_file_query_exists(gFile, cancellable))
    {
        g_warning("sync_dir_list error: \"%s\" does not exist", pathOrUri);
        g_free (pathOrUri);
        return nullptr;
    }

    g_free (pathOrUri);

    GError *error = nullptr;
    GList *gFileInfoList = nullptr;

    auto gFileEnumerator = g_file_enumerate_children (gFile,
                            "*",
                            G_FILE_QUERY_INFO_NONE,
                            cancellable,
                            &error);
    if(error)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_critical("sync_dir_list: Unable to enumerate children, error: %s", error->message);
        g_error_free(error);
        return nullptr;
    }

    GFileInfo *gFileInfoTmp = nullptr;
    do
    {
        gFileInfoTmp = g_file_enumerator_next_file(gFileEnumerator, cancellable, &error);
        if(error)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_critical("sync_dir_list: Unable to enumerate next file, error: %s", error->message);
            break;
        }
        if (gFileInfoTmp)
        {
            gFileInfoList = g_list_append(gFileInfoList, gFileInfoTmp);
        }
    }
    while (gFileInfoTmp && !error);

    if (error)
        g_error_free(error);

    g_file_enumerator_close (gFileEnumerator, nullptr, nullptr);
    g_object_unref(gFileEnumerator);
    return gFileInfoList;
}
