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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-path.h"

enum
{
    UPDATED,
    CLOSE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GnomeCmdCon, gnome_cmd_con, G_TYPE_OBJECT)


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    gnome_cmd_con_set_default_dir (GNOME_CMD_CON (object), nullptr);
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

    G_OBJECT_CLASS (klass)->dispose = dispose;

    klass->updated = nullptr;
    klass->close = nullptr;

}


static void gnome_cmd_con_init (GnomeCmdCon *con)
{
}

