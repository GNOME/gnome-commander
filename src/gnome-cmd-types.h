/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#ifndef __GNOME_CMD_TYPES_H__
#define __GNOME_CMD_TYPES_H__

typedef struct _GnomeCmdBookmarkGroup GnomeCmdBookmarkGroup;
typedef gchar *GnomeCmdDateFormat;

#include "gnome-cmd-con.h"

G_BEGIN_DECLS

typedef enum
{
    GNOME_CMD_LAYOUT_TEXT,
    GNOME_CMD_LAYOUT_TYPE_ICONS,
    GNOME_CMD_LAYOUT_MIME_ICONS
} GnomeCmdLayout;


typedef enum
{
    GNOME_CMD_SIZE_DISP_MODE_PLAIN,
    GNOME_CMD_SIZE_DISP_MODE_LOCALE,
    GNOME_CMD_SIZE_DISP_MODE_GROUPED,
    GNOME_CMD_SIZE_DISP_MODE_POWERED
} GnomeCmdSizeDispMode;


typedef enum
{
    GNOME_CMD_PERM_DISP_MODE_TEXT,
    GNOME_CMD_PERM_DISP_MODE_NUMBER
} GnomeCmdPermDispMode;


typedef enum
{
    GNOME_CMD_EXT_DISP_WITH_FNAME,
    GNOME_CMD_EXT_DISP_STRIPPED,
    GNOME_CMD_EXT_DISP_BOTH
} GnomeCmdExtDispMode;


typedef enum
{
    GNOME_CMD_COLOR_NONE,
    GNOME_CMD_COLOR_MODERN,
    GNOME_CMD_COLOR_FUSION,
    GNOME_CMD_COLOR_CLASSIC,
    GNOME_CMD_COLOR_DEEP_BLUE,
    GNOME_CMD_COLOR_CUSTOM,
    GNOME_CMD_NUM_COLOR_MODES
} GnomeCmdColorMode;


typedef enum  // The (reversed) order of following enums is significant
{
    GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL,
    GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
    GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY
} GnomeCmdConfirmOverwriteMode;


typedef struct
{
    gboolean respect_theme;
    GdkColor *sel_fg, *sel_bg;
    GdkColor *norm_fg, *norm_bg;
    GdkColor *curs_fg, *curs_bg;
} GnomeCmdColorTheme;


struct _GnomeCmdBookmarkGroup
{
    GList *bookmarks;
    GnomeCmdCon *con;
    gpointer *data;
};


typedef struct
{
    gchar *name;
    gchar *path;
    GnomeCmdBookmarkGroup *group;
} GnomeCmdBookmark;

G_END_DECLS

#endif // __GNOME_CMD_TYPES_H__
