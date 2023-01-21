/** 
 * @file gnome-cmd-app.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-app.h"
#include "imageloader.h"

using namespace std;


GnomeCmdApp *gnome_cmd_app_new ()
{
    GnomeCmdApp *app = g_new0 (GnomeCmdApp, 1);
    // app->name = nullptr;
    // app->cmd = nullptr;
    // app->icon_path = nullptr;
    // app->pixmap = nullptr;
    app->target = APP_TARGET_ALL_FILES;
    // app->pattern_string = nullptr;
    // app->pattern_list = nullptr;
    // app->handles_uris = FALSE;
    // app->handles_multiple = FALSE;
    // app->requires_terminal = FALSE;

    return app;
}


GnomeCmdApp *gnome_cmd_app_new_with_values (const gchar *name,
                                            const gchar *cmd,
                                            const gchar *icon_path,
                                            AppTarget target,
                                            const gchar *pattern_string,
                                            gboolean handles_uris,
                                            gboolean handles_multiple,
                                            gboolean requires_terminal,
                                            GAppInfo *gAppInfo)
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
    app->gAppInfo = gAppInfo;

    return app;
}


static char* panel_find_icon (GtkIconTheme *icon_theme, const char *icon_name, gint size)
{
    char *retval  = nullptr;
    GtkIconInfo *icon_info = nullptr;
    char *icon_no_extension;
    char *p;

    if (icon_name == nullptr || strcmp (icon_name, "") == 0)
        return nullptr;

    if (g_path_is_absolute (icon_name)) {
        if (g_file_test (icon_name, G_FILE_TEST_EXISTS)) {
            return g_strdup (icon_name);
        }
        else
        {
            char *basename;

            basename = g_path_get_basename (icon_name);
            retval = panel_find_icon (icon_theme, basename, size);
            g_free (basename);

            return retval;
        }
    }

    /* This is needed because some .desktop files have an icon name *and*
     * an extension as icon */
    icon_no_extension = g_strdup (icon_name);
    p = strrchr (icon_no_extension, '.');
    if (p &&
        (strcmp (p, ".png") == 0 ||
         strcmp (p, ".xpm") == 0 ||
         strcmp (p, ".svg") == 0)) {
        *p = 0;
    }

    icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_no_extension, size, (GtkIconLookupFlags) 0);
    if (!icon_info)
        return nullptr;
    retval = g_strdup (gtk_icon_info_get_filename (icon_info));

    g_free (icon_no_extension);
    gtk_icon_info_free (icon_info);

    return retval;
}


GnomeCmdApp *gnome_cmd_app_new_from_app_info (GAppInfo *gAppInfo)
{
    g_return_val_if_fail (gAppInfo != nullptr, nullptr);

    return gnome_cmd_app_new_with_values (g_app_info_get_name (gAppInfo),
                                          g_app_info_get_commandline (gAppInfo),
                                          get_default_application_icon_path(gAppInfo),
                                          APP_TARGET_ALL_FILES,
                                          nullptr,
                                          g_app_info_supports_uris (gAppInfo),
                                          true,
                                          false,
                                          gAppInfo);
}


GnomeCmdApp *gnome_cmd_app_dup (GnomeCmdApp *app)
{
    return gnome_cmd_app_new_with_values (app->name,
                                          app->cmd,
                                          app->icon_path,
                                          app->target,
                                          app->pattern_string,
                                          app->handles_uris,
                                          app->handles_multiple,
                                          app->requires_terminal,
                                          app->gAppInfo);
}


void gnome_cmd_app_free (GnomeCmdApp *app)
{
    g_return_if_fail (app != nullptr);

    g_free (app->name);
    g_free (app->cmd);
    g_free (app->icon_path);
    gnome_cmd_pixmap_free (app->pixmap);

    g_free (app);
}


void gnome_cmd_app_set_name (GnomeCmdApp *app, const gchar *name)
{
    g_return_if_fail (app != nullptr);
    g_return_if_fail (name != nullptr);

    g_free (app->name);

    app->name = g_strdup (name);
}


void gnome_cmd_app_set_command (GnomeCmdApp *app, const gchar *cmd)
{
    g_return_if_fail (app != nullptr);

    if (!cmd) return;

    g_free (app->cmd);

    app->cmd = g_strdup (cmd);
}


void gnome_cmd_app_set_icon_path (GnomeCmdApp *app, const gchar *icon_path)
{
    g_return_if_fail (app != nullptr);

    if (!icon_path) return;

    g_free (app->icon_path);

    gnome_cmd_pixmap_free (app->pixmap);

    app->icon_path = g_strdup (icon_path);

    //FIXME: Check GError here
    GdkPixbuf *tmp = gdk_pixbuf_new_from_file (icon_path, nullptr);

    if (tmp)
    {
        GdkPixbuf *pixbuf = gdk_pixbuf_scale_simple (tmp, 16, 16, GDK_INTERP_HYPER);

        if (pixbuf)
            app->pixmap = gnome_cmd_pixmap_new_from_pixbuf (pixbuf);

        g_object_unref (tmp);
    }
}


void gnome_cmd_app_set_target (GnomeCmdApp *app, AppTarget target)
{
    g_return_if_fail (app != nullptr);

    app->target = target;
}


void gnome_cmd_app_set_pattern_string (GnomeCmdApp *app, const gchar *pattern_string)
{
    g_return_if_fail (app != nullptr);
    g_return_if_fail (pattern_string != nullptr);

    if (app->pattern_string)
        g_free (app->pattern_string);

    app->pattern_string = g_strdup (pattern_string);

    // Free old list with patterns
    g_list_foreach (app->pattern_list, (GFunc) g_free, nullptr);
    g_list_free (app->pattern_list);
    app->pattern_list = nullptr;

    // Create the new one
    gchar **ents = g_strsplit (pattern_string, ";", 0);
    for (gint i=0; ents[i]; ++i)
        app->pattern_list = g_list_append (app->pattern_list, ents[i]);

    g_free (ents);
}


void gnome_cmd_app_set_handles_uris (GnomeCmdApp *app, gboolean handles_uris)
{
    app->handles_uris = handles_uris;
}


void gnome_cmd_app_set_handles_multiple (GnomeCmdApp *app, gboolean handles_multiple)
{
    app->handles_multiple = handles_multiple;
}


void gnome_cmd_app_set_requires_terminal (GnomeCmdApp *app, gboolean requires_terminal)
{
    app->requires_terminal = requires_terminal;
}


const gchar *gnome_cmd_app_get_name (GnomeCmdApp *app)
{
    g_return_val_if_fail (app != nullptr, nullptr);

    return app->name;
}


const gchar *gnome_cmd_app_get_command (GnomeCmdApp *app)
{
    g_return_val_if_fail (app != nullptr, nullptr);

    return app->cmd;
}


const gchar *gnome_cmd_app_get_icon_path (GnomeCmdApp *app)
{
    g_return_val_if_fail (app != nullptr, nullptr);

    return app->icon_path;
}


GnomeCmdPixmap *gnome_cmd_app_get_pixmap (GnomeCmdApp *app)
{
    return app->pixmap;
}


AppTarget gnome_cmd_app_get_target (GnomeCmdApp *app)
{
    return app->target;
}


const gchar *gnome_cmd_app_get_pattern_string (GnomeCmdApp *app)
{
    return app->pattern_string;
}


GList *gnome_cmd_app_get_pattern_list (GnomeCmdApp *app)
{
    return app->pattern_list;
}


gboolean gnome_cmd_app_get_handles_uris (GnomeCmdApp *app)
{
    return app->handles_uris;
}


gboolean gnome_cmd_app_get_handles_multiple (GnomeCmdApp *app)
{
    return app->handles_multiple;
}


gboolean gnome_cmd_app_get_requires_terminal (GnomeCmdApp *app)
{
    return app->requires_terminal;
}
