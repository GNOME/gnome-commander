/**
 * @file gnome-cmd-mkdir-dialog.cc
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
#include "gnome-cmd-mkdir-dialog.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"
#include "errno.h"

using namespace std;


GSList *make_gfile_list (GnomeCmdDir *dir, string filename)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    // make an absolute filename from one that is starting with a tilde
    if (filename.compare(0, 2, "~/")==0)
    {
        if (gnome_cmd_dir_is_local (dir))
        {
            auto absolutePath = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", g_get_home_dir(), filename.substr(2).c_str());
            stringify (filename, absolutePath);
        }
        else
            filename.erase(0,1);
    }

#ifdef HAVE_SAMBA
    // smb exception handling: test if we are in a samba share...
    // if not - change filename so that we can get a proper error message
    auto dir_gFile = gnome_cmd_dir_get_gfile (dir);
    auto uriScheme = g_file_get_uri_scheme (dir_gFile);

    if (uriScheme && strcmp (uriScheme, "smb")==0 && g_path_is_absolute (filename.c_str()))
    {
        if (get_gfile_attribute_uint32(dir_gFile, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
            && !g_file_get_parent (dir_gFile))
        {
            filename.erase(0,1);
        }
    }
    g_free(uriScheme);
#endif

    GSList *gFile_list = NULL;

    if (g_path_is_absolute (filename.c_str()))
    {
        while (filename.compare("/") != 0)
        {
            gFile_list = g_slist_prepend (gFile_list, gnome_cmd_dir_get_absolute_path_gfile (dir, filename));
            stringify (filename, g_path_get_dirname (filename.c_str()));
        }
    }
    else
    {
        while (filename.compare("..") != 0 && filename.compare(".") != 0)
        {
            gFile_list = g_slist_prepend (gFile_list, gnome_cmd_dir_get_child_gfile (dir, filename.c_str()));
            stringify (filename, g_path_get_dirname (filename.c_str()));
        }
    }
    return gFile_list;
}


static void response_callback (GtkDialog *dialog, int response_id, GnomeCmdDir *dir)
{
    switch (response_id)
    {
        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-create-folder");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GTK_RESPONSE_OK:
            {
                const gchar *filename = gtk_entry_get_text (GTK_ENTRY (lookup_widget (GTK_WIDGET (dialog), "name")));

                // don't create any directory if no name was passed or cancel was selected
                if (!filename || *filename==0)
                {
                    gnome_cmd_show_message (GTK_WINDOW (dialog), _("A directory name must be entered"));
                    g_signal_stop_emission_by_name (dialog, "response");
                }
                else
                {
                    gboolean new_dir_focused = FALSE;

                    // the list of gFiles's to be created
                    GSList *gFileList = make_gfile_list (dir, filename);

                    for (GSList *i = gFileList; i; i = g_slist_next (i))
                    {
                        auto mkdir_gFile = (GFile *) i->data;
                        GError *error = nullptr;
                        // create the directory

                        if (!g_file_make_directory_with_parents (mkdir_gFile, nullptr, &error))
                        {
                            auto mkdirBasename = g_file_get_basename (mkdir_gFile);
                            string dirname = stringify (mkdirBasename);
                            auto msg = g_strdup_printf(_("Make directory failed: %s\n"), error->message);
                            gnome_cmd_show_message (GTK_WINDOW (dialog), dirname, msg);
                            g_free(msg);
                            g_error_free(error);
                            g_signal_stop_emission_by_name (dialog, "response");
                            break;
                        }

                        // focus the created directory (if possible)
                        auto parentGFile = g_file_get_parent (mkdir_gFile);
                        if (g_file_equal (parentGFile, gnome_cmd_dir_get_gfile (dir)) && !new_dir_focused)
                        {
                            string focus_filename = stringify (g_file_get_basename (mkdir_gFile));
                            string mkdir_uri_str = stringify (g_file_get_uri (mkdir_gFile));

                            gnome_cmd_dir_file_created (dir, mkdir_uri_str.c_str());
                            main_win->fs(ACTIVE)->file_list()->focus_file(focus_filename.c_str(), TRUE);
                            new_dir_focused = TRUE;
                        }
                        g_object_unref(parentGFile);
                    }

                    for (GSList *i = gFileList; i; i = g_slist_next (i))
                        g_object_unref ((GFile *) i->data);

                    g_slist_free (gFileList);
                }
            }
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            break;

        default :
            g_assert_not_reached ();
    }
}


gboolean gnome_cmd_mkdir_dialog_new (GnomeCmdDir *dir, GnomeCmdFile *selected_file)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Make Directory"), *main_win,
                                                     GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                     NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    // HIG defaults
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (content_area), 2);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    gtk_box_set_spacing (GTK_BOX (content_area),6);

    GtkWidget *grid, *label, *entry;

    grid = gtk_grid_new ();
    gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_container_add (GTK_CONTAINER (content_area), grid);

    label = gtk_label_new_with_mnemonic (_("Directory name:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    entry = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    if (selected_file)
    {
        if (GNOME_CMD_IS_DIR (selected_file))
            gtk_entry_set_text (GTK_ENTRY (entry), selected_file->get_name());
        else
        {
            gchar *fname = g_strdup (selected_file->get_name());

            char *ext = g_utf8_strrchr (fname, -1, '.');

            if (ext)
                *ext = 0;

            gtk_entry_set_text (GTK_ENTRY (entry), fname);

            g_free (fname);
        }
    }
    g_object_set_data (G_OBJECT (dialog), "name", entry);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

    gtk_widget_show_all (content_area);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), dir);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_window_destroy (GTK_WINDOW (dialog));

    return result==GTK_RESPONSE_OK;
}
