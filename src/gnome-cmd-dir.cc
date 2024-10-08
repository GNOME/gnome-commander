/**
 * @file gnome-cmd-dir.cc
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
#include "gnome-cmd-dir.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-file-collection.h"
#include "utils.h"

using namespace std;


enum
{
    FILE_CREATED,
    FILE_DELETED,
    FILE_CHANGED,
    FILE_RENAMED,
    LIST_OK,
    LIST_FAILED,
    LAST_SIGNAL
};

struct GnomeCmdDirPrivate
{
    GList *files;
    GnomeCmdFileCollection *file_collection;
    GnomeCmdCon *con;
    GnomeCmdPath *path;

    gboolean needs_mtime_update;

    GFileMonitor *gFileMonitor;
    gint monitor_users;
};


G_DEFINE_TYPE (GnomeCmdDir, gnome_cmd_dir, GNOME_CMD_TYPE_FILE)


static guint signals[LAST_SIGNAL] = { 0 };


static void gnome_cmd_dir_set_path (GnomeCmdDir *dir, GnomeCmdPath *path);
static GnomeCmdCon *dir_get_connection (GnomeCmdFile *dir);


static void monitor_callback (GFileMonitor *gFileMonitor, GFile *gFile, GFile *otherGFile,
                              GFileMonitorEvent event_type, GnomeCmdDir *dir)
{
    auto fileUri = g_file_get_uri(gFile);

    switch (event_type)
    {
        case G_FILE_MONITOR_EVENT_CHANGED:
        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
            DEBUG('n', "G_FILE_MONITOR_EVENT_CHANGED for %s\n", fileUri);
            gnome_cmd_dir_file_changed (dir, fileUri);
            break;
        case G_FILE_MONITOR_EVENT_DELETED:
            DEBUG('n', "G_FILE_MONITOR_EVENT_DELETED for %s\n", fileUri);
            gnome_cmd_dir_file_deleted (dir, fileUri);
            break;
        case G_FILE_MONITOR_EVENT_CREATED:
            DEBUG('n', "G_FILE_MONITOR_EVENT_CREATED for %s\n", fileUri);
            gnome_cmd_dir_file_created (dir, fileUri);
            break;
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
        case G_FILE_MONITOR_EVENT_UNMOUNTED:
        case G_FILE_MONITOR_EVENT_MOVED:
        case G_FILE_MONITOR_EVENT_MOVED_IN:
        case G_FILE_MONITOR_EVENT_MOVED_OUT:
        case G_FILE_MONITOR_EVENT_RENAMED:
            break;

        default:
            DEBUG('n', "Unknown monitor event %d\n", event_type);
    }
    g_free(fileUri);
}


static void gnome_cmd_dir_init (GnomeCmdDir *dir)
{
    dir->state = GnomeCmdDir::STATE_EMPTY;

    dir->priv = g_new0 (GnomeCmdDirPrivate, 1);
    dir->priv->file_collection = new GnomeCmdFileCollection;
}


static void gnome_cmd_dir_dispose (GObject *object)
{
    GnomeCmdDir *dir = GNOME_CMD_DIR (object);

    dir->priv->file_collection->clear();

    G_OBJECT_CLASS (gnome_cmd_dir_parent_class)->dispose (object);
}


static void gnome_cmd_dir_finalize (GObject *object)
{
    GnomeCmdDir *dir = GNOME_CMD_DIR (object);

    gnome_cmd_con_remove_from_cache (dir->priv->con, dir);

    delete dir->priv->file_collection;
    delete dir->priv->path;

    g_free (dir->priv);

    G_OBJECT_CLASS (gnome_cmd_dir_parent_class)->finalize (object);
}


static void gnome_cmd_dir_class_init (GnomeCmdDirClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals[FILE_CREATED] =
        g_signal_new ("file-created",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_created),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILE_DELETED] =
        g_signal_new ("file-deleted",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_deleted),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILE_CHANGED] =
        g_signal_new ("file-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILE_RENAMED] =
        g_signal_new ("file-renamed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_renamed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[LIST_OK] =
        g_signal_new ("list-ok",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, list_ok),
            nullptr, nullptr,
            nullptr,
            G_TYPE_NONE,
            0);

    signals[LIST_FAILED] =
        g_signal_new ("list-failed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, list_failed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    object_class->dispose = gnome_cmd_dir_dispose;
    object_class->finalize = gnome_cmd_dir_finalize;

    GNOME_CMD_FILE_CLASS (klass)->get_connection = dir_get_connection;

    klass->file_created = nullptr;
    klass->file_deleted = nullptr;
    klass->file_changed = nullptr;
    klass->file_renamed = nullptr;
    klass->list_ok = nullptr;
    klass->list_failed = nullptr;
}


/***********************************
 * Public functions
 ***********************************/

GnomeCmdDir *gnome_cmd_dir_new_from_gfileinfo (GFileInfo *gFileInfo, GnomeCmdDir *gnomeCmdDirParent)
{
    g_return_val_if_fail (gFileInfo != nullptr, nullptr);
    g_return_val_if_fail (GNOME_CMD_IS_DIR (gnomeCmdDirParent), nullptr);

    GnomeCmdCon *con = gnome_cmd_file_get_connection (GNOME_CMD_FILE (gnomeCmdDirParent));
    auto dirName = g_file_info_get_name(gFileInfo);

    GFile *gFile = nullptr;
    GnomeCmdPath *dirPath = nullptr;

    gFile = gnome_cmd_dir_get_gfile_for_con_and_filename(gnomeCmdDirParent, dirName);
    if (gFile)
    {
        auto uriStringTmp = g_file_get_uri(gFile);
        auto uriTmp = g_uri_parse(uriStringTmp, G_URI_FLAGS_NONE, nullptr);
        auto pathTmp = g_uri_get_path(uriTmp);
        dirPath = gnome_cmd_con_create_path(con, pathTmp);
        g_free(uriStringTmp);
    }
    else
    {
        dirPath = gnome_cmd_dir_get_path (gnomeCmdDirParent)->get_child(dirName);
        if (!dirPath)
        {
            return nullptr;
        }

        gFile = gnome_cmd_con_create_gfile (con, dirPath->get_path());
    }
    auto uriString = g_file_get_uri (gFile);

    auto gnomeCmdDir = gnome_cmd_con_cache_lookup (con, uriString);

    if (gnomeCmdDir)
    {
        delete dirPath;
        GNOME_CMD_FILE (gnomeCmdDir)->update_gFileInfo(gFileInfo);
        g_free (uriString);
        return gnomeCmdDir;
    }

    gnomeCmdDir = static_cast<GnomeCmdDir*> (g_object_new (GNOME_CMD_TYPE_DIR, nullptr));
    GError *error = nullptr;
    if(!gnome_cmd_file_setup(G_OBJECT (gnomeCmdDir), gFile, &error))
    {
        g_warning("gnome_cmd_dir_new_from_gfileinfo error on %s: %s", uriString, error->message);
        g_error_free(error);
        g_free (uriString);
        g_object_unref(gnomeCmdDir);
        return nullptr;
    }
    gnomeCmdDir->priv->con = con;
    gnome_cmd_dir_set_path (gnomeCmdDir, dirPath);
    gnomeCmdDir->priv->needs_mtime_update = FALSE;

    gnome_cmd_con_add_to_cache (con, gnomeCmdDir, uriString);

    return gnomeCmdDir;
}


GnomeCmdDir *gnome_cmd_dir_new_with_con (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GFileInfo *con_base_file_info = gnome_cmd_con_get_base_file_info (con);
    g_return_val_if_fail (con_base_file_info != nullptr, nullptr);

    auto gFile = con->is_local
        ? gnome_cmd_con_create_gfile (con, gnome_cmd_con_get_base_path(con)->get_path())
        : gnome_cmd_con_create_gfile (con, nullptr);

    auto uriString = g_file_get_uri(gFile);
    GnomeCmdDir *dir = gnome_cmd_con_cache_lookup (con, uriString);

    if (dir)
    {
        GNOME_CMD_FILE (dir)->update_gFileInfo(con_base_file_info);
        g_object_unref (gFile);
        g_free (uriString);
        return dir;
    }

    dir = static_cast<GnomeCmdDir*> (g_object_new (GNOME_CMD_TYPE_DIR, nullptr));
    gnome_cmd_dir_set_path (dir, gnome_cmd_con_get_base_path (con)->clone());
    dir->priv->con = con;
    GError *error = nullptr;
    if (!gnome_cmd_file_setup (G_OBJECT(dir), gFile, &error))
    {
        g_warning("gnome_cmd_dir_new_with_con error on %s: %s", uriString, error->message);
        g_warning("%s", error->message);
        g_error_free(error);
        g_free (uriString);
        g_object_unref(dir);
        return nullptr;
    }
    dir->priv->needs_mtime_update = FALSE;
    gnome_cmd_con_add_to_cache (con, dir, uriString);
    return dir;
}


GnomeCmdDir *gnome_cmd_dir_new (GnomeCmdCon *con, GnomeCmdPath *path, gboolean isStartup)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    g_return_val_if_fail (path!=nullptr, nullptr);
    GError *error = nullptr;

    auto gFile = gnome_cmd_con_create_gfile (con, path->get_path());
    if (!gFile)
    {
        return nullptr;
    }

    auto uriString = g_file_get_uri (gFile);

    auto gnomeCmdDir = gnome_cmd_con_cache_lookup (con, uriString);
    if (gnomeCmdDir) // GnomeCmdDir instance is set up already
    {
        g_free (uriString);
        g_object_unref (gFile);
        return gnomeCmdDir;
    }

    gnomeCmdDir = static_cast<GnomeCmdDir*> (g_object_new (GNOME_CMD_TYPE_DIR, nullptr));
    if (!gnome_cmd_file_setup (G_OBJECT (gnomeCmdDir), gFile, &error))
    {
        if (error && !isStartup)
        {
            gnome_cmd_error_message (*main_win, uriString, error);
        }
        g_object_unref(gFile);
        g_free (uriString);
        return nullptr;
    }
    gnome_cmd_dir_set_path (gnomeCmdDir, path);
    gnomeCmdDir->priv->con = con;
    gnomeCmdDir->priv->needs_mtime_update = FALSE;
    gnome_cmd_con_add_to_cache (con, gnomeCmdDir, uriString);

    return gnomeCmdDir;
}


GnomeCmdDir *gnome_cmd_dir_get_parent (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    GnomeCmdPath *path = dir->priv->path->get_parent();

    return path ? gnome_cmd_dir_new (dir->priv->con, path) : nullptr;
}


GnomeCmdDir *gnome_cmd_dir_get_child (GnomeCmdDir *dir, const gchar *child)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    GnomeCmdPath *path = dir->priv->path->get_child(child);

    return path ? gnome_cmd_dir_new (dir->priv->con, path) : nullptr;
}


static GnomeCmdCon *dir_get_connection (GnomeCmdFile *dir)
{
    return GNOME_CMD_DIR (dir)->priv->con;
}


void gnome_cmd_dir_unref (GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    GNOME_CMD_FILE (dir)->unref();
}

GList *gnome_cmd_dir_get_files (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    return dir->priv->files;
}


extern "C" void gnome_cmd_dir_set_state (GnomeCmdDir *dir, gint state)
{
    dir->state = (GnomeCmdDir::State) state;
}


extern "C" void gnome_cmd_dir_set_files (GnomeCmdDir *dir, GList *files)
{
    if (!dir->priv->file_collection->empty())
        dir->priv->file_collection->clear();

    dir->priv->files = files;
    dir->priv->file_collection->add(dir->priv->files);
}


GnomeCmdPath *gnome_cmd_dir_get_path (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    return dir->priv->path;
}


static void gnome_cmd_dir_set_path (GnomeCmdDir *dir, GnomeCmdPath *path)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    delete dir->priv->path;

    dir->priv->path = path;
}


void gnome_cmd_dir_update_path (GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    GnomeCmdDir *parent = gnome_cmd_dir_get_parent (dir);
    if (!parent)
        return;

    GnomeCmdPath *path = gnome_cmd_dir_get_path (parent)->get_child(GNOME_CMD_FILE (dir)->get_name());
    if (path)
        gnome_cmd_dir_set_path (dir, path);
}


gchar *gnome_cmd_dir_get_display_path (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    return g_strdup (dir->priv->path->get_display_path());
}


GFile *gnome_cmd_dir_get_gfile (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    return GNOME_CMD_FILE(dir)->get_file();
}


gchar *gnome_cmd_dir_get_uri_str (GnomeCmdDir *dir, gboolean withTrailingSlash)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    auto gFile = gnome_cmd_dir_get_gfile (dir);

    g_return_val_if_fail(G_IS_FILE(gFile), nullptr);

    auto dirString = g_file_get_uri (gFile);

    if (withTrailingSlash)
    {
        auto tmpString = dirString[strlen(dirString)-1] == G_DIR_SEPARATOR
            ? g_strdup(dirString)
            : g_strdup_printf("%s%s", dirString, G_DIR_SEPARATOR_S);
        g_free(dirString);
        dirString = tmpString;
    }

    return dirString;
}


/**
 * @brief Get the relative directory path string for the given base path
 *
 * For example, let childPath be "/tmp/abc" and basePath is the path "/tmp/"
 * (from the original URI smb://localhost/tmp/). Then the return would be
 * the the address pointing to "/" (which is the second slash from "/tmp/").
 * If childPath is pointing to "/tmp/abc" and the basePath points
 * to "/xyz", then the return points to "/" because /xzy is relative
 * to the /tmp on the same directory level.
 */
gchar *gnome_cmd_dir_get_relative_path_string(const char* childPath, const char* basePath)
{
    if (!childPath || *childPath == '\000')
        return g_strdup(basePath);

    if (!strcmp(basePath, G_DIR_SEPARATOR_S))
        return g_strdup(childPath);

    char *relPath = nullptr;

    if (strstr(childPath, basePath) == childPath)
    {
        auto stringTmp = strchr((childPath + 1), G_DIR_SEPARATOR);
        relPath = g_strdup(stringTmp != nullptr ? stringTmp : childPath);
    }
    return relPath ? relPath : g_strdup(G_DIR_SEPARATOR_S);
}

static gchar *gnome_cmd_dir_get_mount_uri(GnomeCmdCon *con)
{
    auto conUri = gnome_cmd_con_get_uri_string (con);
    auto gFileConTmp = g_file_new_for_uri(conUri);
    g_free (conUri);
    do
    {
        auto gFileConParent = g_file_get_parent(gFileConTmp);
        if (!gFileConParent) break;
        g_object_unref(gFileConTmp);
        gFileConTmp = gFileConParent;
    }
    while (g_file_find_enclosing_mount(gFileConTmp, nullptr, nullptr));
    auto conUriBase = g_file_get_uri(gFileConTmp);
    g_object_unref(gFileConTmp);
    return conUriBase;
}

/**
 * This function returns a GFile object which is the result of the URI construction of
 * two URI's and a path: the connection URI, which is the private member of GnomeCmdDir,
 * the directory path inside of that connection, and the given filename string.
 */
GFile *gnome_cmd_dir_get_gfile_for_con_and_filename(GnomeCmdDir *dir, const gchar *filename)
{
    auto conUri = gnome_cmd_con_get_uri(dir->priv->con);
    if (!conUri) // is usually set for a remote connection
    {
        return nullptr;
    }

    // Get the Uri for the mount which belongs to the GnomeCmdCon object
    auto mountUri = gnome_cmd_dir_get_mount_uri(dir->priv->con);

    // Always let the connection URI to end with '/' because the last entry should be a directory
    auto conLastCharacter = mountUri[strlen(mountUri)-1];
    auto mountUriTmp = conLastCharacter == G_DIR_SEPARATOR
        ? g_strdup(mountUri)
        : g_strdup_printf("%s%s", mountUri, G_DIR_SEPARATOR_S);
    g_free(mountUri);

    // Create the merged URI out of the connection URI, the directory path and the filename
    auto gnomeCmdDirPathString = gnome_cmd_dir_get_path(dir)->get_path();

    auto gUriForMount = g_uri_parse(mountUriTmp, G_URI_FLAGS_NONE, nullptr);
    auto conUriPath = g_uri_get_path(gUriForMount);
    auto relDirToUriPath = gnome_cmd_dir_get_relative_path_string(gnomeCmdDirPathString, conUriPath);
    auto mergedDirAndFileNameString = g_build_filename(".", relDirToUriPath, filename, nullptr);
    g_free(relDirToUriPath);

    GError *error = nullptr;
    auto fullFileNameUri = g_uri_resolve_relative (mountUriTmp, mergedDirAndFileNameString, G_URI_FLAGS_NONE, &error);
    if (!fullFileNameUri || error)
    {
        g_warning ("gnome_cmd_dir_get_uri_str error: %s", error ? error->message : "Could not resolve relative URI");
        g_free(mountUriTmp);
        g_free(fullFileNameUri);
        g_free(mergedDirAndFileNameString);
        g_error_free(error);
        return nullptr;
    }

    auto gFile = g_file_new_for_uri(fullFileNameUri);
    g_free(mountUriTmp);
    g_free(fullFileNameUri);
    g_free(mergedDirAndFileNameString);
    return gFile;
}


GFile *gnome_cmd_dir_get_child_gfile (GnomeCmdDir *dir, const gchar *filename)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    GFile *gFile = gnome_cmd_dir_get_gfile_for_con_and_filename(dir, filename);

    if (gFile)
    {
        return gFile;
    }

    GnomeCmdPath *path = dir->priv->path->get_child(filename);
    if (!path)
    {
        return nullptr;
    }

    gFile = gnome_cmd_con_create_gfile (dir->priv->con, path->get_path());
    delete path;

    return gFile;
}


GFile *gnome_cmd_dir_get_absolute_path_gfile (GnomeCmdDir *dir, string absolute_filename)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    // include workgroups and shares for smb uris
    GFile *dir_gFile = gnome_cmd_dir_get_gfile (dir);

    auto uriScheme = g_file_get_uri_scheme (dir_gFile);

    if (uriScheme && strcmp (uriScheme, "smb") == 0)
    {
        g_object_ref (dir_gFile);

        while (get_gfile_attribute_uint32(dir_gFile, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        {
            auto gFileParent = g_file_get_parent(dir_gFile);
            g_object_unref (dir_gFile);
            dir_gFile = gFileParent;
        }

        auto server_and_share = g_file_get_uri(dir_gFile);
        stringify (absolute_filename, g_build_filename (server_and_share, absolute_filename.c_str(), nullptr));
        g_free (server_and_share);

        g_object_unref (dir_gFile);
    }

    g_free(uriScheme);

    GnomeCmdPath *path = gnome_cmd_con_create_path (dir->priv->con, absolute_filename.c_str());
    auto gFile = gnome_cmd_con_create_gfile (dir->priv->con, path->get_path());

    delete path;

    return gFile;
}


inline gboolean file_already_exists (GnomeCmdDir *dir, const gchar *uri_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), TRUE);
    g_return_val_if_fail (uri_str != nullptr, TRUE);

    return dir->priv->file_collection->find(uri_str) != nullptr;
}


// A file has been created. Create a new GnomeCmdFile object for that file and assign it to dir
void gnome_cmd_dir_file_created (GnomeCmdDir *dir, const gchar *newDirUriStr)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (newDirUriStr != nullptr);
    GError *error = nullptr;

    if (file_already_exists (dir, newDirUriStr))
        return;

    auto gFile = g_file_new_for_uri (newDirUriStr);
    auto gFileInfo = g_file_query_info (gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &error);
    if (error)
    {
        DEBUG ('t', "Could not retrieve file information for %s, error: %s\n", newDirUriStr, error->message);
        g_error_free(error);
    }

    GnomeCmdFile *f;

    if (g_file_info_get_attribute_uint32 (gFileInfo, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        f = reinterpret_cast<GnomeCmdFile*> (gnome_cmd_dir_new_from_gfileinfo (gFileInfo, dir));
    else
        f = gnome_cmd_file_new (gFileInfo, dir);

    if (!f)
    {
        return;
    }

    dir->priv->file_collection->add(f);
    dir->priv->files = dir->priv->file_collection->get_list();

    dir->priv->needs_mtime_update = TRUE;

    g_signal_emit (dir, signals[FILE_CREATED], 0, f);
}


/**
 * This function will search in the file tree to retrieve the first
 * physically existing parent dir for a given dir.
 */
GnomeCmdDir *gnome_cmd_dir_get_existing_parent(GnomeCmdDir *dir)
{
    auto parentDir = gnome_cmd_dir_get_parent(dir);
    gchar *parentDirUri = nullptr;
    GFile *gFileParent = nullptr;

    GnomeCmdDir *returnDir = nullptr;
    do
    {
        parentDirUri = gnome_cmd_dir_get_uri_str(parentDir);
        gFileParent = g_file_new_for_uri(parentDirUri);
        g_free(parentDirUri);
        if (g_file_query_exists(gFileParent, nullptr))
        {
            returnDir = parentDir;
        }
        else
        {
            gnome_cmd_con_remove_from_cache (dir->priv->con, parentDir);
            parentDir = gnome_cmd_dir_get_parent(parentDir);
        }
        g_object_unref(gFileParent);
    } while (!returnDir);

    return returnDir;
}


/**
 * When a directory is deleted, this method will check if we are currently displaying this directory
 * in one of the file panes. If yes, we will update the file pane in question and step up
 * to a directory which is existing.
 */
void gnome_cmd_dir_update_file_selector(GnomeCmdDir *dir, const gchar *deletedDirUriString)
{
    auto dirUriSting = gnome_cmd_dir_get_uri_str(dir);
    if (!g_strcmp0(deletedDirUriString, dirUriSting))
    {
        gnome_cmd_con_remove_from_cache (dir->priv->con, dir);

        auto parentDir = gnome_cmd_dir_get_existing_parent(dir);

        // Go to the parent directory in one of the file lists if necessary
        auto parentDirPath = gnome_cmd_dir_get_path(parentDir)->get_path();

        //ToDo: The following has to be done for the other open tabs, too.

        auto fs = main_win->fs(ACTIVE);
        auto fsDir = fs->get_directory();
        auto urifsDir = gnome_cmd_dir_get_uri_str(fsDir);
        if (!g_strcmp0(urifsDir, deletedDirUriString))
        {
            fs->goto_directory(parentDirPath);
        }
        g_free(urifsDir);

        fs = main_win->fs(INACTIVE);
        fsDir = fs->get_directory();
        urifsDir = gnome_cmd_dir_get_uri_str(fsDir);
        if (!g_strcmp0(urifsDir, deletedDirUriString))
        {
            fs->goto_directory(parentDirPath);
        }
        g_free(urifsDir);
    }
    g_free(dirUriSting);
}


// A file has been deleted. Remove the corresponding GnomeCmdFile
void gnome_cmd_dir_file_deleted (GnomeCmdDir *dir, const gchar *deletedDirUriString)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (deletedDirUriString != nullptr);

    gnome_cmd_dir_update_file_selector(dir, deletedDirUriString);

    GnomeCmdFile *f = dir->priv->file_collection->find(deletedDirUriString);

    if (!GNOME_CMD_IS_FILE (f))
        return;

    dir->priv->needs_mtime_update = TRUE;

    g_signal_emit (dir, signals[FILE_DELETED], 0, f);

    dir->priv->file_collection->remove(deletedDirUriString);
    dir->priv->files = dir->priv->file_collection->get_list();
}


// A file has been changed. Find the corresponding GnomeCmdFile, update its GFileInfo
void gnome_cmd_dir_file_changed (GnomeCmdDir *dir, const gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (uri_str != nullptr);

    GnomeCmdFile *f = dir->priv->file_collection->find(uri_str);

    if (f == nullptr)
    {
        return;
    }

    dir->priv->needs_mtime_update = TRUE;

    auto gFileInfo = g_file_query_info(f->get_file(), "*", G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
    if (gFileInfo == nullptr)
    {
        auto basename = g_file_get_basename(f->get_file());
        DEBUG ('t', "Could not retrieve file information for changed file %s\n", g_file_get_basename(f->get_file()));
        g_free(basename);
        return;
    }

    f->update_gFileInfo(gFileInfo);
    f->invalidate_metadata();
    g_signal_emit (dir, signals[FILE_CHANGED], 0, f);
}


void gnome_cmd_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *f, const gchar *old_uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (GNOME_CMD_IS_FILE (f));
    g_return_if_fail (old_uri_str != nullptr);

    if (GNOME_CMD_IS_DIR (f))
        gnome_cmd_con_remove_from_cache (dir->priv->con, old_uri_str);

    dir->priv->needs_mtime_update = TRUE;

    dir->priv->file_collection->remove(old_uri_str);
    dir->priv->file_collection->add(f);
    g_signal_emit (dir, signals[FILE_RENAMED], 0, f);
}


void gnome_cmd_dir_start_monitoring (GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    if (dir->priv->monitor_users == 0)
    {
        GError *error = nullptr;

        auto gFileMonitor = g_file_monitor_directory (
            GNOME_CMD_FILE (dir)->get_file(),
            //ToDo: We might want to activate G_FILE_MONITOR_WATCH_MOVES in the future
            G_FILE_MONITOR_NONE,
            nullptr,
            &error);

        if (error)
        {
            gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();
            DEBUG ('n', "Failed to add monitor to %p %s: %s\n", dir, uri_str, error->message);
            g_error_free(error);
            g_free(uri_str);
            return;
        }

        g_signal_connect (gFileMonitor, "changed", G_CALLBACK (monitor_callback), dir);

        gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();
        DEBUG('n', "Added monitor to %p %s\n", dir, uri_str);
        dir->priv->gFileMonitor = gFileMonitor;
        g_free (uri_str);
    }

    dir->priv->monitor_users++;
}


void gnome_cmd_dir_cancel_monitoring (GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    if (dir->priv->monitor_users < 1) return;

    dir->priv->monitor_users--;

    if (dir->priv->monitor_users == 0)
    {
        if (dir->priv->gFileMonitor)
        {
            g_file_monitor_cancel (dir->priv->gFileMonitor);
            DEBUG('n', "Removed monitor from %p %s\n", dir, GNOME_CMD_FILE (dir)->get_uri_str());

            dir->priv->gFileMonitor = nullptr;
        }
    }
}


gboolean gnome_cmd_dir_is_monitored (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);

    return dir->priv->monitor_users > 0;
}


gboolean gnome_cmd_dir_update_mtime (GnomeCmdDir *dir)
{
    // this function also determines if cached dir is up-to-date (FALSE=yes)
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);

    // assume cache is updated
    gboolean returnValue = FALSE;

    auto uri              = gnome_cmd_file_get_uri_str(GNOME_CMD_FILE(dir));
    auto tempGFile        = g_file_new_for_uri(uri);
    auto gFileInfoCurrent = g_file_query_info(tempGFile, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
    auto currentMTime     = g_file_info_get_modification_date_time(gFileInfoCurrent);

    auto cachedMTime = g_file_info_get_modification_date_time(GNOME_CMD_FILE(dir)->get_file_info());
    g_free(uri);
    g_object_unref(gFileInfoCurrent);

    if (currentMTime && cachedMTime && g_date_time_compare(cachedMTime, currentMTime))
    {
        // cache is not up-to-date
        g_file_info_set_modification_date_time(GNOME_CMD_FILE (dir)->get_file_info(), currentMTime);
        returnValue = TRUE;
    }

    // after this function we are sure dir's mtime is up-to-date
    dir->priv->needs_mtime_update = FALSE;

    return returnValue;
}


gboolean gnome_cmd_dir_needs_mtime_update (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);

    return dir->priv->needs_mtime_update;
}
