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

#ifndef __GNOME_CMD_APP_H__
#define __GNOME_CMD_APP_H__

#include "gnome-cmd-pixmap.h"

G_BEGIN_DECLS

typedef enum {
    APP_TARGET_ALL_FILES,
    APP_TARGET_ALL_DIRS,
    APP_TARGET_ALL_DIRS_AND_FILES,
    APP_TARGET_SOME_FILES
} AppTarget;


typedef struct _GnomeCmdApp GnomeCmdApp;
typedef struct _GnomeCmdAppPrivate GnomeCmdAppPrivate;

struct _GnomeCmdApp {
    GnomeCmdAppPrivate *priv;
};

GnomeCmdApp*
gnome_cmd_app_new                (void);

GnomeCmdApp*
gnome_cmd_app_new_from_vfs_app   (GnomeVFSMimeApplication *vfs_app);

GnomeCmdApp*
gnome_cmd_app_new_with_values    (const gchar *name,
                                  const gchar *cmd,
                                  const gchar *icon_path,
                                  AppTarget target,
                                  const gchar *pattern_string,
                                  gboolean handles_uris,
                                  gboolean handles_multiple,
                                  gboolean requires_terminal);

GnomeCmdApp*
gnome_cmd_app_dup                (GnomeCmdApp *app);

void
gnome_cmd_app_free               (GnomeCmdApp *app);

void
gnome_cmd_app_set_name           (GnomeCmdApp *app,
                                  const gchar *name);

void
gnome_cmd_app_set_command        (GnomeCmdApp *app,
                                  const gchar *cmd);

void
gnome_cmd_app_set_icon_path      (GnomeCmdApp *app,
                                  const gchar *icon_path);

void
gnome_cmd_app_set_target         (GnomeCmdApp *app,
                                  AppTarget target);

void
gnome_cmd_app_set_pattern_string (GnomeCmdApp *app,
                                  const gchar *extensions);

void
gnome_cmd_app_set_handles_uris   (GnomeCmdApp *app,
                                  gboolean handles_uris);

void
gnome_cmd_app_set_handles_multiple (GnomeCmdApp *app,
                                    gboolean handles_multiple);

void
gnome_cmd_app_set_requires_terminal (GnomeCmdApp *app,
                                     gboolean requires_terminal);

const gchar*
gnome_cmd_app_get_name           (GnomeCmdApp *app);

const gchar*
gnome_cmd_app_get_command        (GnomeCmdApp *app);

const gchar*
gnome_cmd_app_get_icon_path      (GnomeCmdApp *app);

AppTarget
gnome_cmd_app_get_target         (GnomeCmdApp *app);

const gchar*
gnome_cmd_app_get_pattern_string (GnomeCmdApp *app);

GList *
gnome_cmd_app_get_pattern_list   (GnomeCmdApp *app);

gboolean
gnome_cmd_app_get_handles_uris   (GnomeCmdApp *app);

gboolean
gnome_cmd_app_get_handles_multiple (GnomeCmdApp *app);

gboolean
gnome_cmd_app_get_requires_terminal (GnomeCmdApp *app);

GnomeCmdPixmap *
gnome_cmd_app_get_pixmap         (GnomeCmdApp *app);

G_END_DECLS

#endif // __GNOME_CMD_APP_H__
