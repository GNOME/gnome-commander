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

#ifndef __GNOME_CMD_STRING_DIALOG_H__
#define __GNOME_CMD_STRING_DIALOG_H__

G_BEGIN_DECLS

#define GNOME_CMD_STRING_DIALOG(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_string_dialog_get_type (), GnomeCmdStringDialog)
#define GNOME_CMD_STRING_DIALOG_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_string_dialog_get_type (), GnomeCmdStringDialogClass)
#define GNOME_CMD_IS_STRING_DIALOG(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_string_dialog_get_type ())


typedef struct _GnomeCmdStringDialog GnomeCmdStringDialog;
typedef struct _GnomeCmdStringDialogPrivate GnomeCmdStringDialogPrivate;
typedef struct _GnomeCmdStringDialogClass GnomeCmdStringDialogClass;



struct _GnomeCmdStringDialog
{
    GnomeCmdDialog parent;

    gint rows;
    GtkWidget **labels;
    GtkWidget **entries;

    GnomeCmdStringDialogPrivate *priv;
};


struct _GnomeCmdStringDialogClass
{
    GnomeCmdDialogClass parent_class;
};


typedef gboolean (*GnomeCmdStringDialogCallback)(GnomeCmdStringDialog *dialog,
                                                 const gchar **values,
                                                 gpointer user_data);


GtkWidget*
gnome_cmd_string_dialog_new_with_cancel (const gchar *title,
                                         const gchar **labels,
                                         gint rows,
                                         GnomeCmdStringDialogCallback ok_cb,
                                         GtkSignalFunc cancel_cb,
                                         gpointer user_data);

GtkWidget*
gnome_cmd_string_dialog_new (const gchar *title,
                             const gchar **labels,
                             gint rows,
                             GnomeCmdStringDialogCallback ok_cb,
                             gpointer user_data);

void
gnome_cmd_string_dialog_setup_with_cancel (GnomeCmdStringDialog *dialog,
                                           const gchar *title,
                                           const gchar **labels,
                                           gint rows,
                                           GnomeCmdStringDialogCallback ok_cb,
                                           GtkSignalFunc cancel_cb,
                                           gpointer user_data);

void
gnome_cmd_string_dialog_setup (GnomeCmdStringDialog *dialog,
                               const gchar *title,
                               const gchar **labels,
                               gint rows,
                               GnomeCmdStringDialogCallback ok_cb,
                               gpointer user_data);

GtkType gnome_cmd_string_dialog_get_type ();

void gnome_cmd_string_dialog_set_hidden (GnomeCmdStringDialog *dialog, gint row, gboolean hidden);

void gnome_cmd_string_dialog_set_title (GnomeCmdStringDialog *dialog, const gchar *title);

void gnome_cmd_string_dialog_set_label (GnomeCmdStringDialog *dialog, gint row, const gchar *label);

void gnome_cmd_string_dialog_set_userdata (GnomeCmdStringDialog *dialog, gpointer user_data);

void gnome_cmd_string_dialog_set_ok_cb (GnomeCmdStringDialog *dialog, GnomeCmdStringDialogCallback ok_cb);

void gnome_cmd_string_dialog_set_cancel_cb (GnomeCmdStringDialog *dialog, GtkSignalFunc cancel_cb);

void gnome_cmd_string_dialog_set_value (GnomeCmdStringDialog *dialog, gint row, const gchar *value);

void gnome_cmd_string_dialog_set_error_desc (GnomeCmdStringDialog *dialog, gchar *msg);


G_END_DECLS

#endif //__GNOME_CMD_STRING_DIALOG_H__


