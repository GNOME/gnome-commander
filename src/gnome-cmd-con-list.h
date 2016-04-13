/** 
 * @file gnome-cmd-con-list.h
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

#ifndef __GNOME_CMD_CON_LIST_H__
#define __GNOME_CMD_CON_LIST_H__

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


GtkType gnome_cmd_con_list_get_type ();


struct GnomeCmdConList
{
    GtkObject parent;

    class Private;

    Private *priv;

    operator GObject * () const         {  return G_OBJECT (this);    }
    operator GtkObject * () const       {  return GTK_OBJECT (this);  }

    void lock();
    void unlock();

    void add(GnomeCmdConRemote *con);
    void add(GnomeCmdConDevice *con);

    void remove(GnomeCmdConRemote *con);
    void remove(GnomeCmdConDevice *con);

    GnomeCmdCon *find_alias(const gchar *alias) const;
    gboolean has_alias(const gchar *alias) const            {  return find_alias(alias)!=NULL;  }

    GnomeCmdCon *get_home();
    GnomeCmdCon *get_smb();
};

struct GnomeCmdConListClass
{
    GtkObjectClass parent_class;

    /* signals */
    void (* list_changed) (GnomeCmdConList *list);
    void (* ftp_list_changed) (GnomeCmdConList *list);
    void (* device_list_changed) (GnomeCmdConList *list);
    void (* quick_ftp_list_changed) (GnomeCmdConList *list);
};


inline GnomeCmdConList *gnome_cmd_con_list_new ()
{
    return (GnomeCmdConList *) g_object_new (GNOME_CMD_TYPE_CON_LIST, NULL);
}

inline GnomeCmdConList *gnome_cmd_con_list_get ()
{
    return (GnomeCmdConList *) gnome_cmd_data_get_con_list ();
}

void gnome_cmd_con_list_add_quick_ftp (GnomeCmdConList *list, GnomeCmdConRemote *ftp_con);
void gnome_cmd_con_list_remove_quick_ftp (GnomeCmdConList *list, GnomeCmdConRemote *ftp_con);

GList *gnome_cmd_con_list_get_all (GnomeCmdConList *list);
GList *gnome_cmd_con_list_get_all_remote (GnomeCmdConList *list);
GList *gnome_cmd_con_list_get_all_quick_ftp (GnomeCmdConList *list);

GList *gnome_cmd_con_list_get_all_dev (GnomeCmdConList *list);
void gnome_cmd_con_list_set_all_dev (GnomeCmdConList *list, GList *dev_cons);

inline GnomeCmdCon *get_home_con ()
{
    return gnome_cmd_con_list_get()->get_home();
}

#ifdef HAVE_SAMBA
inline GnomeCmdCon *get_smb_con ()
{
    return gnome_cmd_con_list_get()->get_smb();
}
#endif

inline GList *get_remote_cons ()
{
    return gnome_cmd_con_list_get_all_remote (gnome_cmd_con_list_get ());
}

#endif // __GNOME_CMD_CON_LIST_H__
