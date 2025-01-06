/**
 * @file gnome-cmd-con-list.cc
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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con-home.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-con-smb.h"

using namespace std;


struct GnomeCmdConList::Private
{
    gboolean update_lock;
    gboolean changed;

    GList *remote_cons    {nullptr};

    GnomeCmdCon *home_con {nullptr};
    GnomeCmdCon *smb_con  {nullptr};
    GListStore *all_cons;
};

enum
{
    LIST_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GnomeCmdConList, gnome_cmd_con_list, G_TYPE_OBJECT)


static void on_con_updated (GnomeCmdCon *con, GnomeCmdConList *con_list)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));

    g_signal_emit (con_list, signals[LIST_CHANGED], 0);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    GnomeCmdConList *con_list = GNOME_CMD_CON_LIST (object);

    g_clear_pointer (&con_list->priv, g_free);

    G_OBJECT_CLASS (gnome_cmd_con_list_parent_class)->dispose (object);
}


static void gnome_cmd_con_list_class_init (GnomeCmdConListClass *klass)
{
    signals[LIST_CHANGED]           = g_signal_new ("list-changed",
                                                    G_TYPE_FROM_CLASS (klass),
                                                    G_SIGNAL_RUN_LAST,
                                                    G_STRUCT_OFFSET (GnomeCmdConListClass, list_changed),
                                                    nullptr, nullptr,
                                                    g_cclosure_marshal_VOID__VOID,
                                                    G_TYPE_NONE,
                                                    0);

    G_OBJECT_CLASS (klass)->dispose = dispose;
}


static void items_changed (GListModel* self, guint position, guint removed, guint added, gpointer user_data)
{
    auto list = GNOME_CMD_CON_LIST (user_data);
    g_signal_emit (list, signals[LIST_CHANGED], 0);
}


static void gnome_cmd_con_list_init (GnomeCmdConList *con_list)
{
    con_list->priv = g_new0 (GnomeCmdConList::Private, 1);

    con_list->priv->update_lock = FALSE;

    con_list->priv->home_con = gnome_cmd_con_home_new ();

    if (gnome_cmd_data.options.show_samba_workgroups_button)
        con_list->priv->smb_con = gnome_cmd_con_smb_new ();

    con_list->priv->all_cons = g_list_store_new (GNOME_CMD_TYPE_CON);
    g_list_store_append (con_list->priv->all_cons, con_list->priv->home_con);

    if (gnome_cmd_data.options.show_samba_workgroups_button)
        g_list_store_append (con_list->priv->all_cons, con_list->priv->smb_con);

    g_signal_connect (con_list->priv->all_cons, "items-changed", G_CALLBACK (items_changed), con_list);
}

/***********************************
 * Public functions
 ***********************************/

void gnome_cmd_con_list_lock (GnomeCmdConList *list)
{
    list->priv->update_lock = TRUE;
    list->priv->changed = FALSE;
}


void gnome_cmd_con_list_unlock (GnomeCmdConList *list)
{
    if (list->priv->changed)
        g_signal_emit (list, signals[LIST_CHANGED], 0);

    list->priv->update_lock = FALSE;
}


void gnome_cmd_con_list_add_remote (GnomeCmdConList *list, GnomeCmdConRemote *con)
{
    g_return_if_fail (!g_list_store_find (list->priv->all_cons, con, nullptr));
    g_return_if_fail (g_list_index (list->priv->remote_cons, con) == -1);

    g_list_store_append (list->priv->all_cons, con);
    list->priv->remote_cons = g_list_append (list->priv->remote_cons, con);

    g_signal_connect (con, "updated", G_CALLBACK (on_con_updated), list);

    if (list->priv->update_lock)
    {
        list->priv->changed = TRUE;
    }
    else
    {
        g_signal_emit (list, signals[LIST_CHANGED], 0);
    }
}


void gnome_cmd_con_list_remove_remote (GnomeCmdConList *list, GnomeCmdConRemote *con)
{
    guint position;
    g_return_if_fail (g_list_store_find (list->priv->all_cons, con, &position));
    g_return_if_fail (g_list_index (list->priv->remote_cons, con) != -1);

    g_list_store_remove (list->priv->all_cons, position);
    list->priv->remote_cons = g_list_remove (list->priv->remote_cons, con);

    g_signal_handlers_disconnect_by_func (con, (gpointer) on_con_updated, list);

    if (list->priv->update_lock)
    {
        list->priv->changed = TRUE;
    }
    else
    {
        g_signal_emit (list, signals[LIST_CHANGED], 0);
    }
}


void gnome_cmd_con_list_add_dev (GnomeCmdConList *list, GnomeCmdConDevice *con)
{
    g_return_if_fail (!g_list_store_find (list->priv->all_cons, con, nullptr));

    g_list_store_append (list->priv->all_cons, con);
    g_signal_connect (con, "updated", G_CALLBACK (on_con_updated), list);

    if (list->priv->update_lock)
    {
        list->priv->changed = TRUE;
    }
    else
    {
        g_signal_emit (list, signals[LIST_CHANGED], 0);
    }
}


void gnome_cmd_con_list_remove_dev (GnomeCmdConList *list, GnomeCmdConDevice *con)
{
    guint position;
    g_return_if_fail (g_list_store_find (list->priv->all_cons, con, &position));

    g_list_store_remove (list->priv->all_cons, position);
    g_signal_handlers_disconnect_by_func (con, (gpointer) on_con_updated, list);

    if (list->priv->update_lock)
    {
        list->priv->changed = TRUE;
    }
    else
    {
        g_signal_emit (list, signals[LIST_CHANGED], 0);
    }
}


GListModel *gnome_cmd_con_list_get_all (GnomeCmdConList *con_list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), nullptr);

    return G_LIST_MODEL (con_list->priv->all_cons);
}


GList *gnome_cmd_con_list_get_all_remote (GnomeCmdConList *con_list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), nullptr);

    return con_list->priv->remote_cons;
}


GnomeCmdCon *gnome_cmd_con_list_find_by_uuid (GnomeCmdConList *con_list, const gchar *uuid)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), nullptr);
    g_return_val_if_fail (uuid != nullptr, nullptr);

    guint n = g_list_model_get_n_items (G_LIST_MODEL (con_list->priv->all_cons));
    for (guint i = 0; i < n; ++i)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (g_list_model_get_item (G_LIST_MODEL (con_list->priv->all_cons), i));
        if (!strcmp (gnome_cmd_con_get_uuid (con), uuid))
            return con;
    }
    return nullptr;
}


GnomeCmdCon *gnome_cmd_con_list_find_by_alias (GnomeCmdConList *list, const gchar *alias)
{
    g_return_val_if_fail (alias != nullptr, nullptr);

    guint n = g_list_model_get_n_items (G_LIST_MODEL (list->priv->all_cons));
    for (guint i = 0; i < n; ++i)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (g_list_model_get_item (G_LIST_MODEL (list->priv->all_cons), i));
        if (g_utf8_collate (gnome_cmd_con_get_alias (con), alias) == 0)
            return con;
    }
    return nullptr;
}


GnomeCmdCon *gnome_cmd_con_list_get_home (GnomeCmdConList *list)
{
    return list->priv->home_con;
}


GnomeCmdCon *gnome_cmd_con_list_get_smb (GnomeCmdConList *list)
{
    return list->priv->smb_con;
}


GnomeCmdCon *get_remote_con_for_gfile(GFile *gFile)
{
    GnomeCmdCon *gnomeCmdCon = nullptr;
    auto remoteCons = gnome_cmd_con_list_get_all_remote (gnome_cmd_con_list_get ());

    for(auto remoteConEntry = remoteCons; remoteConEntry; remoteConEntry = remoteConEntry->next)
    {
        auto remoteCon = static_cast<GnomeCmdConRemote*>(remoteConEntry->data);
        auto gnomeCmdConParent = &remoteCon->parent;
        auto gFileSrcUri = g_file_get_uri(gFile);
        gchar *uri = gnome_cmd_con_get_uri_string (gnomeCmdConParent);
        if (strstr(gFileSrcUri, uri))
        {
            gnomeCmdCon = gnomeCmdConParent;
            g_free(gFileSrcUri);
            g_free(uri);
            break;
        }
        g_free(gFileSrcUri);
        g_free(uri);
    }
    return gnomeCmdCon;
}

GnomeCmdConList *gnome_cmd_con_list_get ()
{
    return static_cast<GnomeCmdConList *>(gnome_cmd_data_get_con_list ());
}
