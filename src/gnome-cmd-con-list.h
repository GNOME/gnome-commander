/**
 * @file gnome-cmd-con-list.h
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
#include "gnome-cmd-con-remote.h"
#include "gnome-cmd-con-device.h"


struct GnomeCmdConList;


extern "C" GnomeCmdConList *gnome_cmd_con_list_new (gboolean show_samba_workgroups_button);

extern "C" GnomeCmdConList *gnome_cmd_con_list_get ();

extern "C" GListModel *gnome_cmd_con_list_get_all (GnomeCmdConList *list);

extern "C" GnomeCmdCon *gnome_cmd_con_list_get_home (GnomeCmdConList *con_list);
extern "C" GnomeCmdCon *gnome_cmd_con_list_get_smb (GnomeCmdConList *con_list);

inline GnomeCmdCon *get_home_con ()
{
    return gnome_cmd_con_list_get_home (gnome_cmd_con_list_get());
}

inline GnomeCmdCon *get_smb_con ()
{
    return gnome_cmd_con_list_get_smb (gnome_cmd_con_list_get());
}

extern "C" GnomeCmdCon *get_remote_con_for_gfile (GnomeCmdConList *list, GFile *gFile);

extern "C" void gnome_cmd_con_list_load_bookmarks (GnomeCmdConList *list, GVariant *gVariantBookmarks);
extern "C" GVariant *gnome_cmd_con_list_save_bookmarks (GnomeCmdConList *list);

extern "C" void gnome_cmd_con_list_lock (GnomeCmdConList *list);
extern "C" void gnome_cmd_con_list_unlock (GnomeCmdConList *list);

extern "C" void gnome_cmd_con_list_set_volume_monitor (GnomeCmdConList *list);

extern "C" void gnome_cmd_con_list_load_devices (GnomeCmdConList *list, GVariant *variant);
extern "C" GVariant *gnome_cmd_con_list_save_devices (GnomeCmdConList *list);

extern "C" void gnome_cmd_con_list_load_connections (GnomeCmdConList *list, GVariant *variant);
extern "C" GVariant *gnome_cmd_con_list_save_connections (GnomeCmdConList *list);
