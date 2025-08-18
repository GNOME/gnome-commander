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


#include <string>

#include "gnome-cmd-file.h"
#include "gnome-cmd-path.h"

struct GnomeCmdDir
{
    GnomeCmdFile parent;

    enum State
    {
        STATE_EMPTY,
        STATE_LISTED,
        STATE_LISTING,
        STATE_CANCELING
    };
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
    void (* dir_deleted)        (GnomeCmdDir *dir);
};

struct GnomeCmdCon;

extern "C" GnomeCmdDir *gnome_cmd_dir_new_from_gfileinfo (GFileInfo *gFileInfo, GnomeCmdDir *parent);
extern "C" GnomeCmdDir *gnome_cmd_dir_new (GnomeCmdCon *con, GnomeCmdPath *path, gboolean isStartup = false);

extern "C" GnomeCmdPath *gnome_cmd_dir_get_path (GnomeCmdDir *dir);

gchar *gnome_cmd_dir_get_relative_path_string(const char* childPathString, const char* basePath);
GFile *gnome_cmd_dir_get_gfile_for_con_and_filename(GnomeCmdDir *dir, const gchar *filename);
extern "C" GFile *gnome_cmd_dir_get_child_gfile (GnomeCmdDir *dir, const gchar *filename);
