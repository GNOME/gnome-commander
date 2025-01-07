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
#include "gnome-cmd-file-list-actions.h"
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
#include "widget-factory.h"

#include <fnmatch.h>

using namespace std;


#define MAX_OPEN_WITH_APPS 20


static GMenuItem *fav_app_menu_item (GnomeCmdApp *app)
{
    g_return_val_if_fail (app != nullptr, nullptr);

    auto appName = gnome_cmd_app_get_name(app);

    GMenuItem *item = g_menu_item_new (appName, nullptr);
    g_menu_item_set_action_and_target_value (item, "fl.open-with", gnome_cmd_app_save_to_variant (app));
    g_menu_item_set_icon (item, gnome_cmd_app_get_icon (app));

    return item;
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
                    ok |= fnmatch (pattern, g_file_info_get_display_name(gnomeCmdFile->get_file_info()), fn_flags) == 0;
                }

                if (!ok) return FALSE;
            }
            return TRUE;

        default:
            break;
    }

    return FALSE;
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


/** Try to get the script info out of the script */
static void extract_script_info (const gchar *script_path, gchar **script_name, gboolean *in_terminal)
{
    *script_name = nullptr;
    *in_terminal = FALSE;
    FILE *fileStream = fopen (script_path, "r");
    if (fileStream)
    {
        char buf[256];
        while (fgets (buf, 256, fileStream))
        {
            if (strncmp (buf, "#name:", 6) == 0)
            {
                buf[strlen (buf)-1] = '\0';
                // Script name given inside the script file
                *script_name = g_strdup (buf+7);
            }
            else if (strncmp (buf, "#term:", 6) == 0)
            {
                *in_terminal = strncmp (buf + 7, "true", 4) == 0;
            }
        }
        fclose (fileStream);
    }
}


static GMenu *create_action_script_menu(GList *files)
{
    auto scriptsDir = g_build_filename (g_get_user_config_dir (), SCRIPT_DIRECTORY, nullptr);
    auto scriptFileNames = get_list_of_action_script_file_names(scriptsDir);

    GMenu *menu = g_menu_new ();
    for (GList *scriptFileName = scriptFileNames; scriptFileName; scriptFileName = scriptFileName->next)
    {
        string scriptPath (scriptsDir);
        scriptPath.append ("/").append ((char*) scriptFileName->data);

        gboolean inTerminal = FALSE;

        // Try to get the scriptname out of the script, otherwise take the filename
        gchar *scriptName = nullptr;
        extract_script_info (scriptPath.c_str(), &scriptName, &inTerminal);
        scriptName = scriptName ? scriptName : g_strdup((char*) scriptFileName->data);

        GMenuItem *item = g_menu_item_new (scriptName, nullptr);
        g_menu_item_set_action_and_target (item, "fl.execute-script", "(sb)", scriptPath.c_str(), inTerminal);

        g_menu_append_item (menu, item);

        g_free(scriptName);
    }
    g_free (scriptsDir);
    g_list_free_full(scriptFileNames, g_free);

    return menu;
}


/**
 * This method adds all "open" popup entries (for opening the selected files
 * or folders with dedicated programs).
 */
static MenuBuilder add_open_with_entries(MenuBuilder menubuilder, GnomeCmdFileList *gnomeCmdFileList)
{
    auto files = gnomeCmdFileList->get_selected_files();
    auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);

    gchar *openWithDefaultAppName = nullptr;
    gchar *openWithDefaultAppLabel = nullptr;

    // Only try to find a default application for the first file in the list of selected files
    openWithDefaultAppName  = gnomeCmdFile->GetDefaultApplicationNameString();
    auto gAppInfo = gnomeCmdFile->GetAppInfoForContentType();
    openWithDefaultAppLabel = gnomeCmdFile->get_default_application_action_label(gAppInfo);

    if (openWithDefaultAppName != nullptr)
    {
        // Add the default "Open" menu entry at the top of the popup
        GMenuItem *item = g_menu_item_new (openWithDefaultAppLabel, "fl.open-with-default");
        g_menu_item_set_icon (item, g_app_info_get_icon (gAppInfo));

        menubuilder = std::move(menubuilder).item(item);
    }
    else
    {
        DEBUG ('u', "No default application found for the MIME type of: %s\n", gnomeCmdFile->get_name());
        menubuilder = std::move(menubuilder).item(_("Open Wit_h…"), "fl.open-with-other");
    }
    g_free(openWithDefaultAppLabel);

    // Add menu items in the "Open with" submenu
    auto submenubuilder = std::move(menubuilder).submenu(_("Open Wit_h"));

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
            GnomeCmdApp *app = gnome_cmd_app_new_from_app_info (gAppInfoItem);

            gchar* openWithAppName  = g_strdup (gnome_cmd_app_get_name (app));

            // Only add the entry to the submenu if its name different from the default app
            if (openWithDefaultAppName != nullptr
                && strcmp(openWithAppName, openWithDefaultAppName) == 0)
            {
                gnome_cmd_app_free(app);
                g_free(openWithAppName);
                continue;
            }

            GMenuItem *item = g_menu_item_new (openWithAppName, nullptr);
            g_menu_item_set_action_and_target_value (item, "fl.open-with", gnome_cmd_app_save_to_variant (app));
            g_menu_item_set_icon (item, g_app_info_get_icon (gAppInfoItem));

            submenubuilder = std::move(submenubuilder).item(item);

            ii++;
            g_free(openWithAppName);
        }
    }

    // If the number of mime apps is zero, we have added an "OpenWith" entry already further above
    if (g_list_length(tmp_list) > 0)
    {
        submenubuilder = std::move(submenubuilder)
            .item(_("Open Wit_h…"), "fl.open-with-other");
    }

    menubuilder = std::move(submenubuilder).endsubmenu();

    g_free(openWithDefaultAppName);
    g_list_free (tmp_list);

    return menubuilder;
}


/***********************************
 * Public functions
 ***********************************/

GMenu *gnome_cmd_file_popmenu_new (GnomeCmdMainWin *main_win, GnomeCmdFileList *gnomeCmdFileList)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (gnomeCmdFileList), nullptr);

    auto files = gnomeCmdFileList->get_selected_files();
    if (!files) return nullptr;

    auto menu_builder = MenuBuilder();

    // Add execute menu entry
    auto first_file = static_cast<GnomeCmdFile*> (files->data);
    if (first_file->is_executable() && g_list_length (files) == 1)
    {
        menu_builder = std::move(menu_builder)
            .section()
                .item(_("E_xecute"), "fl.execute", nullptr, "system-run")
            .endsection();
    }

    // Add "Open With" popup entries
    menu_builder = add_open_with_entries(menu_builder, gnomeCmdFileList);

    // Add plugin popup entries
    for (auto plugins = plugin_manager_get_all (); plugins; plugins = plugins->next)
    {
        auto pluginData = static_cast<PluginData*> (plugins->data);
        if (pluginData->active && GNOME_CMD_IS_FILE_ACTIONS (pluginData->plugin))
        {
            GMenuModel *plugin_menu = gnome_cmd_file_actions_create_popup_menu_items (GNOME_CMD_FILE_ACTIONS (pluginData->plugin), main_win->get_state());
            if (plugin_menu)
                menu_builder = std::move(menu_builder).section(gnome_cmd_wrap_plugin_menu (pluginData->action_group_name, G_MENU_MODEL (plugin_menu)));
        }
    }

    // Add favorite applications menu entries
    auto fav_menu = g_menu_new ();
    for (auto j = gnome_cmd_data.options.fav_apps; j; j = j->next)
    {
        auto app = static_cast<GnomeCmdApp*> (j->data);
        if (fav_app_matches_files (app, files))
            g_menu_append_item (fav_menu, fav_app_menu_item (app));
    }

    // Add script action menu entries
    auto menu = std::move(menu_builder)
        .section(fav_menu)
        .section(create_action_script_menu(files))
        .section()
            .item(_("Cut"),                 "win.edit-cap-cut")
            .item(_("Copy"),                "win.edit-cap-copy")
            .item(_("Copy file names"),     "win.edit-copy-fnames",                 nullptr, COPYFILENAMES_STOCKID)
            .item(_("Delete"),              "win.file-delete")
            .item(_("Rename"),              "win.file-rename")
            .item(_("Send files"),          "win.file-sendto",                      nullptr, GTK_MAILSEND_STOCKID)
            .item(_("Open _terminal here"), "win.command-open-terminal",            nullptr, GTK_TERMINAL_STOCKID)
        .endsection()
        .item(_("_Properties…"),            "win.file-properties")
        .build();

    return menu.menu;
}
