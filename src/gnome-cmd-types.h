/**
 * @file gnome-cmd-types.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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

typedef gchar *GnomeCmdDateFormat;


enum FileSelectorID
{
    LEFT,
    RIGHT,
    ACTIVE,
    INACTIVE
};


typedef enum
{
    GNOME_CMD_LAYOUT_TEXT,
    GNOME_CMD_LAYOUT_TYPE_ICONS,
    GNOME_CMD_LAYOUT_MIME_ICONS
}GnomeCmdLayout;


typedef enum
{
    GNOME_CMD_SIZE_DISP_MODE_PLAIN,
    GNOME_CMD_SIZE_DISP_MODE_LOCALE,
    GNOME_CMD_SIZE_DISP_MODE_GROUPED,
    GNOME_CMD_SIZE_DISP_MODE_POWERED
}GnomeCmdSizeDispMode;


typedef enum
{
    COPY,
    MOVE,
    LINK
}GnomeCmdTransferType;


typedef enum
{
    GNOME_CMD_PERM_DISP_MODE_TEXT,
    GNOME_CMD_PERM_DISP_MODE_NUMBER
}GnomeCmdPermDispMode;


typedef enum
{
    GNOME_CMD_QUICK_SEARCH_CTRL_ALT,
    GNOME_CMD_QUICK_SEARCH_ALT,
    GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER
}GnomeCmdQuickSearchShortcut;


typedef enum
{
    GNOME_CMD_EXT_DISP_WITH_FNAME,
    GNOME_CMD_EXT_DISP_STRIPPED,
    GNOME_CMD_EXT_DISP_BOTH
}GnomeCmdExtDispMode;

typedef enum // Watch out for the usage of GNOME_CMD_COLOR_CUSTOM in gnome-cmd-data.h and dialogs/gnome-cmd-options-dialog.cc when adding another element here.
{
    GNOME_CMD_COLOR_NONE,
    GNOME_CMD_COLOR_MODERN,
    GNOME_CMD_COLOR_FUSION,
    GNOME_CMD_COLOR_CLASSIC,
    GNOME_CMD_COLOR_DEEP_BLUE,
    GNOME_CMD_COLOR_CAFEZINHO,
    GNOME_CMD_COLOR_GREEN_TIGER,
    GNOME_CMD_COLOR_WINTER,
    GNOME_CMD_COLOR_CUSTOM
}GnomeCmdColorMode;


typedef enum // The (reversed) order of following enums compared to the occurrence in the GUI is significant
{
    GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY,
    GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL,
    GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL,
    GNOME_CMD_CONFIRM_OVERWRITE_QUERY
}GnomeCmdConfirmOverwriteMode;


typedef enum
{
    GNOME_CMD_DEFAULT_DND_QUERY,
    GNOME_CMD_DEFAULT_DND_COPY,
    GNOME_CMD_DEFAULT_DND_MOVE
}GnomeCmdDefaultDndMode;


struct GnomeCmdColorTheme
{
    gboolean respect_theme {TRUE};
    GdkColor *sel_fg {NULL};
    GdkColor *sel_bg {NULL};
    GdkColor *norm_fg {NULL};
    GdkColor *norm_bg {NULL};
    GdkColor *curs_fg {NULL};
    GdkColor *curs_bg {NULL};
    GdkColor *alt_fg {NULL};
    GdkColor *alt_bg {NULL};

    ~GnomeCmdColorTheme()
    {
        if (sel_fg)     gdk_color_free (sel_fg);
        if (sel_bg)     gdk_color_free (sel_bg);
        if (norm_fg)    gdk_color_free (norm_fg);
        if (norm_bg)    gdk_color_free (norm_bg);
        if (curs_fg)    gdk_color_free (curs_fg);
        if (curs_bg)    gdk_color_free (curs_bg);
        if (alt_fg)     gdk_color_free (alt_fg);
        if (alt_bg)     gdk_color_free (alt_bg);
    }

    GnomeCmdColorTheme &operator = (const GnomeCmdColorTheme &colors);
};


inline GnomeCmdColorTheme &GnomeCmdColorTheme::operator = (const GnomeCmdColorTheme &colors)
{
    if (this != &colors)
    {
        respect_theme = colors.respect_theme;
        sel_fg = gdk_color_copy (colors.sel_fg);
        sel_bg = gdk_color_copy (colors.sel_bg);
        norm_fg = gdk_color_copy (colors.norm_fg);
        norm_bg = gdk_color_copy (colors.norm_bg);
        curs_fg = gdk_color_copy (colors.curs_fg);
        curs_bg = gdk_color_copy (colors.curs_bg);
        alt_fg = gdk_color_copy (colors.alt_fg);
        alt_bg = gdk_color_copy (colors.alt_bg);
    }

    return *this;
}


struct GnomeCmdLsColorsPalette
{
    GdkColor *black_fg, *black_bg;
    GdkColor *red_fg, *red_bg;
    GdkColor *green_fg, *green_bg;
    GdkColor *yellow_fg, *yellow_bg;
    GdkColor *blue_fg, *blue_bg;
    GdkColor *magenta_fg, *magenta_bg;
    GdkColor *cyan_fg, *cyan_bg;
    GdkColor *white_fg, *white_bg;
};


struct GnomeCmdCon;


struct GnomeCmdBookmarkGroup
{
    GList *bookmarks;
    GnomeCmdCon *con;
};


struct GnomeCmdBookmark
{
    gchar *name;
    gchar *path;
    GnomeCmdBookmarkGroup *group;
};
