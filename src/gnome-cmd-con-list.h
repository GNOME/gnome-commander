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

#include "gnome-cmd-data.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-con-remote.h"
#include "gnome-cmd-con-device.h"

#define GNOME_CMD_TYPE_CON_LIST              (gnome_cmd_con_list_get_type ())
#define GNOME_CMD_CON_LIST(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CON_LIST, GnomeCmdConList))
#define GNOME_CMD_CON_LIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CON_LIST, GnomeCmdConListClass))
#define GNOME_CMD_IS_CON_LIST(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CON_LIST))
#define GNOME_CMD_IS_CON_LIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CON_LIST))
#define GNOME_CMD_CON_LIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CON_LIST, GnomeCmdConListClass))


extern "C" GType gnome_cmd_con_list_get_type ();


struct GnomeCmdConList
{
    GObject parent;

    struct Private;

    Private *priv;
};

struct GnomeCmdConListClass
{
    GObjectClass parent_class;

    /* signals */
    void (* list_changed) (GnomeCmdConList *list);
};


inline GnomeCmdConList *gnome_cmd_con_list_new ()
{
    return static_cast<GnomeCmdConList *>(g_object_new (GNOME_CMD_TYPE_CON_LIST, nullptr));
}

extern "C" GnomeCmdConList *gnome_cmd_con_list_get ();

extern "C" GListModel *gnome_cmd_con_list_get_all (GnomeCmdConList *list);
extern "C" GList *gnome_cmd_con_list_get_all_remote (GnomeCmdConList *list);

extern "C" GnomeCmdCon *gnome_cmd_con_list_find_by_uuid (GnomeCmdConList *con_list, const gchar *uuid);
extern "C" GnomeCmdCon *gnome_cmd_con_list_find_by_alias (GnomeCmdConList *con_list, const gchar *alias);
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

GnomeCmdCon *get_remote_con_for_gfile(GFile *gFile);

extern "C" void gnome_cmd_con_list_load_bookmarks (GnomeCmdConList *list, GVariant *gVariantBookmarks);
extern "C" GVariant *gnome_cmd_con_list_save_bookmarks (GnomeCmdConList *list);

// FFI
extern "C" void gnome_cmd_con_list_add_remote (GnomeCmdConList *list, GnomeCmdConRemote *con);
extern "C" void gnome_cmd_con_list_add_dev (GnomeCmdConList *list, GnomeCmdConDevice *con);
extern "C" void gnome_cmd_con_list_remove_remote (GnomeCmdConList *list, GnomeCmdConRemote *con);
extern "C" void gnome_cmd_con_list_remove_dev (GnomeCmdConList *list, GnomeCmdConDevice *con);

extern "C" void gnome_cmd_con_list_lock (GnomeCmdConList *list);
extern "C" void gnome_cmd_con_list_unlock (GnomeCmdConList *list);
