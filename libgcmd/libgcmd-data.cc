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
#include "libgcmd-deps.h"
#include "libgcmd-data.h"


static void set_string (const gchar *path, const gchar *value)
{
    gnome_config_set_string (path, value);
}

static void set_int    (const gchar *path, int value)
{
    gnome_config_set_int (path, value);
}

static void set_bool (const gchar *path, gboolean value)
{
    gnome_config_set_bool (path, value);
}

static gchar *get_string (const gchar *path, const gchar *def)
{
    gboolean b = FALSE;
    gchar *value = gnome_config_get_string_with_default (path, &b);
    if (b)
        return g_strdup (def);
    return value;
}

static gint get_int (const gchar *path, int def)
{
    gboolean b = FALSE;
    gint value = gnome_config_get_int_with_default (path, &b);
    if (b)
        return def;
    return value;
}

static gboolean get_bool (const gchar *path, gboolean def)
{
    gboolean b = FALSE;
    gboolean value = gnome_config_get_bool_with_default (path, &b);
    if (b)
        return def;
    return value;
}


static void set_color (const gchar *path, GdkColor *color)
{
    gchar *color_str;
    color_str = g_strdup_printf ("%d %d %d", color->red, color->green, color->blue);
    set_string (path, color_str);
    g_free (color_str);
}


static void get_color (const gchar *path, GdkColor *color)
{
    gint red, green, blue;
    gchar *def = g_strdup_printf ("%d %d %d",
                                  color->red, color->green, color->blue);
    gchar *color_str = get_string (path, def);
    if (sscanf (color_str, "%u %u %u", &red, &green, &blue) != 3)
        g_printerr ("Illegal color in config file\n");

    if (color_str != def)
        g_free (color_str);

    color->red   = (gushort) red;
    color->green = (gushort) green;
    color->blue  = (gushort) blue;

    g_free (def);
}


void
gnome_cmd_data_set_string (const gchar *path, const gchar *value)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    set_string (s, value);

    g_free (s);
}


void
gnome_cmd_data_set_int (const gchar *path, int value)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    set_int (s, value);

    g_free (s);
}

void
gnome_cmd_data_set_bool (const gchar *path, gboolean value)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    set_bool (s, value);

    g_free (s);
}


void
gnome_cmd_data_set_color (const gchar *path, GdkColor *color)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    set_color (s, color);

    g_free (s);
}


gchar*
gnome_cmd_data_get_string (const gchar *path, const gchar *def)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    gchar *v = get_string (s, def);

    g_free (s);

    return v;
}


gint
gnome_cmd_data_get_int (const gchar *path, int def)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    gint v = get_int (s, def);

    g_free (s);

    return v;
}


gboolean
gnome_cmd_data_get_bool (const gchar *path, gboolean def)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    gboolean v = get_bool (s, def);

    g_free (s);

    return v;
}


void
gnome_cmd_data_get_color (const gchar *path, GdkColor *color)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    get_color (s, color);

    g_free (s);
}

