/** 
 * @file gnome-cmd-con-smb.h
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

#include "gnome-cmd-con.h"

#define GNOME_CMD_TYPE_CON_SMB              (gnome_cmd_con_smb_get_type ())
#define GNOME_CMD_CON_SMB(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CON_SMB, GnomeCmdConSmb))
#define GNOME_CMD_CON_SMB_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CON_SMB, GnomeCmdConSmbClass))
#define GNOME_CMD_IS_CON_SMB(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CON_SMB))
#define GNOME_CMD_IS_CON_SMB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CON_SMB))
#define GNOME_CMD_CON_SMB_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CON_SMB, GnomeCmdConSmbClass))

/**
 * @brief Class for connecting to samba and show available workgroups
 *
 * This class is _not_ meant to be used when connecting to a single samba remote, e.g. to smb://server/share.
 * Instead, it is used to search workgroups, therefore it will list available workgroubs through the connection
 * to smb:///.
 */
struct GnomeCmdConSmb
{
    GnomeCmdCon parent;
};

extern "C" GType gnome_cmd_con_smb_get_type ();

inline GnomeCmdCon *gnome_cmd_con_smb_new ()
{
    return GNOME_CMD_CON (g_object_new (GNOME_CMD_TYPE_CON_SMB, NULL));
}
