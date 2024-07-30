/**
 * @file gnome-cmd-file-selector-actions.cc
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

#include <fnmatch.h>

using namespace std;

static void do_mime_exec_multiple (gboolean success, gpointer user_data)
{
    gpointer *args = (gpointer *) user_data;
    auto gnomeCmdApp = static_cast<GnomeCmdApp*> (args[0]);
    auto files = static_cast<GList*> (args[1]);
    if (success && files)
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


static void mime_exec_multiple (GList *files, GnomeCmdApp *app, GtkWindow *parent_window)
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
        auto scheme = g_file_get_uri_scheme (gnomeCmdFile->get_file());

        if (g_strcmp0(scheme, "file") == 0)
        {
            localGFileList = g_list_append (localGFileList, gnomeCmdFile->get_file());
        }
        else
        {
            ++no_of_remote_files;
            if (gnome_cmd_app_get_handles_uris (app) && gnome_cmd_data.options.honor_expect_uris)
            {
                localGFileList = g_list_append (localGFileList, gnomeCmdFile->get_file());
            }
            else
            {
                if (!asked)
                {
                    gchar *msg = g_strdup_printf (ngettext("%s does not know how to open remote file. Do you want to download the file to a temporary location and then open it?",
                                                           "%s does not know how to open remote files. Do you want to download the files to a temporary location and then open them?", no_of_remote_files),
                                                  gnome_cmd_app_get_name (app));
                    retid = run_simple_dialog (parent_window, TRUE, GTK_MESSAGE_QUESTION, msg, "", -1, _("No"), _("Yes"), nullptr);
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
        gnome_cmd_tmp_download(parent_window,
                               srcGFileList,
                               destGFileList,
                               G_FILE_COPY_OVERWRITE,
                               do_mime_exec_multiple,
                               args);
    else
      do_mime_exec_multiple (TRUE, args);
}


/* Used by exec_default for each list of files that shares the same default application
 * This is a hash-table callback function
 */
static void htcb_exec_with_app (GAppInfo *appInfo, GList *files, gpointer user_data)
{
    GtkWindow *parent_window = GTK_WINDOW (user_data);

    GnomeCmdApp *app = gnome_cmd_app_new_from_app_info (appInfo);
    mime_exec_multiple (files, app, parent_window);
}


/**
 * Executes a list of files with the same application
 */
void gnome_cmd_file_selector_action_open_with (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const gchar *app_id = g_variant_get_string (parameter, nullptr);
    GnomeCmdFileList *file_list = static_cast<GnomeCmdFileList *>(user_data);

    auto files = file_list->get_selected_files();

    GAppInfo *appInfo = nullptr;
    for (GList *apps = g_app_info_get_all(); apps; apps = apps->next)
        if (!strcmp(g_app_info_get_id (G_APP_INFO (apps->data)), app_id))
        {
            appInfo = G_APP_INFO (apps->data);
            break;
        }

    g_return_if_fail (appInfo != nullptr);
    GnomeCmdApp *app = gnome_cmd_app_new_from_app_info (appInfo);

    GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (file_list)));
    mime_exec_multiple (files, app, parent_window);
}


static guint app_info_hash (gconstpointer appInfo)
{
    const char *id = g_app_info_get_id (G_APP_INFO (appInfo));
    return g_str_hash (id);
}


static gboolean app_info_equal_ids (gconstpointer v1, gconstpointer v2)
{
    return g_str_equal (
        g_app_info_get_id (G_APP_INFO (v1)),
        g_app_info_get_id (G_APP_INFO (v2)));
}


/* Iterates through all files and gets their default application.
 * All files with the same default app are grouped together and opened in one call.
 */
void gnome_cmd_file_selector_action_open_with_default (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GnomeCmdFileList *file_list = static_cast<GnomeCmdFileList *>(user_data);

    auto files = file_list->get_selected_files();

    auto gHashTable = g_hash_table_new (app_info_hash, app_info_equal_ids);

    for (; files; files = files->next)
    {
        auto file = static_cast<GnomeCmdFile*> (files->data);
        auto appInfo = file->GetAppInfoForContentType();

        if (g_app_info_get_commandline (appInfo))
        {
            auto data = static_cast<GList*> (g_hash_table_lookup (gHashTable, appInfo));

            if (data)
                g_hash_table_replace (gHashTable, appInfo, g_list_append (data, file));
            else
                g_hash_table_insert (gHashTable, appInfo, g_list_append (nullptr, file));
        }
        else
            gnome_cmd_show_message (nullptr,
                g_file_info_get_display_name(file->get_file_info()),
                _("Couldn’t retrieve MIME type of the file."));
    }

    GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (file_list)));
    g_hash_table_foreach (gHashTable, (GHFunc) htcb_exec_with_app, parent_window);

    g_hash_table_destroy (gHashTable);
}


static gboolean on_open_with_other_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdFileList *file_list)
{
    GtkWidget *term_check = lookup_widget (GTK_WIDGET (string_dialog), "term_check");
    bool in_terminal = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (term_check));

    if (!values[0] || strlen(values[0]) < 1)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("Invalid command")));
        return FALSE;
    }

    string cmdString = values[0];

    for (GList *filesTmp = file_list->get_selected_files(); filesTmp; filesTmp = filesTmp->next)
    {
        cmdString += ' ';
        cmdString += stringify (GNOME_CMD_FILE (filesTmp->data)->get_quoted_real_path());
    }

    gchar *dpath = GNOME_CMD_FILE (file_list->cwd)->get_real_path();
    auto returnValue = run_command_indir (cmdString.c_str(), dpath, in_terminal);
    if (!returnValue)
    {
        gnome_cmd_file_selector_action_open_with_other (nullptr, nullptr, file_list);
    }

    g_free (dpath);

    return TRUE;
}


void gnome_cmd_file_selector_action_open_with_other (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GnomeCmdFileList *file_list = static_cast<GnomeCmdFileList *>(user_data);

    const gchar *labels[] = {_("Application:")};
    GtkWidget *dialog;

    dialog = gnome_cmd_string_dialog_new (_("Open with other…"), labels, 1,
                                          (GnomeCmdStringDialogCallback) on_open_with_other_ok, file_list);

    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    GtkWidget *term_check = create_check (dialog, _("Needs terminal"), "term_check");

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), term_check);

    gtk_widget_show (dialog);
}


void gnome_cmd_file_selector_action_execute (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GnomeCmdFileList *file_list = static_cast<GnomeCmdFileList *>(user_data);

    auto files = file_list->get_selected_files();

    GnomeCmdFile *f = GNOME_CMD_FILE (files->data);

    f->execute();
}


void gnome_cmd_file_selector_action_execute_script (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GnomeCmdFileList *file_list = static_cast<GnomeCmdFileList *>(user_data);

    auto files = file_list->get_selected_files();

    const gchar *script_path;
    gboolean run_in_terminal;
    g_variant_get (parameter, "(sb)", &script_path, &run_in_terminal);

    GdkModifierType mask = get_modifiers_state();

    gchar *dirName = nullptr;

    if (state_is_shift (mask))
    {
        // Run script per file
        for (auto file = files; file; file = file->next)
        {
            auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
            string commandString (script_path);
            commandString.append (" ").append(gnomeCmdFile->get_quoted_name());

            dirName = gnomeCmdFile->get_dirname ();
            run_command_indir (commandString.c_str (), dirName, run_in_terminal);
            g_free(dirName);
        }
    }
    else
    {
        // Run script with list of files
        string commandString (script_path);
        for (auto file = files; file; file = file->next)
        {
            auto gnomeCmdFile = static_cast<GnomeCmdFile*> (files->data);
            commandString.append(" ").append(gnomeCmdFile->get_quoted_name());
        }

        dirName = static_cast<GnomeCmdFile*> (files->data)->get_dirname ();
        run_command_indir (commandString.c_str (), dirName, run_in_terminal);
        g_free(dirName);
    }
}

