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
#include <sys/types.h>
#include <regex.h>
#include <fnmatch.h>

#include "gnome-cmd-includes.h"
#include "filter.h"

using namespace std;


Filter::Filter(const gchar *exp, gboolean case_sens, Type type): re_exp(NULL), fn_exp(NULL), fn_flags(0)
{
    this->type = type;

    switch (type)
    {
        case TYPE_REGEX:
            re_exp = g_new (regex_t, 1);
            regcomp (re_exp, exp, case_sens ? 0 : REG_ICASE);
            break;

        case TYPE_FNMATCH:
            fn_exp = g_strdup (exp);
            fn_flags = FNM_NOESCAPE;
#ifdef FNM_CASEFOLD
            if (!case_sens)
                fn_flags |= FNM_CASEFOLD;
#endif
            break;

        default:
            g_printerr ("Unknown Filter::Type (%d) in constructor\n", type);
    }
}


Filter::~Filter()
{
    if (type==TYPE_REGEX)
        regfree (re_exp);

    g_free (re_exp);
    g_free (fn_exp);
}


gboolean Filter::match(const gchar *text)
{
    static regmatch_t match;

    switch (type)
    {
        case TYPE_REGEX:
            return regexec (re_exp, text, 1, &match, 0) == 0;

        case TYPE_FNMATCH:
            return fnmatch (fn_exp, text, fn_flags) == 0;

        default:
            return FALSE;
    }
}
