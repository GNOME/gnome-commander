/** 
 * @file gnome-cmd-cmdline.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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

#define GNOME_CMD_TYPE_CMDLINE              (gnome_cmd_cmdline_get_type ())
#define GNOME_CMD_CMDLINE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CMDLINE, GnomeCmdCmdline))
#define GNOME_CMD_CMDLINE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CMDLINE, GnomeCmdCmdlineClass))
#define GNOME_CMD_IS_CMDLINE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CMDLINE))
#define GNOME_CMD_IS_CMDLINE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CMDLINE))
#define GNOME_CMD_CMDLINE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CMDLINE, GnomeCmdCmdlineClass))


struct GnomeCmdCmdlinePrivate;


struct GnomeCmdCmdline
{
    GtkHBox parent;
    GnomeCmdCmdlinePrivate *priv;
};


struct GnomeCmdCmdlineClass
{
    GtkHBoxClass parent_class;
};


GtkWidget *gnome_cmd_cmdline_new ();

GType gnome_cmd_cmdline_get_type ();

void gnome_cmd_cmdline_set_dir (GnomeCmdCmdline *cmdline, const gchar *cwd);

void gnome_cmd_cmdline_append_text (GnomeCmdCmdline *cmdline, const gchar *text);
void gnome_cmd_cmdline_insert_text (GnomeCmdCmdline *cmdline, const gchar *text);
void gnome_cmd_cmdline_set_text (GnomeCmdCmdline *cmdline, const gchar *text);

gboolean gnome_cmd_cmdline_is_empty (GnomeCmdCmdline *cmdline);

void gnome_cmd_cmdline_exec (GnomeCmdCmdline *cmdline);

void gnome_cmd_cmdline_focus (GnomeCmdCmdline *cmdline);

void gnome_cmd_cmdline_update_style (GnomeCmdCmdline *cmdline);

GList *gnome_cmd_cmdline_get_history (GnomeCmdCmdline *cmdline);
void gnome_cmd_cmdline_set_history  (GnomeCmdCmdline *cmdline, GList *history);
void gnome_cmd_cmdline_show_history (GnomeCmdCmdline *cmdline);

gboolean gnome_cmd_cmdline_keypressed (GnomeCmdCmdline *cmdline, GdkEventKey *event);
