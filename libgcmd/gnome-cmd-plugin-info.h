/*
 * Copyright (C) 2001-2006 Marcus Bjurman
 * Copyright (C) 2007-2012 Piotr Eljasiak
 * Copyright (C) 2013-2024 Uwe Scholz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <glib.h>


/* This one should be increased when an api-incompatible change
 * is done to the plugin system. By doing that gcmd can detect
 * old plugins and avoid loading them.
 */
#define GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION 1


/* This struct must never ever be changed
 * since it is needed to determinate the version of a plugin
 */
typedef struct GnomeCmdPluginInfo
{
    gint plugin_system_version;

    const gchar *name;
    const gchar *version;
    const gchar *copyright;
    const gchar *comments;
    gchar **authors;
    gchar **documenters;
    const gchar *translator;
    const gchar *webpage;
} GnomeCmdPluginInfo;


/* This function prototype must never ever be changed
 * since it is needed to determinate the version of a plugin
 */
typedef GnomeCmdPluginInfo *(*GnomeCmdPluginInfoFunc)(void);
