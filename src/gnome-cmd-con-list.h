/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2003 Marcus Bjurman

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
#ifndef __GNOME_CMD_CON_LIST_H__
#define __GNOME_CMD_CON_LIST_H__

#include "gnome-cmd-con.h"
#include "gnome-cmd-con-ftp.h"
#include "gnome-cmd-con-device.h"


#define GNOME_CMD_CON_LIST(obj) \
	GTK_CHECK_CAST (obj, gnome_cmd_con_list_get_type (), GnomeCmdConList)
#define GNOME_CMD_CON_LIST_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, gnome_cmd_con_list_get_type (), GnomeCmdConListClass)
#define GNOME_CMD_IS_CON_LIST(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_con_list_get_type ())


typedef struct _GnomeCmdConList GnomeCmdConList;
typedef struct _GnomeCmdConListClass GnomeCmdConListClass;
typedef struct _GnomeCmdConListPrivate GnomeCmdConListPrivate;


struct _GnomeCmdConList
{
	GtkObject parent;

	GnomeCmdConListPrivate *priv;
};

struct _GnomeCmdConListClass
{
	GtkObjectClass parent_class;

	/* signals */
	void (* list_changed) (GnomeCmdConList *list);
	void (* ftp_list_changed) (GnomeCmdConList *list);
	void (* device_list_changed) (GnomeCmdConList *list);
};


GtkType
gnome_cmd_con_list_get_type (void);

GnomeCmdConList *
gnome_cmd_con_list_new (void);


void
gnome_cmd_con_list_begin_update (GnomeCmdConList *list);

void
gnome_cmd_con_list_end_update (GnomeCmdConList *list);

void
gnome_cmd_con_list_add_ftp (GnomeCmdConList *list, GnomeCmdConFtp *ftp_con);

void
gnome_cmd_con_list_remove_ftp (GnomeCmdConList *list, GnomeCmdConFtp *ftp_con);

void
gnome_cmd_con_list_add_device (GnomeCmdConList *list, GnomeCmdConDevice *device_con);

void
gnome_cmd_con_list_remove_device (GnomeCmdConList *list, GnomeCmdConDevice *device_con);

GList *
gnome_cmd_con_list_get_all (GnomeCmdConList *list);

GList *
gnome_cmd_con_list_get_all_ftp (GnomeCmdConList *list);

void
gnome_cmd_con_list_set_all_ftp (GnomeCmdConList *list, GList *ftp_cons);

GList *
gnome_cmd_con_list_get_all_dev (GnomeCmdConList *list);

void
gnome_cmd_con_list_set_all_dev (GnomeCmdConList *list, GList *dev_cons);

GnomeCmdCon *
gnome_cmd_con_list_get_home (GnomeCmdConList *list);

GnomeCmdCon *
gnome_cmd_con_list_get_smb (GnomeCmdConList *con_list);

#endif //__GNOME_CMD_CON_LIST_H__
