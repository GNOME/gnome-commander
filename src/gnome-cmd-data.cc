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

#define DEFAULT_GUI_UPDATE_RATE 100


GnomeCmdData gnome_cmd_data;
struct GnomeCmdData::Private
{
    GnomeCmdConList *con_list;
};

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

static void on_bookmarks_changed (GnomeCmdMainWin *main_win)
{
    gnome_cmd_data.load_bookmarks();
}

static void gcmd_settings_class_init (GcmdSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = gcmd_settings_dispose;
}


static void gcmd_connect_gsettings_signals(GcmdSettings *gs, GnomeCmdMainWin *main_win)
{
    g_signal_connect_swapped (gs->general,
                      "changed::bookmarks",
                      G_CALLBACK (on_bookmarks_changed),
                      main_win);
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


GnomeCmdData::Options::Options(const Options &cfg)
{
    gcmd_settings = nullptr;
}


GnomeCmdData::Options &GnomeCmdData::Options::operator = (const Options &cfg)
{
    if (this != &cfg)
    {
        this->~Options();       //  free allocated data

        gcmd_settings = nullptr;
    }

    return *this;
}


extern "C" void gnome_cmd_search_config_load();
extern "C" void gnome_cmd_search_config_save();


void GnomeCmdData::save_bookmarks()
{
    GVariant *bookmarksToStore = gnome_cmd_con_list_save_bookmarks (priv->con_list);
    if (bookmarksToStore == nullptr)
        bookmarksToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_BOOKMARKS);

    g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_BOOKMARKS, bookmarksToStore);
}


/**
 * Save devices in gSettings
 */
void GnomeCmdData::save_devices()
{
    GVariant* devicesToStore = gnome_cmd_con_list_save_devices (priv->con_list);
    if (!devicesToStore)
        devicesToStore = g_settings_get_default_value(options.gcmd_settings->general, GCMD_SETTINGS_DEVICE_LIST);
    g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_DEVICE_LIST, devicesToStore);
    g_variant_unref(devicesToStore);
}


/**
 * Save connections in gSettings
 */
void GnomeCmdData::save_connections()
{
    GVariant* connectionsToStore = gnome_cmd_con_list_save_connections (priv->con_list);
    if (!connectionsToStore)
        connectionsToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_CONNECTIONS);
    g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_CONNECTIONS, connectionsToStore);
    g_variant_unref(connectionsToStore);
}


void GnomeCmdData::load_bookmarks()
{
    auto *gVariantBookmarks = g_settings_get_value (options.gcmd_settings->general, GCMD_SETTINGS_BOOKMARKS);
    gnome_cmd_con_list_load_bookmarks (priv->con_list, gVariantBookmarks);
}


void GnomeCmdData::save_directory_history(bool save_dir_history)
{
    if (save_dir_history)
    {
        auto dir_history = gnome_cmd_con_export_dir_history (gnome_cmd_con_list_get_home (priv->con_list));
        g_settings_set_strv (options.gcmd_settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY, dir_history);
        g_strfreev (dir_history);
    }
    else
    {
        GVariant* dirHistoryToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY);
        g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY, dirHistoryToStore);
    }
}


inline void GnomeCmdData::load_directory_history()
{
    GStrv entries = g_settings_get_strv (options.gcmd_settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY);
    gnome_cmd_con_import_dir_history (
        gnome_cmd_con_list_get_home (priv->con_list),
        entries);
    g_strfreev (entries);
}


GnomeCmdData::GnomeCmdData()
{
    //TODO: Include into GnomeCmdData::Options
    gui_update_rate = DEFAULT_GUI_UPDATE_RATE;
}


GnomeCmdData::~GnomeCmdData()
{
    if (priv)
    {
        // free the connections
        g_object_ref_sink (priv->con_list);
        g_object_unref (priv->con_list);

        g_free (priv);
    }
}

void GnomeCmdData::gsettings_init()
{
    options.gcmd_settings = gcmd_settings_new();
}


void GnomeCmdData::connect_signals(GnomeCmdMainWin *main_win)
{
    gcmd_connect_gsettings_signals (options.gcmd_settings, main_win);
}


/**
 * Loads devices from gSettings into gcmd options
 */
void GnomeCmdData::load_devices()
{
    GVariant *gvDevices = g_settings_get_value(options.gcmd_settings->general, GCMD_SETTINGS_DEVICE_LIST);
    gnome_cmd_con_list_load_devices (priv->con_list, gvDevices);
}

/**
 * Loads connections from gSettings into gcmd options
 */
void GnomeCmdData::load_connections()
{
    GVariant *gvConnections = g_settings_get_value(options.gcmd_settings->general, GCMD_SETTINGS_CONNECTIONS);
    gnome_cmd_con_list_load_connections (priv->con_list, gvConnections);
}


void GnomeCmdData::load()
{
    if (!priv)
        priv = g_new0 (Private, 1);

    gui_update_rate = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE);

    gboolean show_samba_workgroups_button = g_settings_get_boolean(options.gcmd_settings->general, GCMD_SETTINGS_SHOW_SAMBA_WORKGROUP_BUTTON);

    if (!priv->con_list)
        priv->con_list = gnome_cmd_con_list_new (show_samba_workgroups_button);

    gnome_cmd_con_list_lock (priv->con_list);
    load_devices();
    gnome_cmd_search_config_load();
    load_connections        ();
    load_bookmarks          ();
    load_directory_history  ();
    gnome_cmd_con_list_unlock (priv->con_list);

    gnome_cmd_con_list_set_volume_monitor (priv->con_list);
}


void GnomeCmdData::save(GnomeCmdMainWin *main_win, bool save_dir_history)
{
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE, &(gui_update_rate));

    save_devices                    ();
    save_directory_history          (save_dir_history);
    gnome_cmd_search_config_save();
    save_connections                ();
    save_bookmarks                  ();

    g_settings_sync ();
}


#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
/**
 * This method stores the value for a given key if the value is different from the currently stored one
 * under the keys value. This function is able of storing several types of GSettings values.
 * Therefore, it first checks the type of GVariant of the default value of the given key. Depending on
 * the result, the gpointer is than casted to the correct type so that *value can be saved.
 * @returns TRUE if new value could be stored, else FALSE
 */
gboolean GnomeCmdData::set_gsettings_when_changed (GSettings *settings_given, const char *key, gpointer value)
{
    GVariant *default_val;
    gboolean rv = true;
    default_val = g_settings_get_default_value (settings_given, key);

    switch (g_variant_classify(default_val))
    {
        case G_VARIANT_CLASS_INT32:
        {
            gint old_value;
            gint new_value = *(gint*) value;

            old_value = g_settings_get_int (settings_given, key);
            if (old_value != new_value)
                rv = g_settings_set_int (settings_given, key, new_value);
            break;
        }
        case G_VARIANT_CLASS_UINT32:
        {
            gint old_value;
            gint new_value = *(gint*) value;

            old_value = g_settings_get_uint (settings_given, key);
            if (old_value != new_value)
                rv = g_settings_set_uint (settings_given, key, new_value);
            break;
        }
        case G_VARIANT_CLASS_STRING:
        {
            gchar *old_value;
            gchar *new_value = (char*) value;

            old_value = g_settings_get_string (settings_given, key);
            if (strcmp(old_value, new_value) != 0)
                rv = g_settings_set_string (settings_given, key, new_value);
            g_free(old_value);
            break;
        }
        case G_VARIANT_CLASS_BOOLEAN:
        {
            gboolean old_value;
            gboolean new_value = *(gboolean*) value;

            old_value = g_settings_get_boolean (settings_given, key);
            if (old_value != new_value)
                rv = g_settings_set_boolean (settings_given, key, new_value);
            break;
        }
        default:
        {
            g_warning("Could not store value of type '%s' for key '%s'\n", g_variant_get_type_string (default_val), key);
            rv = false;
            break;
        }
    }
    if (default_val)
        g_variant_unref (default_val);

    return rv;
}
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif




gpointer gnome_cmd_data_get_con_list ()
{
    return gnome_cmd_data.priv->con_list;
}

// FFI

extern "C" GnomeCmdData::Options *gnome_cmd_data_options ()
{
    return &gnome_cmd_data.options;
}

extern "C" void gnome_cmd_data_save (GnomeCmdMainWin *mw, gboolean save_dir_history)
{
    gnome_cmd_data.save (mw, save_dir_history);
}
