/** 
 * @file gnome-cmd-file-selector.h
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

#define GNOME_CMD_TYPE_FILE_SELECTOR              (gnome_cmd_file_selector_get_type ())
#define GNOME_CMD_FILE_SELECTOR(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE_SELECTOR, GnomeCmdFileSelector))
#define GNOME_CMD_FILE_SELECTOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE_SELECTOR, GnomeCmdFileSelectorClass))
#define GNOME_CMD_IS_FILE_SELECTOR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE_SELECTOR))
#define GNOME_CMD_IS_FILE_SELECTOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE_SELECTOR))
#define GNOME_CMD_FILE_SELECTOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE_SELECTOR, GnomeCmdFileSelectorClass))

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-dir.h"


struct GnomeCmdFileSelector
{
    GtkGrid parent;

  public:

    GnomeCmdFileList *file_list()
    {
        GnomeCmdFileList *list = nullptr;
        g_object_get (this, "list", &list, nullptr);
        return list;
    }
};

extern "C" GType gnome_cmd_file_selector_get_type ();

extern "C" void gnome_cmd_file_selector_go_to_file(GnomeCmdFileSelector *fs, GnomeCmdFile *f);
