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

#ifndef __GNOME_CMD_DIALOG_H__
#define __GNOME_CMD_DIALOG_H__

G_BEGIN_DECLS

#define GNOME_CMD_DIALOG(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_dialog_get_type (), GnomeCmdDialog)
#define GNOME_CMD_DIALOG_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_dialog_get_type (), GnomeCmdDialogClass)
#define GNOME_CMD_IS_DIALOG(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_dialog_get_type ())


typedef struct _GnomeCmdDialog GnomeCmdDialog;
typedef struct _GnomeCmdDialogPrivate GnomeCmdDialogPrivate;
typedef struct _GnomeCmdDialogClass GnomeCmdDialogClass;



struct _GnomeCmdDialog
{
    GtkWindow parent;

    GList *buttons;

    GnomeCmdDialogPrivate *priv;
};


struct _GnomeCmdDialogClass
{
    GtkWindowClass parent_class;
};


GtkWidget *gnome_cmd_dialog_new (const gchar *title);

void gnome_cmd_dialog_setup (GnomeCmdDialog *dialog, const gchar *title);

GtkType gnome_cmd_dialog_get_type ();

GtkWidget *gnome_cmd_dialog_add_button (GnomeCmdDialog *dialog, const gchar *stock_id, GtkSignalFunc on_click, gpointer data);

void gnome_cmd_dialog_add_category (GnomeCmdDialog *dialog, GtkWidget *category);

void gnome_cmd_dialog_add_expanding_category (GnomeCmdDialog *dialog, GtkWidget *category);

void gnome_cmd_dialog_editable_enters (GnomeCmdDialog *dialog, GtkEditable *editable);

void gnome_cmd_dialog_set_transient_for (GnomeCmdDialog *dialog, GtkWindow *win);

void gnome_cmd_dialog_set_modal (GnomeCmdDialog *dialog);

void gnome_cmd_dialog_set_resizable (GnomeCmdDialog *dialog, gboolean value);

G_END_DECLS

#endif //__GNOME_CMD_DIALOG_H__


