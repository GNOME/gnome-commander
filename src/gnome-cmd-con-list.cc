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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con-home.h"
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-con-list.h"

using namespace std;


struct GnomeCmdConList::Private
{
    gboolean update_lock;
    gboolean changed;
    gboolean ftp_cons_changed;
    gboolean device_cons_changed;
    gboolean quick_ftp_cons_changed;

    GList *ftp_cons;
    GList *device_cons;
    GList *quick_ftp_cons;

    GnomeCmdCon *home_con;
    GnomeCmdCon *smb_con;

    GList *all_cons;
};


static GtkObjectClass *parent_class = NULL;

enum
{
    LIST_CHANGED,
    FTP_LIST_CHANGED,
    DEVICE_LIST_CHANGED,
    QUICK_FTP_LIST_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void on_con_updated (GnomeCmdCon *con, GnomeCmdConList *con_list)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));

    if (GNOME_CMD_IS_CON_FTP (con))
        gtk_signal_emit (GTK_OBJECT (con_list), signals[FTP_LIST_CHANGED]);
    else
        if (GNOME_CMD_IS_CON_DEVICE (con))
            gtk_signal_emit (GTK_OBJECT (con_list), signals[DEVICE_LIST_CHANGED]);

    gtk_signal_emit (GTK_OBJECT (con_list), signals[LIST_CHANGED]);
}


static gint compare_alias (const GnomeCmdCon *c1, const GnomeCmdCon *c2)
{
    return g_utf8_collate (c1->alias, c2->alias);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdConList *con_list = GNOME_CMD_CON_LIST (object);

    g_free (con_list->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdConListClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    parent_class = (GtkObjectClass *) gtk_type_class (gtk_object_get_type ());

    signals[LIST_CHANGED]           = gtk_signal_new ("list-changed",
                                                              GTK_RUN_LAST,
                                                              G_OBJECT_CLASS_TYPE (object_class),
                                                              GTK_SIGNAL_OFFSET (GnomeCmdConListClass, list_changed),
                                                              gtk_marshal_NONE__NONE,
                                                              GTK_TYPE_NONE,
                                                              0);

    signals[FTP_LIST_CHANGED]       = gtk_signal_new ("ftp-list-changed",
                                                               GTK_RUN_LAST,
                                                               G_OBJECT_CLASS_TYPE (object_class),
                                                               GTK_SIGNAL_OFFSET (GnomeCmdConListClass, ftp_list_changed),
                                                               gtk_marshal_NONE__NONE,
                                                               GTK_TYPE_NONE,
                                                               0);

    signals[DEVICE_LIST_CHANGED]    = gtk_signal_new ("device-list-changed",
                                                               GTK_RUN_LAST,
                                                               G_OBJECT_CLASS_TYPE (object_class),
                                                               GTK_SIGNAL_OFFSET (GnomeCmdConListClass, device_list_changed),
                                                               gtk_marshal_NONE__NONE,
                                                               GTK_TYPE_NONE,
                                                               0);

    signals[QUICK_FTP_LIST_CHANGED] = gtk_signal_new ("quick-ftp-list-changed",
                                                               GTK_RUN_LAST,
                                                               G_OBJECT_CLASS_TYPE (object_class),
                                                               GTK_SIGNAL_OFFSET (GnomeCmdConListClass, quick_ftp_list_changed),
                                                               gtk_marshal_NONE__NONE,
                                                               GTK_TYPE_NONE,
                                                               0);

    object_class->destroy = destroy;
}


static void init (GnomeCmdConList *con_list)
{
    con_list->priv = g_new0 (GnomeCmdConList::Private, 1);

    con_list->priv->update_lock = FALSE;

    con_list->priv->home_con = gnome_cmd_con_home_new ();
    con_list->priv->smb_con = gnome_cmd_con_smb_new ();

    // con_list->priv->ftp_cons = NULL;
    // con_list->priv->device_cons = NULL;
    // con_list->priv->quick_ftp_cons = NULL;
    con_list->priv->all_cons = g_list_append (NULL, con_list->priv->home_con);
    con_list->priv->all_cons = g_list_append (con_list->priv->all_cons, con_list->priv->smb_con);
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_con_list_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdConList",
            sizeof (GnomeCmdConList),
            sizeof (GnomeCmdConListClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_object_get_type (), &info);
    }
    return type;
}


void GnomeCmdConList::lock()
{
    priv->update_lock = TRUE;
    priv->changed = FALSE;
    priv->ftp_cons_changed = FALSE;
    priv->device_cons_changed = FALSE;
}


void GnomeCmdConList::unlock()
{
    if (priv->changed)
        gtk_signal_emit (*this, signals[LIST_CHANGED]);
    if (priv->ftp_cons_changed)
        gtk_signal_emit (*this, signals[FTP_LIST_CHANGED]);
    if (priv->device_cons_changed)
        gtk_signal_emit (*this, signals[DEVICE_LIST_CHANGED]);

    priv->update_lock = FALSE;
}


void GnomeCmdConList::add(GnomeCmdConFtp *con)
{
    g_return_if_fail (g_list_index (priv->all_cons, con) == -1);
    g_return_if_fail (g_list_index (priv->ftp_cons, con) == -1);

    priv->all_cons = g_list_append (priv->all_cons, con);
    priv->ftp_cons = g_list_append (priv->ftp_cons, con);

    g_signal_connect (con, "updated", G_CALLBACK (on_con_updated), this);

    if (priv->update_lock)
    {
        priv->changed = TRUE;
        priv->ftp_cons_changed = TRUE;
    }
    else
    {
        gtk_signal_emit (*this, signals[LIST_CHANGED]);
        gtk_signal_emit (*this, signals[FTP_LIST_CHANGED]);
    }
}


void GnomeCmdConList::remove(GnomeCmdConFtp *con)
{
    g_return_if_fail (g_list_index (priv->all_cons, con) != -1);
    g_return_if_fail (g_list_index (priv->ftp_cons, con) != -1);

    priv->all_cons = g_list_remove (priv->all_cons, con);
    priv->ftp_cons = g_list_remove (priv->ftp_cons, con);

    g_signal_handlers_disconnect_by_func (con, (gpointer) on_con_updated, this);

    if (priv->update_lock)
    {
        priv->changed = TRUE;
        priv->ftp_cons_changed = TRUE;
    }
    else
    {
        gtk_signal_emit (*this, signals[LIST_CHANGED]);
        gtk_signal_emit (*this, signals[FTP_LIST_CHANGED]);
    }
}


void gnome_cmd_con_list_add_quick_ftp (GnomeCmdConList *con_list, GnomeCmdConFtp *ftp_con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));
    g_return_if_fail (g_list_index (con_list->priv->all_cons, ftp_con) == -1);
    g_return_if_fail (g_list_index (con_list->priv->quick_ftp_cons, ftp_con) == -1);

    con_list->priv->all_cons = g_list_append (con_list->priv->all_cons, ftp_con);
    con_list->priv->quick_ftp_cons = g_list_append (con_list->priv->quick_ftp_cons, ftp_con);

    g_signal_connect (ftp_con, "updated", G_CALLBACK (on_con_updated), con_list);

    if (con_list->priv->update_lock)
    {
        con_list->priv->changed = TRUE;
        con_list->priv->quick_ftp_cons_changed = TRUE;
    }
    else
    {
        gtk_signal_emit (GTK_OBJECT (con_list), signals[LIST_CHANGED]);
        gtk_signal_emit (GTK_OBJECT (con_list), signals[QUICK_FTP_LIST_CHANGED]);
    }
}


void gnome_cmd_con_list_remove_quick_ftp (GnomeCmdConList *con_list, GnomeCmdConFtp *ftp_con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));
    g_return_if_fail (g_list_index (con_list->priv->all_cons, ftp_con) != -1);
    g_return_if_fail (g_list_index (con_list->priv->quick_ftp_cons, ftp_con) != -1);

    con_list->priv->all_cons = g_list_remove (con_list->priv->all_cons, ftp_con);
    con_list->priv->quick_ftp_cons = g_list_remove (con_list->priv->quick_ftp_cons, ftp_con);

    g_signal_handlers_disconnect_by_func (ftp_con, (gpointer) on_con_updated, con_list);

    if (con_list->priv->update_lock)
    {
        con_list->priv->changed = TRUE;
        con_list->priv->quick_ftp_cons_changed = TRUE;
    }
    else
    {
        gtk_signal_emit (GTK_OBJECT (con_list), signals[LIST_CHANGED]);
        gtk_signal_emit (GTK_OBJECT (con_list), signals[QUICK_FTP_LIST_CHANGED]);
    }
}


void gnome_cmd_con_list_add_device (GnomeCmdConList *con_list, GnomeCmdConDevice *device_con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));
    g_return_if_fail (g_list_index (con_list->priv->all_cons, device_con) == -1);
    g_return_if_fail (g_list_index (con_list->priv->device_cons, device_con) == -1);

    con_list->priv->all_cons = g_list_append (con_list->priv->all_cons, device_con);
    con_list->priv->device_cons = g_list_append (con_list->priv->device_cons, device_con);
    g_signal_connect (device_con, "updated", G_CALLBACK (on_con_updated), con_list);

    if (con_list->priv->update_lock)
    {
        con_list->priv->changed = TRUE;
        con_list->priv->device_cons_changed = TRUE;
    }
    else
    {
        gtk_signal_emit (GTK_OBJECT (con_list), signals[LIST_CHANGED]);
        gtk_signal_emit (GTK_OBJECT (con_list), signals[DEVICE_LIST_CHANGED]);
    }
}


void gnome_cmd_con_list_remove_device (GnomeCmdConList *con_list, GnomeCmdConDevice *device_con)
{
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));
    g_return_if_fail (g_list_index (con_list->priv->all_cons, device_con) != -1);
    g_return_if_fail (g_list_index (con_list->priv->device_cons, device_con) != -1);

    con_list->priv->all_cons = g_list_remove (con_list->priv->all_cons, device_con);
    con_list->priv->device_cons = g_list_remove (con_list->priv->device_cons, device_con);
    g_signal_handlers_disconnect_by_func (device_con, (gpointer) on_con_updated, con_list);

    if (con_list->priv->update_lock)
    {
        con_list->priv->changed = TRUE;
        con_list->priv->device_cons_changed = TRUE;
    }
    else
    {
        gtk_signal_emit (GTK_OBJECT (con_list), signals[LIST_CHANGED]);
        gtk_signal_emit (GTK_OBJECT (con_list), signals[DEVICE_LIST_CHANGED]);
    }
}


GList *gnome_cmd_con_list_get_all (GnomeCmdConList *con_list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), NULL);

    return con_list->priv->all_cons;
}


GList *gnome_cmd_con_list_get_all_ftp (GnomeCmdConList *con_list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), NULL);

    return con_list->priv->ftp_cons;
}


GList *gnome_cmd_con_list_get_all_dev (GnomeCmdConList *con_list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (con_list), NULL);

    return con_list->priv->device_cons;
}


void gnome_cmd_con_list_set_all_dev (GnomeCmdConList *con_list, GList *dev_cons)
{
    g_return_if_fail (GNOME_CMD_IS_CON_LIST (con_list));

    con_list->priv->device_cons = dev_cons;
}


GnomeCmdCon *GnomeCmdConList::find_alias(const gchar *alias) const
{
    g_return_val_if_fail (alias!=NULL, NULL);

    GnomeCmdCon con;                // used as reference element to be looked for, no allocation necessary
    con.alias = (gchar *) alias;

    GList *elem = g_list_find_custom (priv->all_cons, &con, (GCompareFunc) compare_alias);

    return elem ? (GnomeCmdCon *) elem->data : NULL;
}


GnomeCmdCon *gnome_cmd_con_list_get_home (GnomeCmdConList *list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (list), NULL);

    return list->priv->home_con;
}


GnomeCmdCon *gnome_cmd_con_list_get_smb (GnomeCmdConList *list)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON_LIST (list), NULL);

    return list->priv->smb_con;
}
