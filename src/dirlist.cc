/**
 * @file dirlist.cc
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
#include "dirlist.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-data.h"
#include "utils.h"

using namespace std;


#define FILES_PER_NOTIFICATION 50
#define LIST_PRIORITY 0
#define FILES_PER_UPDATE 50


void async_list (GnomeCmdDir *dir);
void sync_list  (GnomeCmdDir *dir);
static void enumerate_children_callback(GObject *direnum, GAsyncResult *result, gpointer user_data);

static gboolean update_list_progress (GnomeCmdDir *dir)
{
    DEBUG ('l', "Checking list progress...\n");

    if (dir->state == GnomeCmdDir::STATE_LISTING)
    {
        gchar *msg = g_strdup_printf (ngettext ("%d file listed", "%d files listed", dir->list_counter), dir->list_counter);
        gtk_label_set_text (GTK_LABEL (dir->label), msg);
        progress_bar_update (dir->pbar, 50);
        DEBUG('l', "%s\n", msg);
        g_free (msg);
        return TRUE;
    }

    return FALSE;
}

static GFileEnumerator *get_gfileenumerator_sync(GFile *gFile, GError *error)
{
    GError *tmpError;
    auto gFileEnumerator = g_file_enumerate_children (gFile,
                            "*",
                            G_FILE_QUERY_INFO_NONE,
                            nullptr,
                            &tmpError);
    if(tmpError)
    {
        g_critical("Unable to enumerate children, error: %s", tmpError->message);
        g_propagate_error(&error, tmpError);
        return nullptr;
    }
    return gFileEnumerator;
}


static void get_gfileenumerator_async_callback(GObject *gFileObject, GAsyncResult *result, gpointer gnomeCmdDirPointer)
{
    auto dir = GNOME_CMD_DIR(gnomeCmdDirPointer);
    auto gFile = (GFile*) gFileObject;
    GError *error = nullptr;

    auto gFileEnumerator = g_file_enumerate_children_finish (gFile, result, &error);
    if (error)
    {
        g_warning("g_file_enumerate_children_finish error: %s\n", error->message);
        g_error_free(error);
        return;
    }

    g_file_enumerator_next_files_async(gFileEnumerator,
                    FILES_PER_UPDATE,
                    G_PRIORITY_LOW,
                    nullptr,
                    enumerate_children_callback,
                    dir);

    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_list_progress, dir);
}

static void get_gfileenumerator_async(GFile *gFile, GnomeCmdDir *dir, GError *error)
{
    g_file_enumerate_children_async (gFile,
                                     "*",
                                     G_FILE_QUERY_INFO_NONE,
                                     G_PRIORITY_DEFAULT,
                                     nullptr,
                                     get_gfileenumerator_async_callback,
                                     dir);
}


void async_list (GnomeCmdDir *dir)
{
    g_return_if_fail(dir != nullptr);
    GError *error = nullptr;

    dir->gFileInfoList = nullptr;
    GFile *gFile;
    auto con = gnome_cmd_dir_get_connection(dir);
    if (con->is_local)
    {
        gFile = GNOME_CMD_FILE (dir)->gFile;
    }
    else
    {
        auto conUri = gnome_cmd_con_get_uri(con);
        auto gFileTmp = g_file_new_for_uri (conUri);
        auto dirPath = GNOME_CMD_FILE (dir)->get_path();
        gFile = g_file_resolve_relative_path (gFileTmp, dirPath);

        g_object_unref (gFileTmp);
        g_free(dirPath);
    }

    gchar *uri_str = g_file_get_uri(gFile);
    DEBUG('l', "async_list: %s\n", uri_str);
    g_free (uri_str);

    dir->state = GnomeCmdDir::STATE_LISTING;

    GFileEnumerator *gFileEnumerator;
    if (con->is_local)
    {
        gFileEnumerator = get_gfileenumerator_sync(gFile, error);
        if (error)
        {
            return;
        }
        g_file_enumerator_next_files_async(gFileEnumerator,
                        FILES_PER_UPDATE,
                        G_PRIORITY_LOW,
                        nullptr,
                        enumerate_children_callback,
                        dir);

        g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_list_progress, dir);
    }
    else
    {
        get_gfileenumerator_async(gFile, dir, error);
    }
}

void sync_list (GnomeCmdDir *dir)
{
    g_return_if_fail(dir != nullptr);

    GError *error = nullptr;

    gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();
    DEBUG('l', "sync_list: %s\n", uri_str);
    g_free (uri_str);

    dir->gFileInfoList = nullptr;

    auto gFile = GNOME_CMD_FILE (dir)->gFile;

    auto gFileEnumerator = g_file_enumerate_children (gFile,
                            "*",
                            G_FILE_QUERY_INFO_NONE,
                            nullptr,
                            &error);
    if(error)
    {
        dir->error = nullptr;
        g_critical("sync_list: Unable to enumerate children, error: %s", error->message);
        g_propagate_error(&(dir->error), error);
        dir->state = GnomeCmdDir::STATE_EMPTY;
        dir->done_func (dir, dir->gFileInfoList, dir->error);
        return;
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
            dir->gFileInfoList = g_list_append(dir->gFileInfoList, gFileInfoTmp);
        }
    }
    while (gFileInfoTmp && !error);

    g_file_enumerator_close (gFileEnumerator, nullptr, nullptr);

    dir->state = error ? GnomeCmdDir::STATE_EMPTY : GnomeCmdDir::STATE_LISTED;
    dir->done_func (dir, dir->gFileInfoList, error);
}

static void enumerate_children_callback(GObject *direnum, GAsyncResult *result, gpointer user_data)
{
    auto gFileEnumerator = G_FILE_ENUMERATOR(direnum);
    auto dir = GNOME_CMD_DIR(user_data);
    GError *error = nullptr;

    GList *gFileInfosList = g_file_enumerator_next_files_finish(gFileEnumerator, result, &error);

    if(error)
    {
        g_critical("Unable to iterate the g_file_enumerator, error: %s", error->message);
        dir->state = GnomeCmdDir::STATE_EMPTY;
        g_object_unref(direnum);
        dir->done_func (dir, dir->gFileInfoList, error);
        g_file_enumerator_close(gFileEnumerator, nullptr, nullptr);
        g_error_free(error);
        return;
    }
    else if(gFileInfosList == nullptr)
    {
        /* DONE */
        dir->state = GnomeCmdDir::STATE_LISTED;
        DEBUG('l', "All files listed, calling list_done func\n");
        dir->done_func (dir, dir->gFileInfoList, nullptr);
        g_file_enumerator_close(gFileEnumerator, nullptr, nullptr);
        g_object_unref(direnum);
        return;
    }
    else
    {
        dir->gFileInfoList = g_list_concat (dir->gFileInfoList, g_list_copy (gFileInfosList));
        dir->list_counter += FILES_PER_UPDATE;
        g_file_enumerator_next_files_async(G_FILE_ENUMERATOR(direnum),
                        FILES_PER_UPDATE,
                        G_PRIORITY_LOW,
                        nullptr,
                        enumerate_children_callback,
                        dir);
    }
    g_list_free(gFileInfosList);
}


void dirlist_list (GnomeCmdDir *dir, gboolean visualProgress)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    dir->infolist = NULL;
    dir->gFileInfoList = nullptr;
    dir->list_handle = NULL;
    dir->list_counter = 0;
    dir->state = GnomeCmdDir::STATE_LISTING;

    if (!visualProgress)
    {
        sync_list (dir);
        return;
    }

    async_list (dir);
}


void dirlist_cancel (GnomeCmdDir *dir)
{
    dir->state = GnomeCmdDir::STATE_EMPTY;

    // ToDo: Add a cancel-trigger for the async dir listing
    DEBUG('l', "Cancel dir-listing not implemented yet...\n");
}
