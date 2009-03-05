/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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

#ifndef __GNOME_CMD_XFER_PROGRESS_WIN_H__
#define __GNOME_CMD_XFER_PROGRESS_WIN_H__

#define GNOME_CMD_XFER_PROGRESS_WIN(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_xfer_progress_win_get_type (), GnomeCmdXferProgressWin)
#define GNOME_CMD_XFER_PROGRESS_WIN_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_xfer_progress_win_get_type (), GnomeCmdXferProgressWinClass)
#define GNOME_CMD_IS_XFER_PROGRESS_WIN(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_xfer_progress_win_get_type ())


struct GnomeCmdXferProgressWin
{
    GtkWindow parent;

    GtkWidget *totalprog;
    GtkWidget *fileprog;
    GtkWidget *msg_label;
    GtkWidget *fileprog_label;

    gboolean cancel_pressed;
};


struct GnomeCmdXferProgressWinClass
{
    GtkWindowClass parent_class;
};


GtkWidget *gnome_cmd_xfer_progress_win_new (guint no_of_files=0);

GtkType gnome_cmd_xfer_progress_win_get_type ();

void gnome_cmd_xfer_progress_win_set_total_progress (GnomeCmdXferProgressWin *win,
                                                     GnomeVFSFileSize file_bytes_copied,
                                                     GnomeVFSFileSize file_size,
                                                     GnomeVFSFileSize bytes_copied,
                                                     GnomeVFSFileSize bytes_total);

void gnome_cmd_xfer_progress_win_set_msg (GnomeCmdXferProgressWin *win, const gchar *string);

void gnome_cmd_xfer_progress_win_set_action (GnomeCmdXferProgressWin *win, const gchar *string);

#endif // __GNOME_CMD_XFER_PROGRESS_WIN_H__
