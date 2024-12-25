/**
 * @file gnome-cmd-file-base.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 * @copyright (C) 2024-2025 Andrey Kutejko\n
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

#include <glib.h>
#include <gio/gio.h>

#ifdef __cplusplus
extern "C" {
#endif


#define GNOME_CMD_TYPE_FILE_BASE              (gnome_cmd_file_base_get_type ())
#define GNOME_CMD_FILE_BASE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE_BASE, GnomeCmdFileBase))
#define GNOME_CMD_FILE_BASE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE_BASE, GnomeCmdFileBaseClass))
#define GNOME_CMD_IS_FILE_BASE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE_BASE))
#define GNOME_CMD_IS_FILE_BASE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE_BASE))
#define GNOME_CMD_FILE_BASE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE_BASE, GnomeCmdFileBaseClass))


GType gnome_cmd_file_base_get_type ();


typedef struct GnomeCmdFileBase
{
    GObject parent;

    GFile *gFile;
    GFileInfo *gFileInfo;
} GnomeCmdFileBase;

typedef struct GnomeCmdFileBaseClass
{
    GObjectClass parent_class;
} GnomeCmdFileBaseClass;

GFile *gnome_cmd_file_base_get_file(GnomeCmdFileBase *file_base);
GFileInfo *gnome_cmd_file_base_get_file_info(GnomeCmdFileBase *file_base);

#ifdef __cplusplus
}
#endif
