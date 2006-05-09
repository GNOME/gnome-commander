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
#ifndef __GNOME_CMD_DIR_POOL_H__
#define __GNOME_CMD_DIR_POOL_H__

#define GNOME_CMD_DIR_POOL(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_dir_pool_get_type (), GnomeCmdDirPool)
#define GNOME_CMD_DIR_POOL_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_dir_pool_get_type (), GnomeCmdDirPoolClass)
#define GNOME_CMD_IS_DIR_POOL(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_dir_pool_get_type ())


typedef struct _GnomeCmdDirPool GnomeCmdDirPool;
typedef struct _GnomeCmdDirPoolClass GnomeCmdDirPoolClass;
typedef struct _GnomeCmdDirPoolPrivate GnomeCmdDirPoolPrivate;

#include "gnome-cmd-dir.h"


struct _GnomeCmdDirPool
{
    GtkObject parent;

    GnomeCmdDirPoolPrivate *priv;
};

struct _GnomeCmdDirPoolClass
{
    GtkObjectClass parent_class;
};


GtkType
gnome_cmd_dir_pool_get_type (void);

GnomeCmdDirPool *
gnome_cmd_dir_pool_new (void);

GnomeCmdDir *
gnome_cmd_dir_pool_get (GnomeCmdDirPool *pool, const gchar *path);

void
gnome_cmd_dir_pool_add (GnomeCmdDirPool *pool, GnomeCmdDir *dir);

void
gnome_cmd_dir_pool_remove (GnomeCmdDirPool *pool, GnomeCmdDir *dir);

void
gnome_cmd_dir_pool_show_state (GnomeCmdDirPool *pool);

#endif //__GNOME_CMD_DIR_POOL_H__
