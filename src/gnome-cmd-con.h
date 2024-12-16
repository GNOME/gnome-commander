/**
 * @file gnome-cmd-con.h
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

#define GNOME_CMD_TYPE_CON              (gnome_cmd_con_get_type ())
#define GNOME_CMD_CON(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CON, GnomeCmdCon))
#define GNOME_CMD_CON_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CON, GnomeCmdConClass))
#define GNOME_CMD_IS_CON(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CON))
#define GNOME_CMD_IS_CON_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CON))
#define GNOME_CMD_CON_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CON, GnomeCmdConClass))

#include <string>

#include "gnome-cmd-path.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-data.h"
#include "history.h"
#include "utils.h"

struct GnomeCmdCon
{
    GObject parent;

    enum State
    {
        STATE_CLOSED,
        STATE_OPEN,
        STATE_OPENING,
        STATE_CANCELLING
    };

    enum OpenResult
    {
        OPEN_OK,
        OPEN_FAILED,
        OPEN_CANCELLED,
        OPEN_IN_PROGRESS,
        OPEN_NOT_STARTED
    };

    gchar               *alias;                 // coded as UTF-8

    gchar               *open_msg;
    gboolean            should_remember_dir;
    gboolean            needs_open_visprog;
    gboolean            needs_list_visprog;     // Defines if a graphical progress bar should be drawn when opening a folder
    gboolean            can_show_free_space;
    State               state;
    gboolean            is_local;
    gboolean            is_closeable;

    OpenResult          open_result;
    GError              *open_failed_error;
    gchar               *open_failed_msg;
};

struct GnomeCmdConClass
{
    GObjectClass parent_class;

    /* signals */
    void (* updated) (GnomeCmdCon *con);
    void (* open_done) (GnomeCmdCon *con);
    void (* open_failed) (GnomeCmdCon *con);

    /* virtual functions */
    void (* open) (GnomeCmdCon *con, GtkWindow *parent_window);
    void (* cancel_open) (GnomeCmdCon *con);
    gboolean (* close) (GnomeCmdCon *con, GtkWindow *parent_window);
    gboolean (* open_is_needed) (GnomeCmdCon *con);
    GFile *(* create_gfile) (GnomeCmdCon *con, const gchar *path_str);
    GnomeCmdPath *(* create_path) (GnomeCmdCon *con, const gchar *path_str);

    gchar *(* get_go_text) (GnomeCmdCon *con);
    gchar *(* get_open_text) (GnomeCmdCon *con);
    gchar *(* get_close_text) (GnomeCmdCon *con);
    gchar *(* get_go_tooltip) (GnomeCmdCon *con);
    gchar *(* get_open_tooltip) (GnomeCmdCon *con);
    gchar *(* get_close_tooltip) (GnomeCmdCon *con);
    GIcon *(* get_go_icon) (GnomeCmdCon *con);
    GIcon *(* get_open_icon) (GnomeCmdCon *con);
    GIcon *(* get_close_icon) (GnomeCmdCon *con);
};


extern "C" GType gnome_cmd_con_get_type ();

extern "C" const gchar *gnome_cmd_con_get_uuid (GnomeCmdCon *con);

extern "C" GnomeCmdPath *gnome_cmd_con_get_base_path(GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_base_path(GnomeCmdCon *con, GnomeCmdPath *path);

extern "C" GFileInfo *gnome_cmd_con_get_base_file_info(GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_base_file_info(GnomeCmdCon *con, GFileInfo *file_info);

void gnome_cmd_con_open (GnomeCmdCon *con, GtkWindow *parent_window);

extern "C" gboolean gnome_cmd_con_is_open (GnomeCmdCon *con);

void gnome_cmd_con_cancel_open (GnomeCmdCon *con);

extern "C" gboolean gnome_cmd_con_close (GnomeCmdCon *con);

gboolean gnome_cmd_con_open_is_needed (GnomeCmdCon *con);

extern "C" GUri *gnome_cmd_con_get_uri (GnomeCmdCon *con);
extern "C" gchar *gnome_cmd_con_get_uri_string (GnomeCmdCon *con);

extern "C" void gnome_cmd_con_set_uri (GnomeCmdCon *con, GUri *uri);
extern "C" void gnome_cmd_con_set_uri_string (GnomeCmdCon *con, const gchar *uri_string);

extern "C" GFile *gnome_cmd_con_create_gfile (GnomeCmdCon *con, const gchar *path = nullptr);

extern "C" GnomeCmdPath *gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str);

const gchar *gnome_cmd_con_get_open_msg (GnomeCmdCon *con);

extern "C" const gchar *gnome_cmd_con_get_alias (GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_alias (GnomeCmdCon *con, const gchar *alias=NULL);

extern "C" GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir);

gboolean gnome_cmd_con_should_remember_dir (GnomeCmdCon *con);
gboolean gnome_cmd_con_needs_open_visprog (GnomeCmdCon *con);
gboolean gnome_cmd_con_needs_list_visprog (GnomeCmdCon *con);
gboolean gnome_cmd_con_can_show_free_space (GnomeCmdCon *con);

extern "C" gboolean gnome_cmd_con_is_local (GnomeCmdCon *con);

gboolean gnome_cmd_con_is_closeable (GnomeCmdCon *con);

History *gnome_cmd_con_get_dir_history (GnomeCmdCon *con);

gchar *gnome_cmd_con_get_go_text (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_open_text (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_close_text (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_go_tooltip (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_open_tooltip (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_close_tooltip (GnomeCmdCon *con);
GIcon *gnome_cmd_con_get_go_icon (GnomeCmdCon *con);
GIcon *gnome_cmd_con_get_open_icon (GnomeCmdCon *con);
GIcon *gnome_cmd_con_get_close_icon (GnomeCmdCon *con);

extern "C" GListModel *gnome_cmd_con_get_bookmarks (GnomeCmdCon *con);

struct GnomeCmdBookmark;

extern "C" void gnome_cmd_con_add_bookmark (GnomeCmdCon *con, GnomeCmdBookmark *bookmark);

extern "C" void gnome_cmd_con_erase_bookmarks (GnomeCmdCon *con);

void gnome_cmd_con_updated (GnomeCmdCon *con);

extern "C" gboolean gnome_cmd_con_get_path_target_type (GnomeCmdCon *con, const gchar *path, GFileType *type);

extern "C" gboolean gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str, GError *error);

void gnome_cmd_con_add_to_cache (GnomeCmdCon *con, GnomeCmdDir *dir, gchar *uri_str = nullptr);

void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, GnomeCmdDir *dir);

void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, const gchar *uri);

GnomeCmdDir *gnome_cmd_con_cache_lookup (GnomeCmdCon *con, const gchar *uri);

void gnome_cmd_con_close_active_or_inactive_connection (GMount *gMount);


extern "C" GType gnome_cmd_bookmark_get_type ();
gchar *gnome_cmd_bookmark_get_name (GnomeCmdBookmark *bookmark);
gchar *gnome_cmd_bookmark_get_path (GnomeCmdBookmark *bookmark);
