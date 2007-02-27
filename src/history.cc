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
#include "gnome-cmd-includes.h"
#include "history.h"
#include "utils.h"

using namespace std;


History *history_new (gint max)
{
    History *history = g_new (History, 1);

    history->ents = NULL;
    history->pos = NULL;
    history->max = max;
    history->lock = FALSE;

    return history;
}


void history_free (History *history)
{
    g_return_if_fail (history != NULL);

    if (history->ents)
    {
        g_list_foreach (history->ents, (GFunc)g_free, NULL);
        g_list_free (history->ents);
    }

    g_free (history);
}


void history_add (History *history, const gchar *text)
{
    GList *l, *n;

    g_return_if_fail (history != NULL);

    if (history->lock) return;

    /* If we are in the middle of the history list, lets kill
       all items that are in front of us */
    l = history->ents;
    while (l && l != history->pos)
    {
        g_free (l->data);
        n = g_list_remove_link (l, l);
        g_list_free (l);
        l = n;
    }

    history->ents = string_history_add (l, text, history->max);
    history->pos = history->ents;
}


gboolean history_can_back (History *history)
{
    g_return_val_if_fail (history != NULL, FALSE);
    g_return_val_if_fail (history->pos != NULL, FALSE);

    return history->pos->next != NULL;
}


gboolean history_can_forward (History *history)
{
    g_return_val_if_fail (history != NULL, FALSE);
    g_return_val_if_fail (history->pos != NULL, FALSE);

    return history->pos->prev != NULL;
}


const gchar *history_first (History *history)
{
    g_return_val_if_fail (history != NULL, NULL);
    g_return_val_if_fail (history->pos != NULL, NULL);

    if (history->pos->next)
        history->pos = g_list_last(history->pos);

    return (const gchar*)history->pos->data;
}


const gchar *history_back (History *history)
{
    g_return_val_if_fail (history != NULL, NULL);
    g_return_val_if_fail (history->pos != NULL, NULL);

    if (history->pos->next)
        history->pos = history->pos->next;

    return (const gchar*)history->pos->data;
}


const gchar *history_forward (History *history)
{
    g_return_val_if_fail (history != NULL, NULL);
    g_return_val_if_fail (history->pos != NULL, NULL);

    if (history->pos->prev)
        history->pos = history->pos->prev;

    return (const gchar*)history->pos->data;
}


const gchar *history_last (History *history)
{
    g_return_val_if_fail (history != NULL, NULL);
    g_return_val_if_fail (history->pos != NULL, NULL);

    if (history->pos->prev)
        history->pos = g_list_first(history->pos);

    return (const gchar*)history->pos->data;
}


