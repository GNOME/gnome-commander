/**
 * @file gnome-cmd-settings.cc
 * @brief Functions for storing settings of GCMD using GSettings
 * @copyright (C) 2013-2016 Uwe Scholz\n
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

#include <stdio.h>
#include <gio/gio.h>
#include "gnome-cmd-settings.h"

struct _GcmdSettings
{
    GObject parent;

    GSettings *interface;
    GSettings *layout;
    GSettings *history;
    GSettings *advanced_rename_tool;
    GSettings *search_tool;
    GSettings *bookmarks_tool;
    GSettings *connections;
    GSettings *bookmarks;
    GSettings *selections;
    GSettings *key_bindings;
};

G_DEFINE_TYPE (GcmdSettings, gcmd_settings, G_TYPE_OBJECT)

static void
gcmd_settings_finalize (GObject *object)
{
//    GcmdSettings *gs = GCMD_SETTINGS (object);
//
//    g_free (gs->old_scheme);
//
    G_OBJECT_CLASS (gcmd_settings_parent_class)->finalize (object);
}

static void
gcmd_settings_dispose (GObject *object)
{
    GcmdSettings *gs = GCMD_SETTINGS (object);

    g_clear_object (&gs->interface);
    g_clear_object (&gs->layout);
    g_clear_object (&gs->history);
    g_clear_object (&gs->advanced_rename_tool);
    g_clear_object (&gs->search_tool);
    g_clear_object (&gs->bookmarks_tool);
    g_clear_object (&gs->connections);
    g_clear_object (&gs->bookmarks);
    g_clear_object (&gs->selections);
    g_clear_object (&gs->key_bindings);

    G_OBJECT_CLASS (gcmd_settings_parent_class)->dispose (object);
}

static void
set_font (GcmdSettings *gs,
          const gchar *font)
{
    //Hier muss jetzt die Schrift in den Panels aktualisiert werden!
    printf("%s\n", font);
}

static void
on_system_font_changed (GSettings     *settings,
                        const gchar   *key,
                        GcmdSettings *gs)
{

    gboolean use_default_font;

    use_default_font = g_settings_get_boolean (gs->layout,
                           GCMD_SETTINGS_USE_DEFAULT_FONT);

    if (use_default_font)
    {
        gchar *font;

        font = g_settings_get_string (settings, key);
        set_font (gs, font);
        g_free (font);
    }
}

static void
on_use_default_font_changed (GSettings     *settings,
                             const gchar   *key,
                             GcmdSettings *gs)
{
    gboolean def;
    gchar *font;

    def = g_settings_get_boolean (settings, key);

    if (def)
    {
        font = g_settings_get_string (gs->interface,
                          GCMD_SETTINGS_SYSTEM_FONT);
    }
    else
    {
        font = g_settings_get_string (gs->layout,
                          GCMD_SETTINGS_PANEL_FONT);
    }

    set_font (gs, font);

    g_free (font);
}

static void
on_layout_font_changed (GSettings     *settings,
                        const gchar   *key,
                        GcmdSettings *gs)
{
    gboolean use_default_font;

    use_default_font = g_settings_get_boolean (gs->layout,
                           GCMD_SETTINGS_USE_DEFAULT_FONT);

    if (!use_default_font)
    {
        gchar *font;

        font = g_settings_get_string (settings, key);
        set_font (gs, font);
        g_free (font);
    }
}

static void
gcmd_settings_class_init (GcmdSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gcmd_settings_finalize;
    object_class->dispose = gcmd_settings_dispose;
}

GcmdSettings *
gcmd_settings_new ()
{
    return (GcmdSettings *) g_object_new (GCMD_TYPE_SETTINGS, NULL);
}

static void
gcmd_settings_init (GcmdSettings *gs)
{
    gs->interface = g_settings_new ("org.gnome.desktop.interface");
    gs->layout = g_settings_new ("org.gnome.gnome-commander.preferences.layout");

    g_signal_connect (gs->interface,
                      "changed::monospace-font-name",
                      G_CALLBACK (on_system_font_changed),
                      gs);

    g_signal_connect (gs->layout,
                      "changed::use-default-font",
                      G_CALLBACK (on_use_default_font_changed),
                      gs);

    g_signal_connect (gs->layout,
                      "changed::panel-font",
                      G_CALLBACK (on_layout_font_changed),
                      gs);

    //gs->history = g_settings_new ("org.gnome.gnome-commander.preferences.history");
    //gs->advanced_rename_tool = g_settings_new ("org.gnome.gnome-commander.preferences.rename-tool");
    //gs->search_tool = g_settings_new ("org.gnome.gnome-commander.preferences.search-tool");
    //gs->bookmarks_tool = g_settings_new ("org.gnome.gnome-commander.preferences.bookmarks-tool");
    //gs->connections = g_settings_new ("org.gnome.gnome-commander.preferences.connections");
    //gs->bookmarks = g_settings_new ("org.gnome.gnome-commander.preferences.bookmarks");
    //gs->selections = g_settings_new ("org.gnome.gnome-commander.preferences.selections");
    //gs->key_bindings = g_settings_new ("org.gnome.gnome-commander.preferences.key-bindings");
    
}
