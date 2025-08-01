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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con.h"

using namespace std;


enum
{
    FILE_CREATED,
    FILE_DELETED,
    FILE_CHANGED,
    FILE_RENAMED,
    LIST_OK,
    LIST_FAILED,
    DIR_DELETED,
    LAST_SIGNAL
};

G_DEFINE_TYPE (GnomeCmdDir, gnome_cmd_dir, GNOME_CMD_TYPE_FILE)


static guint signals[LAST_SIGNAL] = { 0 };


extern "C" GnomeCmdCon *dir_get_connection (GnomeCmdFile *dir);
extern "C" void gnome_cmd_dir_on_dispose (GnomeCmdDir *dir);


static void gnome_cmd_dir_init (GnomeCmdDir *dir)
{
}


static void gnome_cmd_dir_dispose (GObject *object)
{
    gnome_cmd_dir_on_dispose (GNOME_CMD_DIR (object));
    G_OBJECT_CLASS (gnome_cmd_dir_parent_class)->dispose (object);
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
            1, GNOME_CMD_TYPE_FILE);

    signals[FILE_DELETED] =
        g_signal_new ("file-deleted",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_deleted),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, GNOME_CMD_TYPE_FILE);

    signals[FILE_CHANGED] =
        g_signal_new ("file-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, GNOME_CMD_TYPE_FILE);

    signals[FILE_RENAMED] =
        g_signal_new ("file-renamed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_renamed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, GNOME_CMD_TYPE_FILE);

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
            nullptr,
            G_TYPE_NONE,
            1, G_TYPE_ERROR);

    signals[DIR_DELETED] =
        g_signal_new ("dir-deleted",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, dir_deleted),
            nullptr, nullptr,
            nullptr,
            G_TYPE_NONE,
            0);

    object_class->dispose = gnome_cmd_dir_dispose;

    GNOME_CMD_FILE_CLASS (klass)->get_connection = dir_get_connection;

    klass->file_created = nullptr;
    klass->file_deleted = nullptr;
    klass->file_changed = nullptr;
    klass->file_renamed = nullptr;
    klass->list_ok = nullptr;
    klass->list_failed = nullptr;
    klass->dir_deleted = nullptr;
}


/***********************************
 * Public functions
 ***********************************/

extern "C" GnomeCmdDir *gnome_cmd_dir_find_or_create (GnomeCmdCon *con, GFile *gFile, GFileInfo *gFileInfo, GnomeCmdPath *path, GError **error);


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
        auto gnomeCmdDirParentPath = gnome_cmd_dir_get_path (gnomeCmdDirParent);
        dirPath = gnome_cmd_path_get_child (gnomeCmdDirParentPath, dirName);
        gnome_cmd_path_free (gnomeCmdDirParentPath);
        if (!dirPath)
        {
            return nullptr;
        }

        gchar *p = gnome_cmd_path_get_path (dirPath);
        gFile = gnome_cmd_con_create_gfile (con, p);
        g_free (p);
    }

    GError *error = nullptr;
    auto gnomeCmdDir = gnome_cmd_dir_find_or_create (con, gFile, gFileInfo, dirPath, &error);
    if (!gnomeCmdDir)
    {
        auto uriString = g_file_get_uri (gFile);
        g_warning("gnome_cmd_dir_new_from_gfileinfo error on %s: %s", uriString, error->message);
        g_free (uriString);
        g_error_free(error);
        return nullptr;
    }
    return gnomeCmdDir;
}


GnomeCmdDir *gnome_cmd_dir_new_with_con (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GFileInfo *con_base_file_info = gnome_cmd_con_get_base_file_info (con);
    g_return_val_if_fail (con_base_file_info != nullptr, nullptr);

    gchar *path = con->is_local
        ? gnome_cmd_path_get_path (gnome_cmd_con_get_base_path(con))
        : nullptr;

    auto gFile = gnome_cmd_con_create_gfile (con, path);
    g_free (path);

    GError *error = nullptr;
    auto dir = gnome_cmd_dir_find_or_create (con, gFile, con_base_file_info, gnome_cmd_path_clone (gnome_cmd_con_get_base_path (con)), &error);
    if (!dir)
    {
        auto uriString = g_file_get_uri(gFile);
        g_warning("gnome_cmd_dir_new_with_con error on %s: %s", uriString, error->message);
        g_free (uriString);
        g_error_free(error);
        return nullptr;
    }
    return dir;
}


GnomeCmdDir *gnome_cmd_dir_new (GnomeCmdCon *con, GnomeCmdPath *path, gboolean isStartup)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    g_return_val_if_fail (path!=nullptr, nullptr);

    gchar *path_str = gnome_cmd_path_get_path (path);
    auto gFile = gnome_cmd_con_create_gfile (con, path_str);
    g_free (path_str);
    if (!gFile)
    {
        return nullptr;
    }

    GError *error = nullptr;
    auto gnomeCmdDir = gnome_cmd_dir_find_or_create (con, gFile, nullptr, path, &error);
    if (!gnomeCmdDir)
    {
        if (!isStartup)
        {
            auto uriString = g_file_get_uri (gFile);
            gnome_cmd_error_message (GTK_WINDOW (main_win), uriString, error);
            g_free (uriString);
        }
        g_object_unref(gFile);
        return nullptr;
    }
    return gnomeCmdDir;
}


GnomeCmdDir *gnome_cmd_dir_get_parent (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);
    auto con = dir_get_connection (GNOME_CMD_FILE (dir));

    GnomeCmdPath *self_path = gnome_cmd_dir_get_path (dir);
    GnomeCmdPath *path = gnome_cmd_path_get_parent (self_path);
    gnome_cmd_path_free (self_path);

    return path ? gnome_cmd_dir_new (con, path) : nullptr;
}


GnomeCmdDir *gnome_cmd_dir_get_child (GnomeCmdDir *dir, const gchar *child)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);
    auto con = dir_get_connection (GNOME_CMD_FILE (dir));

    GnomeCmdPath *self_path = gnome_cmd_dir_get_path (dir);
    GnomeCmdPath *path = gnome_cmd_path_get_child (self_path, child);
    gnome_cmd_path_free (self_path);

    return path ? gnome_cmd_dir_new (con, path) : nullptr;
}


GFile *gnome_cmd_dir_get_gfile (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), nullptr);

    return GNOME_CMD_FILE(dir)->get_file();
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
    auto con = dir_get_connection (GNOME_CMD_FILE (dir));

    auto conUri = gnome_cmd_con_get_uri(con);
    if (!conUri) // is usually set for a remote connection
    {
        return nullptr;
    }

    // Get the Uri for the mount which belongs to the GnomeCmdCon object
    auto mountUri = gnome_cmd_dir_get_mount_uri(con);

    // Always let the connection URI to end with '/' because the last entry should be a directory
    auto conLastCharacter = mountUri[strlen(mountUri)-1];
    auto mountUriTmp = conLastCharacter == G_DIR_SEPARATOR
        ? g_strdup(mountUri)
        : g_strdup_printf("%s%s", mountUri, G_DIR_SEPARATOR_S);
    g_free(mountUri);

    // Create the merged URI out of the connection URI, the directory path and the filename
    auto path = gnome_cmd_dir_get_path (dir);
    auto gnomeCmdDirPathString = gnome_cmd_path_get_path (path);
    gnome_cmd_path_free (path);

    auto gUriForMount = g_uri_parse(mountUriTmp, G_URI_FLAGS_NONE, nullptr);
    auto conUriPath = g_uri_get_path(gUriForMount);
    auto relDirToUriPath = gnome_cmd_dir_get_relative_path_string(gnomeCmdDirPathString, conUriPath);
    g_free (gnomeCmdDirPathString);
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
    auto con = dir_get_connection (GNOME_CMD_FILE (dir));

    GFile *gFile = gnome_cmd_dir_get_gfile_for_con_and_filename(dir, filename);

    if (gFile)
    {
        return gFile;
    }

    GnomeCmdPath *self_path = gnome_cmd_dir_get_path (dir);
    GnomeCmdPath *path = gnome_cmd_path_get_child (self_path, filename);
    gnome_cmd_path_free (self_path);
    if (!path)
    {
        return nullptr;
    }

    gchar *path_str = gnome_cmd_path_get_path (path);
    gFile = gnome_cmd_con_create_gfile (con, path_str);
    gnome_cmd_path_free (path);
    g_free (path_str);

    return gFile;
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
    gnome_cmd_dir_set_needs_mtime_update (dir, FALSE);

    return returnValue;
}
