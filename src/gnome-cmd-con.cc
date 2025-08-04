/**
 * @file gnome-cmd-con.cc
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
#include "gnome-cmd-con.h"
#include "gnome-cmd-path.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"

using namespace std;


struct GnomeCmdConPrivate
{
    GnomeCmdPath   *base_path;
    GFileInfo      *base_gFileInfo;

    GUri           *uri;
    GnomeCmdDir    *default_dir;   // the start dir of this connection
};

enum
{
    UPDATED,
    CLOSE,
    OPEN_DONE,
    OPEN_FAILED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdCon, gnome_cmd_con, G_TYPE_OBJECT)


static void on_open_done (GnomeCmdCon *con)
{
    gnome_cmd_con_updated (con);
}


static void on_open_failed (GnomeCmdCon *con)
{
    // gnome_cmd_con_updated (con);
    // Free the error because the error handling is done now. (Logging happened already.)
    g_clear_error (&con->open_failed_error);
    con->open_failed_msg = nullptr;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    GnomeCmdCon *con = GNOME_CMD_CON (object);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    g_clear_pointer (&con->alias, g_free);
    g_clear_pointer (&priv->uri, g_uri_unref);
    g_clear_pointer (&priv->base_path, gnome_cmd_path_free);
    g_clear_object (&priv->base_gFileInfo);
    g_clear_error (&con->open_failed_error);

    g_clear_pointer (&priv->default_dir, g_object_unref);

    G_OBJECT_CLASS (gnome_cmd_con_parent_class)->dispose (object);
}


static void gnome_cmd_con_class_init (GnomeCmdConClass *klass)
{
    signals[UPDATED] =
        g_signal_new ("updated",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdConClass, updated),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[CLOSE] =
        g_signal_new ("close",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdConClass, close),
            nullptr, nullptr,
            nullptr,
            G_TYPE_NONE,
            1, GTK_TYPE_WINDOW);

    signals[OPEN_DONE] =
        g_signal_new ("open-done",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdConClass, open_done),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[OPEN_FAILED] =
        g_signal_new ("open-failed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdConClass, open_failed),
            nullptr, nullptr,
            nullptr,
            G_TYPE_NONE,
            1, G_TYPE_STRING);

    G_OBJECT_CLASS (klass)->dispose = dispose;

    klass->updated = nullptr;
    klass->open_done = on_open_done;
    klass->open_failed = on_open_failed;

    klass->open = nullptr;
    klass->close = nullptr;
    klass->cancel_open = nullptr;
    klass->open_is_needed = nullptr;
    klass->create_gfile = nullptr;
    klass->create_path = nullptr;

    klass->get_go_text = nullptr;
    klass->get_open_text = nullptr;
    klass->get_close_text = nullptr;
    klass->get_go_tooltip = nullptr;
    klass->get_open_tooltip = nullptr;
    klass->get_close_tooltip = nullptr;
    klass->get_go_icon = nullptr;
    klass->get_open_icon = nullptr;
    klass->get_close_icon = nullptr;
}


static void gnome_cmd_con_init (GnomeCmdCon *con)
{
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    priv->uri = nullptr;

    con->alias = nullptr;

    con->open_msg = nullptr;
    con->should_remember_dir = FALSE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = FALSE;
    con->is_local = FALSE;
    con->is_closeable = FALSE;

    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_result = GnomeCmdCon::OPEN_NOT_STARTED;
    con->open_failed_msg = nullptr;
    con->open_failed_error = nullptr;

    priv->base_path = nullptr;
    priv->default_dir = nullptr;
}


/***********************************
 * Public functions
 ***********************************/

static gboolean check_con_open_progress (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    g_return_val_if_fail (con->open_result != GnomeCmdCon::OPEN_NOT_STARTED, FALSE);

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (con->open_result)
    {
        case GnomeCmdCon::OPEN_IN_PROGRESS:
            return TRUE;

        case GnomeCmdCon::OPEN_OK:
            {
                DEBUG('m', "GnomeCmdCon::OPEN_OK detected\n");

                GnomeCmdDir *dir = gnome_cmd_dir_new_with_con (con);

                gnome_cmd_con_set_default_dir (con, dir);

                DEBUG ('m', "Emitting 'open-done' signal\n");
                g_signal_emit (con, signals[OPEN_DONE], 0);
            }
            return FALSE;

        case GnomeCmdCon::OPEN_FAILED:
            {
                DEBUG ('m', "GnomeCmdCon::OPEN_FAILED detected\n");
                DEBUG ('m', "Emitting 'open-failed' signal\n");
                g_signal_emit (con, signals[OPEN_FAILED], 0, con->open_failed_msg);
            }
            return FALSE;

        default:
            return FALSE;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


GnomeCmdPath *gnome_cmd_con_get_base_path(GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    return priv->base_path;
}


void gnome_cmd_con_set_base_path(GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    if (priv->base_path)
        gnome_cmd_path_free (priv->base_path);

    priv->base_path = path;
}


GFileInfo *gnome_cmd_con_get_base_file_info(GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    return priv->base_gFileInfo;
}


void gnome_cmd_con_set_base_file_info(GnomeCmdCon *con, GFileInfo *file_info)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    g_set_object (&priv->base_gFileInfo, file_info);
}


void gnome_cmd_con_open (GnomeCmdCon *con, GtkWindow *parent_window)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    DEBUG ('m', "Opening connection\n");

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    if (con->state != GnomeCmdCon::STATE_OPEN)
        klass->open (con, parent_window);

    g_timeout_add (gui_update_rate(), (GSourceFunc) check_con_open_progress, con);
}


gboolean gnome_cmd_con_is_open (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->state == GnomeCmdCon::STATE_OPEN;
}


void gnome_cmd_con_cancel_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (con->state == GnomeCmdCon::STATE_OPENING)
    {
        GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
        klass->cancel_open (con);
    }
}


void gnome_cmd_con_close (GnomeCmdCon *con, GtkWindow *parent_window)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (gnome_cmd_con_is_closeable (con) && gnome_cmd_con_is_open(con))
    {
        g_signal_emit (con, signals[CLOSE], 0, parent_window);
        g_signal_emit (con, signals[UPDATED], 0);
    }
}


gboolean gnome_cmd_con_open_is_needed (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->open_is_needed (con);
}


const gchar *gnome_cmd_con_get_open_msg (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_msg;
}


void gnome_cmd_con_set_open_msg (GnomeCmdCon *con, const gchar *msg)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->open_msg);
    con->open_msg = g_strdup (msg);
}


GUri *gnome_cmd_con_get_uri (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));
    return priv->uri;
}

gchar *gnome_cmd_con_get_uri_string (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));
    return priv->uri ? g_uri_to_string (priv->uri) : nullptr;
}

void gnome_cmd_con_set_uri (GnomeCmdCon *con, GUri *uri)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));
    if (uri)
        g_uri_ref (uri);
    g_clear_pointer (&priv->uri, g_uri_unref);
    priv->uri = uri;
}

void gnome_cmd_con_set_uri_string (GnomeCmdCon *con, const gchar *uri_string)
{
    auto uri = g_uri_parse (uri_string, G_URI_FLAGS_NONE, nullptr);
    gnome_cmd_con_set_uri (con, uri);
}

GFile *gnome_cmd_con_create_gfile (GnomeCmdCon *con, const gchar *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    return klass->create_gfile (con, path);
}


GnomeCmdPath *gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    return klass->create_path (con, path_str);
}


void gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    g_set_object(&priv->default_dir, dir);
}


GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    return priv->default_dir;
}


gboolean gnome_cmd_con_should_remember_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->should_remember_dir;
}


gboolean gnome_cmd_con_needs_open_visprog (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->needs_open_visprog;
}


gboolean gnome_cmd_con_needs_list_visprog (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->needs_list_visprog;
}


gboolean gnome_cmd_con_can_show_free_space (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->can_show_free_space;
}


gboolean gnome_cmd_con_is_closeable (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->is_closeable;
}


void gnome_cmd_con_updated (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    g_signal_emit (con, signals[UPDATED], 0);
}


/**
 *  Get the type of the file at the specified path.
 *  If the file does not exists, the function will return false.
 *  If the operation succeeds, true is returned and type is set.
 */
gboolean gnome_cmd_con_get_path_target_type (GnomeCmdCon *con, const gchar *path_str, GFileType *gFileType)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), false);
    g_return_val_if_fail (path_str != nullptr, false);

    GnomeCmdPath *path = gnome_cmd_con_create_path (con, path_str);
    gchar *path_str2 = gnome_cmd_path_get_path (path);
    auto gFile = gnome_cmd_con_create_gfile(con, path_str2);
    g_free (path_str2);

    if (!gFile || !g_file_query_exists(gFile, nullptr))
    {
        if (gFile)
            g_object_unref(gFile);
        *gFileType = G_FILE_TYPE_UNKNOWN;
        gnome_cmd_path_free (path);
        return false;
    }

    *gFileType = g_file_query_file_type(gFile, G_FILE_QUERY_INFO_NONE, nullptr);
    g_object_unref(gFile);
    gnome_cmd_path_free (path);

    return true;
}


gboolean gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str, GError *error)
{
    GError *tmpError = nullptr;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), false);
    g_return_val_if_fail (path_str != nullptr, false);

    auto path = gnome_cmd_con_create_path (con, path_str);
    gchar *path_str2 = gnome_cmd_path_get_path (path);
    auto gFile = gnome_cmd_con_create_gfile (con, path_str2);
    g_free (path_str2);

    if (!g_file_make_directory (gFile, nullptr, &tmpError))
    {
        g_warning("g_file_make_directory error: %s\n", tmpError ? tmpError->message : "unknown error");
        g_propagate_error(&error, tmpError);
        return false;
    }

    g_object_unref (gFile);
    gnome_cmd_path_free (path);

    return true;
}


gchar *gnome_cmd_con_get_go_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    if (klass->get_go_text)
        return klass->get_go_text (con);

    const gchar *alias = con->alias;
    if (!alias)
        alias = _("<New connection>");

    return g_strdup_printf (_("Go to: %s"), alias);
}


gchar *gnome_cmd_con_get_go_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_go_tooltip ? klass->get_go_tooltip (con) : nullptr;
}


GIcon *gnome_cmd_con_get_go_icon (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_go_icon ? klass->get_go_icon (con) : nullptr;
}


gchar *gnome_cmd_con_get_open_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    if (klass->get_open_text)
        return klass->get_open_text (con);

    const gchar *alias = con->alias;
    if (!alias)
        alias = _("<New connection>");

    return g_strdup_printf (_("Connect to: %s"), alias);
}


gchar *gnome_cmd_con_get_open_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_open_tooltip ? klass->get_open_tooltip (con) : nullptr;
}


GIcon *gnome_cmd_con_get_open_icon (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_open_icon ? klass->get_open_icon (con) : nullptr;
}


gchar *gnome_cmd_con_get_close_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    if (klass->get_close_text)
        return klass->get_close_text (con);

    const gchar *alias = con->alias;
    if (!alias)
        alias = _("<New connection>");

    return g_strdup_printf (_("Disconnect from: %s"), alias);
}


gchar *gnome_cmd_con_get_close_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_close_tooltip ? klass->get_close_tooltip (con) : nullptr;
}


GIcon *gnome_cmd_con_get_close_icon (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_close_icon ? klass->get_close_icon (con) : nullptr;
}


// FFI

const gchar *gnome_cmd_con_get_alias (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->alias;
}

void gnome_cmd_con_set_alias (GnomeCmdCon *con, const gchar *alias)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->alias);
    con->alias = g_strdup (alias);
}

gboolean gnome_cmd_con_is_local (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->is_local;
}
