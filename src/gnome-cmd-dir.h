/**
 * @file gnome-cmd-dir.h
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

#pragma once

#define GNOME_CMD_TYPE_DIR              (gnome_cmd_dir_get_type ())
#define GNOME_CMD_DIR(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_DIR, GnomeCmdDir))
#define GNOME_CMD_DIR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_DIR, GnomeCmdDirClass))
#define GNOME_CMD_IS_DIR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_DIR))
#define GNOME_CMD_IS_DIR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_DIR))
#define GNOME_CMD_DIR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_DIR, GnomeCmdDirClass))


extern "C" GType gnome_cmd_dir_get_type ();


struct GnomeCmdDir;
struct GnomeCmdDirPrivate;

#include <string>

#include "gnome-cmd-file.h"
#include "gnome-cmd-path.h"

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

    State state;
};

struct GnomeCmdDirClass
{
    GnomeCmdFileClass parent_class;

    void (* file_created)       (GnomeCmdDir *dir, GnomeCmdFile *file);
    void (* file_deleted)       (GnomeCmdDir *dir, GnomeCmdFile *file);
    void (* file_changed)       (GnomeCmdDir *dir, GnomeCmdFile *file);
    void (* file_renamed)       (GnomeCmdDir *dir, GnomeCmdFile *file);
    void (* list_ok)            (GnomeCmdDir *dir, GList *files);
    void (* list_failed)        (GnomeCmdDir *dir, GError *error);
};

struct GnomeCmdCon;

GnomeCmdDir *gnome_cmd_dir_new_from_gfileinfo (GFileInfo *gFileInfo, GnomeCmdDir *parent);
GnomeCmdDir *gnome_cmd_dir_new_with_con (GnomeCmdCon *con);
extern "C" GnomeCmdDir *gnome_cmd_dir_new (GnomeCmdCon *con, GnomeCmdPath *path, gboolean isStartup = false);
GnomeCmdDir *gnome_cmd_dir_get_parent (GnomeCmdDir *dir);
GnomeCmdDir *gnome_cmd_dir_get_child (GnomeCmdDir *dir, const gchar *child);
extern "C" GnomeCmdCon *gnome_cmd_dir_get_connection (GnomeCmdDir *dir);

inline GnomeCmdFile *gnome_cmd_dir_new_parent_dir_file (GnomeCmdDir *dir)
{
    auto info = g_file_info_new ();

    g_file_info_set_name(info, "..");
    g_file_info_set_display_name(info, "..");
    g_file_info_set_file_type(info, G_FILE_TYPE_DIRECTORY);
    g_file_info_set_size(info, 0);

    auto gnomeCmdFile = gnome_cmd_file_new (info, dir);
    gnomeCmdFile->is_dotdot = true;
    gnomeCmdFile->ref();
    return gnomeCmdFile;
}

inline GnomeCmdDir *gnome_cmd_dir_ref (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);
    GNOME_CMD_FILE (dir)->ref();
    return dir;
}

void gnome_cmd_dir_unref (GnomeCmdDir *dir);

GList *gnome_cmd_dir_get_files (GnomeCmdDir *dir);
extern "C" void gnome_cmd_dir_relist_files (GtkWindow *parent_window, GnomeCmdDir *dir, gboolean visprog);
void gnome_cmd_dir_list_files (GtkWindow *parent_window, GnomeCmdDir *dir, gboolean visprog);

GnomeCmdPath *gnome_cmd_dir_get_path (GnomeCmdDir *dir);
void gnome_cmd_dir_update_path (GnomeCmdDir *dir);
extern "C" gchar *gnome_cmd_dir_get_display_path (GnomeCmdDir *dir);

GFile       *gnome_cmd_dir_get_gfile (GnomeCmdDir *dir);
gchar       *gnome_cmd_dir_get_uri_str (GnomeCmdDir *dir, gboolean withTrailingSlash = false);

gchar *gnome_cmd_dir_get_relative_path_string(const char* childPathString, const char* basePath);
GFile *gnome_cmd_dir_get_gfile_for_con_and_filename(GnomeCmdDir *dir, const gchar *filename);
GFile *gnome_cmd_dir_get_child_gfile (GnomeCmdDir *dir, const gchar *filename);

GFile *gnome_cmd_dir_get_absolute_path_gfile (GnomeCmdDir *dir, std::string absolute_filename);

GnomeCmdDir *gnome_cmd_dir_get_existing_parent(GnomeCmdDir *dir);
void gnome_cmd_dir_update_file_selector(GnomeCmdDir *dir, const gchar *deletedDirUriString);

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
