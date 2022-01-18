/** 
 * @file gnome-cmd-con-home.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2022 Uwe Scholz\n
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

#include "gnome-cmd-con.h"

#define GNOME_CMD_TYPE_CON_HOME              (gnome_cmd_con_home_get_type ())
#define GNOME_CMD_CON_HOME(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CON_HOME, GnomeCmdConHome))
#define GNOME_CMD_CON_HOME_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CON_HOME, GnomeCmdConHomeClass))
#define GNOME_CMD_IS_CON_HOME(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CON_HOME))
#define GNOME_CMD_IS_CON_HOME_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CON_HOME))
#define GNOME_CMD_CON_HOME_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CON_HOME, GnomeCmdConHomeClass))


struct GnomeCmdConHome
{
    GnomeCmdCon parent;
};


struct GnomeCmdConHomeClass
{
    GnomeCmdConClass parent_class;
};


GtkType gnome_cmd_con_home_get_type ();

GnomeCmdCon *gnome_cmd_con_home_new ();
