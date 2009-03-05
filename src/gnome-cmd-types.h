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
#ifndef __GNOME_CMD_TYPES_H__
#define __GNOME_CMD_TYPES_H__

typedef gchar *GnomeCmdDateFormat;

enum GnomeCmdLayout
{
    GNOME_CMD_LAYOUT_TEXT,
    GNOME_CMD_LAYOUT_TYPE_ICONS,
    GNOME_CMD_LAYOUT_MIME_ICONS
};


enum GnomeCmdSizeDispMode
{
    GNOME_CMD_SIZE_DISP_MODE_PLAIN,
    GNOME_CMD_SIZE_DISP_MODE_LOCALE,
    GNOME_CMD_SIZE_DISP_MODE_GROUPED,
    GNOME_CMD_SIZE_DISP_MODE_POWERED
};


enum GnomeCmdPermDispMode
{
    GNOME_CMD_PERM_DISP_MODE_TEXT,
    GNOME_CMD_PERM_DISP_MODE_NUMBER
};


enum GnomeCmdExtDispMode
{
    GNOME_CMD_EXT_DISP_WITH_FNAME,
    GNOME_CMD_EXT_DISP_STRIPPED,
    GNOME_CMD_EXT_DISP_BOTH
};


enum GnomeCmdColorMode
{
    GNOME_CMD_COLOR_NONE,
    GNOME_CMD_COLOR_MODERN,
    GNOME_CMD_COLOR_FUSION,
    GNOME_CMD_COLOR_CLASSIC,
    GNOME_CMD_COLOR_DEEP_BLUE,
    GNOME_CMD_COLOR_CAFEZINHO,
    GNOME_CMD_COLOR_CUSTOM,
    GNOME_CMD_NUM_COLOR_MODES
};


enum GnomeCmdConfirmOverwriteMode  // The (reversed) order of following enums is significant
{
    GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL,
    GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
    GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY
};


struct GnomeCmdColorTheme
{
    gboolean respect_theme;
    GdkColor *sel_fg, *sel_bg;
    GdkColor *norm_fg, *norm_bg;
    GdkColor *curs_fg, *curs_bg;
    GdkColor *alt_fg, *alt_bg;
};


struct GnomeCmdCon;


struct GnomeCmdBookmarkGroup
{
    GList *bookmarks;
    GnomeCmdCon *con;
    gpointer *data;
};


struct GnomeCmdBookmark
{
    gchar *name;
    gchar *path;
    GnomeCmdBookmarkGroup *group;
};

#endif // __GNOME_CMD_TYPES_H__
