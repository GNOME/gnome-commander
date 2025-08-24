/**
 * @file gnome-cmd-file.h
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

#include "gnome-cmd-types.h"

#define GNOME_CMD_TYPE_FILE              (gnome_cmd_file_get_type ())
#define GNOME_CMD_FILE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE, GnomeCmdFile))
#define GNOME_CMD_FILE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE, GnomeCmdFileClass))
#define GNOME_CMD_IS_FILE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE))
#define GNOME_CMD_IS_FILE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE))
#define GNOME_CMD_FILE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE, GnomeCmdFileClass))


extern "C" GType gnome_cmd_file_get_type ();


struct GnomeCmdDir;
struct GnomeCmdCon;
struct GnomeCmdPath;


struct GnomeCmdFile
{
    GObject parent;

    GFile *get_file() { return gnome_cmd_file_descriptor_get_file (GNOME_CMD_FILE_DESCRIPTOR (this)); }
    GFileInfo *get_file_info() { return gnome_cmd_file_descriptor_get_file_info (GNOME_CMD_FILE_DESCRIPTOR (this)); }

    const gchar *get_name();
    gchar *get_real_path();

    gchar *get_uri_str();
};

struct GnomeCmdFileClass
{
    GObjectClass parent_class;

    /* virtual functions */
    GnomeCmdCon *(* get_connection) (GnomeCmdFile *f);
};


inline const gchar *GnomeCmdFile::get_name()
{
    g_return_val_if_fail (get_file_info () != NULL, NULL);
    return g_file_info_get_display_name (get_file_info ());
}

extern "C" GnomeCmdFile *gnome_cmd_file_new (GFileInfo *gFileInfo, GnomeCmdDir *dir);
extern "C" GnomeCmdFile *gnome_cmd_file_new_full (GFileInfo *gFileInfo, GFile *gFile, GnomeCmdDir *dir);

extern "C" gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *f);

extern "C" gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f);
extern "C" gboolean gnome_cmd_file_is_local (GnomeCmdFile *f);
extern "C" GnomeCmdCon *gnome_cmd_file_get_connection (GnomeCmdFile *f);
