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

#include <config.h>
#include <sys/types.h>
#include <regex.h>
#include "gnome-cmd-includes.h"
#include "filter.h"
#include "gnome-cmd-data.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fnmatch.h>

static Filter *
new_regex (const gchar *exp, gboolean case_sens)
{
    int flags = 0;
    Filter *filter = g_new (Filter, 1);

    filter->type = FILTER_TYPE_REGEX;
    filter->re_exp = g_new (regex_t, 1);

    if (case_sens) flags = REG_ICASE;
    regcomp (filter->re_exp, exp, flags);

    return filter;
}


static Filter *
new_fnmatch (const gchar *exp, gboolean case_sens)
{
    Filter *filter = g_new (Filter, 1);

    filter->type = FILTER_TYPE_FNMATCH;
    filter->fn_exp = g_strdup (exp);
    filter->fn_flags = FNM_NOESCAPE;

    if (!case_sens)
        filter->fn_flags |= FNM_CASEFOLD;

    return filter;
}


Filter *
filter_new (const gchar *exp, gboolean case_sens)
{
    FilterType type = gnome_cmd_data_get_filter_type ();

    switch (type)
    {
        case FILTER_TYPE_REGEX:
            return new_regex (exp, case_sens);

        case FILTER_TYPE_FNMATCH:
            return new_fnmatch (exp, case_sens);

        default:
            g_printerr ("Unknown FilterType (%d) in filter_new\n", type);
    }

    return NULL;
}


void
filter_free (Filter *filter)
{
    g_return_if_fail (filter != NULL);

    switch (filter->type)
    {
        case FILTER_TYPE_REGEX:
            regfree (filter->re_exp);
            g_free (filter->re_exp);
            break;

        case FILTER_TYPE_FNMATCH:
            g_free (filter->fn_exp);
            break;

        default:
            g_printerr ("Unknown FilterType (%d) in filter_free\n", filter->type);
    }

    g_free (filter);
}


gboolean
filter_match (Filter *filter, gchar *text)
{
    static regmatch_t match;
    g_return_val_if_fail (filter != NULL, FALSE);

    switch (filter->type)
    {
        case FILTER_TYPE_REGEX:
            return regexec (filter->re_exp, text, 1, &match, 0) == 0;

        case FILTER_TYPE_FNMATCH:
            return (fnmatch (filter->fn_exp, text, filter->fn_flags) == 0);
            break;

        default:
            g_printerr ("Unknown FilterType (%d) in filter_match\n", filter->type);
    }

    return FALSE;
}


