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
#ifndef __GNOME_CMD_CLIST_H__
#define __GNOME_CMD_CLIST_H__

#define GNOME_CMD_CLIST(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_clist_get_type (), GnomeCmdCList)
#define GNOME_CMD_CLIST_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_clist_get_type (), GnomeCmdCListClass)
#define GNOME_CMD_IS_CLIST(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_clist_get_type ())


struct GnomeCmdCListPrivate;


struct GnomeCmdCList
{
    GtkCList parent;

    gint drag_motion_row;

    GnomeCmdCListPrivate *priv;
};


struct GnomeCmdCListClass
{
    GtkCListClass parent_class;
};



GtkType gnome_cmd_clist_get_type ();

GtkWidget *gnome_cmd_clist_new_with_titles (gint columns, gchar **titles);

inline GtkWidget *gnome_cmd_clist_new (gint columns)
{
    return gnome_cmd_clist_new_with_titles (columns, NULL);
}

void gnome_cmd_clist_update_style        (GnomeCmdCList *clist);

gint gnome_cmd_clist_get_voffset (GnomeCmdCList *clist);
void gnome_cmd_clist_set_voffset (GnomeCmdCList *clist, gint voffset);

gint gnome_cmd_clist_get_row (GnomeCmdCList *clist, gint x, gint y);
void gnome_cmd_clist_set_drag_row (GnomeCmdCList *clist, gint row);

#endif // __GNOME_CMD_CLIST_H__
