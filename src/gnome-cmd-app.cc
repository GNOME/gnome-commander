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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-app.h"

using namespace std;


struct _GnomeCmdAppPrivate
{
    gchar *name;
    gchar *cmd;
    gchar *icon_path;

    AppTarget target;
    gchar *pattern_string;
    GList *pattern_list;
    gboolean handles_uris;
    gboolean handles_multiple;
    gboolean requires_terminal;

    GnomeCmdPixmap *pixmap;
};


GnomeCmdApp *gnome_cmd_app_new ()
{
    GnomeCmdApp *app = g_new0 (GnomeCmdApp, 1);
    app->priv = g_new0 (GnomeCmdAppPrivate, 1);
    app->priv->name = NULL;
    app->priv->cmd = NULL;
    app->priv->icon_path = NULL;
    app->priv->pixmap = NULL;
    app->priv->target = APP_TARGET_ALL_FILES;
    app->priv->pattern_string = NULL;
    app->priv->pattern_list = NULL;
    app->priv->handles_uris = FALSE;
    app->priv->handles_multiple = FALSE;
    app->priv->requires_terminal = FALSE;

    return app;
}


GnomeCmdApp *gnome_cmd_app_new_with_values (const gchar *name,
                                            const gchar *cmd,
                                            const gchar *icon_path,
                                            AppTarget target,
                                            const gchar *pattern_string,
                                            gboolean handles_uris,
                                            gboolean handles_multiple,
                                            gboolean requires_terminal)
{
    GnomeCmdApp *app = gnome_cmd_app_new ();

    gnome_cmd_app_set_name (app, name);
    gnome_cmd_app_set_command (app, cmd);
    gnome_cmd_app_set_icon_path (app, icon_path);
    if (pattern_string)
        gnome_cmd_app_set_pattern_string (app, pattern_string);
    gnome_cmd_app_set_target (app, target);
    gnome_cmd_app_set_handles_uris (app, handles_uris);
    gnome_cmd_app_set_handles_multiple (app, handles_multiple);
    gnome_cmd_app_set_requires_terminal (app, requires_terminal);

    return app;
}


GnomeCmdApp *gnome_cmd_app_new_from_vfs_app (GnomeVFSMimeApplication *vfs_app)
{
    g_return_val_if_fail (vfs_app != NULL, NULL);

    return gnome_cmd_app_new_with_values (vfs_app->name,
                                          vfs_app->command,
                                          NULL,
                                          APP_TARGET_ALL_FILES,
                                          NULL,
                                          vfs_app->expects_uris == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS,
                                          vfs_app->can_open_multiple_files,
                                          vfs_app->requires_terminal);
}


GnomeCmdApp *gnome_cmd_app_dup (GnomeCmdApp *app)
{
    return gnome_cmd_app_new_with_values (app->priv->name,
                                          app->priv->cmd,
                                          app->priv->icon_path,
                                          app->priv->target,
                                          app->priv->pattern_string,
                                          app->priv->handles_uris,
                                          app->priv->handles_multiple,
                                          app->priv->requires_terminal);
}


void gnome_cmd_app_free (GnomeCmdApp *app)
{
    g_return_if_fail (app != NULL);
    g_return_if_fail (app->priv != NULL);

    g_free (app->priv->name);
    g_free (app->priv->cmd);
    g_free (app->priv->icon_path);
    gnome_cmd_pixmap_free (app->priv->pixmap);

    g_free (app->priv);
    g_free (app);
}


void gnome_cmd_app_set_name (GnomeCmdApp *app, const gchar *name)
{
    g_return_if_fail (app != NULL);
    g_return_if_fail (app->priv != NULL);
    g_return_if_fail (name != NULL);

    g_free (app->priv->name);

    app->priv->name = g_strdup (name);
}


void gnome_cmd_app_set_command (GnomeCmdApp *app, const gchar *cmd)
{
    g_return_if_fail (app != NULL);
    g_return_if_fail (app->priv != NULL);

    if (!cmd) return;

    g_free (app->priv->cmd);

    app->priv->cmd = g_strdup (cmd);
}


void gnome_cmd_app_set_icon_path (GnomeCmdApp *app, const gchar *icon_path)
{
    g_return_if_fail (app != NULL);
    g_return_if_fail (app->priv != NULL);

    if (!icon_path) return;

    g_free (app->priv->icon_path);

    gnome_cmd_pixmap_free (app->priv->pixmap);

    app->priv->icon_path = g_strdup (icon_path);

    //FIXME: Check GError here
    GdkPixbuf *tmp = gdk_pixbuf_new_from_file (icon_path, NULL);

    if (tmp)
    {
        GdkPixbuf *pixbuf = gdk_pixbuf_scale_simple (tmp, 16, 16, GDK_INTERP_HYPER);

        if (pixbuf)
            app->priv->pixmap = gnome_cmd_pixmap_new_from_pixbuf (pixbuf);

        gdk_pixbuf_unref (tmp);
    }
}


void gnome_cmd_app_set_target (GnomeCmdApp *app, AppTarget target)
{
    g_return_if_fail (app != NULL);
    g_return_if_fail (app->priv != NULL);

    app->priv->target = target;
}


void gnome_cmd_app_set_pattern_string (GnomeCmdApp *app, const gchar *pattern_string)
{
    g_return_if_fail (app != NULL);
    g_return_if_fail (app->priv != NULL);
    g_return_if_fail (pattern_string != NULL);

    if (app->priv->pattern_string)
        g_free (app->priv->pattern_string);

    app->priv->pattern_string = g_strdup (pattern_string);

    // Free old list with patterns
    g_list_foreach (app->priv->pattern_list, (GFunc) g_free, NULL);
    g_list_free (app->priv->pattern_list);
    app->priv->pattern_list = NULL;

    // Create the new one
    gchar **ents = g_strsplit (pattern_string, ";", 0);
    for (gint i=0; ents[i]; ++i)
        app->priv->pattern_list = g_list_append (app->priv->pattern_list, ents[i]);

    g_free (ents);
}


void gnome_cmd_app_set_handles_uris (GnomeCmdApp *app, gboolean handles_uris)
{
    app->priv->handles_uris = handles_uris;
}


void gnome_cmd_app_set_handles_multiple (GnomeCmdApp *app, gboolean handles_multiple)
{
    app->priv->handles_multiple = handles_multiple;
}


void gnome_cmd_app_set_requires_terminal (GnomeCmdApp *app, gboolean requires_terminal)
{
    app->priv->requires_terminal = requires_terminal;
}


const gchar *gnome_cmd_app_get_name (GnomeCmdApp *app)
{
    g_return_val_if_fail (app != NULL, NULL);
    g_return_val_if_fail (app->priv != NULL, NULL);

    return app->priv->name;
}


const gchar *gnome_cmd_app_get_command (GnomeCmdApp *app)
{
    g_return_val_if_fail (app != NULL, NULL);
    g_return_val_if_fail (app->priv != NULL, NULL);

    return app->priv->cmd;
}


const gchar *gnome_cmd_app_get_icon_path (GnomeCmdApp *app)
{
    g_return_val_if_fail (app != NULL, NULL);
    g_return_val_if_fail (app->priv != NULL, NULL);

    return app->priv->icon_path;
}


GnomeCmdPixmap *gnome_cmd_app_get_pixmap (GnomeCmdApp *app)
{
    return app->priv->pixmap;
}


AppTarget gnome_cmd_app_get_target (GnomeCmdApp *app)
{
    return app->priv->target;
}


const gchar *gnome_cmd_app_get_pattern_string (GnomeCmdApp *app)
{
    return app->priv->pattern_string;
}


GList *gnome_cmd_app_get_pattern_list (GnomeCmdApp *app)
{
    return app->priv->pattern_list;
}


gboolean gnome_cmd_app_get_handles_uris (GnomeCmdApp *app)
{
    return app->priv->handles_uris;
}


gboolean gnome_cmd_app_get_handles_multiple (GnomeCmdApp *app)
{
    return app->priv->handles_multiple;
}


gboolean gnome_cmd_app_get_requires_terminal (GnomeCmdApp *app)
{
    return app->priv->requires_terminal;
}
