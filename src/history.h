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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __HISTORY_H__
#define __HISTORY_H__

typedef struct
{
    GList *ents;
    GList *pos;
    gint max;
    gboolean lock;
} History;


History *history_new (gint max);
void     history_free (History *history);

void     history_add (History *history, const gchar *text);

gboolean history_can_back (History *history);
gboolean history_can_forward (History *history);

const gchar   *history_first (History *history);
const gchar   *history_back (History *history);
const gchar   *history_forward (History *history);
const gchar   *history_last (History *history);

#endif //__HISTORY_H__
