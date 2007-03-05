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
#ifndef __GNOME_CMD_CON_SMB_H__
#define __GNOME_CMD_CON_SMB_H__

#include "gnome-cmd-con.h"

G_BEGIN_DECLS

#define GNOME_CMD_CON_SMB(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_con_smb_get_type (), GnomeCmdConSmb)
#define GNOME_CMD_CON_SMB_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_con_smb_get_type (), GnomeCmdConSmbClass)
#define GNOME_CMD_IS_CON_SMB(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_con_smb_get_type ())


typedef struct _GnomeCmdConSmb GnomeCmdConSmb;
typedef struct _GnomeCmdConSmbClass GnomeCmdConSmbClass;
typedef struct _GnomeCmdConSmbPrivate GnomeCmdConSmbPrivate;


struct _GnomeCmdConSmb
{
    GnomeCmdCon parent;

    GnomeCmdConSmbPrivate *priv;
};

struct _GnomeCmdConSmbClass
{
    GnomeCmdConClass parent_class;
};


GtkType
gnome_cmd_con_smb_get_type (void);

GnomeCmdCon *
gnome_cmd_con_smb_new (void);

G_END_DECLS

#endif // __GNOME_CMD_CON_SMB_H__
