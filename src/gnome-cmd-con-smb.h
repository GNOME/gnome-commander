/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

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

#define GNOME_CMD_TYPE_CON_SMB              (gnome_cmd_con_smb_get_type ())
#define GNOME_CMD_CON_SMB(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CON_SMB, GnomeCmdConSmb))
#define GNOME_CMD_CON_SMB_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CON_SMB, GnomeCmdConSmbClass))
#define GNOME_CMD_IS_CON_SMB(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CON_SMB))
#define GNOME_CMD_IS_CON_SMB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CON_SMB))
#define GNOME_CMD_CON_SMB_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CON_SMB, GnomeCmdConSmbClass))


struct GnomeCmdConSmb
{
    GnomeCmdCon parent;
};

struct GnomeCmdConSmbClass
{
    GnomeCmdConClass parent_class;
};


GtkType gnome_cmd_con_smb_get_type ();

inline GnomeCmdCon *gnome_cmd_con_smb_new ()
{
    return GNOME_CMD_CON (g_object_new (GNOME_CMD_TYPE_CON_SMB, NULL));
}

#endif // __GNOME_CMD_CON_SMB_H__
