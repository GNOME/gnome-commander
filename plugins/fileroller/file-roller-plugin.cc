/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2024 Uwe Scholz

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

#include "config.h"
#include <stdlib.h>
#include <string>
#include <glib/gi18n.h>
#include <libgcmd.h>
#include "file-roller-plugin.h"
#include "file-roller.xpm"

#define NAME "File Roller Plugin"
#define COPYRIGHT "Copyright \n\xc2\xa9 2003-2006 Marcus Bjurman\n\xc2\xa9 2013-2024 Uwe Scholz"
#define AUTHOR "Marcus Bjurman <marbj499@student.liu.se>"
#define TRANSLATOR_CREDITS "Translations: https://l10n.gnome.org/module/gnome-commander/"
#define WEBPAGE "https://gcmd.github.io"

#define GCMD_PLUGINS_FILE_ROLLER                     "org.gnome.gnome-commander.plugins.file-roller-plugin"
#define GCMD_PLUGINS_FILE_ROLLER_DEFAULT_TYPE        "default-type"
#define GCMD_PLUGINS_FILE_ROLLER_PREFIX_PATTERN      "prefix-pattern"

static GnomeCmdPluginInfo plugin_nfo = {
    GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
    NAME,
    PACKAGE_VERSION,
    COPYRIGHT,
    nullptr,
    nullptr,
    nullptr,
    TRANSLATOR_CREDITS,
    WEBPAGE
};

#define NUMBER_OF_EXTENSIONS           26
static const gchar *handled_extensions[NUMBER_OF_EXTENSIONS + 1] =
{
    ".7z",          // 7-zip
    ".ar",          // ar
    ".arj",         // arj
    ".bin",         // stuffit
    ".deb",         // Debian archives
    ".ear",         // jar
    ".jar",         // jar
    ".lzh",         // lha
    ".rar",         // rar
    ".rpm",         // RPM archives
    ".sit",         // stuffit
    ".tar",         // tar
    ".tar.Z",       // tar+compress
    ".tar.bz",      // tar+bz
    ".tar.bz2",     // tar+bz2
    ".tar.gz",      // tar+gz
    ".tar.lzo",     // tar+lzop
    ".tar.xz",      // tar.xz
    ".taz",         // tar+compress
    ".tbz",         // tar+bz
    ".tbz2",        // tar+bz2
    ".tgz",         // tar+gz
    ".tzo",         // tar+lzop
    ".war",         // jar
    ".zip",         // zip
    ".zoo",         // zoo
    nullptr
};


/***********************************
 * Functions for using GSettings
 ***********************************/

static GSettingsSchemaSource* GetGlobalSchemaSource()
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

struct _PluginSettings
{
    GObject parent;
    GSettings *file_roller_plugin;
};

G_DEFINE_TYPE (PluginSettings, plugin_settings, G_TYPE_OBJECT)

static void plugin_settings_finalize (GObject *object)
{
    G_OBJECT_CLASS (plugin_settings_parent_class)->finalize (object);
}

static void plugin_settings_dispose (GObject *object)
{
    PluginSettings *gs = GCMD_SETTINGS (object);

    g_clear_object (&gs->file_roller_plugin);

    G_OBJECT_CLASS (plugin_settings_parent_class)->dispose (object);
}

static void plugin_settings_class_init (PluginSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = plugin_settings_finalize;
    object_class->dispose = plugin_settings_dispose;
}

PluginSettings *plugin_settings_new ()
{
    return (PluginSettings *) g_object_new (PLUGIN_TYPE_SETTINGS, nullptr);
}

static void plugin_settings_init (PluginSettings *gs)
{
    GSettingsSchemaSource   *global_schema_source;
    GSettingsSchema         *global_schema;

    global_schema_source = GetGlobalSchemaSource();
    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PLUGINS_FILE_ROLLER, FALSE);
    gs->file_roller_plugin = g_settings_new_full (global_schema, nullptr, nullptr);
}


/***********************************
 * The File-Roller-Plugin
 ***********************************/

struct _GnomeCmdFileRollerPlugin
{
    GObject parent;
};


struct GnomeCmdFileRollerPluginPrivate
{
    GtkWidget *conf_dialog;

    PluginSettings *settings;
};


static void gnome_cmd_configurable_init (GnomeCmdConfigurableInterface *iface);
static void gnome_cmd_file_actions_init (GnomeCmdFileActionsInterface *iface);


G_DEFINE_TYPE_WITH_CODE (GnomeCmdFileRollerPlugin, gnome_cmd_file_roller_plugin, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GnomeCmdFileRollerPlugin)
                         G_IMPLEMENT_INTERFACE (GNOME_CMD_TYPE_CONFIGURABLE, gnome_cmd_configurable_init)
                         G_IMPLEMENT_INTERFACE (GNOME_CMD_TYPE_FILE_ACTIONS, gnome_cmd_file_actions_init))



gchar *GetGfileAttributeString(GnomeCmdFileDescriptor *fd, const char *attribute);

static void run_cmd (const gchar *work_dir, const gchar *cmd, GtkWindow *parent_window)
{
    gint argc;
    gchar **argv;
    GError *err = nullptr;

    g_shell_parse_argv (cmd, &argc, &argv, nullptr);
    if (!g_spawn_async (work_dir, argv, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &err))
    {
        GtkWidget* dialog = gtk_message_dialog_new (parent_window,
            (GtkDialogFlags) 0,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            _("Error running \"%s\"\n\n%s"), cmd, err->message);
        g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_window_destroy), dialog);
        gtk_window_present (GTK_WINDOW (dialog));
        g_error_free (err);
    }

    g_strfreev (argv);
}

static void on_extract_cwd (GnomeCmdFileRollerPlugin *plugin, GVariant *parameter, GtkWindow *parent_window)
{
    gchar *local_path;
    gchar *target_dir;
    g_variant_get (parameter, "(sms)", &local_path, &target_dir);

    gchar *target_arg, *archive_arg;
    gchar *cmd, *t;

    if (!target_dir)
        target_dir = g_path_get_dirname (local_path);

    t = g_strdup_printf ("--extract-to=%s", target_dir);
    target_arg = g_shell_quote (t);
    g_free (t);

    archive_arg = g_shell_quote (local_path);
    cmd = g_strdup_printf ("file-roller %s %s", target_arg, archive_arg);

    t = g_path_get_dirname (local_path);
    run_cmd (t, cmd, parent_window);
    g_free (t);

    g_free (target_arg);
    g_free (target_dir);
    g_free (archive_arg);
    g_free (local_path);
    g_free (cmd);
}


inline void do_add_to_archive (const gchar *name, GnomeCmdState *state, GtkWindow *parent_window)
{
    gchar *t = g_strdup_printf ("--add-to=%s", name);
    gchar *arg = g_shell_quote (t);
    gchar *cmd = g_strdup_printf ("file-roller %s ", arg);
    gchar *active_dir_path;
    GList *files;

    for (files = gnome_cmd_state_get_active_dir_selected_files (state); files; files = files->next)
    {
        auto *gFile = gnome_cmd_file_descriptor_get_file (GNOME_CMD_FILE_DESCRIPTOR (files->data));
        gchar *path = g_file_get_path (gFile);
        gchar *tmp = cmd;
        gchar *arg_file = g_shell_quote (path);
        cmd = g_strdup_printf ("%s %s", tmp, arg_file);
        g_free (arg_file);
        g_free (path);
        g_free (tmp);
    }

    g_printerr ("add: %s\n", cmd);
    active_dir_path = g_file_get_path (gnome_cmd_file_descriptor_get_file (gnome_cmd_state_get_active_dir (state)));
    run_cmd (active_dir_path, cmd, parent_window);

    g_free (cmd);
    g_free (active_dir_path);
}


static gchar* new_string_with_replaced_keyword(const char* string, const char* keyword, const char* replacement)
{
    gchar* new_string = nullptr;
    gchar* longer_string = nullptr;
    gchar* replacement_tmp;

    if (keyword == nullptr || strlen(keyword) == 0)
    {
        new_string = g_strdup(string);
        return new_string;
    }
    if (replacement == nullptr)
        replacement_tmp = g_strdup("");
    else
        replacement_tmp = (char*) replacement;

    if (gchar *temp_ptr = g_strrstr(string, keyword))
    {
        gchar *in_filename = (char*) string;
        guint i = 0;

        new_string = (char*) calloc(1, 2);
        while (in_filename != temp_ptr)
        {
            if (new_string)
            {
                longer_string = (char*) realloc(new_string, strlen(new_string) + 1);
                if (longer_string)
                    new_string = longer_string;
                else
                {
                    g_warning("Error (re)allocating memory!");
                    g_free(replacement_tmp);
                    return (char*) string;
                }
            }
            else
                new_string = (char*) calloc(1, 2);
            new_string[i] = *(in_filename++);
            ++i;
        }

        if (new_string)
        {
            longer_string = (char*) realloc(new_string, strlen(new_string) + strlen(replacement_tmp) + 1);
            if (longer_string)
                new_string = longer_string;
            else
            {
                g_warning("Error (re)allocating memory!");
                g_free(replacement_tmp);
                return (char*) string;
            }
        }
        else
        {
            new_string = (char*) calloc(1, strlen(replacement_tmp) + 1);
        }

        strcat(new_string, replacement_tmp);
        i += strlen(replacement_tmp);
        in_filename += strlen(keyword);
        while (*in_filename != '\0')
        {
            new_string = (char*) realloc(new_string, strlen(new_string) + 2);
            new_string[i] = *(in_filename++);
            ++i;
            new_string[i] = '\0';
        }
    }

    if (replacement == nullptr)
        g_free(replacement_tmp);

    if (!new_string)
        new_string = g_strdup(string);

    return new_string;
}


struct CreateArchiveClosure
{
    GtkWindow *dialog;
    GtkWidget *entry;
    GnomeCmdState *state;
};


static void on_create_archive (GtkButton *button, gpointer userdata)
{
    CreateArchiveClosure *closure = static_cast<CreateArchiveClosure*>(userdata);

    const gchar *name = gtk_editable_get_text (GTK_EDITABLE (closure->entry));
    if (name != nullptr && strlen (name) > 0) {
        do_add_to_archive (name, closure->state, closure->dialog);
        gtk_window_destroy (GTK_WINDOW (closure->dialog));
        g_object_unref (closure->state);
        g_free (userdata);
    }
}


static void on_add_to_archive (GnomeCmdFileRollerPlugin *plugin, GnomeCmdState *state, GtkWindow *parent_window)
{
    auto priv = (GnomeCmdFileRollerPluginPrivate *) gnome_cmd_file_roller_plugin_get_instance_private (plugin);

    GList *files = gnome_cmd_state_get_active_dir_selected_files (state);

    GtkWidget *dialog = gtk_dialog_new ();
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_title (GTK_WINDOW (dialog), _("Create Archive"));
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_widget_set_size_request (GTK_WIDGET (dialog), 300, -1);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_widget_set_margin_start (content_area, 12);
    gtk_widget_set_margin_end (content_area, 12);
    gtk_widget_set_margin_top (content_area, 12);
    gtk_widget_set_margin_bottom (content_area, 12);
    gtk_box_set_spacing (GTK_BOX (content_area), 6);

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data ((const char**)file_roller_xpm);
    GtkWidget *image = gtk_image_new_from_pixbuf (pixbuf);
    gtk_box_append (GTK_BOX (content_area), image);
    g_object_unref (pixbuf);

    GtkWidget *label = gtk_label_new (_("What file name should the new archive have?"));
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX (content_area), label);

    GtkWidget *entry = gtk_entry_new ();
    gtk_box_append (GTK_BOX (content_area), entry);

    gchar *file_prefix_pattern = g_settings_get_string (priv->settings->file_roller_plugin, GCMD_PLUGINS_FILE_ROLLER_PREFIX_PATTERN);
    gchar *locale_format = g_locale_from_utf8 (file_prefix_pattern, -1, nullptr, nullptr, nullptr);
    g_free (file_prefix_pattern);

    char s[256];
    time_t t = time (nullptr);

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    strftime (s, sizeof(s), locale_format, localtime (&t));
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
    g_free (locale_format);
    gchar *file_prefix = g_locale_to_utf8 (s, -1, nullptr, nullptr, nullptr);

    gchar *default_ext = g_settings_get_string (priv->settings->file_roller_plugin, GCMD_PLUGINS_FILE_ROLLER_DEFAULT_TYPE);
    gchar *archive_name_tmp = g_strdup_printf("%s%s", file_prefix, default_ext);
    g_free (default_ext);
    auto file_name_tmp = GetGfileAttributeString(GNOME_CMD_FILE_DESCRIPTOR (files->data), G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    gchar *archive_name = new_string_with_replaced_keyword(archive_name_tmp, "$N", file_name_tmp);
    gtk_editable_set_text (GTK_EDITABLE (entry), archive_name);
    g_free(file_name_tmp);
    g_free(archive_name);
    g_free(archive_name_tmp);

    GtkWidget *bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    GtkSizeGroup* bbox_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    GtkWidget *cancel_button = gtk_button_new_with_label (_("_Cancel"));
    gtk_button_set_use_underline (GTK_BUTTON (cancel_button), TRUE);
    gtk_widget_set_hexpand (cancel_button, TRUE);
    gtk_widget_set_halign (cancel_button, GTK_ALIGN_END);
    gtk_size_group_add_widget (bbox_size_group, cancel_button);
    gtk_box_append (GTK_BOX (bbox), cancel_button);

    GtkWidget *ok_button = gtk_button_new_with_label (_("_OK"));
    gtk_button_set_use_underline (GTK_BUTTON (ok_button), TRUE);
    gtk_size_group_add_widget (bbox_size_group, ok_button);
    gtk_box_append (GTK_BOX (bbox), ok_button);

    gtk_box_append (GTK_BOX (content_area), bbox);

    g_signal_connect_swapped (cancel_button, "clicked", G_CALLBACK (gtk_window_destroy), dialog);

    CreateArchiveClosure *closure = g_new0 (CreateArchiveClosure, 1);
    closure->dialog = GTK_WINDOW (dialog);
    closure->entry = entry;
    closure->state = g_object_ref (state);
    g_signal_connect (ok_button, "clicked", G_CALLBACK (on_create_archive), closure);

    gtk_window_present (GTK_WINDOW (dialog));
}


static GMenuModel *create_main_menu (GnomeCmdFileActions *iface)
{
    return nullptr;
}


static GMenuModel *create_popup_menu_items (GnomeCmdFileActions *iface, GnomeCmdState *state)
{
    GMenu *menu;
    gint num_files;
    GList *gnomeCmdFileBaseGList;

    gnomeCmdFileBaseGList = gnome_cmd_state_get_active_dir_selected_files (state);
    num_files = g_list_length (gnomeCmdFileBaseGList);

    if (num_files <= 0)
        return nullptr;

    menu = g_menu_new ();

    g_menu_append (menu, _("Create Archive…"), "add-to-archive");

    if (num_files == 1)
    {
        auto fd = GNOME_CMD_FILE_DESCRIPTOR (gnomeCmdFileBaseGList->data);
        GFile *file = gnome_cmd_file_descriptor_get_file (fd);
        auto fname = GetGfileAttributeString(fd, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
        gint i;

        gchar *local_path = g_file_get_path(file);

        for (i=0; handled_extensions[i]; ++i)
            if (g_str_has_suffix (fname, handled_extensions[i]))
            {
                GMenuItem *item;

                // extract to current directory

                item = g_menu_item_new (_("Extract in Current Directory"), nullptr);
                g_menu_item_set_action_and_target (item, "extract", "(sms)", local_path, nullptr);
                g_menu_append_item (menu, item);

                // extract to a new directory

                fname[strlen(fname)-strlen(handled_extensions[i])] = '\0';

                gchar *text = g_strdup_printf (_("Extract to “%s”"), fname);
                item = g_menu_item_new (text, nullptr);
                g_free (text);

                gchar *dir = g_path_get_dirname (local_path);
                gchar *target_dir = g_build_filename (dir, fname, nullptr);
                g_menu_item_set_action_and_target (item, "extract", "(sms)", local_path, target_dir);
                g_free (target_dir);
                g_free (dir);

                g_menu_append_item (menu, item);

                // extract to an opposite panel's directory

                auto activeDir = gnome_cmd_state_get_active_dir(state);
                auto inactiveDir = gnome_cmd_state_get_inactive_dir(state);

                auto activeDirId = GetGfileAttributeString(activeDir, G_FILE_ATTRIBUTE_ID_FILE);
                auto inactiveDirId = GetGfileAttributeString(inactiveDir, G_FILE_ATTRIBUTE_ID_FILE);

                if (activeDirId && inactiveDirId && !g_str_equal(activeDirId, inactiveDirId))
                {
                    gchar *basenameString = GetGfileAttributeString(inactiveDir, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
                    gchar *target_dir = g_file_get_path(gnome_cmd_file_descriptor_get_file(inactiveDir));

                    text = g_strdup_printf (_("Extract to “%s”"), basenameString);
                    item = g_menu_item_new (text, nullptr);
                    g_free (text);
                    g_menu_item_set_action_and_target (item, "extract", "(sms)", local_path, target_dir);
                    g_menu_append_item (menu, item);
                    g_free (basenameString);
                    g_free (target_dir);
                }

                g_free(activeDirId);
                g_free(inactiveDirId);

                break;
            }

        g_free (fname);
    }

    return G_MENU_MODEL (menu);
}


static void execute (GnomeCmdFileActions *iface, const gchar *action, GVariant *parameter, GtkWindow *parent_window, GnomeCmdState *state)
{
    auto plugin = GNOME_CMD_FILE_ROLLER_PLUGIN (iface);

    if (!g_strcmp0 (action, "add-to-archive"))
        on_add_to_archive (plugin, state, parent_window);
    else if (!g_strcmp0 (action, "extract"))
        on_extract_cwd (plugin, parameter, parent_window);
}


static gboolean get_archive_type (GValue* value, GVariant* variant, gpointer user_data)
{
    const gchar *str = g_variant_get_string (variant, nullptr);
    for (guint i=0; handled_extensions[i]; ++i)
        if (g_strcmp0 (str, handled_extensions[i]) == 0)
        {
            g_value_set_uint (value, i);
            return TRUE;
        }
    return FALSE;
}


static GVariant *set_archive_type(const GValue* value, const GVariantType* expected_type, gpointer user_data)
{
    guint i = g_value_get_uint (value);
    return g_variant_new_string (handled_extensions[i]);
}


static void on_date_format_update (GtkWidget *options_dialog, ...)
{
    auto format_entry = GTK_EDITABLE (g_object_get_data (G_OBJECT (options_dialog), "file_prefix_pattern_entry"));
    auto format = gtk_editable_get_text (format_entry);

    auto combo = GTK_DROP_DOWN (g_object_get_data (G_OBJECT (options_dialog), "combo"));
    auto file_suffix_obj = GTK_STRING_OBJECT (gtk_drop_down_get_selected_item (combo));
    auto file_suffix = gtk_string_object_get_string (file_suffix_obj);

    auto test_label = GTK_LABEL (g_object_get_data (G_OBJECT (options_dialog), "date_format_test_label"));

    gchar *locale_format = g_locale_from_utf8 (format, -1, nullptr, nullptr, nullptr);

    char s[256];
    time_t t = time (nullptr);

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    strftime (s, sizeof(s), locale_format, localtime (&t));
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
    gchar *file_prefix = g_locale_to_utf8 (s, -1, nullptr, nullptr, nullptr);
    gchar *filename_tmp = g_strdup_printf("%s%s", file_prefix, file_suffix);
    gchar *replacement = g_strdup(_("File"));
    gchar *filename = new_string_with_replaced_keyword(filename_tmp, "$N", replacement);
    gtk_label_set_text (test_label, filename);
    g_free (file_prefix);
    g_free (replacement);
    g_free (filename);
    g_free (filename_tmp);
    g_free (locale_format);
}


static void configure (GnomeCmdConfigurable *iface, GtkWindow *parent_window)
{
    auto plugin = GNOME_CMD_FILE_ROLLER_PLUGIN (iface);
    auto priv = (GnomeCmdFileRollerPluginPrivate *) gnome_cmd_file_roller_plugin_get_instance_private (plugin);

    GtkWidget *dialog, *grid, *label, *entry, *combo, *button;

    dialog = gtk_window_new ();
    gtk_window_set_title (GTK_WINDOW (dialog), _("File-roller options"));
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent_window);

    grid = gtk_grid_new ();
    gtk_widget_set_margin_top (grid, 12);
    gtk_widget_set_margin_bottom (grid, 12);
    gtk_widget_set_margin_start (grid, 12);
    gtk_widget_set_margin_end (grid, 12);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_window_set_child (GTK_WINDOW (dialog), grid);

    label = gtk_label_new (_("Default archive type"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    combo = gtk_drop_down_new_from_strings (handled_extensions);
    g_object_set_data (G_OBJECT (dialog), "combo", combo);
    g_signal_connect_swapped (G_OBJECT(combo), "notify::selected", G_CALLBACK (on_date_format_update), dialog);
    gtk_widget_set_hexpand (combo, TRUE);
    gtk_grid_attach (GTK_GRID (grid), combo, 1, 0, 1, 1);

    // The pattern defining the file name prefix of the archive to be created
    label = gtk_label_new_with_mnemonic (_("File prefix pattern"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

    entry = gtk_entry_new ();
    g_object_set_data (G_OBJECT (dialog), "file_prefix_pattern_entry", entry);
    g_signal_connect_swapped (entry, "realize", G_CALLBACK (on_date_format_update), dialog);
    g_signal_connect_swapped (entry, "changed", G_CALLBACK (on_date_format_update), dialog);
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);

    label = gtk_label_new (_("Test result:"));
    gtk_widget_set_valign (label, GTK_ALIGN_START);
    gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

    label = gtk_label_new ("");
    g_object_set (G_OBJECT (label), "width-request", 400, nullptr);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 1);
    gtk_label_set_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    g_object_set_data (G_OBJECT (dialog), "date_format_test_label", label);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);

    gchar* text = g_strdup_printf("<small>%s</small>",_("Use $N as a pattern for the original file name. See the manual page for “strftime” for other patterns."));
    label = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 1);
    gtk_label_set_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_markup (GTK_LABEL (label), text);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 3, 1, 1);
    g_free(text);

    button = gtk_button_new_with_mnemonic (_("_OK"));
    gtk_widget_set_hexpand (button, TRUE);
    gtk_widget_set_halign (button, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (grid), button, 0, 4, 2, 1);
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_window_destroy), dialog);

    g_settings_bind_with_mapping (priv->settings->file_roller_plugin, GCMD_PLUGINS_FILE_ROLLER_DEFAULT_TYPE, combo, "selected", G_SETTINGS_BIND_DEFAULT,
        get_archive_type, set_archive_type, nullptr, nullptr);
    g_settings_bind (priv->settings->file_roller_plugin, GCMD_PLUGINS_FILE_ROLLER_PREFIX_PATTERN, entry, "text", G_SETTINGS_BIND_DEFAULT);

    gtk_window_present (GTK_WINDOW (dialog));
    gtk_widget_grab_focus (entry);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    auto plugin = GNOME_CMD_FILE_ROLLER_PLUGIN (object);
    auto priv = (GnomeCmdFileRollerPluginPrivate *) gnome_cmd_file_roller_plugin_get_instance_private (plugin);

    G_OBJECT_CLASS (gnome_cmd_file_roller_plugin_parent_class)->dispose (object);
}


static void gnome_cmd_file_roller_plugin_class_init (GnomeCmdFileRollerPluginClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = dispose;
}


static void gnome_cmd_configurable_init (GnomeCmdConfigurableInterface *iface)
{
    iface->configure = configure;
}


static void gnome_cmd_file_actions_init (GnomeCmdFileActionsInterface *iface)
{
    iface->create_main_menu = create_main_menu;
    iface->create_popup_menu_items = create_popup_menu_items;
    iface->execute = execute;
}


static void gnome_cmd_file_roller_plugin_init (GnomeCmdFileRollerPlugin *plugin)
{
    auto priv = (GnomeCmdFileRollerPluginPrivate *) gnome_cmd_file_roller_plugin_get_instance_private (plugin);

    priv->settings = plugin_settings_new();
}

/**
 * Gets a string attribute of a GFile instance and returns the newlo allocated string.
 * The return value has to be g_free'd.
 */
gchar *GetGfileAttributeString(GnomeCmdFileDescriptor *fd, const char *attribute)
{
    auto gFile = gnome_cmd_file_descriptor_get_file (fd);

    GError *error;
    error = nullptr;
    auto gcmdFileInfo = g_file_query_info(gFile,
                                   attribute,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   nullptr,
                                   &error);
    if (gcmdFileInfo && error)
    {
        g_message ("retrieving file info failed: %s", error->message);
        g_error_free (error);
        return nullptr;
    }

    auto gFileAttributeString = g_strdup(g_file_info_get_attribute_string (gcmdFileInfo, attribute));
    g_object_unref(gcmdFileInfo);

    return gFileAttributeString;
}


/***********************************
 * Public functions
 ***********************************/

GObject *create_plugin ()
{
    return G_OBJECT (g_object_new (GNOME_CMD_TYPE_FILE_ROLLER_PLUGIN, nullptr));
}

GnomeCmdPluginInfo *get_plugin_info ()
{
    if (!plugin_nfo.authors)
    {
        plugin_nfo.authors = g_new0 (gchar *, 2);
        plugin_nfo.authors[0] = (gchar*) AUTHOR;
        plugin_nfo.authors[1] = nullptr;
        plugin_nfo.comments = g_strdup (_("A plugin that adds File Roller shortcuts for creating "
                                        "and extracting compressed archives."));
    }
    return &plugin_nfo;
}
