/**
 * @file gnome-cmd-con-home.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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
#include "gnome-cmd-data.h"
#include "gnome-cmd-con-home.h"
#include "gnome-cmd-plain-path.h"
#include "imageloader.h"

using namespace std;


static GnomeCmdConClass *parent_class = nullptr;



static void home_open (GnomeCmdCon *con)
{
}


static gboolean home_close (GnomeCmdCon *con)
{
    return FALSE;
}


static void home_cancel_open (GnomeCmdCon *con)
{
}


static gboolean home_open_is_needed (GnomeCmdCon *con)
{
    return FALSE;
}


static GFile *home_create_gfile (GnomeCmdCon *con, GnomeCmdPath *path)
{
    return g_file_new_for_path(path->get_path());
}


static GnomeCmdPath *home_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    return new GnomeCmdPlainPath(path_str);
}



/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdConHome *con_home = GNOME_CMD_CON_HOME (object);

    gnome_cmd_pixmap_free (con_home->parent.go_pixmap);
    gnome_cmd_pixmap_free (con_home->parent.open_pixmap);
    gnome_cmd_pixmap_free (con_home->parent.close_pixmap);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdConHomeClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    parent_class = static_cast<GnomeCmdConClass*>(gtk_type_class (GNOME_CMD_TYPE_CON));

    object_class->destroy = destroy;

    con_class->open = home_open;
    con_class->close = home_close;
    con_class->cancel_open = home_cancel_open;
    con_class->open_is_needed = home_open_is_needed;
    con_class->create_gfile = home_create_gfile;
    con_class->create_path = home_create_path;
}


static void init (GnomeCmdConHome *home_con)
{
    guint dev_icon_size = gnome_cmd_data.dev_icon_size;

    GnomeCmdCon *con = GNOME_CMD_CON (home_con);

    con->state = GnomeCmdCon::STATE_OPEN;
    con->alias = g_strdup (_("Home"));
    con->method = CON_FILE;
    con->open_msg = g_strdup ("This should not be visible anywhere");
    con->should_remember_dir = TRUE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = TRUE;
    con->is_local = TRUE;
    con->is_closeable = FALSE;
    con->go_text = g_strdup (_("Go to: Home"));
    con->go_pixmap = gnome_cmd_pixmap_new_from_icon ("gnome-fs-home", dev_icon_size);
    con->open_pixmap = gnome_cmd_pixmap_new_from_icon ("gnome-fs-home", dev_icon_size);
    con->close_pixmap = gnome_cmd_pixmap_new_from_icon ("gnome-fs-home", dev_icon_size);

    GnomeCmdDir *dir = gnome_cmd_dir_new (con, new GnomeCmdPlainPath(g_get_home_dir ()));

    gnome_cmd_con_set_default_dir (con, dir);
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_con_home_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            (gchar*) "GnomeCmdConHome",
            sizeof (GnomeCmdConHome),
            sizeof (GnomeCmdConHomeClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ nullptr,
            /* reserved_2 */ nullptr,
            (GtkClassInitFunc) nullptr
        };

        type = gtk_type_unique (GNOME_CMD_TYPE_CON, &info);
    }
    return type;
}


GnomeCmdCon *gnome_cmd_con_home_new ()
{
    auto con = static_cast<GnomeCmdConHome*>(g_object_new (GNOME_CMD_TYPE_CON_HOME, nullptr));

    return GNOME_CMD_CON (con);
}

