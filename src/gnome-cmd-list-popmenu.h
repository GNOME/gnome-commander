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
#ifndef __GNOME_CMD_LIST_POPMENU_H__
#define __GNOME_CMD_LIST_POPMENU_H__

#include "gnome-cmd-file-selector.h"

#define GNOME_CMD_LIST_POPMENU(obj) \
	GTK_CHECK_CAST (obj, gnome_cmd_list_popmenu_get_type (), GnomeCmdListPopmenu)
#define GNOME_CMD_LIST_POPMENU_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, gnome_cmd_list_popmenu_get_type (), GnomeCmdListPopmenuClass)
#define GNOME_CMD_IS_LIST_POPMENU(obj) \
	GTK_CHECK_TYPE (obj, gnome_cmd_list_popmenu_get_type ())


typedef struct _GnomeCmdListPopmenu GnomeCmdListPopmenu;
typedef struct _GnomeCmdListPopmenuPrivate GnomeCmdListPopmenuPrivate;
typedef struct _GnomeCmdListPopmenuClass GnomeCmdListPopmenuClass;



struct _GnomeCmdListPopmenu
{
	GtkMenu parent;
	GnomeCmdListPopmenuPrivate *priv;
};


struct _GnomeCmdListPopmenuClass
{
	GtkMenuClass parent_class;
};


GtkWidget*
gnome_cmd_list_popmenu_new (GnomeCmdFileSelector *fs);

GtkType
gnome_cmd_list_popmenu_get_type (void);


#endif //__GNOME_CMD_LIST_POPMENU_H__


