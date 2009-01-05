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

#ifndef __PARSER_H__
#define __PARSER_H__

#include <gnome.h>

typedef struct {
    gchar *number;
    gchar *date;
    gchar *author;
    gchar *state;
    gchar *lines;
    gchar *message;
} Revision;

#include "cvs-plugin.h"

G_BEGIN_DECLS

typedef struct {
    gchar *fname;
    GList *revisions;
    GList *rev_names;
    GHashTable *rev_map;
    CvsPlugin *plugin;

    GtkWidget *rev_label;
    GtkWidget *date_label;
    GtkWidget *author_label;
    GtkWidget *state_label;
    GtkWidget *lines_label;
    GtkWidget *msg_text_view;
} LogHistory;


LogHistory *log_create (CvsPlugin *plugin, const gchar *fpath);
void log_free (LogHistory *log);

G_END_DECLS

#endif //__PARSER_H__
