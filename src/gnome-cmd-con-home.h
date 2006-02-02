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
#ifndef __GNOME_CMD_CON_HOME_H__
#define __GNOME_CMD_CON_HOME_H__

#include "gnome-cmd-con.h"

#define GNOME_CMD_CON_HOME(obj) \
	GTK_CHECK_CAST (obj, gnome_cmd_con_home_get_type (), GnomeCmdConHome)
#define GNOME_CMD_CON_HOME_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, gnome_cmd_con_home_get_type (), GnomeCmdConHomeClass)
#define GNOME_CMD_IS_CON_HOME(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_con_home_get_type ())


typedef struct _GnomeCmdConHome GnomeCmdConHome;
typedef struct _GnomeCmdConHomeClass GnomeCmdConHomeClass;
typedef struct _GnomeCmdConHomePrivate GnomeCmdConHomePrivate;


struct _GnomeCmdConHome
{
	GnomeCmdCon parent;

	GnomeCmdConHomePrivate *priv;
};

struct _GnomeCmdConHomeClass
{
	GnomeCmdConClass parent_class;
};


GtkType
gnome_cmd_con_home_get_type (void);

GnomeCmdCon *
gnome_cmd_con_home_new (void);


#endif //__GNOME_CMD_CON_HOME_H__
