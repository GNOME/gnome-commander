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
    gboolean remote_cons_changed;
    gboolean device_cons_changed;

    GList *remote_cons    {nullptr};
    GList *device_cons    {nullptr};

    GnomeCmdCon *home_con {nullptr};
    GnomeCmdCon *smb_con  {nullptr};
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


static gint compare_alias (GnomeCmdCon *con, const gchar *alias)
{
    return g_utf8_collate (gnome_cmd_con_get_alias (con), alias);
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

    if (gnome_cmd_data.options.show_samba_workgroups_button)
        con_list->priv->smb_con = gnome_cmd_con_smb_new ();

    con_list->priv->all_cons = g_list_append (nullptr, con_list->priv->home_con);

    if (gnome_cmd_data.options.show_samba_workgroups_button)
        con_list->priv->all_cons = g_list_append (con_list->priv->all_cons, con_list->priv->smb_con);
}

/***********************************
 * Public functions
 ***********************************/

void gnome_cmd_con_list_lock (GnomeCmdConList *list)
{
    list->priv->update_lock = TRUE;
    list->priv->changed = FALSE;
    list->priv->remote_cons_changed = FALSE;
    list->priv->device_cons_changed = FALSE;
}


void gnome_cmd_con_list_unlock (GnomeCmdConList *list)
{
    if (list->priv->changed)
        g_signal_emit (list, signals[LIST_CHANGED], 0);
    if (list->priv->remote_cons_changed)
        g_signal_emit (list, signals[REMOTE_LIST_CHANGED], 0);
    if (list->priv->device_cons_changed)
        g_signal_emit (list, signals[DEVICE_LIST_CHANGED], 0);

    list->priv->update_lock = FALSE;
}


void gnome_cmd_con_list_add_remote (GnomeCmdConList *list, GnomeCmdConRemote *con)
{
    g_return_if_fail (g_list_index (list->priv->all_cons, con) == -1);
    g_return_if_fail (g_list_index (list->priv->remote_cons, con) == -1);

    list->priv->all_cons = g_list_append (list->priv->all_cons, con);
    list->priv->remote_cons = g_list_append (list->priv->remote_cons, con);

    g_signal_connect (con, "updated", G_CALLBACK (on_con_updated), list);

    if (list->priv->update_lock)
    {
        list->priv->changed = TRUE;
        list->priv->remote_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (list, signals[LIST_CHANGED], 0);
        g_signal_emit (list, signals[REMOTE_LIST_CHANGED], 0);
    }
}


void gnome_cmd_con_list_remove_remote (GnomeCmdConList *list, GnomeCmdConRemote *con)
{
    g_return_if_fail (g_list_index (list->priv->all_cons, con) != -1);
    g_return_if_fail (g_list_index (list->priv->remote_cons, con) != -1);

    list->priv->all_cons = g_list_remove (list->priv->all_cons, con);
    list->priv->remote_cons = g_list_remove (list->priv->remote_cons, con);

    g_signal_handlers_disconnect_by_func (con, (gpointer) on_con_updated, list);

    if (list->priv->update_lock)
    {
        list->priv->changed = TRUE;
        list->priv->remote_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (list, signals[LIST_CHANGED], 0);
        g_signal_emit (list, signals[REMOTE_LIST_CHANGED], 0);
    }
}


void gnome_cmd_con_list_add_dev (GnomeCmdConList *list, GnomeCmdConDevice *con)
{
    g_return_if_fail (g_list_index (list->priv->all_cons, con) == -1);
    g_return_if_fail (g_list_index (list->priv->device_cons, con) == -1);

    list->priv->all_cons = g_list_append (list->priv->all_cons, con);
    list->priv->device_cons = g_list_append (list->priv->device_cons, con);
    g_signal_connect (con, "updated", G_CALLBACK (on_con_updated), list);

    if (list->priv->update_lock)
    {
        list->priv->changed = TRUE;
        list->priv->device_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (list, signals[LIST_CHANGED], 0);
        g_signal_emit (list, signals[DEVICE_LIST_CHANGED], 0);
    }
}


void gnome_cmd_con_list_remove_dev (GnomeCmdConList *list, GnomeCmdConDevice *con)
{
    g_return_if_fail (g_list_index (list->priv->all_cons, con) != -1);
    g_return_if_fail (g_list_index (list->priv->device_cons, con) != -1);

    list->priv->all_cons = g_list_remove (list->priv->all_cons, con);
    list->priv->device_cons = g_list_remove (list->priv->device_cons, con);
    g_signal_handlers_disconnect_by_func (con, (gpointer) on_con_updated, list);

    if (list->priv->update_lock)
    {
        list->priv->changed = TRUE;
        list->priv->device_cons_changed = TRUE;
    }
    else
    {
        g_signal_emit (list, signals[LIST_CHANGED], 0);
        g_signal_emit (list, signals[DEVICE_LIST_CHANGED], 0);
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


GnomeCmdCon *gnome_cmd_con_list_find_by_uuid (GnomeCmdConList *con_list, const gchar *uuid)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), nullptr);
    g_return_val_if_fail (uuid != nullptr, nullptr);

    for (GList *elem = con_list->priv->all_cons; elem; elem = elem->next)
    {
        auto con = static_cast<GnomeCmdCon*> (elem->data);
        if (!strcmp (gnome_cmd_con_get_uuid (con), uuid))
            return con;
    }
    return nullptr;
}


GnomeCmdCon *gnome_cmd_con_list_find_by_alias (GnomeCmdConList *list, const gchar *alias)
{
    g_return_val_if_fail (alias != nullptr, nullptr);

    auto elem = g_list_find_custom (list->priv->all_cons, alias, (GCompareFunc) compare_alias);

    return elem ? static_cast<GnomeCmdCon*> (elem->data) : nullptr;
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
    auto remoteCons = get_remote_cons();

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


void gnome_cmd_con_list_load_bookmarks (GnomeCmdConList *list, GVariant *gVariantBookmarks)
{
    for (GList *cons = gnome_cmd_con_list_get_all (list); cons; cons = cons->next)
        gnome_cmd_con_erase_bookmark (GNOME_CMD_CON (cons->data));

    GnomeCmdCon *gnomeCmdCon {nullptr};

    g_autoptr(GVariantIter) iter1 {nullptr};

    g_variant_get (gVariantBookmarks, GCMD_SETTINGS_BOOKMARKS_FORMAT_STRING, &iter1);

    gboolean isRemote {false};
    gchar *bookmarkGroupName {nullptr};
    gchar *bookmarkName {nullptr};
    gchar *bookmarkPath {nullptr};

    while (g_variant_iter_loop (iter1,
            GCMD_SETTINGS_BOOKMARK_FORMAT_STRING,
            &isRemote,
            &bookmarkGroupName,
            &bookmarkName,
            &bookmarkPath))
    {
        if (isRemote)
            gnomeCmdCon = gnome_cmd_con_list_find_by_alias (list, bookmarkGroupName);
        else if (strcmp(bookmarkGroupName, "Home") == 0)
            gnomeCmdCon = list->priv->home_con;
        else if (strcmp(bookmarkGroupName, "SMB") == 0)
            gnomeCmdCon = list->priv->smb_con;
        else
            gnomeCmdCon = nullptr;

        if (!gnomeCmdCon)
            g_warning ("<Bookmarks> unknown connection: '%s' - ignored", bookmarkGroupName);
        else
            gnome_cmd_con_add_bookmark (gnomeCmdCon, bookmarkName, bookmarkPath);
    }

    g_variant_unref(gVariantBookmarks);
}


static gboolean add_bookmark_to_gvariant_builder(GVariantBuilder *gVariantBuilder, std::string bookmarkGroupName, GnomeCmdCon *con)
{
    if (!con)
        return FALSE;

    GList *bookmarks = gnome_cmd_con_get_bookmarks (con)->bookmarks;

    if (!bookmarks)
        return FALSE;

    gboolean isRemote = GNOME_CMD_IS_CON_REMOTE (con) ? TRUE : FALSE;

    for (GList *i = bookmarks; i; i = i->next)
    {
        auto bookmark = static_cast<GnomeCmdBookmark*> (i->data);

        g_variant_builder_add (gVariantBuilder, GCMD_SETTINGS_BOOKMARK_FORMAT_STRING,
                               isRemote,
                               bookmarkGroupName.c_str(),
                               bookmark->name,
                               bookmark->path
                              );
    }
    return TRUE;
}


GVariant *gnome_cmd_con_list_save_bookmarks (GnomeCmdConList *list)
{
    gboolean hasBookmarks = false;
    GVariantBuilder* gVariantBuilder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
    g_variant_builder_init (gVariantBuilder, G_VARIANT_TYPE_ARRAY);

    // Home
    auto *con = list->priv->home_con;
    hasBookmarks |= add_bookmark_to_gvariant_builder(gVariantBuilder, "Home", con);

    // Samba
    con = list->priv->smb_con;
    hasBookmarks |= add_bookmark_to_gvariant_builder(gVariantBuilder, "SMB", con);

    // Others
    for (GList *i = gnome_cmd_con_list_get_all_remote (list); i; i=i->next)
    {
        con = GNOME_CMD_CON (i->data);
        string bookmarkGroupName = gnome_cmd_con_get_alias (con);
        hasBookmarks |= add_bookmark_to_gvariant_builder(gVariantBuilder, bookmarkGroupName, con);
    }

    if (!hasBookmarks)
    {
        g_variant_builder_unref(gVariantBuilder);
        return nullptr;
    }
    else
    {
        GVariant* bookmarksToStore = g_variant_builder_end (gVariantBuilder);
        g_variant_builder_unref(gVariantBuilder);
        return bookmarksToStore;
    }
}
