/** 
 * @file gnome-cmd-dir-indicator.h
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

#include "gnome-cmd-file-selector.h"

#define GNOME_CMD_TYPE_DIR_INDICATOR              (gnome_cmd_dir_indicator_get_type ())
#define GNOME_CMD_DIR_INDICATOR(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_DIR_INDICATOR, GnomeCmdDirIndicator))
#define GNOME_CMD_DIR_INDICATOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_DIR_INDICATOR, GnomeCmdDirIndicatorClass))
#define GNOME_CMD_IS_DIR_INDICATOR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_DIR_INDICATOR))
#define GNOME_CMD_IS_DIR_INDICATOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_DIR_INDICATOR))
#define GNOME_CMD_DIR_INDICATOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_DIR_INDICATOR, GnomeCmdDirIndicatorClass))


struct GnomeCmdDirIndicator
{
    GtkWidget parent;
};

struct GnomeCmdDirIndicatorClass
{
    GtkWidgetClass parent_class;

    void (*navigate) (const gchar *path, gboolean new_tab);
};


GType gnome_cmd_dir_indicator_get_type ();

GtkWidget *gnome_cmd_dir_indicator_new (GnomeCmdFileSelector *fs);

void gnome_cmd_dir_indicator_set_dir (GnomeCmdDirIndicator *indicator, GnomeCmdDir *dir);

void gnome_cmd_dir_indicator_set_active (GnomeCmdDirIndicator *indicator, gboolean value);

void gnome_cmd_dir_indicator_show_history (GnomeCmdDirIndicator *indicator);

void gnome_cmd_dir_indicator_show_bookmarks (GnomeCmdDirIndicator *indicator);
