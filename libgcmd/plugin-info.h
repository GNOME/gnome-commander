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

#ifndef __PLUGIN_INFO_H__
#define __PLUGIN_INFO_H__

G_BEGIN_DECLS

/* This one should be increased when an api-incompatible change
 * is done to the plugin system. By doing that gcmd can detect
 * old plugins and avoid loading them.
 */
#define GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION 1


/* This struct must never ever be changed
 * since it is needed to determinate the version of a plugin
 */
typedef struct {
    gint plugin_system_version;

    gchar *name;
    gchar *version;
    gchar *copyright;
    gchar *comments;
    gchar **authors;
    gchar **documenters;
    gchar *translator;
    gchar *webpage;
} PluginInfo;


/* This function prototype must never ever be changed
 * since it is needed to determinate the version of a plugin
 */
typedef PluginInfo *(*PluginInfoFunc)(void);

G_END_DECLS

#endif //__PLUGIN_INFO_H__
