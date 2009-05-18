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

#ifndef __GNOME_CMD_ADVRENAME_DIALOG_H__
#define __GNOME_CMD_ADVRENAME_DIALOG_H__

#include "gnome-cmd-data.h"
#include "gnome-cmd-file-list.h"

#define GNOME_CMD_TYPE_ADVRENAME_DIALOG         (gnome_cmd_advrename_dialog_get_type())
#define GNOME_CMD_ADVRENAME_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_ADVRENAME_DIALOG, GnomeCmdAdvrenameDialog))
#define GNOME_CMD_ADVRENAME_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_CMD_TYPE_ADVRENAME_DIALOG, GnomeCmdAdvrenameDialogClass))
#define GNOME_CMD_IS_ADVRENAME_DIALOG(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), GNOME_CMD_TYPE_ADVRENAME_DIALOG))


GType gnome_cmd_advrename_dialog_get_type ();


struct GnomeCmdAdvrenameDialog
{
    GtkDialog parent;

    class Private;

    Private *priv;

    operator GtkWidget * ()             {  return GTK_WIDGET (this);  }
    operator GtkWindow * ()             {  return GTK_WINDOW (this);  }
    operator GtkDialog * ()             {  return GTK_DIALOG (this);  }

    void *operator new (size_t size)    {  return g_object_new (GNOME_CMD_TYPE_ADVRENAME_DIALOG, NULL);  }
    void operator delete (void *p)      {  g_object_unref (p);  }

    enum {GCMD_RESPONSE_PROFILES=123, GCMD_RESPONSE_RESET};

    enum {COL_FILE, COL_NAME, COL_NEW_NAME, COL_SIZE, COL_DATE, COL_RENAME_FAILED, NUM_FILE_COLS};

    GnomeCmdData::AdvrenameConfig &defaults;

    GtkTreeModel *files;

    GnomeCmdAdvrenameDialog(GnomeCmdData::AdvrenameConfig &defaults);
    ~GnomeCmdAdvrenameDialog();

    void set(GList *files);
    void unset();
    void update_new_filenames();
};

#endif // __GNOME_CMD_ADVRENAME_DIALOG_H__
