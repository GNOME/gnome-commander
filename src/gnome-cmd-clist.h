/** 
 * @file gnome-cmd-clist.h
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

#define GNOME_CMD_TYPE_CLIST              (gnome_cmd_clist_get_type ())
#define GNOME_CMD_CLIST(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CLIST, GnomeCmdCList))
#define GNOME_CMD_CLIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CLIST, GnomeCmdCListClass))
#define GNOME_CMD_IS_CLIST(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CLIST))
#define GNOME_CMD_IS_CLIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CLIST))
#define GNOME_CMD_CLIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CLIST, GnomeCmdCListClass))


struct GnomeCmdCList
{
    GtkCList parent;

    gint drag_motion_row;
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

void gnome_cmd_clist_update_style (GnomeCmdCList *clist);

gint gnome_cmd_clist_get_voffset (GnomeCmdCList *clist);
void gnome_cmd_clist_set_voffset (GnomeCmdCList *clist, gint voffset);

gint gnome_cmd_clist_get_row (GnomeCmdCList *clist, gint x, gint y);
void gnome_cmd_clist_set_drag_row (GnomeCmdCList *clist, gint row);
