/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2014 Uwe Scholz

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

#ifndef __FILTER_H__
#define __FILTER_H__

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

inline gboolean gnome_cmd_filter_fnmatch (const gchar *pattern, const gchar *string, gboolean case_sens)
{
#ifdef FNM_CASEFOLD
    return pattern && string && fnmatch (pattern, string, case_sens ? FNM_NOESCAPE : FNM_NOESCAPE|FNM_CASEFOLD)==0;
#else
    return pattern && string && fnmatch (pattern, string, FNM_NOESCAPE) == 0;
#endif
}

#endif // __FILTER_H__
