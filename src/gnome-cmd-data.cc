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
    GSettings *filter;
    GSettings *confirm;
    GSettings *colors;
    GSettings *programs;
    GSettings *network;
};

G_DEFINE_TYPE (GcmdSettings, gcmd_settings, G_TYPE_OBJECT)

static void gcmd_settings_finalize (GObject *object)
{
//    GcmdSettings *gs = GCMD_SETTINGS (object);
//
//    g_free (gs->old_scheme);
//
    G_OBJECT_CLASS (gcmd_settings_parent_class)->finalize (object);
}

static void gcmd_settings_dispose (GObject *object)
{
    GcmdSettings *gs = GCMD_SETTINGS (object);

    g_clear_object (&gs->general);
    g_clear_object (&gs->filter);
    g_clear_object (&gs->confirm);
    g_clear_object (&gs->colors);
    g_clear_object (&gs->programs);
    g_clear_object (&gs->network);

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

static void on_size_display_mode_changed (GnomeCmdMainWin *main_win)
{
    gint size_disp_mode;

    size_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
    gnome_cmd_data.options.size_disp_mode = (GnomeCmdSizeDispMode) size_disp_mode;

    main_win->update_view();
}

static void on_perm_display_mode_changed (GnomeCmdMainWin *main_win)
{
    gint perm_disp_mode;

    perm_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE);
    gnome_cmd_data.options.perm_disp_mode = (GnomeCmdPermDispMode) perm_disp_mode;

    main_win->update_view();
}

static void on_graphical_layout_mode_changed (GnomeCmdMainWin *main_win)
{
    gint graphical_layout_mode;

    graphical_layout_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE);
    gnome_cmd_data.options.layout = (GnomeCmdLayout) graphical_layout_mode;

    main_win->update_view();
}

static void on_list_row_height_changed (GnomeCmdMainWin *main_win)
{
    guint list_row_height;

    list_row_height = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT);
    gnome_cmd_data.options.list_row_height = list_row_height;

    main_win->update_view();
}

static void on_date_disp_format_changed (GnomeCmdMainWin *main_win)
{
    GnomeCmdDateFormat date_format;

    date_format = (GnomeCmdDateFormat) g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT);
    g_free(gnome_cmd_data.options.date_format);
    gnome_cmd_data.options.date_format = date_format;

    main_win->update_view();
}

static void on_symbolic_links_as_regular_files_changed (GnomeCmdMainWin *main_win)
{
    gboolean symbolic_links_as_regular_files;

    symbolic_links_as_regular_files = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SYMBOLIC_LINKS_AS_REG_FILES);
    gnome_cmd_data.options.symbolic_links_as_regular_files = symbolic_links_as_regular_files;

    main_win->update_view();
}

static void on_list_font_changed (GnomeCmdMainWin *main_win)
{
    g_free(gnome_cmd_data.options.list_font);
    gnome_cmd_data.options.list_font = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT);

    main_win->update_view();
}

static void on_ext_disp_mode_changed (GnomeCmdMainWin *main_win)
{
    gint ext_disp_mode;

    ext_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE);
    gnome_cmd_data.options.ext_disp_mode = (GnomeCmdExtDispMode) ext_disp_mode;

    main_win->update_view();
}

static void on_icon_size_changed (GnomeCmdMainWin *main_win)
{
    guint icon_size;

    icon_size = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE);
    gnome_cmd_data.options.icon_size = icon_size;

    main_win->update_view();
}

static void on_use_trash_changed (GnomeCmdMainWin *main_win)
{
    gboolean use_trash;

    use_trash = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_USE_TRASH);
    gnome_cmd_data.options.deleteToTrash = use_trash;
}

static void on_select_dirs_changed (GnomeCmdMainWin *main_win)
{
    gboolean select_dirs;

    select_dirs = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS);
    gnome_cmd_data.options.select_dirs = select_dirs;
}

static void on_case_sensitive_changed (GnomeCmdMainWin *main_win)
{
    gboolean case_sensitive;

    case_sensitive = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE);
    gnome_cmd_data.options.case_sens_sort = case_sensitive;
}

static void on_use_ls_colors_changed (GnomeCmdMainWin *main_win)
{
    gboolean use_ls_colors;

    use_ls_colors = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS);
    gnome_cmd_data.options.use_ls_colors = use_ls_colors;

    main_win->update_view();
}

static void on_multiple_instances_changed (GnomeCmdMainWin *main_win)
{
    gboolean allow_multiple_instances;

    allow_multiple_instances = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES);
    gnome_cmd_data.options.allow_multiple_instances = allow_multiple_instances;
}

static void on_quick_search_shortcut_changed (GnomeCmdMainWin *main_win)
{
    GnomeCmdQuickSearchShortcut quick_search;
    quick_search = (GnomeCmdQuickSearchShortcut) g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT);
    gnome_cmd_data.options.quick_search = quick_search;
}

static void on_quick_search_exact_match_begin_changed (GnomeCmdMainWin *main_win)
{
    gboolean quick_search_exact_match;

    quick_search_exact_match = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN);
    gnome_cmd_data.options.quick_search_exact_match_begin = quick_search_exact_match;
}

static void on_quick_search_exact_match_end_changed (GnomeCmdMainWin *main_win)
{
    gboolean quick_search_exact_match;

    quick_search_exact_match = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END);
    gnome_cmd_data.options.quick_search_exact_match_end = quick_search_exact_match;
}

static void on_opts_dialog_width_changed()
{
    gnome_cmd_data.opts_dialog_width = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_WIDTH);
}

static void on_opts_dialog_height_changed()
{
    gnome_cmd_data.opts_dialog_height = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_HEIGHT);
}

static void gcmd_settings_class_init (GcmdSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gcmd_settings_finalize;
    object_class->dispose = gcmd_settings_dispose;
}


static void gcmd_connect_gsettings_signals(GcmdSettings *gs, GnomeCmdMainWin *main_win)
{
    g_signal_connect_swapped (gs->general,
                      "changed::bookmarks",
                      G_CALLBACK (on_bookmarks_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::size-display-mode",
                      G_CALLBACK (on_size_display_mode_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::perm-display-mode",
                      G_CALLBACK (on_perm_display_mode_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::graphical-layout-mode",
                      G_CALLBACK (on_graphical_layout_mode_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::list-row-height",
                      G_CALLBACK (on_list_row_height_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::date-disp-format",
                      G_CALLBACK (on_date_disp_format_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::list-font",
                      G_CALLBACK (on_list_font_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::symbolic-links-as-regular-files",
                      G_CALLBACK (on_symbolic_links_as_regular_files_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::extension-display-mode",
                      G_CALLBACK (on_ext_disp_mode_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::icon-size",
                      G_CALLBACK (on_icon_size_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::delete-to-trash",
                      G_CALLBACK (on_use_trash_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::select-dirs",
                      G_CALLBACK (on_select_dirs_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::case-sensitive",
                      G_CALLBACK (on_case_sensitive_changed),
                      main_win);

    g_signal_connect_swapped (gs->colors,
                      "changed::use-ls-colors",
                      G_CALLBACK (on_use_ls_colors_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::allow-multiple-instances",
                      G_CALLBACK (on_multiple_instances_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::quick-search",
                      G_CALLBACK (on_quick_search_shortcut_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::quick-search-exact-match-begin",
                      G_CALLBACK (on_quick_search_exact_match_begin_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::quick-search-exact-match-end",
                      G_CALLBACK (on_quick_search_exact_match_end_changed),
                      main_win);

    g_signal_connect (gs->general,
                      "changed::opts-dialog-width",
                      G_CALLBACK (on_opts_dialog_width_changed),
                      nullptr);

    g_signal_connect (gs->general,
                      "changed::opts-dialog-height",
                      G_CALLBACK (on_opts_dialog_height_changed),
                      nullptr);
}


static void gcmd_settings_init (GcmdSettings *gs)
{
    GSettingsSchemaSource   *global_schema_source;
    GSettingsSchema         *global_schema;

    global_schema_source = GnomeCmdData::GetGlobalSchemaSource();

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_GENERAL, FALSE);
    gs->general = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_FILTER, FALSE);
    gs->filter = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_CONFIRM, FALSE);
    gs->confirm = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_COLORS, FALSE);
    gs->colors = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_PROGRAMS, FALSE);
    gs->programs = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_NETWORK, FALSE);
    gs->network = g_settings_new_full (global_schema, nullptr, nullptr);
}


GcmdSettings *gcmd_settings_new ()
{
    auto gs = (GcmdSettings *) g_object_new (GCMD_TYPE_SETTINGS, nullptr);
    return gs;
}


GnomeCmdData::Options::Options(const Options &cfg)
{
    left_mouse_button_mode = cfg.left_mouse_button_mode;
    left_mouse_button_unselects = cfg.left_mouse_button_unselects;
    middle_mouse_button_mode = cfg.middle_mouse_button_mode;
    right_mouse_button_mode = cfg.right_mouse_button_mode;
    select_dirs = cfg.select_dirs;
    case_sens_sort = cfg.case_sens_sort;
    quick_search = cfg.quick_search;
    quick_search_exact_match_begin = cfg.quick_search_exact_match_begin;
    quick_search_exact_match_end = cfg.quick_search_exact_match_end;
    allow_multiple_instances = cfg.allow_multiple_instances;
    save_dirs_on_exit = cfg.save_dirs_on_exit;
    save_tabs_on_exit = cfg.save_tabs_on_exit;
    save_dir_history_on_exit = cfg.save_dir_history_on_exit;
    save_cmdline_history_on_exit = cfg.save_cmdline_history_on_exit;
    save_search_history_on_exit = cfg.save_search_history_on_exit;
    size_disp_mode = cfg.size_disp_mode;
    perm_disp_mode = cfg.perm_disp_mode;
    date_format = g_strdup (cfg.date_format);
    list_font = g_strdup (cfg.list_font);
    list_row_height = cfg.list_row_height;
    ext_disp_mode = cfg.ext_disp_mode;
    layout = cfg.layout;
    use_ls_colors = cfg.use_ls_colors;
    icon_size = cfg.icon_size;
    icon_scale_quality = cfg.icon_scale_quality;
    theme_icon_dir = cfg.theme_icon_dir;
    deleteToTrash = cfg.deleteToTrash;
    gcmd_settings = nullptr;
}


GnomeCmdData::Options &GnomeCmdData::Options::operator = (const Options &cfg)
{
    if (this != &cfg)
    {
        this->~Options();       //  free allocated data

        left_mouse_button_mode = cfg.left_mouse_button_mode;
        left_mouse_button_unselects = cfg.left_mouse_button_unselects;
        middle_mouse_button_mode = cfg.middle_mouse_button_mode;
        right_mouse_button_mode = cfg.right_mouse_button_mode;
        select_dirs = cfg.select_dirs;
        case_sens_sort = cfg.case_sens_sort;
        quick_search = cfg.quick_search;
        quick_search_exact_match_begin = cfg.quick_search_exact_match_begin;
        quick_search_exact_match_end = cfg.quick_search_exact_match_end;
        allow_multiple_instances = cfg.allow_multiple_instances;
        save_dirs_on_exit = cfg.save_dirs_on_exit;
        save_tabs_on_exit = cfg.save_tabs_on_exit;
        save_dir_history_on_exit = cfg.save_dir_history_on_exit;
        save_cmdline_history_on_exit = cfg.save_cmdline_history_on_exit;
        save_search_history_on_exit = cfg.save_search_history_on_exit;
        size_disp_mode = cfg.size_disp_mode;
        perm_disp_mode = cfg.perm_disp_mode;
        date_format = g_strdup (cfg.date_format);
        list_font = g_strdup (cfg.list_font);
        list_row_height = cfg.list_row_height;
        ext_disp_mode = cfg.ext_disp_mode;
        layout = cfg.layout;
        use_ls_colors = cfg.use_ls_colors;
        icon_size = cfg.icon_size;
        icon_scale_quality = cfg.icon_scale_quality;
        theme_icon_dir = cfg.theme_icon_dir;
        gcmd_settings = nullptr;
    }

    return *this;
}


GnomeCmdColorMode GnomeCmdData::Options::color_mode()
{
    return (GnomeCmdColorMode) g_settings_get_enum (gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);
}


void GnomeCmdData::Options::set_color_mode(GnomeCmdColorMode color_mode)
{
    g_settings_set_enum (gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME, color_mode);
}


extern "C" GList *gnome_cmd_search_config_get_name_patterns(GnomeCmdData::SearchConfig *search_config)
{
    return search_config->name_patterns.ents;
}

extern "C" void gnome_cmd_search_config_add_name_pattern(GnomeCmdData::SearchConfig *search_config, const gchar *p)
{
    search_config->name_patterns.add(p);
}

extern "C" GList *gnome_cmd_search_config_get_content_patterns(GnomeCmdData::SearchConfig *search_config)
{
    return search_config->content_patterns.ents;
}

extern "C" SearchProfile *gnome_cmd_search_config_get_default_profile(GnomeCmdData::SearchConfig *search_config)
{
    return search_config->default_profile;
}

extern "C" GListStore *gnome_cmd_search_config_get_profiles(GnomeCmdData::SearchConfig *search_config)
{
    return search_config->profiles;
}

extern "C" void gnome_cmd_search_config_load(GnomeCmdData::SearchConfig *search_config);
extern "C" void gnome_cmd_search_config_save(GnomeCmdData::SearchConfig *search_config, gboolean save_search_history);


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


/**
 * This function converts a GList into a NULL terminated array of char pointers.
 * This array is stored into the given GSettings key.
 * @returns The return value of g_settings_set_strv if the length of the GList is > 0, else true.
 */
gboolean GnomeCmdData::set_gsettings_string_array_from_glist (GSettings *settings_given, const gchar *key, GList *strings)
{
    gboolean rv = true;

    if (strings == nullptr)
    {
        rv = g_settings_set_strv(settings_given, key, nullptr);
    }
    else
    {
        guint ii;
        auto numberOfStrings = g_list_length (strings);
        gchar** str_array;
        str_array = new char * [numberOfStrings + 1];

        // Build up a NULL terminated char array for storage in GSettings
        for (ii = 0; strings; strings = strings->next, ++ii)
        {
            str_array[ii] = (gchar*) strings->data;
        }
        str_array[ii] = nullptr;

        rv = g_settings_set_strv(settings_given, key, str_array);

        delete[](str_array);
    }
    return rv;
}


void GnomeCmdData::save_cmdline_history(GnomeCmdMainWin *main_win)
{
    if (options.save_cmdline_history_on_exit)
    {
        if (main_win != nullptr)
        {
            cmdline_history = gnome_cmd_cmdline_get_history (gnome_cmd_main_win_get_cmdline (main_win));
            g_settings_set_strv (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY, cmdline_history);
        }
    }
    else
    {
        set_gsettings_string_array_from_glist(options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY, nullptr);
    }
}


void GnomeCmdData::save_directory_history()
{
    if (options.save_dir_history_on_exit)
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


void GnomeCmdData::save_search_history()
{
    if (options.save_search_history_on_exit)
    {
        set_gsettings_string_array_from_glist(
            options.gcmd_settings->general,
            GCMD_SETTINGS_SEARCH_PATTERN_HISTORY,
            search_defaults.name_patterns.ents);

        set_gsettings_string_array_from_glist(
            options.gcmd_settings->general,
            GCMD_SETTINGS_SEARCH_TEXT_HISTORY,
            search_defaults.content_patterns.ents);
    }
    else
    {
        GVariant* searchHistoryToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PATTERN_HISTORY);
        g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PATTERN_HISTORY, searchHistoryToStore);

        searchHistoryToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_TEXT_HISTORY);
        g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_TEXT_HISTORY, searchHistoryToStore);
    }
}


/**
 * Returns a GList with newly allocated char strings
 */
inline GList* GnomeCmdData::get_list_from_gsettings_string_array (GSettings *settings_given, const gchar *key)
{
    GList *list = nullptr;
    gchar** gsettings_array;
    gsettings_array = g_settings_get_strv (settings_given, key);

    for(gint i = 0; gsettings_array[i]; ++i)
    {
        list = g_list_append (list, gsettings_array[i]);
    }

    g_free(gsettings_array);
    return list;
}


inline void GnomeCmdData::load_cmdline_history()
{
    g_strfreev(cmdline_history);

    cmdline_history = g_settings_get_strv (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY);
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

    cmdline_history = nullptr;
    cmdline_history_length = 0;
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

    options.use_ls_colors = g_settings_get_boolean (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS);

    options.size_disp_mode = (GnomeCmdSizeDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
    options.perm_disp_mode = (GnomeCmdPermDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE);

    gchar *utf8_date_format = g_settings_get_string (options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT);
    options.date_format = g_locale_from_utf8 (utf8_date_format, -1, nullptr, nullptr, nullptr);
    g_free (utf8_date_format);

    options.layout = (GnomeCmdLayout) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE);

    options.list_row_height = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT);

    options.select_dirs = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS);
    options.case_sens_sort = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE);
    options.symbolic_links_as_regular_files = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SYMBOLIC_LINKS_AS_REG_FILES);

    opts_dialog_width = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_WIDTH);
    opts_dialog_height = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_HEIGHT);

    options.list_font = g_settings_get_string (options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT);

    options.ext_disp_mode = (GnomeCmdExtDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE);
    options.left_mouse_button_mode = (LeftMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_CLICKS_TO_OPEN_ITEM);
    options.left_mouse_button_unselects = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_LEFT_MOUSE_BUTTON_UNSELECTS);
    options.middle_mouse_button_mode = (MiddleMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_MIDDLE_MOUSE_BUTTON_MODE);
    options.right_mouse_button_mode = (RightMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_RIGHT_MOUSE_BUTTON_MODE);
    options.deleteToTrash = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_USE_TRASH);
    options.icon_size = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE);
    options.icon_scale_quality = (GdkInterpType) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SCALE_QUALITY);
    options.theme_icon_dir = g_settings_get_string(options.gcmd_settings->general, GCMD_SETTINGS_MIME_ICON_DIR);
    cmdline_history_length = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH);
    gui_update_rate = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE);

    options.allow_multiple_instances = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES);
    options.quick_search = (GnomeCmdQuickSearchShortcut) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT);
    options.quick_search_exact_match_begin = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN);
    options.quick_search_exact_match_end = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END);

    gboolean show_samba_workgroups_button = g_settings_get_boolean(options.gcmd_settings->general, GCMD_SETTINGS_SHOW_SAMBA_WORKGROUP_BUTTON);

    options.save_dirs_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIRS_ON_EXIT);
    options.save_tabs_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_TABS_ON_EXIT);
    options.save_dir_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT);
    options.save_cmdline_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_CMDLINE_HISTORY_ON_EXIT);
    options.save_search_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_SEARCH_HISTORY_ON_EXIT);
    options.search_window_is_transient = g_settings_get_boolean(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_IS_TRANSIENT);
    search_defaults.content_patterns.ents = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_TEXT_HISTORY);
    search_defaults.name_patterns.ents = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PATTERN_HISTORY);

    load_cmdline_history();

    if (!priv->con_list)
        priv->con_list = gnome_cmd_con_list_new (show_samba_workgroups_button);

    gnome_cmd_con_list_lock (priv->con_list);
    load_devices();
    gnome_cmd_search_config_load(&search_defaults);
    load_connections        ();
    load_bookmarks          ();
    load_directory_history  ();
    gnome_cmd_con_list_unlock (priv->con_list);

    gnome_cmd_con_list_set_volume_monitor (priv->con_list);
}


void GnomeCmdData::save(GnomeCmdMainWin *main_win)
{
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE, options.size_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE, options.perm_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE, options.layout);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT, &(options.list_row_height));

    gchar *utf8_date_format = g_locale_to_utf8 (options.date_format, -1, nullptr, nullptr, nullptr);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT, utf8_date_format);
    g_free (utf8_date_format);

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS, &(options.select_dirs));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE, &(options.case_sens_sort));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SYMBOLIC_LINKS_AS_REG_FILES, &(options.symbolic_links_as_regular_files));

    set_gsettings_when_changed      (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS, &(options.use_ls_colors));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT, options.list_font);

    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE, options.ext_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_CLICKS_TO_OPEN_ITEM, options.left_mouse_button_mode);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LEFT_MOUSE_BUTTON_UNSELECTS, &(options.left_mouse_button_unselects));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_MIDDLE_MOUSE_BUTTON_MODE, options.middle_mouse_button_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_RIGHT_MOUSE_BUTTON_MODE, options.right_mouse_button_mode);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE, &(options.icon_size));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_USE_TRASH, &(options.deleteToTrash));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SCALE_QUALITY, options.icon_scale_quality);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MIME_ICON_DIR, options.theme_icon_dir);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH, &(cmdline_history_length));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE, &(gui_update_rate));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES, &(options.allow_multiple_instances));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT, options.quick_search);

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN, &(options.quick_search_exact_match_begin));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END, &(options.quick_search_exact_match_end));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_WIDTH, &(opts_dialog_width));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_HEIGHT, &(opts_dialog_height));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIRS_ON_EXIT, &(options.save_dirs_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_TABS_ON_EXIT, &(options.save_tabs_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT, &(options.save_dir_history_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_CMDLINE_HISTORY_ON_EXIT, &(options.save_cmdline_history_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_SEARCH_HISTORY_ON_EXIT, &(options.save_search_history_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_IS_TRANSIENT , &(options.search_window_is_transient));

    save_devices                    ();
    save_cmdline_history            (main_win);
    save_directory_history          ();
    save_search_history             ();
    gnome_cmd_search_config_save(&search_defaults, options.save_search_history_on_exit);
    save_connections                ();
    save_bookmarks                  ();

    g_settings_sync ();
}


/**
 * As GSettings enum-type is of GVARIANT_CLASS String, we need a separate function for
 * finding out if a key value has changed. This is done here. For storing the other GSettings
 * types, see @link set_gsettings_when_changed @endlink .
 * @returns TRUE if new value could be stored, else FALSE
 */
gboolean GnomeCmdData::set_gsettings_enum_when_changed (GSettings *settings_given, const char *key, gint new_value)
{
    GVariant *default_val;
    gboolean rv = true;

    default_val = g_settings_get_default_value (settings_given, key);

    // An enum key must be of type G_VARIANT_CLASS_STRING
    if (g_variant_classify(default_val) == G_VARIANT_CLASS_STRING)
    {
        gint old_value;
        old_value = g_settings_get_enum(settings_given, key);
        if (old_value != new_value)
            rv = g_settings_set_enum (settings_given, key, new_value);
    }
    else
    {
        g_warning("Could not store value of type '%s' for key '%s'\n", g_variant_get_type_string (default_val), key);
        rv = false;
    }

    if (default_val)
        g_variant_unref (default_val);

    return rv;
}


#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
/**
 * This method stores the value for a given key if the value is different from the currently stored one
 * under the keys value. This function is able of storing several types of GSettings values (except enums
 * which is done in @link set_gsettings_enum_when_changed @endlink, and complex variant types).
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

extern "C" GnomeCmdData::SearchConfig *gnome_cmd_data_search_defaults ()
{
    return &gnome_cmd_data.search_defaults;
}

extern "C" void gnome_cmd_data_save (GnomeCmdMainWin *mw)
{
    gnome_cmd_data.save (mw);
}
