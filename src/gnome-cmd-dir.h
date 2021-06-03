/** 
 * @file gnome-cmd-dir.h
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

#pragma once

#define GNOME_CMD_TYPE_DIR              (gnome_cmd_dir_get_type ())
#define GNOME_CMD_DIR(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_DIR, GnomeCmdDir))
#define GNOME_CMD_DIR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_DIR, GnomeCmdDirClass))
#define GNOME_CMD_IS_DIR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_DIR))
#define GNOME_CMD_IS_DIR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_DIR))
#define GNOME_CMD_DIR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_DIR, GnomeCmdDirClass))


GType gnome_cmd_dir_get_type ();


struct GnomeCmdDir;
struct GnomeCmdDirPrivate;

typedef void (* DirListDoneFunc) (GnomeCmdDir *dir, GList *files, GError *error);

#include <string>

#include "gnome-cmd-file.h"
#include "gnome-cmd-path.h"
#include "handle.h"

struct GnomeCmdDir
{
    GnomeCmdFile parent;      // this MUST be the first member

    GnomeCmdDirPrivate *priv;

    enum State
    {
        STATE_EMPTY,
        STATE_LISTED,
        STATE_LISTING,
        STATE_CANCELING
    };

    gint voffset;
    GList *infolist;
    GList *gFileInfoList;
    GnomeVFSAsyncHandle *list_handle;
    gint list_counter;
    State state;

    DirListDoneFunc done_func;

    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *pbar;
};

struct GnomeCmdDirClass
{
    GnomeCmdFileClass parent_class;

    void (* file_created)       (GnomeCmdDir *dir, GnomeCmdFile *file);
    void (* file_deleted)       (GnomeCmdDir *dir, GnomeCmdFile *file);
    void (* file_changed)       (GnomeCmdDir *dir, GnomeCmdFile *file);
    void (* file_renamed)       (GnomeCmdDir *dir, GnomeCmdFile *file);
    void (* list_ok)            (GnomeCmdDir *dir, GList *files);
    void (* list_failed)        (GnomeCmdDir *dir, GnomeVFSResult result);
};

struct GnomeCmdCon;

GnomeCmdDir *gnome_cmd_dir_new_from_info (GnomeVFSFileInfo *info, GnomeCmdDir *parent);
GnomeCmdDir *gnome_cmd_dir_new_from_gfileinfo (GFileInfo *gFileInfo, GnomeCmdDir *parent);
GnomeCmdDir *gnome_cmd_dir_new_with_con (GnomeCmdCon *con);
GnomeCmdDir *gnome_cmd_dir_new (GnomeCmdCon *con, GnomeCmdPath *path, gboolean isStartup = false);
GnomeCmdDir *gnome_cmd_dir_get_parent (GnomeCmdDir *dir);
GnomeCmdDir *gnome_cmd_dir_get_child (GnomeCmdDir *dir, const gchar *child);
GnomeCmdCon *gnome_cmd_dir_get_connection (GnomeCmdDir *dir);
Handle *gnome_cmd_dir_get_handle (GnomeCmdDir *dir);

//inline GnomeCmdFile *gnome_cmd_dir_new_parent_dir_file (GnomeCmdDir *dir)
//{
//    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
//
//    memset (info, '\0', sizeof (GnomeVFSFileInfo));
//    info->name = g_strdup ("..");
//    info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
//    info->mime_type = g_strdup ("x-directory/normal");
//    info->size = 0;
//    info->refcount = 1;
//    info->valid_fields = (GnomeVFSFileInfoFields) (GNOME_VFS_FILE_INFO_FIELDS_TYPE |
//                                                   GNOME_VFS_FILE_INFO_FIELDS_SIZE |
//                                                   GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE);
//
//    return gnome_cmd_file_new (info, dir);
//}
//
inline GnomeCmdFile *gnome_cmd_dir_new_parent_dir_file (GnomeCmdDir *dir)
{
    auto info = g_file_info_new ();

    //memset (info, '\0', sizeof (GFileInfo));
    g_file_info_set_name(info, "..");
    g_file_info_set_display_name(info, "..");
    g_file_info_set_file_type(info, G_FILE_TYPE_DIRECTORY);
    //info->mime_type = g_strdup ("x-directory/normal");
    g_file_info_set_size(info, 0);
    //info->refcount = 1;
    //info->valid_fields = (GnomeVFSFileInfoFields) (GNOME_VFS_FILE_INFO_FIELDS_TYPE |
    //                                               GNOME_VFS_FILE_INFO_FIELDS_SIZE |
    //                                               GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE);

    return gnome_cmd_file_new (info, dir);
}

inline GnomeCmdDir *gnome_cmd_dir_ref (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);
    GNOME_CMD_FILE (dir)->ref();
    return dir;
}

void gnome_cmd_dir_unref (GnomeCmdDir *dir);

GList *gnome_cmd_dir_get_files (GnomeCmdDir *dir);
void gnome_cmd_dir_relist_files (GnomeCmdDir *dir, gboolean visprog);
void gnome_cmd_dir_list_files (GnomeCmdDir *dir, gboolean visprog);

GnomeCmdPath *gnome_cmd_dir_get_path (GnomeCmdDir *dir);
void gnome_cmd_dir_set_path (GnomeCmdDir *dir, GnomeCmdPath *path);
void gnome_cmd_dir_update_path (GnomeCmdDir *dir);
gchar *gnome_cmd_dir_get_display_path (GnomeCmdDir *dir);

GnomeVFSURI *gnome_cmd_dir_get_uri (GnomeCmdDir *dir);
gchar       *gnome_cmd_dir_get_uri_str (GnomeCmdDir *dir);

GnomeVFSURI *gnome_cmd_dir_get_child_uri (GnomeCmdDir *dir, const gchar *filename);
GFile *gnome_cmd_dir_get_child_gfile (GnomeCmdDir *dir, const gchar *filename);

GnomeVFSURI *gnome_cmd_dir_get_absolute_path_uri (GnomeCmdDir *dir, std::string absolute_filename);

void gnome_cmd_dir_file_created (GnomeCmdDir *dir, const gchar *uri_str);
void gnome_cmd_dir_file_deleted (GnomeCmdDir *dir, const gchar *uri_str);
void gnome_cmd_dir_file_changed (GnomeCmdDir *dir, const gchar *uri_str);
void gnome_cmd_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *f, const gchar *old_uri_str);

void gnome_cmd_dir_start_monitoring (GnomeCmdDir *dir);
void gnome_cmd_dir_cancel_monitoring (GnomeCmdDir *dir);
gboolean gnome_cmd_dir_is_monitored (GnomeCmdDir *dir);
gboolean gnome_cmd_dir_is_local (GnomeCmdDir *dir);
void gnome_cmd_dir_set_content_changed (GnomeCmdDir *dir);

gboolean gnome_cmd_dir_update_mtime (GnomeCmdDir *dir);
gboolean gnome_cmd_dir_needs_mtime_update (GnomeCmdDir *dir);

inline gchar *gnome_cmd_dir_get_free_space (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    GnomeVFSFileSize free_space;
    GnomeVFSURI *uri = GNOME_CMD_FILE (dir)->get_uri();
    GnomeVFSResult res = gnome_vfs_get_volume_free_space (uri, &free_space);
    gnome_vfs_uri_unref (uri);

    if (res!=GNOME_VFS_OK)
        return NULL;

    return gnome_vfs_format_file_size_for_display (free_space);
}
