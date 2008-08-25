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

#ifndef __GNOME_CMD_FILE_COLLECTION_H__
#define __GNOME_CMD_FILE_COLLECTION_H__


#include "gnome-cmd-file.h"

#define GNOME_CMD_TYPE_FILE_COLLECTION      (gnome_cmd_file_collection_get_type ())
#define GNOME_CMD_FILE_COLLECTION(obj)      GTK_CHECK_CAST (obj, GNOME_CMD_TYPE_FILE_COLLECTION, GnomeCmdFileCollection)
#define GNOME_CMD_IS_FILE_COLLECTION(obj)   GTK_CHECK_TYPE (obj, GNOME_CMD_TYPE_FILE_COLLECTION)


GtkType gnome_cmd_file_collection_get_type ();


struct GnomeCmdFileCollection
{
    GtkObject parent;

    class Private;

    Private *priv;

    operator GtkObject * ()             {  return GTK_OBJECT (this);    }

    GList *get_list();

    guint size()        {  return g_list_length (get_list());  }
    gboolean empty()    {  return get_list()==NULL;            }
    void clear();

    void add(GnomeCmdFile *file);
    void add(GList *files);
    void remove(GnomeCmdFile *file);
    void remove(const gchar *uri_str);

    GnomeCmdFile *find(const gchar *uri_str);

    GList *sort(GCompareDataFunc compare_func, gpointer user_data);
};


inline void GnomeCmdFileCollection::add(GList *files)
{
    for (; files; files = files->next)
        add(GNOME_CMD_FILE (files->data));
}


inline GnomeCmdFileCollection *gnome_cmd_file_collection_new ()
{
    return (GnomeCmdFileCollection *) gtk_type_new (GNOME_CMD_TYPE_FILE_COLLECTION);
}

#endif // __GNOME_CMD_FILE_COLLECTION_H__
