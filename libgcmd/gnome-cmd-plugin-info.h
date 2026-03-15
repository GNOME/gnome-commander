/*
 * SPDX-FileCopyrightText: 2003 Marcus Bjurman
 * SPDX-FileCopyrightText: 2005 Piotr Eljasiak
 * SPDX-FileCopyrightText: 2014 Uwe Scholz <u.scholz83@gmx.de>
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
