/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2003 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 
#ifndef __GNOME_CMD_FILE_COLLECTION_H__
#define __GNOME_CMD_FILE_COLLECTION_H__


#include "gnome-cmd-file.h"

#define GNOME_CMD_FILE_COLLECTION(obj) \
	GTK_CHECK_CAST (obj, gnome_cmd_file_collection_get_type (), GnomeCmdFileCollection)
#define GNOME_CMD_FILE_COLLECTION_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, gnome_cmd_file_collection_get_type (), GnomeCmdFileCollectionClass)
#define GNOME_CMD_IS_FILE_COLLECTION(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_file_collection_get_type ())
#define GNOME_CMD_FILE_COLLECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_CMD_FILE_COLLECTION, GnomeCmdFileCollectionClass))


typedef struct _GnomeCmdFileCollection GnomeCmdFileCollection;
typedef struct _GnomeCmdFileCollectionClass GnomeCmdFileCollectionClass;
typedef struct _GnomeCmdFileCollectionPrivate GnomeCmdFileCollectionPrivate;


struct _GnomeCmdFileCollection
{
	GtkObject parent;
	
	GnomeCmdFileCollectionPrivate *priv;
};

struct _GnomeCmdFileCollectionClass
{
	GtkObjectClass parent_class;
};


GtkType
gnome_cmd_file_collection_get_type (void);

GnomeCmdFileCollection *
gnome_cmd_file_collection_new (void);

void
gnome_cmd_file_collection_add (GnomeCmdFileCollection *collection,
							   GnomeCmdFile *file);

void
gnome_cmd_file_collection_add_list (GnomeCmdFileCollection *collection,
									GList *files);

void
gnome_cmd_file_collection_remove (GnomeCmdFileCollection *collection,
								  GnomeCmdFile *file);

GnomeCmdFile *
gnome_cmd_file_collection_lookup (GnomeCmdFileCollection *collection,
								  const gchar *uri_str);

gint
gnome_cmd_file_collection_get_size (GnomeCmdFileCollection *collection);

void
gnome_cmd_file_collection_clear (GnomeCmdFileCollection *collection);

GList *
gnome_cmd_file_collection_get_list (GnomeCmdFileCollection *collection);


#endif //__GNOME_CMD_FILE_COLLECTION_H__
