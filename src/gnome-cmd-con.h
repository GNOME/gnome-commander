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
#include "utils.h"

struct GnomeCmdCon
{
    GObject parent;

    enum State
    {
        STATE_CLOSED,
        STATE_OPEN,
        STATE_OPENING
    };

    enum OpenResult
    {
        OPEN_OK,
        OPEN_FAILED,
        OPEN_CANCELLED,
        OPEN_IN_PROGRESS,
        OPEN_NOT_STARTED
    };
};

struct GnomeCmdConClass
{
    GObjectClass parent_class;

    /* signals */
    void (* updated) (GnomeCmdCon *con);
    void (* close) (GnomeCmdCon *con, GtkWindow *parent_window);

    /* virtual functions */
};


extern "C" GType gnome_cmd_con_get_type ();

extern "C" GnomeCmdPath *gnome_cmd_con_get_base_path(GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_base_path(GnomeCmdCon *con, GnomeCmdPath *path);

extern "C" GFileInfo *gnome_cmd_con_get_base_file_info(GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_base_file_info(GnomeCmdCon *con, GFileInfo *file_info);

extern "C" void gnome_cmd_con_set_state (GnomeCmdCon *con, GnomeCmdCon::State state);

extern "C" gboolean gnome_cmd_con_is_open (GnomeCmdCon *con);

extern "C" GUri *gnome_cmd_con_get_uri (GnomeCmdCon *con);
extern "C" gchar *gnome_cmd_con_get_uri_string (GnomeCmdCon *con);

extern "C" void gnome_cmd_con_set_uri (GnomeCmdCon *con, GUri *uri);
extern "C" void gnome_cmd_con_set_uri_string (GnomeCmdCon *con, const gchar *uri_string);

extern "C" GFile *gnome_cmd_con_create_gfile (GnomeCmdCon *con, GnomeCmdPath *path);

extern "C" GnomeCmdPath *gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str);

extern "C" void gnome_cmd_con_set_alias (GnomeCmdCon *con, const gchar *alias=NULL);

extern "C" GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir);

inline gboolean gnome_cmd_con_should_remember_dir (GnomeCmdCon *con)
{
    return TRUE;
}

extern "C" gboolean gnome_cmd_con_is_local (GnomeCmdCon *con);
