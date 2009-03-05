/* Ripped from libgnomeui and adjusted to my needs. Original comments left intact below.

    Copyright (C) 2003-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak
*/

 /* gnome-about.h - An about box widget for gnome.

   Copyright (C) 2001 CodeFactory AB
   Copyright (C) 2001 Anders Carlsson <andersca@codefactory.se>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Anders Carlsson <andersca@codefactory.se>
*/

#ifndef __GNOME_CMD_ABOUT_PLUGIN_H__
#define __GNOME_CMD_ABOUT_PLUGIN_H__

#include <gtk/gtkdialog.h>

#define GNOME_CMD_TYPE_ABOUT_PLUGIN            (gnome_cmd_about_plugin_get_type ())
#define GNOME_CMD_ABOUT_PLUGIN(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GNOME_CMD_TYPE_ABOUT_PLUGIN, GnomeCmdAboutPlugin))
#define GNOME_CMD_ABOUT_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_ABOUT, GnomeCmdAboutPluginClass))
#define GNOME_CMD_IS_ABOUT_PLUGIN(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GNOME_CMD_TYPE_ABOUT_PLUGIN))
#define GNOME_CMD_IS_ABOUT_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_ABOUT_PLUGIN))
#define GNOME_CMD_ABOUT_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_CMD_TYPE_ABOUT_PLUGIN, GnomeCmdAboutPluginClass))

struct GnomeCmdAboutPluginPrivate;

struct GnomeCmdAboutPlugin
{
    GtkDialog parent;

    GnomeCmdAboutPluginPrivate *priv;
};

struct GnomeCmdAboutPluginClass
{
    GtkDialogClass parent_class;
};

GType gnome_cmd_about_plugin_get_type (void) G_GNUC_CONST;

GtkWidget *gnome_cmd_about_plugin_new (PluginInfo *info);

#endif // __GNOME_CMD_ABOUT_PLUGIN_H__
