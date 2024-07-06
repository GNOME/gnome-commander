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

#include <config.h>
#include <stdlib.h>
#include <string>
#include <libgcmd/libgcmd.h>
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

static PluginInfo plugin_nfo = {
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


struct FileRollerPluginPrivate
{
    gchar *action_group_name;

    GtkWidget *conf_dialog;
    GtkWidget *conf_combo;
    GtkWidget *conf_entry;

    GnomeCmdState *state;

    gchar *default_ext;
    gchar *file_prefix_pattern;
    PluginSettings *settings;
};


G_DEFINE_TYPE_WITH_PRIVATE (FileRollerPlugin, file_roller_plugin, GNOME_CMD_TYPE_PLUGIN)


gchar *GetGfileAttributeString(GFile *gFile, const char *attribute);

static void run_cmd (const gchar *work_dir, const gchar *cmd)
{
    gint argc;
    gchar **argv;
    GError *err = nullptr;

    g_shell_parse_argv (cmd, &argc, &argv, nullptr);
    if (!g_spawn_async (work_dir, argv, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &err))
    {
        GtkWidget* dialog = gtk_message_dialog_new (nullptr,
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

static void on_extract_cwd (GSimpleAction *action, GVariant *parameter, gpointer user_data)
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
    run_cmd (t, cmd);
    g_free (t);

    g_free (target_arg);
    g_free (target_dir);
    g_free (archive_arg);
    g_free (local_path);
    g_free (cmd);
}


inline void do_add_to_archive (const gchar *name, GnomeCmdState *state)
{
    gchar *t = g_strdup_printf ("--add-to=%s", name);
    gchar *arg = g_shell_quote (t);
    gchar *cmd = g_strdup_printf ("file-roller %s ", arg);
    gchar *active_dir_path;
    GList *files;

    for (files = state->active_dir_selected_files; files; files = files->next)
    {
        auto *gFile = GNOME_CMD_FILE_BASE (files->data)->gFile;
        gchar *path = g_file_get_path (gFile);
        gchar *tmp = cmd;
        gchar *arg_file = g_shell_quote (path);
        cmd = g_strdup_printf ("%s %s", tmp, arg_file);
        g_free (arg_file);
        g_free (path);
        g_free (tmp);
    }

    g_printerr ("add: %s\n", cmd);
    active_dir_path = g_file_get_path (state->activeDirGfile);
    run_cmd (active_dir_path, cmd);

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


static void on_add_to_archive (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gint ret;
    GtkWidget *dialog = nullptr;
    const gchar *name;
    gboolean name_ok = FALSE;
    GList *files;

    FileRollerPlugin *plugin = FILE_ROLLER_PLUGIN (user_data);
    FileRollerPluginPrivate *priv = (FileRollerPluginPrivate *) file_roller_plugin_get_instance_private (plugin);

    files = priv->state->active_dir_selected_files;

    do
    {
        GdkPixbuf *pixbuf;
        GtkWidget *entry;
        gchar *archive_name;

        if (dialog)
            gtk_window_destroy (GTK_WINDOW (dialog));

        dialog = gtk_message_dialog_new (
            nullptr,
            (GtkDialogFlags) 0,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK_CANCEL,
            _("What file name should the new archive have?"));

        gtk_window_set_title (GTK_WINDOW (dialog), _("Create Archive"));

        entry = gtk_entry_new ();
        gtk_widget_set_margin_start (entry, 6);
        gtk_widget_set_margin_end (entry, 6);
        gtk_widget_set_margin_top (entry, 6);
        gtk_widget_set_margin_bottom (entry, 6);
        gtk_widget_set_hexpand (entry, TRUE);
        gtk_widget_show (entry);
        gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), entry);

        gchar *locale_format = g_locale_from_utf8 (priv->file_prefix_pattern, -1, nullptr, nullptr, nullptr);
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

        gchar *archive_name_tmp = g_strdup_printf("%s%s", file_prefix, priv->default_ext);
        auto file_name_tmp = GetGfileAttributeString(GNOME_CMD_FILE_BASE (files->data)->gFile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
        archive_name = new_string_with_replaced_keyword(archive_name_tmp, "$N", file_name_tmp);
        gtk_editable_set_text (GTK_EDITABLE (entry), archive_name);
        g_free(file_name_tmp);
        g_free(archive_name);
        g_free(archive_name_tmp);


        pixbuf = gdk_pixbuf_new_from_xpm_data ((const char**)file_roller_xpm);
        gtk_image_set_from_pixbuf (GTK_IMAGE (gtk_message_dialog_get_image (GTK_MESSAGE_DIALOG (dialog))), pixbuf);
        g_object_unref (pixbuf);

        ret = gtk_dialog_run (GTK_DIALOG (dialog));

        name = gtk_editable_get_text (GTK_EDITABLE (entry));
        if (name != nullptr && strlen (name) > 0)
            name_ok = TRUE;
    }
    while (name_ok == FALSE && ret == GTK_RESPONSE_OK);

    if (ret == GTK_RESPONSE_OK)
        do_add_to_archive (name, priv->state);

    gtk_window_destroy (GTK_WINDOW (dialog));
}


static GSimpleActionGroup *create_actions (GnomeCmdPlugin *plugin, const gchar *name)
{
    FileRollerPluginPrivate *priv = (FileRollerPluginPrivate *) file_roller_plugin_get_instance_private (FILE_ROLLER_PLUGIN (plugin));

    priv->action_group_name = g_strdup (name);

    GSimpleActionGroup *group = g_simple_action_group_new ();
    static const GActionEntry entries[] = {
        { "add-to-archive", on_add_to_archive },
        { "extract", on_extract_cwd, "(sms)" }
    };
    g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), plugin);
    return group;
}


static GMenuModel *create_main_menu (GnomeCmdPlugin *plugin)
{
    return nullptr;
}


static GMenuModel *create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GMenu *menu;
    gchar *action;
    gint num_files;
    GList *gnomeCmdFileBaseGList;

    gnomeCmdFileBaseGList = state->active_dir_selected_files;
    num_files = g_list_length (gnomeCmdFileBaseGList);

    if (num_files <= 0)
        return nullptr;

    FileRollerPluginPrivate *priv = (FileRollerPluginPrivate *) file_roller_plugin_get_instance_private (FILE_ROLLER_PLUGIN (plugin));

    priv->state = state;

    menu = g_menu_new ();

    action = g_strdup_printf ("%s.add-to-archive", priv->action_group_name);
    g_menu_append (menu, _("Create Archive…"), action);
    g_free (action);

    if (num_files == 1)
    {
        GnomeCmdFileBase *gnomeCmdFileBase = GNOME_CMD_FILE_BASE (gnomeCmdFileBaseGList->data);
        auto fname = GetGfileAttributeString(gnomeCmdFileBase->gFile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
        gint i;

        gchar *local_path = g_file_get_path(gnomeCmdFileBase->gFile);

        for (i=0; handled_extensions[i]; ++i)
            if (g_str_has_suffix (fname, handled_extensions[i]))
            {
                action = g_strdup_printf ("%s.extract", priv->action_group_name);
                GMenuItem *item;

                // extract to current directory

                item = g_menu_item_new (_("Extract in Current Directory"), nullptr);
                g_menu_item_set_action_and_target (item, action, "(sms)", local_path, nullptr);
                g_menu_append_item (menu, item);

                // extract to a new directory

                fname[strlen(fname)-strlen(handled_extensions[i])] = '\0';

                gchar *text = g_strdup_printf (_("Extract to “%s”"), fname);
                item = g_menu_item_new (text, nullptr);
                g_free (text);

                gchar *dir = g_path_get_dirname (local_path);
                gchar *target_dir = g_build_filename (dir, fname, nullptr);
                g_menu_item_set_action_and_target (item, action, "(sms)", local_path, target_dir);
                g_free (target_dir);
                g_free (dir);

                g_menu_append_item (menu, item);

                // extract to an opposite panel's directory

                auto activeDirId = GetGfileAttributeString(state->activeDirGfile, G_FILE_ATTRIBUTE_ID_FILE);
                auto inactiveDirId = GetGfileAttributeString(state->inactiveDirGfile, G_FILE_ATTRIBUTE_ID_FILE);

                if (activeDirId && inactiveDirId && !g_str_equal(activeDirId, inactiveDirId))
                {
                    gchar *basenameString = GetGfileAttributeString(state->inactiveDirGfile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
                    gchar *target_dir = g_file_get_path(state->inactiveDirGfile);

                    text = g_strdup_printf (_("Extract to “%s”"), basenameString);
                    item = g_menu_item_new (text, nullptr);
                    g_free (text);
                    g_menu_item_set_action_and_target (item, action, "(sms)", local_path, target_dir);
                    g_menu_append_item (menu, item);
                    g_free (basenameString);
                    g_free (target_dir);
                }

                g_free(activeDirId);
                g_free(inactiveDirId);

                g_free (action);

                break;
            }

        g_free (fname);
    }

    return G_MENU_MODEL (menu);
}


static void on_configure_close (GtkButton *btn, FileRollerPlugin *plugin)
{
    FileRollerPluginPrivate *priv = (FileRollerPluginPrivate *) file_roller_plugin_get_instance_private (plugin);

    priv->default_ext = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->conf_combo));
    priv->file_prefix_pattern = g_strdup (get_entry_text (priv->conf_entry, "file_prefix_pattern_entry"));

    g_settings_set_string (priv->settings->file_roller_plugin, GCMD_PLUGINS_FILE_ROLLER_DEFAULT_TYPE, priv->default_ext);
    g_settings_set_string (priv->settings->file_roller_plugin, GCMD_PLUGINS_FILE_ROLLER_PREFIX_PATTERN, priv->file_prefix_pattern);

    gtk_widget_hide (priv->conf_dialog);
}


static void on_date_format_update (GtkEditable *editable, GtkWidget *options_dialog)
{
    GtkWidget *format_entry = lookup_widget (options_dialog, "file_prefix_pattern_entry");
    GtkWidget *test_label = lookup_widget (options_dialog, "date_format_test_label");
    GtkWidget *combo = lookup_widget (options_dialog, "combo");
    gchar *file_suffix = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));

    const char *format = gtk_editable_get_text (GTK_EDITABLE (format_entry));
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
    gtk_label_set_text (GTK_LABEL (test_label), filename);
    g_free (file_prefix);
    g_free (file_suffix);
    g_free (replacement);
    g_free (filename);
    g_free (filename_tmp);
    g_free (locale_format);
}


static void configure (GnomeCmdPlugin *plugin, GtkWindow *parent_window)
{
    GtkWidget *dialog, *grid, *cat, *label, *vbox, *entry;
    GtkWidget *combo;

    FileRollerPluginPrivate *priv = (FileRollerPluginPrivate *) file_roller_plugin_get_instance_private (FILE_ROLLER_PLUGIN (plugin));

    dialog = gnome_cmd_dialog_new (parent_window, _("Options"));
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_OK"),
                                 G_CALLBACK (on_configure_close), plugin);

    vbox = create_vbox (dialog, FALSE, 12);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), vbox);


    grid = create_grid (dialog);
    cat = create_category (dialog, grid, _("File-roller options"));
    gtk_box_append (GTK_BOX (vbox), cat);

    label = create_label (dialog, _("Default archive type"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    combo = create_combo_box_text (dialog, nullptr);
    g_signal_connect (G_OBJECT(combo), "changed", G_CALLBACK (on_date_format_update), dialog);
    gtk_widget_set_hexpand (combo, TRUE);
    gtk_grid_attach (GTK_GRID (grid), combo, 1, 0, 1, 1);

    // The pattern defining the file name prefix of the archive to be created
    label = create_label (dialog, _("File prefix pattern"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

    gchar *utf8_date_format = g_locale_to_utf8 (priv->file_prefix_pattern, -1, nullptr, nullptr, nullptr);
    entry = create_entry (dialog, "file_prefix_pattern_entry", utf8_date_format);
    g_free (utf8_date_format);
    gtk_widget_grab_focus (entry);
    g_signal_connect (entry, "realize", G_CALLBACK (on_date_format_update), dialog);
    g_signal_connect (entry, "changed", G_CALLBACK (on_date_format_update), dialog);
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);

    label = create_label (dialog, _("Test result:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

    label = create_label (dialog, "");
    g_object_set_data (G_OBJECT (dialog), "date_format_test_label", label);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);

    gchar* text = g_strdup_printf("<small>%s</small>",_("Use $N as a pattern for the original file name. See the manual page for “strftime” for other patterns."));
    label = create_label (dialog, text);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_markup (GTK_LABEL (label), text);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 3, 1, 1);
    g_free(text);

    for (gint i=0; handled_extensions[i] != nullptr; i++)
        gtk_combo_box_text_append_text ((GtkComboBoxText*) combo, handled_extensions[i]);

    for (gint i=0; handled_extensions[i]; ++i)
        if (g_str_has_suffix (priv->default_ext, handled_extensions[i]))
            gtk_combo_box_set_active((GtkComboBox*) combo, i);

    // The text entry stored in default_ext (set in init()) should be the active entry in the combo now.
    // If strlen of the active comby == 0, prepend the stored value to the list.
    gchar* test = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));
    if (test && strlen(test) == 0)
    {
        gtk_combo_box_text_prepend_text ((GtkComboBoxText*) combo, priv->default_ext);
        gtk_combo_box_set_active((GtkComboBox*) combo, 0);
        g_free (test);
    }

    priv->conf_dialog = dialog;
    priv->conf_combo = combo;
    priv->conf_entry = entry;

    gtk_widget_show (dialog);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    FileRollerPlugin *plugin = FILE_ROLLER_PLUGIN (object);
    FileRollerPluginPrivate *priv = (FileRollerPluginPrivate *) file_roller_plugin_get_instance_private (plugin);

    g_clear_pointer (&priv->default_ext, g_free);
    g_clear_pointer (&priv->file_prefix_pattern, g_free);
    g_clear_pointer (&priv->action_group_name, g_free);

    G_OBJECT_CLASS (file_roller_plugin_parent_class)->dispose (object);
}


static void file_roller_plugin_class_init (FileRollerPluginClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GnomeCmdPluginClass *plugin_class = GNOME_CMD_PLUGIN_CLASS (klass);

    object_class->dispose = dispose;

    plugin_class->create_actions = create_actions;
    plugin_class->create_main_menu = create_main_menu;
    plugin_class->create_popup_menu_items = create_popup_menu_items;
    plugin_class->configure = configure;
}


static void file_roller_plugin_init (FileRollerPlugin *plugin)
{
    FileRollerPluginPrivate *priv = (FileRollerPluginPrivate *) file_roller_plugin_get_instance_private (plugin);

    priv->settings = plugin_settings_new();

    GSettings *gsettings = priv->settings->file_roller_plugin;
    priv->default_ext = g_settings_get_string (gsettings, GCMD_PLUGINS_FILE_ROLLER_DEFAULT_TYPE);
    priv->file_prefix_pattern = g_settings_get_string (gsettings, GCMD_PLUGINS_FILE_ROLLER_PREFIX_PATTERN);

    //Set gsettings values to default values if they are empty
    if (strlen(priv->default_ext) == 0)
    {
        g_free(priv->default_ext);

        GVariant *variant;
        variant = g_settings_get_default_value (gsettings, GCMD_PLUGINS_FILE_ROLLER_DEFAULT_TYPE);
        g_settings_set_string (gsettings, GCMD_PLUGINS_FILE_ROLLER_DEFAULT_TYPE, g_variant_get_string(variant, nullptr));
        g_variant_unref (variant);

        priv->default_ext = g_settings_get_string (gsettings, GCMD_PLUGINS_FILE_ROLLER_DEFAULT_TYPE);
    }
    if (strlen(priv->file_prefix_pattern) == 0)
    {
        g_free(priv->file_prefix_pattern);

        GVariant *variant;
        variant = g_settings_get_default_value (gsettings, GCMD_PLUGINS_FILE_ROLLER_PREFIX_PATTERN);
        g_settings_set_string (gsettings, GCMD_PLUGINS_FILE_ROLLER_PREFIX_PATTERN, g_variant_get_string(variant, nullptr));
        g_variant_unref (variant);

        priv->file_prefix_pattern = (gchar*) g_settings_get_default_value (gsettings, GCMD_PLUGINS_FILE_ROLLER_PREFIX_PATTERN);
    }
}

/**
 * Gets a string attribute of a GFile instance and returns the newlo allocated string.
 * The return value has to be g_free'd.
 */
gchar *GetGfileAttributeString(GFile *gFile, const char *attribute)
{
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

GnomeCmdPlugin *file_roller_plugin_new ()
{
    FileRollerPlugin *plugin = (FileRollerPlugin *) g_object_new (file_roller_plugin_get_type (), nullptr);

    return GNOME_CMD_PLUGIN (plugin);
}


extern "C"
{
    GnomeCmdPlugin *create_plugin ()
    {
        return file_roller_plugin_new ();
    }
}


extern "C"
{
    PluginInfo *get_plugin_info ()
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
}
