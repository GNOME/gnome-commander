/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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

#ifndef __GNOME_CMD_ADVRENAME_DIALOG_H__
#define __GNOME_CMD_ADVRENAME_DIALOG_H__

#define GNOME_CMD_ADVRENAME_DIALOG(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_advrename_dialog_get_type (), GnomeCmdAdvrenameDialog)
#define GNOME_CMD_ADVRENAME_DIALOG_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_advrename_dialog_get_type (), GnomeCmdAdvrenameDialogClass)
#define GNOME_CMD_IS_ADVRENAME_DIALOG(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_advrename_dialog_get_type ())


typedef struct _GnomeCmdAdvrenameDialog GnomeCmdAdvrenameDialog;
typedef struct _GnomeCmdAdvrenameDialogPrivate GnomeCmdAdvrenameDialogPrivate;
typedef struct _GnomeCmdAdvrenameDialogClass GnomeCmdAdvrenameDialogClass;


typedef struct
{
    gchar *from;
    gchar *to;
    gboolean case_sens;
    gboolean malformed_pattern;
} PatternEntry;


struct _GnomeCmdAdvrenameDialog
{
    GnomeCmdDialog parent;
    GnomeCmdAdvrenameDialogPrivate *priv;
};


struct _GnomeCmdAdvrenameDialogClass
{
    GnomeCmdDialogClass parent_class;
};

#define ADVRENAME_DIALOG_PAT_NUM_COLUMNS 3
#define ADVRENAME_DIALOG_RES_NUM_COLUMNS 2

extern guint advrename_dialog_default_pat_column_width[ADVRENAME_DIALOG_PAT_NUM_COLUMNS];
extern guint advrename_dialog_default_res_column_width[ADVRENAME_DIALOG_RES_NUM_COLUMNS];

GtkWidget *gnome_cmd_advrename_dialog_new (GList *files);

GtkType gnome_cmd_advrename_dialog_get_type (void);

#endif // __GNOME_CMD_ADVRENAME_DIALOG_H__
