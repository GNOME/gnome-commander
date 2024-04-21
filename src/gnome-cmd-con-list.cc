/**
 * @file gnome-cmd-con-list.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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
#ifdef HAVE_SAMBA
#include "gnome-cmd-con-smb.h"
#endif

using namespace std;


struct GnomeCmdConList::Private
{
    gboolean update_lock;
    gboolean changed;
    gboolean remote_cons_changed;
    gboolean device_cons_changed;
    gboolean quick_ftp_cons_changed;

    GList *remote_cons    {nullptr};
    GList *device_cons    {nullptr};
    GList *quick_ftp_cons {nullptr};

    GnomeCmdCon *home_con {nullptr};
#ifdef HAVE_SAMBA
    GnomeCmdCon *smb_con  {nullptr};
#endif
    GList *all_cons;
};

enum
{
    LIST_CHANGED,
    REMOTE_LIST_CHANGED,
    DEVICE_LIST_CHANGED,
    QUICK_FTP_LIST_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GnomeCmdConList, gnome_cmd_con_list, G_TYPE_OBJECT)


static void on_con_updated (GnomeCmdCon *con, GnomeCmdConList *con_list)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));

    if (GNOME_CMD_IS_CON_REMOTE (con))
        g_signal_emit (con_list, signals[REMOTE_LIST_CHANGED], 0);
    else
        if (GNOME_CMD_IS_CON_DEVICE (con))
            g_signal_emit (con_list, signals[DEVICE_LIST_CHANGED], 0);

    g_signal_emit (con_list, signals[LIST_CHANGED], 0);
}


static gint compare_alias (const GnomeCmdCon *c1, const GnomeCmdCon *c2)
{
    return g_utf8_collate (c1->alias, c2->alias);
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

    signals[REMOTE_LIST_CHANGED]    = g_signal_new ("remote-list-changed",
                                                    G_TYPE_FROM_CLASS (klass),
                                                    G_SIGNAL_RUN_LAST,
                                                    G_STRUCT_OFFSET (GnomeCmdConListClass, ftp_list_changed),
                                                    nullptr, nullptr,
                                                    g_cclosure_marshal_VOID__VOID,
                                                    G_TYPE_NONE,
                                                    0);

    signals[DEVICE_LIST_CHANGED]    = g_signal_new ("device-list-changed",
                                                    G_TYPE_FROM_CLASS (klass),
                                                    G_SIGNAL_RUN_LAST,
                                                    G_STRUCT_OFFSET (GnomeCmdConListClass, device_list_changed),
                                                    nullptr, nullptr,
                                                    g_cclosure_marshal_VOID__VOID,
                                                    G_TYPE_NONE,
                                                    0);

    signals[QUICK_FTP_LIST_CHANGED] = g_signal_new ("quick-ftp-list-changed",
                                                    G_TYPE_FROM_CLASS (klass),
                                                    G_SIGNAL_RUN_LAST,
                                                    G_STRUCT_OFFSET (GnomeCmdConListClass, quick_ftp_list_changed),
                                                    nullptr, nullptr,
                                                    g_cclosure_marshal_VOID__VOID,
                                                    G_TYPE_NONE,
                                                    0);

    G_OBJECT_CLASS (klass)->dispose = dispose;
}


static void gnome_cmd_con_list_init (GnomeCmdConList *con_list)
{
    con_list->priv = g_new0 (GnomeCmdConList::Private, 1);

    con_list->priv->update_lock = FALSE;

    con_list->priv->home_con = gnome_cmd_con_home_new ();

#ifdef HAVE_SAMBA
    if (gnome_cmd_data.options.show_samba_workgroups_button)
        con_list->priv->smb_con = gnome_cmd_con_smb_new ();
#endif

    con_list->priv->all_cons = g_list_append (nullptr, con_list->priv->home_con);

#ifdef HAVE_SAMBA
    if (gnome_cmd_data.options.show_samba_workgroups_button)
        con_list->priv->all_cons = g_list_append (con_list->priv->all_cons, con_list->priv->smb_con);
#endif
}

/***********************************
 * Public functions
 ***********************************/

void GnomeCmdConList::lock()
{
    priv->update_lock = TRUE;
    priv->changed = FALSE;
    priv->remote_cons_changed = FALSE;
    priv->device_cons_changed = FALSE;
}


void GnomeCmdConList::unlock()
{
    if (priv->changed)
        g_signal_emit (this, signals[LIST_CHANGED], 0);
    if (priv->remote_cons_changed)
        g_signal_emit (this, signals[REMOTE_LIST_CHANGED], 0);
    if (priv->device_cons_changed)
        g_signal_emit (this, signals[DEVICE_LIST_CHANGED], 0);

    priv->update_lock = FALSE;
}


void GnomeCmdConList::add(GnomeCmdConRemote *con)
{
    g_return_if_fail (g_list_index (priv->all_cons, con) == -1);
    g_return_if_fail (g_list_index (priv->remote_cons, con) == -1);

    priv->all_cons = g_list_append (priv->all_cons, con);
    priv->remote_cons = g_list_append (priv->remote_cons, con);

    g_signal_connect (con, "updated", G_CALLBACK (on_con_updated), this);

    if (priv->update_lock)
    {
        priv->changed = TRUE;
        priv->remote_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (this, signals[LIST_CHANGED], 0);
        g_signal_emit (this, signals[REMOTE_LIST_CHANGED], 0);
    }
}


void GnomeCmdConList::remove(GnomeCmdConRemote *con)
{
    g_return_if_fail (g_list_index (priv->all_cons, con) != -1);
    g_return_if_fail (g_list_index (priv->remote_cons, con) != -1);

    priv->all_cons = g_list_remove (priv->all_cons, con);
    priv->remote_cons = g_list_remove (priv->remote_cons, con);

    g_signal_handlers_disconnect_by_func (con, (gpointer) on_con_updated, this);

    if (priv->update_lock)
    {
        priv->changed = TRUE;
        priv->remote_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (this, signals[LIST_CHANGED], 0);
        g_signal_emit (this, signals[REMOTE_LIST_CHANGED], 0);
    }
}


void gnome_cmd_con_list_add_quick_ftp (GnomeCmdConList *con_list, GnomeCmdConRemote *remote_con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));
    g_return_if_fail (g_list_index (con_list->priv->all_cons, remote_con) == -1);
    g_return_if_fail (g_list_index (con_list->priv->quick_ftp_cons, remote_con) == -1);

    con_list->priv->all_cons = g_list_append (con_list->priv->all_cons, remote_con);
    con_list->priv->quick_ftp_cons = g_list_append (con_list->priv->quick_ftp_cons, remote_con);

    g_signal_connect (remote_con, "updated", G_CALLBACK (on_con_updated), con_list);

    if (con_list->priv->update_lock)
    {
        con_list->priv->changed = TRUE;
        con_list->priv->quick_ftp_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (con_list, signals[LIST_CHANGED], 0);
        g_signal_emit (con_list, signals[QUICK_FTP_LIST_CHANGED], 0);
    }
}


void GnomeCmdConList::add(GnomeCmdConDevice *con)
{
    g_return_if_fail (g_list_index (priv->all_cons, con) == -1);
    g_return_if_fail (g_list_index (priv->device_cons, con) == -1);

    priv->all_cons = g_list_append (priv->all_cons, con);
    priv->device_cons = g_list_append (priv->device_cons, con);
    g_signal_connect (con, "updated", G_CALLBACK (on_con_updated), this);

    if (priv->update_lock)
    {
        priv->changed = TRUE;
        priv->device_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (this, signals[LIST_CHANGED], 0);
        g_signal_emit (this, signals[DEVICE_LIST_CHANGED], 0);
    }
}


void GnomeCmdConList::remove(GnomeCmdConDevice *con)
{
    g_return_if_fail (g_list_index (priv->all_cons, con) != -1);
    g_return_if_fail (g_list_index (priv->device_cons, con) != -1);

    priv->all_cons = g_list_remove (priv->all_cons, con);
    priv->device_cons = g_list_remove (priv->device_cons, con);
    g_signal_handlers_disconnect_by_func (con, (gpointer) on_con_updated, this);

    if (priv->update_lock)
    {
        priv->changed = TRUE;
        priv->device_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (this, signals[LIST_CHANGED], 0);
        g_signal_emit (this, signals[DEVICE_LIST_CHANGED], 0);
    }
}


GList *gnome_cmd_con_list_get_all (GnomeCmdConList *con_list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), nullptr);

    return con_list->priv->all_cons;
}


GList *gnome_cmd_con_list_get_all_remote (GnomeCmdConList *con_list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), nullptr);

    return con_list->priv->remote_cons;
}


GList *gnome_cmd_con_list_get_all_dev (GnomeCmdConList *con_list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), nullptr);

    return con_list->priv->device_cons;
}


void gnome_cmd_con_list_set_all_dev (GnomeCmdConList *con_list, GList *dev_cons)
{
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));

    con_list->priv->device_cons = dev_cons;
}


GnomeCmdCon *GnomeCmdConList::find_alias(const gchar *alias) const
{
    g_return_val_if_fail (alias != nullptr, nullptr);

    GnomeCmdCon con;                // used as reference element to be looked for, no allocation necessary
    con.alias = (gchar *) alias;

    auto elem = g_list_find_custom (priv->all_cons, &con, (GCompareFunc) compare_alias);

    return elem ? static_cast<GnomeCmdCon*> (elem->data) : nullptr;
}


GnomeCmdCon *GnomeCmdConList::get_home()
{
    return priv->home_con;
}

#ifdef HAVE_SAMBA
GnomeCmdCon *GnomeCmdConList::get_smb()
{
    return priv->smb_con;
}
#endif


GnomeCmdCon *get_remote_con_for_gfile(GFile *gFile)
{
    GnomeCmdCon *gnomeCmdCon = nullptr;
    auto remoteCons = get_remote_cons();

    for(auto remoteConEntry = remoteCons; remoteConEntry; remoteConEntry = remoteConEntry->next)
    {
        auto remoteCon = static_cast<GnomeCmdConRemote*>(remoteConEntry->data);
        auto gnomeCmdConParent = &remoteCon->parent;
        auto gFileSrcUri = g_file_get_uri(gFile);
        if (strstr(gFileSrcUri, gnomeCmdConParent->uri))
        {
            gnomeCmdCon = gnomeCmdConParent;
            g_free(gFileSrcUri);
            break;
        }
        g_free(gFileSrcUri);
    }
    return gnomeCmdCon;
}
