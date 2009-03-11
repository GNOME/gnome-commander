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
#include "history.h"
#include "utils.h"

using namespace std;


History::~History()
{
    if (ents)
    {
        g_list_foreach (ents, (GFunc) g_free, NULL);
        g_list_free (ents);
    }
}


void History::add(const gchar *text)
{
    if (is_locked)
        return;

    // If we are in the middle of the history list, lets kill all items that are in front of us
    GList *l = ents;
    while (l && l != pos)
    {
        g_free (l->data);
        GList *n = g_list_remove_link (l, l);
        g_list_free (l);
        l = n;
    }

    ents = string_history_add (l, text, max);
    pos = ents;
}


const gchar *History::first()
{
    g_return_val_if_fail (pos != NULL, NULL);

    if (pos->next)
        pos = g_list_last(pos);

    return (const gchar *) pos->data;
}


const gchar *History::back()
{
    g_return_val_if_fail (pos != NULL, NULL);

    if (pos->next)
        pos = pos->next;

    return (const gchar *) pos->data;
}


const gchar *History::forward()
{
    g_return_val_if_fail (pos != NULL, NULL);

    if (pos->prev)
        pos = pos->prev;

    return (const gchar *) pos->data;
}


const gchar *History::last()
{
    g_return_val_if_fail (pos != NULL, NULL);

    if (pos->prev)
        pos = g_list_first(pos);

    return (const gchar *) pos->data;
}
