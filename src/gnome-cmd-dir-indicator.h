/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#ifndef __GNOME_CMD_DIR_INDICATOR_H__
#define __GNOME_CMD_DIR_INDICATOR_H__

#include "gnome-cmd-con.h"
#include "gnome-cmd-con-ftp.h"
#include "gnome-cmd-con-device.h"
#include "gnome-cmd-file-selector.h"

#define GNOME_CMD_DIR_INDICATOR(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_dir_indicator_get_type (), GnomeCmdDirIndicator)
#define GNOME_CMD_DIR_INDICATOR_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_dir_indicator_get_type (), GnomeCmdDirIndicatorClass)
#define GNOME_CMD_IS_DIR_INDICATOR(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_dir_indicator_get_type ())


struct GnomeCmdDirIndicatorPrivate;


struct GnomeCmdDirIndicator
{
    GtkFrame parent;

    GnomeCmdDirIndicatorPrivate *priv;
};

struct GnomeCmdDirIndicatorClass
{
    GtkFrameClass parent_class;
};


GtkType gnome_cmd_dir_indicator_get_type ();

GtkWidget *gnome_cmd_dir_indicator_new (GnomeCmdFileSelector *fs);

void gnome_cmd_dir_indicator_set_dir (GnomeCmdDirIndicator *indicator, gchar *path);

void gnome_cmd_dir_indicator_set_active (GnomeCmdDirIndicator *indicator, gboolean value);

void gnome_cmd_dir_indicator_show_history (GnomeCmdDirIndicator *indicator);

void gnome_cmd_dir_indicator_show_bookmarks (GnomeCmdDirIndicator *indicator);

#endif // __GNOME_CMD_DIR_INDICATOR_H__
