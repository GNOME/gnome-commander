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

