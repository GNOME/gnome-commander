/** 
 * @file filter.h
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

#include <fnmatch.h>
#include <regex.h>


struct Filter
{
    enum Type
    {
        TYPE_REGEX,
        TYPE_FNMATCH
    };

    Type type;          // common stuff
    regex_t *re_exp;    // regex filtering stuff
    char *fn_exp;       // fnmatch filtering stuff
    int fn_flags;       // fnmatch filtering stuff

    Filter(const gchar *exp, gboolean case_sens, Type type);
    ~Filter();

    gboolean match(const gchar *text);
};

extern "C" Filter *gnome_cmd_filter_new (const gchar *exp, gboolean case_sens, Filter::Type type);
extern "C" void gnome_cmd_filter_free (Filter *filter);
extern "C" gboolean gnome_cmd_filter_match (Filter *filter, const gchar *text);

extern "C" gboolean gnome_cmd_filter_fnmatch (const gchar *pattern, const gchar *string, gboolean case_sens);
