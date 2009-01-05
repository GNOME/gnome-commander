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

#ifndef __CVS_PLUGIN_H__
#define __CVS_PLUGIN_H__

#define CVS_PLUGIN(obj) \
    GTK_CHECK_CAST (obj, cvs_plugin_get_type (), CvsPlugin)
#define CVS_PLUGIN_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, cvs_plugin_get_type (), CvsPluginClass)
#define IS_CVS_PLUGIN(obj) \
    GTK_CHECK_TYPE (obj, cvs_plugin_get_type ())

typedef struct _CvsPlugin CvsPlugin;
typedef struct _CvsPluginClass CvsPluginClass;
typedef struct _CvsPluginPrivate CvsPluginPrivate;

#include <libgcmd/libgcmd.h>
#include "parser.h"

G_BEGIN_DECLS

struct _CvsPlugin
{
    GnomeCmdPlugin parent;

    Revision *selected_rev;

    GtkWidget *diff_win;
    GtkWidget *log_win;

    gint compression_level;
    gboolean unidiff;

    CvsPluginPrivate *priv;
};

struct _CvsPluginClass
{
    GnomeCmdPluginClass parent_class;
};


GtkType cvs_plugin_get_type ();

GnomeCmdPlugin *cvs_plugin_new ();

G_END_DECLS

#endif //__CVS_PLUGIN_H__
