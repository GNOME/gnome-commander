/**
 * @file gnome-cmd-data.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-cmdline.h"

using namespace std;


GnomeCmdData gnome_cmd_data;

GSettingsSchemaSource* GnomeCmdData::GetGlobalSchemaSource()
{
    GSettingsSchemaSource   *global_schema_source;
    std::string              g_schema_path(PREFIX);

    g_schema_path.append("/share/glib-2.0/schemas");

    global_schema_source = g_settings_schema_source_get_default ();

    GSettingsSchemaSource *parent = global_schema_source;
    GError *error = nullptr;

    global_schema_source = g_settings_schema_source_new_from_directory
                               ((gchar*) g_schema_path.c_str(),
                                parent,
                                FALSE,
                                &error);

    if (global_schema_source == nullptr)
    {
        g_printerr(_("Could not load schemas from %s: %s\n"),
                   (gchar*) g_schema_path.c_str(), error->message);
        g_clear_error (&error);
    }

    return global_schema_source;
}

struct _GcmdSettings
{
    GObject parent;

    GSettings *general;
};

G_DEFINE_TYPE (GcmdSettings, gcmd_settings, G_TYPE_OBJECT)

static void gcmd_settings_dispose (GObject *object)
{
    GcmdSettings *gs = GCMD_SETTINGS (object);

    g_clear_object (&gs->general);

    G_OBJECT_CLASS (gcmd_settings_parent_class)->dispose (object);
}

GSettings *gcmd_settings_get_general (GcmdSettings *gcmd_settings)
{
    return gcmd_settings->general;
}

static void on_bookmarks_changed (GnomeCmdData *data)
{
    data->load_bookmarks();
}

static void gcmd_settings_class_init (GcmdSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = gcmd_settings_dispose;
}


static void gcmd_settings_init (GcmdSettings *gs)
{
    GSettingsSchemaSource   *global_schema_source;
    GSettingsSchema         *global_schema;

    global_schema_source = GnomeCmdData::GetGlobalSchemaSource();

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_GENERAL, FALSE);
    gs->general = g_settings_new_full (global_schema, nullptr, nullptr);
}


GcmdSettings *gcmd_settings_new ()
{
    auto gs = (GcmdSettings *) g_object_new (GCMD_TYPE_SETTINGS, nullptr);
    return gs;
}


extern "C" void gnome_cmd_search_config_load();
extern "C" void gnome_cmd_search_config_save();


void GnomeCmdData::save_bookmarks()
{
    auto con_list = gnome_cmd_con_list_get();

    GVariant *bookmarksToStore = gnome_cmd_con_list_save_bookmarks (con_list);
    if (bookmarksToStore == nullptr)
        bookmarksToStore = g_settings_get_default_value (settings->general, GCMD_SETTINGS_BOOKMARKS);

    g_settings_set_value(settings->general, GCMD_SETTINGS_BOOKMARKS, bookmarksToStore);
}


/**
 * Save devices in gSettings
 */
void GnomeCmdData::save_devices()
{
    auto con_list = gnome_cmd_con_list_get();

    GVariant* devicesToStore = gnome_cmd_con_list_save_devices (con_list);
    if (!devicesToStore)
        devicesToStore = g_settings_get_default_value(settings->general, GCMD_SETTINGS_DEVICE_LIST);
    g_settings_set_value(settings->general, GCMD_SETTINGS_DEVICE_LIST, devicesToStore);
    g_variant_unref(devicesToStore);
}


/**
 * Save connections in gSettings
 */
void GnomeCmdData::save_connections()
{
    auto con_list = gnome_cmd_con_list_get();

    GVariant* connectionsToStore = gnome_cmd_con_list_save_connections (con_list);
    if (!connectionsToStore)
        connectionsToStore = g_settings_get_default_value (settings->general, GCMD_SETTINGS_CONNECTIONS);
    g_settings_set_value(settings->general, GCMD_SETTINGS_CONNECTIONS, connectionsToStore);
    g_variant_unref(connectionsToStore);
}


void GnomeCmdData::load_bookmarks()
{
    auto con_list = gnome_cmd_con_list_get();

    auto *gVariantBookmarks = g_settings_get_value (settings->general, GCMD_SETTINGS_BOOKMARKS);
    gnome_cmd_con_list_load_bookmarks (con_list, gVariantBookmarks);
}


void GnomeCmdData::save_directory_history(bool save_dir_history)
{
    auto con_list = gnome_cmd_con_list_get();

    if (save_dir_history)
    {
        auto dir_history = gnome_cmd_con_export_dir_history (gnome_cmd_con_list_get_home (con_list));
        g_settings_set_strv (settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY, dir_history);
        g_strfreev (dir_history);
    }
    else
    {
        GVariant* dirHistoryToStore = g_settings_get_default_value (settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY);
        g_settings_set_value(settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY, dirHistoryToStore);
    }
}


inline void GnomeCmdData::load_directory_history()
{
    auto con_list = gnome_cmd_con_list_get();

    GStrv entries = g_settings_get_strv (settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY);
    gnome_cmd_con_import_dir_history (
        gnome_cmd_con_list_get_home (con_list),
        entries);
    g_strfreev (entries);
}

/**
 * Loads devices from gSettings into gcmd options
 */
void GnomeCmdData::load_devices()
{
    auto con_list = gnome_cmd_con_list_get();

    GVariant *gvDevices = g_settings_get_value(settings->general, GCMD_SETTINGS_DEVICE_LIST);
    gnome_cmd_con_list_load_devices (con_list, gvDevices);
}

/**
 * Loads connections from gSettings into gcmd options
 */
void GnomeCmdData::load_connections()
{
    auto con_list = gnome_cmd_con_list_get();

    GVariant *gvConnections = g_settings_get_value(settings->general, GCMD_SETTINGS_CONNECTIONS);
    gnome_cmd_con_list_load_connections (con_list, gvConnections);
}


void GnomeCmdData::init()
{
    auto con_list = gnome_cmd_con_list_get();

    settings = gcmd_settings_new();

    gnome_cmd_con_list_lock (con_list);
    load_devices();
    gnome_cmd_search_config_load();
    load_connections        ();
    load_bookmarks          ();
    load_directory_history  ();
    gnome_cmd_con_list_unlock (con_list);

    gnome_cmd_con_list_set_volume_monitor (con_list);

    g_signal_connect_swapped (settings->general,
                      "changed::bookmarks",
                      G_CALLBACK (on_bookmarks_changed), this);
}


void GnomeCmdData::save(bool save_dir_history)
{
    save_devices                    ();
    save_directory_history          (save_dir_history);
    gnome_cmd_search_config_save();
    save_connections                ();
    save_bookmarks                  ();

    g_settings_sync ();
}

// FFI

extern "C" void gnome_cmd_data_save (gboolean save_dir_history)
{
    gnome_cmd_data.save (save_dir_history);
}
