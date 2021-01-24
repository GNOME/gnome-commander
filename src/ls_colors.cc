/** 
 * @file ls_colors.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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

#include <config.h>
#include <stdlib.h>

#include "gnome-cmd-includes.h"
#include "ls_colors.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-data.h"

using namespace std;


#define DEFAULT_COLORS "no=00:fi=00:di=01;34:ln=01;36:pi=40;33:so=01;35:do=01;35:bd=40;33;01:cd=40;33;01:or=40;31;01:ex=01;32:*.tar=01;31:*.tgz=01;31:*.arj=01;31:*.taz=01;31:*.lzh=01;31:*.zip=01;31:*.z=01;31:*.Z=01;31:*.gz=01;31:*.bz2=01;31:*.deb=01;31:*.rpm=01;31:*.jar=01;31:*.jpg=01;35:*.jpeg=01;35:*.png=01;35:*.gif=01;35:*.bmp=01;35:*.pbm=01;35:*.pgm=01;35:*.ppm=01;35:*.tga=01;35:*.xbm=01;35:*.xpm=01;35:*.tif=01;35:*.tiff=01;35:*.mpg=01;35:*.mpeg=01;35:*.avi=01;35:*.fli=01;35:*.gl=01;35:*.dl=01;35:*.xcf=01;35:*.xwd=01;35:*.ogg=01;35:*.mp3=01;35:"


static GHashTable *map;
static LsColor *type_colors[8];


/*
# Attribute codes:
# 00=none 01=bold 04=underscore 05=blink 07=reverse 08=concealed
# Text color codes:
# 30=black 31=red 32=green 33=yellow 34=blue 35=magenta 36=cyan 37=white
# Background color codes:
# 40=black 41=red 42=green 43=yellow 44=blue 45=magenta 46=cyan 47=white
*/
inline GdkColor *code2color (gint code)
{
    GnomeCmdLsColorsPalette &palette = gnome_cmd_data.options.ls_colors_palette;

    switch (code)
    {
        case 30: return palette.black_fg;
        case 31: return palette.red_fg;
        case 32: return palette.green_fg;
        case 33: return palette.yellow_fg;
        case 34: return palette.blue_fg;
        case 35: return palette.magenta_fg;
        case 36: return palette.cyan_fg;
        case 37: return palette.white_fg;

        case 40: return palette.black_bg;
        case 41: return palette.red_bg;
        case 42: return palette.green_bg;
        case 43: return palette.yellow_bg;
        case 44: return palette.blue_bg;
        case 45: return palette.magenta_bg;
        case 46: return palette.cyan_bg;
        case 47: return palette.white_bg;
        default: break;
    }

    return nullptr;
}


static LsColor *ext_color (gchar *key, gchar *val)
{
    int i, ret, n[3];
    LsColor *col;

    ret = sscanf (val, "%d;%d;%d", &n[0], &n[1], &n[2]);
    if (ret < 1)
        return nullptr;

    do
        key++;
    while (key[0] == '.');
    col = g_new (LsColor, 1);
    col->type = G_FILE_TYPE_REGULAR;
    col->ext = g_strdup (key);
    col->fg = nullptr;
    col->bg = nullptr;

    for (i=0; i<ret; i++)
    {
        if (n[i] >= 30 && n[i] <= 37)
            col->fg = code2color (n[i]);
        else
            if (n[i] >= 40 && n[i] <= 47)
                col->bg = code2color (n[i]);
    }

    return col;
}


static LsColor *type_color (gchar *key, gchar *val)
{
    int i, n[3];
    LsColor *col = g_new0 (LsColor, 1);
    // col->ext = nullptr;
    // col->fg = nullptr;
    // col->bg = nullptr;

    if (strcmp (key, "fi") == 0)
        col->type = G_FILE_TYPE_REGULAR;
    else if (strcmp (key, "di") == 0)
        col->type = G_FILE_TYPE_DIRECTORY;
    else if (strcmp (key, "ln") == 0)
        col->type = G_FILE_TYPE_SYMBOLIC_LINK;
    else if (strcmp (key, "pi") == 0)
        col->type = G_FILE_TYPE_SPECIAL;
    else if (strcmp (key, "sc") == 0)
        col->type = G_FILE_TYPE_SHORTCUT;
    else if (strcmp (key, "mn") == 0)
        col->type = G_FILE_TYPE_MOUNTABLE;
    else
    {
        g_free (col);
        return nullptr;
    }

    int ret = sscanf (val, "%d;%d;%d", &n[0], &n[1], &n[2]);
    for (i=0; i<ret; i++)
    {
        if (n[i] >= 30 && n[i] <= 37)
            col->fg = code2color (n[i]);
        else
            if (n[i] >= 40 && n[i] <= 47)
                col->bg = code2color (n[i]);
    }

    return col;
}


static LsColor *create_color (gchar *ls_color)
{
    LsColor *col = nullptr;

    gchar **s = g_strsplit (ls_color, "=", 0);
    gchar *key = s[0];

    if (key)
    {
        gchar *val = s[1];
        col = key[0] == '*' ? ext_color (key, val) : type_color (key, val);
    }

    g_strfreev (s);
    return col;
}


static void init (const gchar *ls_colors)
{
    gchar **ents = g_strsplit (ls_colors, ":", 0);

    if (!ents) return;

    gint i=0;

    for (i=0; ents[i]; ++i)
    {
        LsColor *col = create_color (ents[i]);
        if (col)
        {
            if (col->ext)
                g_hash_table_insert (::map, col->ext, col);
            else
                type_colors[col->type] = col;
        }
    }

    g_strfreev (ents);
}


void ls_colors_init ()
{
    const gchar *s = getenv ("LS_COLORS");
    if (!s)
        s = DEFAULT_COLORS;

    ::map = g_hash_table_new (g_str_hash, g_str_equal);
    init (s);
}


LsColor *ls_colors_get (GnomeCmdFile *f)
{
    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_SYMBOLIC_LINK)
        return type_colors[G_FILE_TYPE_SYMBOLIC_LINK];

    LsColor *col = nullptr;
    const gchar *ext = f->get_extension();

    if (ext)
        col = static_cast<LsColor*> (g_hash_table_lookup (::map, ext));

    if (!col)
        col = type_colors[f->info->type];

    return col;
}
