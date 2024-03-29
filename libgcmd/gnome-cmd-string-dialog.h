/** 
 * @file gnome-cmd-string-dialog.h
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

#define GNOME_CMD_TYPE_STRING_DIALOG              (gnome_cmd_string_dialog_get_type ())
#define GNOME_CMD_STRING_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_STRING_DIALOG, GnomeCmdStringDialog))
#define GNOME_CMD_STRING_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_STRING_DIALOG, GnomeCmdStringDialogClass))
#define GNOME_CMD_IS_STRING_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_STRING_DIALOG))
#define GNOME_CMD_IS_STRING_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_STRING_DIALOG))
#define GNOME_CMD_STRING_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_STRING_DIALOG, GnomeCmdStringDialogClass))


struct GnomeCmdStringDialog
{
    GnomeCmdDialog parent;

    struct Private;

    Private *priv;

    gint rows;
    GtkWidget **labels;
    GtkWidget **entries;
};


struct GnomeCmdStringDialogClass
{
    GnomeCmdDialogClass parent_class;
};


typedef gboolean (*GnomeCmdStringDialogCallback) (GnomeCmdStringDialog *dialog, const gchar **values, gpointer user_data);


GtkWidget *gnome_cmd_string_dialog_new_with_cancel (const gchar *title, const gchar **labels, gint rows, GnomeCmdStringDialogCallback ok_cb, GnomeCmdCallback<GtkButton*> cancel_cb, gpointer user_data);

inline GtkWidget *gnome_cmd_string_dialog_new (const gchar *title, const gchar **labels, gint rows, GnomeCmdStringDialogCallback ok_cb, gpointer user_data)
{
    return gnome_cmd_string_dialog_new_with_cancel (title, labels, rows, ok_cb, NULL, user_data);
}

void gnome_cmd_string_dialog_setup_with_cancel (GnomeCmdStringDialog *dialog, const gchar *title, const gchar **labels, gint rows, GnomeCmdStringDialogCallback ok_cb, GnomeCmdCallback<GtkButton*> cancel_cb, gpointer user_data);

inline void gnome_cmd_string_dialog_setup (GnomeCmdStringDialog *dialog, const gchar *title, const gchar **labels, gint rows, GnomeCmdStringDialogCallback ok_cb, gpointer user_data)
{
    gnome_cmd_string_dialog_setup_with_cancel (dialog, title, labels, rows, ok_cb, NULL, user_data);
}

GType gnome_cmd_string_dialog_get_type ();

void gnome_cmd_string_dialog_set_title (GnomeCmdStringDialog *dialog, const gchar *title);

void gnome_cmd_string_dialog_set_label (GnomeCmdStringDialog *dialog, gint row, const gchar *label);

void gnome_cmd_string_dialog_set_userdata (GnomeCmdStringDialog *dialog, gpointer user_data);

void gnome_cmd_string_dialog_set_ok_cb (GnomeCmdStringDialog *dialog, GnomeCmdStringDialogCallback ok_cb);

void gnome_cmd_string_dialog_set_cancel_cb (GnomeCmdStringDialog *dialog, GnomeCmdCallback<GtkButton*> cancel_cb);

void gnome_cmd_string_dialog_set_value (GnomeCmdStringDialog *dialog, gint row, const gchar *value);

void gnome_cmd_string_dialog_set_error_desc (GnomeCmdStringDialog *dialog, gchar *msg);
