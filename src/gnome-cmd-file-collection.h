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

#ifndef __GNOME_CMD_FILE_COLLECTION_H__
#define __GNOME_CMD_FILE_COLLECTION_H__


#include "gnome-cmd-file.h"


class GnomeCmdFileCollection
{
    GHashTable *map;
    GList *list;

  public:

    GnomeCmdFileCollection();
    ~GnomeCmdFileCollection();

    guint size()        {  return g_list_length (list);  }
    gboolean empty()    {  return list==NULL;            }
    void clear();

    void add(GnomeCmdFile *file);
    void add(GList *files);
    gboolean remove(GnomeCmdFile *file);
    gboolean remove(const gchar *uri_str);

    GList *get_list()   {  return list;  }

    GnomeCmdFile *find(const gchar *uri_str);

    GList *sort(GCompareDataFunc compare_func, gpointer user_data);
};


inline GnomeCmdFileCollection::GnomeCmdFileCollection()
{
    map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gnome_cmd_file_unref);
    list = NULL;
}


inline GnomeCmdFileCollection::~GnomeCmdFileCollection()
{
    g_hash_table_destroy (map);
    g_list_free (list);
}


inline void GnomeCmdFileCollection::add(GList *files)
{
    for (; files; files = files->next)
        add(GNOME_CMD_FILE (files->data));
}

#endif // __GNOME_CMD_FILE_COLLECTION_H__
