/** 
 * @file gnome-cmd-quicksearch-popup.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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
#ifndef __GNOME_CMD_QUICKSEARCH_POPUP_TYPES_H__
#define __GNOME_CMD_QUICKSEARCH_POPUP_TYPES_H__

#include "gnome-cmd-file-list.h"

#define GNOME_CMD_TYPE_QUICKSEARCH_POPUP              (gnome_cmd_quicksearch_popup_get_type ())
#define GNOME_CMD_QUICKSEARCH_POPUP(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_QUICKSEARCH_POPUP, GnomeCmdQuicksearchPopup))
#define GNOME_CMD_QUICKSEARCH_POPUP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_QUICKSEARCH_POPUP, GnomeCmdQuicksearchPopupClass))
#define GNOME_CMD_IS_QUICKSEARCH_POPUP(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_QUICKSEARCH_POPUP))
#define GNOME_CMD_IS_QUICKSEARCH_POPUP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_QUICKSEARCH_POPUP))
#define GNOME_CMD_QUICKSEARCH_POPUP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_QUICKSEARCH_POPUP, GnomeCmdQuicksearchPopupClass))


struct GnomeCmdQuicksearchPopupPrivate;


struct GnomeCmdQuicksearchPopup
{
    GtkWindow parent;

    GtkWidget *frame;
    GtkWidget *win;
    GtkWidget *box;
    GtkWidget *lbl;
    GtkWidget *entry;

    GnomeCmdQuicksearchPopupPrivate *priv;
};


struct GnomeCmdQuicksearchPopupClass
{
    GtkWindowClass parent_class;
};


GtkType gnome_cmd_quicksearch_popup_get_type ();

GtkWidget *gnome_cmd_quicksearch_popup_new (GnomeCmdFileList *fl);

#endif // __GNOME_CMD_QUICKSEARCH_POPUP_TYPES_H__
