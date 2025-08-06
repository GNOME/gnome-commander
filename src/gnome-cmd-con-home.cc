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


static void home_open (GnomeCmdCon *con, GtkWindow *parent_window, GCancellable *cancellable)
{
}


static void home_close (GnomeCmdCon *con, GtkWindow *parent_window)
{
}


static GFile *home_create_gfile (GnomeCmdCon *con, const gchar *path)
{
    return g_file_new_for_path(path);
}


static GnomeCmdPath *home_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    return gnome_cmd_plain_path_new (path_str);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_con_home_class_init (GnomeCmdConHomeClass *klass)
{
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    con_class->open = home_open;
    con_class->close = home_close;
    con_class->create_gfile = home_create_gfile;
    con_class->create_path = home_create_path;
}


static void gnome_cmd_con_home_init (GnomeCmdConHome *home_con)
{
    GnomeCmdCon *con = GNOME_CMD_CON (home_con);

    con->state = GnomeCmdCon::STATE_OPEN;
    con->alias = g_strdup (_("Home"));

    GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_plain_path_new (g_get_home_dir ()));

    gnome_cmd_con_set_default_dir (con, dir);
    gnome_cmd_con_set_uri_string (con, "file:");
}
