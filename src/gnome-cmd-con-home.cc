/**
 * @file gnome-cmd-con-home.cc
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
#include "gnome-cmd-path.h"

using namespace std;


G_DEFINE_TYPE (GnomeCmdConHome, gnome_cmd_con_home, GNOME_CMD_TYPE_CON)


static void home_open (GnomeCmdCon *con, GtkWindow *parent_window)
{
}


static void home_close (GnomeCmdCon *con, GtkWindow *parent_window)
{
}


static void home_cancel_open (GnomeCmdCon *con)
{
}


static gboolean home_open_is_needed (GnomeCmdCon *con)
{
    return FALSE;
}


static GFile *home_create_gfile (GnomeCmdCon *con, const gchar *path)
{
    return g_file_new_for_path(path);
}


static GnomeCmdPath *home_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    return gnome_cmd_plain_path_new (path_str);
}


static gchar *home_get_go_text (GnomeCmdCon *con)
{
    return g_strdup (_("Go to: Home"));
}


static gchar *home_get_open_text (GnomeCmdCon *con)
{
    return nullptr;
}


static gchar *home_get_close_text (GnomeCmdCon *con)
{
    return nullptr;
}


static GIcon *home_get_icon (GnomeCmdCon *con)
{
    return g_themed_icon_new ("user-home");
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_con_home_class_init (GnomeCmdConHomeClass *klass)
{
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    con_class->open = home_open;
    con_class->close = home_close;
    con_class->cancel_open = home_cancel_open;
    con_class->open_is_needed = home_open_is_needed;
    con_class->create_gfile = home_create_gfile;
    con_class->create_path = home_create_path;

    con_class->get_go_text = home_get_go_text;
    con_class->get_open_text = home_get_open_text;
    con_class->get_close_text = home_get_close_text;

    con_class->get_go_icon = home_get_icon;
    con_class->get_open_icon = home_get_icon;
    con_class->get_close_icon = home_get_icon;
}


static void gnome_cmd_con_home_init (GnomeCmdConHome *home_con)
{
    GnomeCmdCon *con = GNOME_CMD_CON (home_con);

    con->state = GnomeCmdCon::STATE_OPEN;
    con->alias = g_strdup (_("Home"));
    con->open_msg = g_strdup ("This should not be visible anywhere");
    con->should_remember_dir = TRUE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = TRUE;
    con->is_local = TRUE;
    con->is_closeable = FALSE;

    GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_plain_path_new (g_get_home_dir ()));

    gnome_cmd_con_set_default_dir (con, dir);
    gnome_cmd_con_set_uri_string (con, "file:");
}
