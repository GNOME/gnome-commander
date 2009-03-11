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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-types.h"

using namespace std;


GtkStyle *list_style = NULL;
GtkStyle *alt_list_style = NULL;
GtkStyle *sel_list_style = NULL;
GtkStyle *alt_sel_list_style = NULL;


inline GtkStyle *create_list_style ()
{
    GnomeCmdColorTheme *cols = gnome_cmd_data_get_current_color_theme ();
    const gchar *font_name = gnome_cmd_data_get_list_font ();
    GtkStyle *style = gtk_style_new ();

    if (strcmp (font_name, "default") != 0)
    {
        PangoFontDescription *pfont;
        pfont = pango_font_description_from_string (font_name);

        if (pfont && pango_font_description_get_size (pfont) != 0)
        {
            if (style->font_desc)
                pango_font_description_free (style->font_desc);
            style->font_desc = pfont;
        }
    }

    if (!cols->respect_theme)
    {
        style->fg[GTK_STATE_SELECTED] = *cols->curs_fg;
        style->text[GTK_STATE_NORMAL] = *cols->norm_fg;
        style->fg[GTK_STATE_NORMAL] = *cols->norm_fg;

        style->bg[GTK_STATE_SELECTED] = *cols->curs_bg;
        style->base[GTK_STATE_NORMAL] = *cols->norm_bg;
        style->base[GTK_STATE_ACTIVE] = *cols->norm_bg;
    }

    return style;
}


inline GtkStyle *create_alt_list_style ()
{
    GnomeCmdColorTheme *cols = gnome_cmd_data_get_current_color_theme ();
    const gchar *font_name = gnome_cmd_data_get_list_font ();
    GtkStyle *style = gtk_style_new ();

    if (strcmp (font_name, "default") != 0)
    {
        PangoFontDescription *pfont = pango_font_description_from_string (font_name);

        if (pfont && pango_font_description_get_size (pfont) != 0)
        {
            if (style->font_desc)
                pango_font_description_free (style->font_desc);
            style->font_desc = pfont;
        }
    }

    if (!cols->respect_theme)
    {
        style->fg[GTK_STATE_SELECTED] = *cols->curs_fg;
        style->text[GTK_STATE_NORMAL] = *cols->alt_fg;
        style->fg[GTK_STATE_NORMAL] = *cols->alt_fg;

        style->bg[GTK_STATE_SELECTED] = *cols->curs_bg;
        style->base[GTK_STATE_NORMAL] = *cols->alt_bg;
        style->base[GTK_STATE_ACTIVE] = *cols->alt_bg;
    }

    return style;
}


inline GtkStyle *create_sel_list_style ()
{
    GnomeCmdColorTheme *cols = gnome_cmd_data_get_current_color_theme ();
    const gchar *font_name = gnome_cmd_data_get_list_font ();
    GtkStyle *style = gtk_style_new ();

    if (strcmp (font_name, "default") != 0)
    {
        PangoFontDescription *pfont = pango_font_description_from_string (font_name);

        if (pfont && pango_font_description_get_size (pfont) != 0)
        {
            if (style->font_desc)
                pango_font_description_free (style->font_desc);
            style->font_desc = pfont;
        }
    }

    if (!cols->respect_theme)
    {
        style->fg[GTK_STATE_SELECTED] = *cols->sel_fg;
        style->fg[GTK_STATE_NORMAL] = *cols->sel_fg;
        style->text[GTK_STATE_NORMAL] = *cols->sel_fg;

        style->bg[GTK_STATE_SELECTED] = *cols->curs_bg;
        style->bg[GTK_STATE_NORMAL] = *cols->sel_bg;
        style->base[GTK_STATE_NORMAL] = *cols->norm_bg;
        style->base[GTK_STATE_ACTIVE] = *cols->norm_bg;
    }

    return style;
}


inline GtkStyle *create_alt_sel_list_style ()
{
    GnomeCmdColorTheme *cols = gnome_cmd_data_get_current_color_theme ();
    const gchar *font_name = gnome_cmd_data_get_list_font ();
    GtkStyle *style = gtk_style_new ();

    if (strcmp (font_name, "default") != 0)
    {
        PangoFontDescription *pfont = pango_font_description_from_string (font_name);

        if (pfont && pango_font_description_get_size (pfont) != 0)
        {
            if (style->font_desc)
                pango_font_description_free (style->font_desc);
            style->font_desc = pfont;
        }
    }

    if (!cols->respect_theme)
    {
        style->fg[GTK_STATE_SELECTED] = *cols->sel_fg;
        style->fg[GTK_STATE_NORMAL] = *cols->sel_fg;
        style->text[GTK_STATE_NORMAL] = *cols->sel_fg;

        style->bg[GTK_STATE_SELECTED] = *cols->curs_bg;
        style->bg[GTK_STATE_NORMAL] = *cols->sel_bg;
        style->base[GTK_STATE_NORMAL] = *cols->alt_bg;
        style->base[GTK_STATE_ACTIVE] = *cols->alt_bg;
    }

    return style;
}


void gnome_cmd_style_create ()
{
    if (list_style) gtk_style_unref (list_style);
    if (alt_list_style) gtk_style_unref (alt_list_style);
    if (sel_list_style) gtk_style_unref (sel_list_style);
    if (alt_sel_list_style) gtk_style_unref (alt_sel_list_style);

    list_style = create_list_style ();
    alt_list_style = create_alt_list_style ();
    sel_list_style = create_sel_list_style ();
    alt_sel_list_style = create_alt_sel_list_style ();
}
