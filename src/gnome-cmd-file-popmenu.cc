/**
 * @file gnome-cmd-file-popmenu.cc
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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-popmenu.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-main-win.h"
#include "plugin_manager.h"
#include "gnome-cmd-user-actions.h"
#include "utils.h"
#include "cap.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-xfer.h"
#include "imageloader.h"
#include "dirlist.h"

#include <fnmatch.h>

using namespace std;


#define MAX_OPEN_WITH_APPS 20


struct OpenWithData
{
    GList *files;
    GnomeCmdApp *app;
};

struct ScriptData
{
    GList *files;
    char *path;
    gboolean inTerminal;

    // this list is only used to free memory of all script paths
    // after one script is executed:
    GList *allScripts;
};

static GtkMenuClass *parent_class = nullptr;


struct GnomeCmdFilePopmenuPrivate
{
    GList *data_list;
};

static void do_mime_exec_multiple (gpointer *args)
{
    auto gnomeCmdApp = static_cast<GnomeCmdApp*> (args[0]);
    auto files = static_cast<GList*> (args[1]);
    if (files)
    {
        if(gnomeCmdApp->gAppInfo != nullptr)
        {
            // gio app default by system
            DEBUG('g', "Launching \"%s\"\n", g_app_info_get_commandline(gnomeCmdApp->gAppInfo));
            g_app_info_launch(gnomeCmdApp->gAppInfo, files, nullptr, nullptr);
        }
        else
        {
            // own app by user defined
            string cmdString = gnome_cmd_app_get_command (gnomeCmdApp);
            set<string> dirs;
            for (auto files_tmp = files; files_tmp; files_tmp = files_tmp->next)
            {
                auto gFile = (GFile *) files_tmp->data;
                auto localpath = g_file_get_path(gFile);
                auto dpath = g_path_get_dirname (localpath);
                if (dpath)
                    dirs.insert (stringify (dpath));
                g_free(localpath);
            }
            run_command_indir (cmdString.c_str(), dirs.begin()->c_str(), gnome_cmd_app_get_requires_terminal (gnomeCmdApp));
        }
        g_list_free (files);
    }
    gnome_cmd_app_free (gnomeCmdApp);
    g_free (args);
}


static void mime_exec_multiple (GList *files, GnomeCmdApp *app)
{
    g_return_if_fail (files != nullptr);
    g_return_if_fail (app != nullptr);

    GList *srcGFileList = nullptr;
    GList *destGFileList = nullptr;
    GList *localGFileList = nullptr;
    gboolean asked = FALSE;
    guint no_of_remote_files = 0;
    gint retid;

    for (; files; files = files->next)
    {
        auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
        auto scheme = g_file_get_uri_scheme (gnomeCmdFile->gFile);

        if (g_strcmp0(scheme, "file") == 0)
        {
            localGFileList = g_list_append (localGFileList, gnomeCmdFile->gFile);
        }
        else
        {
            ++no_of_remote_files;
            if (gnome_cmd_app_get_handles_uris (app) && gnome_cmd_data.options.honor_expect_uris)
            {
                localGFileList = g_list_append (localGFileList, gnomeCmdFile->gFile);
            }
            else
            {
                if (!asked)
                {
                    gchar *msg = g_strdup_printf (ngettext("%s does not know how to open remote file. Do you want to download the file to a temporary location and then open it?",
                                                           "%s does not know how to open remote files. Do you want to download the files to a temporary location and then open them?", no_of_remote_files),
                                                  gnome_cmd_app_get_name (app));
                    retid = run_simple_dialog (*main_win, TRUE, GTK_MESSAGE_QUESTION, msg, "", -1, _("No"), _("Yes"), nullptr);
                    asked = TRUE;
                }

                if (retid==1)
                {
                    gchar *path_str = get_temp_download_filepath (gnomeCmdFile->get_name());

                    if (!path_str) return;

                    auto srcGFile = g_file_dup (gnomeCmdFile->get_gfile());
                    GnomeCmdPlainPath path(path_str);
                    auto destGFile = gnome_cmd_con_create_gfile (get_home_con (), &path);

                    srcGFileList = g_list_append (srcGFileList, srcGFile);
                    destGFileList = g_list_append (destGFileList, destGFile);
                    localGFileList = g_list_append (localGFileList, destGFile);
                }
            }
        }
        g_free(scheme);
    }

    g_list_free (files);

    gpointer *args = g_new0 (gpointer, 2);
    args[0] = app;
    args[1] = localGFileList;

    if (srcGFileList)
        gnome_cmd_tmp_download(srcGFileList,
                               destGFileList,
                               G_FILE_COPY_OVERWRITE,
                               GTK_SIGNAL_FUNC (do_mime_exec_multiple),
                               args);
    else
      do_mime_exec_multiple (args);
}


inline void exec_with_app (GList *files, GnomeCmdApp *app)
{
    mime_exec_multiple (files, app);
}


/* Used by exec_default for each list of files that shares the same default application
 * This is a hash-table callback function
 */
static void htcb_exec_with_app (const gchar *key, OpenWithData *data, gpointer user_data)
{
    exec_with_app (data->files, data->app);
    g_free (data);
}


/**
 * Executes a list of files with the same application
 */
static void cb_exec_with_app (GtkMenuItem *menu_item, OpenWithData *data)
{
    exec_with_app (data->files, data->app);
}


/* Iterates through all files and gets their default application.
 * All files with the same default app are grouped together and opened in one call.
 */
static void cb_exec_default (GtkMenuItem *menu_item, GList *files)
{
    auto gHashTable = g_hash_table_new (g_str_hash, g_str_equal);

    for (; files; files = files->next)
    {
        auto file = static_cast<GnomeCmdFile*> (files->data);
        auto appInfo = file->GetAppInfoForContentType();

        if (g_app_info_get_commandline (appInfo))
        {
            auto data = static_cast<OpenWithData*> (g_hash_table_lookup (gHashTable, g_app_info_get_id(appInfo)));

            if (!data)
            {
                data = g_new0 (OpenWithData, 1);
                data->app = gnome_cmd_app_new_from_app_info (appInfo);
                data->files = nullptr;
                g_hash_table_insert (gHashTable, (gpointer) g_app_info_get_id(appInfo), data);
            }
            data->files = g_list_append (data->files, file);
        }
        else
            gnome_cmd_show_message (nullptr,
                g_file_info_get_display_name(file->gFileInfo),
                _("Couldn’t retrieve MIME type of the file."));
    }

    g_hash_table_foreach (gHashTable, (GHFunc) htcb_exec_with_app, nullptr);

    g_hash_table_destroy (gHashTable);
}


static gboolean on_open_with_other_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GList *files)
{
    GtkWidget *term_check = lookup_widget (GTK_WIDGET (string_dialog), "term_check");

    if (!values[0] || strlen(values[0]) < 1)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("Invalid command")));
        return FALSE;
    }

    string cmdString = values[0];

    GList *filesTmp = files;

    for (; filesTmp; filesTmp = filesTmp->next)
    {
        cmdString += ' ';
        cmdString += stringify (GNOME_CMD_FILE (files->data)->get_quoted_real_path());
    }

    GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);
    GnomeCmdDir *dir = fs->get_directory();
    gchar *dpath = GNOME_CMD_FILE (dir)->get_real_path();
    auto returnValue = run_command_indir (cmdString.c_str(), dpath, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (term_check)));
    if (!returnValue)
    {
        on_open_with_other (nullptr, files);
    }

    g_free (dpath);

    return TRUE;
}


void on_open_with_other (GtkMenuItem *menu_item, GList *files)
{
    const gchar *labels[] = {_("Application:")};
    GtkWidget *dialog;

    dialog = gnome_cmd_string_dialog_new (_("Open with other…"), labels, 1,
                                          (GnomeCmdStringDialogCallback) on_open_with_other_ok, files);

    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    GtkWidget *term_check = create_check (dialog, _("Needs terminal"), "term_check");

    g_object_ref (dialog);
    g_object_set_data_full (G_OBJECT (menu_item), "new_textfile_dialog", dialog, g_object_unref);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), term_check);

    gtk_widget_show (dialog);
}


static void on_execute (GtkMenuItem *menu_item, GList *files)
{
    GnomeCmdFile *f = GNOME_CMD_FILE (files->data);

    f->execute();
}


static void on_execute_script (GtkMenuItem *menuItem, ScriptData *scriptData)
{
    GdkModifierType mask;

    gdk_window_get_pointer (nullptr, nullptr, nullptr, &mask);

    char *dirName = nullptr;

    string quotationMarks = scriptData->inTerminal ? "\\\"" : "\"";

    if (state_is_shift (mask))
    {
        // Run script per file
        for (auto files = scriptData->files; files; files = files->next)
        {
            auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
            string command (scriptData->path);
            command.append (" ").append(gnomeCmdFile->get_quoted_name());

            dirName = gnomeCmdFile->get_dirname (); // must be g_free'd
            run_command_indir (command.c_str (), dirName, scriptData->inTerminal);
            g_free(dirName);
        }
    }
    else
    {
        // Run script with list of files
        string commandString (scriptData->path);
        commandString.append (" ");
        for (auto files = scriptData->files; files; files = files->next)
        {
            auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
            commandString.append(gnomeCmdFile->get_quoted_name()).append(" ");
        }

        dirName = static_cast<GnomeCmdFile*> (scriptData->files->data)->get_dirname (); // must be g_free'd
        run_command_indir (commandString.c_str (), dirName, scriptData->inTerminal);
        g_free(dirName);
    }

    for (auto script = scriptData->allScripts; script; script = script->next)
    {
        g_free(((ScriptData*) script->data)->path); // this is the pathName of each script
    }
    g_list_free (scriptData->allScripts); // free all scriptData structs
    g_free (scriptData);
}


static void on_rename (GtkMenuItem *item, gpointer not_used)
{
    gnome_cmd_file_list_show_rename_dialog (get_fl (ACTIVE));
}


static void on_properties (GtkMenuItem *item, gpointer not_used)
{
    gnome_cmd_file_list_show_properties_dialog (get_fl (ACTIVE));
}


static void add_fav_app_menu_item (GtkUIManager *uiManager, guint mergeIdFavApp, GnomeCmdApp *app, GList *files)
{
    g_return_if_fail(app != nullptr);
    g_return_if_fail(g_list_length(files) != 0);

    OpenWithData *data = g_new0 (OpenWithData, 1);

    data->app = gnome_cmd_app_dup (app);
    data->files = files;

    auto iconPath = gnome_cmd_app_get_icon_path(app);
    auto appName = gnome_cmd_app_get_name(app);
    auto stockId = register_application_stock_icon(appName, iconPath);

    auto dynamicActionGroup = gtk_action_group_new (appName);
    gtk_ui_manager_insert_action_group (uiManager, dynamicActionGroup, 0);

    auto dynAction = gtk_action_new (appName, appName, nullptr, stockId);
    g_signal_connect (dynAction, "activate", G_CALLBACK (cb_exec_with_app), data);
    gtk_action_group_add_action (dynamicActionGroup, dynAction);
    gtk_ui_manager_add_ui (uiManager, mergeIdFavApp, "/FilePopup/FavoriteApps", appName, appName,
                           GTK_UI_MANAGER_AUTO, true);
    g_free(stockId);
}


inline gboolean fav_app_matches_files (GnomeCmdApp *app, GList *files)
{

    switch (gnome_cmd_app_get_target (app))
    {
        case APP_TARGET_ALL_DIRS:
            for (; files; files = files->next)
            {
                auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
                if (gnomeCmdFile->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE)
                      != G_FILE_TYPE_DIRECTORY)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_ALL_FILES:
            for (; files; files = files->next)
            {
                auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
                if (gnomeCmdFile->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE)
                      != G_FILE_TYPE_REGULAR)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_ALL_DIRS_AND_FILES:
            for (; files; files = files->next)
            {
                auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
                auto gFileType = gnomeCmdFile->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE);
                if (gFileType != G_FILE_TYPE_REGULAR && gFileType != G_FILE_TYPE_DIRECTORY)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_SOME_FILES:
            for (; files; files = files->next)
            {
                gboolean ok = FALSE;
#ifdef FNM_CASEFOLD
                gint fn_flags = FNM_NOESCAPE | FNM_CASEFOLD;
#else
                gint fn_flags = FNM_NOESCAPE;
#endif

                auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
                if (gnomeCmdFile->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE)
                      != G_FILE_TYPE_REGULAR)
                    return FALSE;

                // Check that the file matches at least one pattern
                GList *patterns = gnome_cmd_app_get_pattern_list (app);
                for (; patterns; patterns = patterns->next)
                {
                    auto pattern = (gchar *) patterns->data;
                    ok |= fnmatch (pattern, g_file_info_get_display_name(gnomeCmdFile->gFileInfo), fn_flags) == 0;
                }

                if (!ok) return FALSE;
            }
            return TRUE;

        default:
            break;
    }

    return FALSE;
}


inline void add_plugin_menu_items (GtkMenuShell *menu, GList *items, gint pos)
{
    for (; items; items = items->next)
    {
        GtkWidget *item = GTK_WIDGET (items->data);
        gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, pos);
    }
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdFilePopmenu *menu = GNOME_CMD_FILE_POPMENU (object);

    g_list_foreach (menu->priv->data_list, (GFunc) g_free, nullptr);
    g_list_free (menu->priv->data_list);

    g_free(menu->scriptsDir);

    g_free (menu->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != nullptr)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdFilePopmenuClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkMenuClass *) gtk_type_class (gtk_menu_get_type ());

    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdFilePopmenu *menu)
{
    menu->priv = g_new0 (GnomeCmdFilePopmenuPrivate, 1);

    menu->scriptsDir = g_build_filename (g_get_user_config_dir (), PACKAGE SCRIPT_DIRECTORY, nullptr);

    // Create scripts directory if needed
    if (!is_dir_existing(menu->scriptsDir))
    {
        create_dir(menu->scriptsDir);
    }
}


inline GList *get_list_of_action_script_file_names(const gchar* scriptsDir)
{
    g_return_val_if_fail (scriptsDir != nullptr, nullptr);

    auto gFileInfoList = sync_dir_list(scriptsDir);

    if (g_list_length (gFileInfoList) == 0)
        return nullptr;

    GList *scriptList = nullptr;
    for (auto gFileInfoListItem = gFileInfoList; gFileInfoListItem; gFileInfoListItem = gFileInfoListItem->next)
    {
        auto gFileInfo = (GFileInfo*) gFileInfoListItem->data;
        if (g_file_info_get_file_type(gFileInfo) == G_FILE_TYPE_REGULAR)
        {
            DEBUG('p', "Adding \'%s\' to the list of scripts.\n", g_file_info_get_name(gFileInfo));
            scriptList = g_list_append (scriptList, g_strdup(g_file_info_get_name(gFileInfo)));
        }

    }
    g_list_free_full(gFileInfoList, g_object_unref);
    return scriptList;
}

guint add_action_script_entries(GtkUIManager *uiManager, GList *files)
{
    guint position = 0;
    auto scriptsDir = g_build_filename (g_get_user_config_dir (), SCRIPT_DIRECTORY, nullptr);
    auto scriptFileNames = get_list_of_action_script_file_names(scriptsDir);

    // This list is used to store a reference to each ScriptData structure
    // It can be used to free the memory for each scriptData structure later on...
    GList* scriptDataList = nullptr;

    static guint mergeIdActionScripts = 0;
    if (mergeIdActionScripts != 0)
    {
        gtk_ui_manager_remove_ui (uiManager, mergeIdActionScripts);
        mergeIdActionScripts = 0;
    }
    mergeIdActionScripts = gtk_ui_manager_new_merge_id (uiManager);

    static auto gtkActionGroup = gtk_action_group_new ("actionScripts");
    gtk_ui_manager_insert_action_group (uiManager, gtkActionGroup, 0);

    for (GList *scriptFileName = scriptFileNames; scriptFileName; scriptFileName = scriptFileName->next)
    {
        auto scriptData = g_new0 (ScriptData, 1);
        scriptData->files = files;

        // store a reference to a list of all scripts in scriptData->allScripts
        scriptDataList = g_list_append (scriptDataList, scriptData);
        scriptData->allScripts = scriptDataList;

        string scriptPath (scriptsDir);
        scriptPath.append ("/").append ((char*) scriptFileName->data);
        scriptData->path = g_strdup (scriptPath.c_str());

        // Try to get the scriptname out of the script, otherwise take the filename
        char *scriptName = nullptr;
        FILE *fileStream = fopen (scriptData->path, "r");
        if (fileStream)
        {
            char buf[256];
            while (fgets (buf, 256, fileStream))
            {
                if (strncmp (buf, "#name:", 6) == 0)
                {
                    buf[strlen (buf)-1] = '\0';
                    // Script name given inside the script file
                    scriptName = g_strdup (buf+7);
                }
                else if (strncmp (buf, "#term:", 6) == 0)
                {
                    scriptData->inTerminal = strncmp (buf + 7, "true", 4) == 0;
                }
            }
            fclose (fileStream);
        }
        scriptName = scriptName ? scriptName : g_strdup((char*) scriptFileName->data);

        auto dynAction = gtk_action_new (scriptName, scriptName, nullptr, GTK_STOCK_EXECUTE);

        // remove action if it was added in a previous run of this method
        if (gtk_action_group_get_action (gtkActionGroup, scriptName))
        {
            gtk_action_group_remove_action(gtkActionGroup, dynAction);
        }

        g_signal_connect (dynAction, "activate", G_CALLBACK (on_execute_script), scriptData);
        gtk_action_group_add_action (gtkActionGroup, dynAction);

        gtk_ui_manager_add_ui (uiManager, mergeIdActionScripts, "/FilePopup/ActionScripts", scriptName,
                               scriptName, GTK_UI_MANAGER_AUTO, true);
        g_free(scriptName);
        g_free(scriptData->path);
        g_free(scriptData);
        position++;
    }
    g_free (scriptsDir);
    g_list_free_full(scriptFileNames, g_free);

    return position;
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_file_popmenu_new (GnomeCmdFileList *gnomeCmdFileList)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (gnomeCmdFileList), nullptr);

    auto files = gnomeCmdFileList->get_selected_files();
    if (!files) return nullptr;

    guint position;
    auto uiManager = get_file_popup_ui_manager(gnomeCmdFileList);

    // Add "Open With" popup entries
    position = add_open_with_entries(uiManager, gnomeCmdFileList);
    position += 2;

    // Add plugin popup entries
    for (auto plugins = plugin_manager_get_all (); plugins; plugins = plugins->next)
    {
        auto pluginData = static_cast<PluginData*> (plugins->data);
        if (pluginData->active)
        {
            GList *items = gnome_cmd_plugin_create_popup_menu_items (pluginData->plugin, main_win->get_state());
            if (items)
            {
                GtkWidget *menu = gtk_ui_manager_get_widget (uiManager, "/FilePopup");
                add_plugin_menu_items (GTK_MENU_SHELL(menu), items, ++position);
            }
        }
    }

    static guint mergeIdFavApp = 0;
    // Add favorite applications menu entries
    if (mergeIdFavApp != 0)
    {
        gtk_ui_manager_remove_ui (uiManager, mergeIdFavApp);
        mergeIdFavApp = 0;
    }
    mergeIdFavApp = gtk_ui_manager_new_merge_id (uiManager);
    for (auto j = gnome_cmd_data.options.fav_apps; j; j = j->next)
    {
        auto app = static_cast<GnomeCmdApp*> (j->data);
        if (fav_app_matches_files (app, files))
        {
            add_fav_app_menu_item (uiManager, mergeIdFavApp, app, files);
            position++;
        }
    }

    // Add script action menu entries
    add_action_script_entries(uiManager, files);

    // Add execute menu entry
    add_execute_entry(uiManager, files);

    return GTK_WIDGET (gtk_ui_manager_get_widget (uiManager, "/FilePopup"));
}


GtkType gnome_cmd_file_popmenu_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            (gchar*) "GnomeCmdFilePopmenu",
            sizeof (GnomeCmdFilePopmenu),
            sizeof (GnomeCmdFilePopmenuClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ nullptr,
            /* reserved_2 */ nullptr,
            (GtkClassInitFunc) nullptr
        };

        dlg_type = gtk_type_unique (gtk_menu_get_type (), &dlg_info);
    }
    return dlg_type;
}

/**
 * In this ui_manager the generic popup entries are placed. Dynamic entries will be added later.
 */
 GtkUIManager *get_file_popup_ui_manager(GnomeCmdFileList *gnomeCmdFileList)
{
    auto files = gnomeCmdFileList->get_selected_files();
    static const GtkActionEntry entries[] =
    {
        { "OpenWith",      nullptr,                 _("Open Wit_h") },
        { "OtherApp",      nullptr,                 _("Other _Application…"), nullptr, nullptr, (GCallback) on_open_with_other },
        { "OpenWithOther", nullptr,                 _("Open Wit_h…"),         nullptr, nullptr, (GCallback) on_open_with_other },
        { "Cut",           GTK_STOCK_CUT,           _("Cut"),                 nullptr, nullptr, (GCallback) edit_cap_cut },
        { "Copy",          GTK_STOCK_COPY,          _("Copy"),                nullptr, nullptr, (GCallback) edit_cap_copy },
        { "Delete",        GTK_STOCK_DELETE,        _("Delete"),              nullptr, nullptr, (GCallback) file_delete },
        { "Rename",        GTK_STOCK_EDIT,          _("Rename"),              nullptr, nullptr, (GCallback) on_rename },
        { "Send",          MAILSEND_STOCKID,        _("Send files"),          nullptr, nullptr, (GCallback) file_sendto },
        { "Properties",    GTK_STOCK_PROPERTIES,    _("_Properties…"),        nullptr, nullptr, (GCallback) on_properties },
        { "Execute",       GTK_STOCK_EXECUTE,       _("E_xecute"),            nullptr, nullptr, (GCallback) on_execute },
        { "Terminal",      TERMINAL_STOCKID,        _("Open _terminal here"), nullptr, nullptr, (GCallback) command_open_terminal__internal },
        { "CopyFileNames", COPYFILENAMES_STOCKID,   _("Copy file names"),     nullptr, nullptr, (GCallback) edit_copy_fnames },
    };

    static const char *ui_description =
    "<ui>"
    "  <popup action='FilePopup'>"
    "    <placeholder name='Execute'/>"
    "    <separator/>"
    "    <placeholder name='OpenWithDefault'/>"
    "    <menu action='OpenWith'>"
    "      <placeholder name='OpenWithPlaceholder'/>"
    "    </menu>"
    "    <separator/>"
    "    <placeholder name='FavoriteApps'/>"
    "    <separator/>"
    "    <placeholder name='ActionScripts'/>"
    "    <separator/>"
    "    <menuitem action='Cut'/>"
    "    <menuitem action='Copy'/>"
    "    <menuitem action='CopyFileNames'/>"
    "    <menuitem action='Delete'/>"
    "    <menuitem action='Rename'/>"
    "    <menuitem action='Send'/>"
    "    <menuitem action='Terminal'/>"
    "    <separator/>"
    "    <menuitem action='Properties'/>"
    "  </popup>"
    "</ui>";

    GtkActionGroup *action_group;
    GtkUIManager *ui_manager;
    GError *error;

    action_group = gtk_action_group_new ("PopupActions");
    gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), files);

    ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

    error = nullptr;
    if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &error))
    {
        g_message ("building menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
    }

    return ui_manager;
}

/**
 * This method adds all "open" popup entries (for opening the selected files
 * or folders with dedicated programs) into the given GtkUIManager.
 * Returns: the number of new entries in the main level of the popup menu
 */
guint add_open_with_entries(GtkUIManager *ui_manager, GnomeCmdFileList *gnomeCmdFileList)
{
    guint newTopMenuItems = 0;
    GtkAction *dynAction;
    static guint mergeIdOpenWith = 0;
    static GtkActionGroup *dynamicActionGroup = nullptr;
    auto files = gnomeCmdFileList->get_selected_files();
    auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);

    if (mergeIdOpenWith != 0)
    {
        gtk_ui_manager_remove_ui (ui_manager, mergeIdOpenWith);
        mergeIdOpenWith = 0;
    }

    mergeIdOpenWith = gtk_ui_manager_new_merge_id (ui_manager);
    dynamicActionGroup = gtk_action_group_new ("dynamicActionGroup");
    gtk_ui_manager_insert_action_group (ui_manager, dynamicActionGroup, 0);

    gchar *defaultAppIconPath = nullptr;
    gchar *appStockId = nullptr;
    gchar *openWithDefaultAppName = nullptr;
    gchar *openWithDefaultAppLabel = nullptr;

    // Only try to find a default application for the first file in the list of selected files
    openWithDefaultAppName  = gnomeCmdFile->GetDefaultApplicationNameString();
    auto gAppInfo = gnomeCmdFile->GetAppInfoForContentType();
    openWithDefaultAppLabel = gnomeCmdFile->get_default_application_action_label(gAppInfo);

    if (openWithDefaultAppName != nullptr)
    {
        // Add the default "Open" menu entry at the top of the popup
        defaultAppIconPath = g_strdup(get_default_application_icon_path(gAppInfo));
        appStockId = register_application_stock_icon(openWithDefaultAppName, defaultAppIconPath);
        dynAction = gtk_action_new ("Open", openWithDefaultAppLabel, nullptr, appStockId);
        g_signal_connect (dynAction, "activate", G_CALLBACK (cb_exec_default), files);
        gtk_action_group_add_action (dynamicActionGroup, dynAction);
        gtk_ui_manager_add_ui (ui_manager, mergeIdOpenWith, "/FilePopup/OpenWithDefault", openWithDefaultAppLabel, "Open",
                               GTK_UI_MANAGER_AUTO, true);
        g_free (defaultAppIconPath);
        g_free(appStockId);
    }
    else
    {
        DEBUG ('u', "No default application found for the MIME type of: %s\n", gnomeCmdFile->get_name());
        gtk_ui_manager_add_ui (ui_manager, mergeIdOpenWith, "/FilePopup/OpenWithDefault", "OpenWithOther", "OpenWithOther",
                               GTK_UI_MANAGER_AUTO, true);
        gtk_ui_manager_add_ui (ui_manager, mergeIdOpenWith, "/FilePopup/OpenWithDefault", "dynsep", nullptr,
                               GTK_UI_MANAGER_SEPARATOR, false);
    }
    newTopMenuItems++;
    g_free(openWithDefaultAppLabel);

    // Add menu items in the "Open with" submenu
    gint ii = -1;
    GList *gAppInfos, *tmp_list;
    auto contentTypeString = gnomeCmdFile->GetContentType();
    gAppInfos = tmp_list = g_app_info_get_all_for_type(contentTypeString);
    g_free(contentTypeString);
    for (; gAppInfos && ii < MAX_OPEN_WITH_APPS; gAppInfos = gAppInfos->next)
    {
        auto gAppInfoItem = (GAppInfo *) gAppInfos->data;

        if (gAppInfoItem)
        {
            OpenWithData *data = g_new0 (OpenWithData, 1);

            data->files = files;
            data->app = gnome_cmd_app_new_from_app_info (gAppInfoItem);

            gchar* openWithAppName  = g_strdup (gnome_cmd_app_get_name (data->app));

            // Only add the entry to the submenu if its name different from the default app
            if (openWithDefaultAppName != nullptr
                && strcmp(openWithAppName, openWithDefaultAppName) == 0)
            {
                gnome_cmd_app_free(data->app);
                g_free(data);
                g_free(openWithAppName);
                continue;
            }
            appStockId = register_application_stock_icon(openWithAppName, gnome_cmd_app_get_icon_path(data->app));
            dynAction = gtk_action_new (openWithAppName, openWithAppName, nullptr, appStockId);
            g_signal_connect (dynAction, "activate", G_CALLBACK (cb_exec_with_app), data);
            gtk_action_group_add_action (dynamicActionGroup, dynAction);
            gtk_ui_manager_add_ui (ui_manager, mergeIdOpenWith, "/FilePopup/OpenWith", openWithAppName, openWithAppName,
                                   GTK_UI_MANAGER_AUTO, true);
            ii++;
            g_free(appStockId);
            g_free(openWithAppName);
        }
    }

    // If the number of mime apps is zero, we have added an "OpenWith" entry already further above
    if (g_list_length(tmp_list) > 0)
    {
        gtk_ui_manager_add_ui (ui_manager, mergeIdOpenWith, "/FilePopup/OpenWith", "dynsep", nullptr,
                               GTK_UI_MANAGER_SEPARATOR, false);
        gtk_ui_manager_add_ui (ui_manager, mergeIdOpenWith, "/FilePopup/OpenWith", "OtherApp", "OtherApp",
                               GTK_UI_MANAGER_AUTO, false);
        newTopMenuItems++;
    }

    g_free(openWithDefaultAppName);
    g_list_free (tmp_list);
    return newTopMenuItems;
}

void add_execute_entry(GtkUIManager *ui_manager, GList *files)
{
    static guint mergeIdExecute = 0;
    static GtkActionGroup *dynamicActionGroup = nullptr;
    auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);

    if (mergeIdExecute != 0)
    {
        gtk_ui_manager_remove_ui (ui_manager, mergeIdExecute);
        mergeIdExecute = 0;
    }

    if (gnomeCmdFile->is_executable() && g_list_length (files) == 1)
    {
        mergeIdExecute = gtk_ui_manager_new_merge_id (ui_manager);
        dynamicActionGroup = gtk_action_group_new ("executeActionGroup");
        gtk_ui_manager_insert_action_group (ui_manager, dynamicActionGroup, 0);

        gtk_ui_manager_add_ui (ui_manager, mergeIdExecute, "/FilePopup/Execute", "Execute", "Execute",
                               GTK_UI_MANAGER_AUTO, true);
    }
}
