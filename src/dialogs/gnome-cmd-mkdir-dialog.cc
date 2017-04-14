/** 
 * @file gnome-cmd-mkdir-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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

using namespace std;


inline GSList *make_uri_list (GnomeCmdDir *dir, string filename)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    // make an absolute filename from one that is starting with a tilde
    if (filename.compare(0, 2, "~/")==0)
    {
        if (gnome_cmd_dir_is_local (dir))
            stringify (filename, gnome_vfs_expand_initial_tilde (filename.c_str()));
        else
            filename.erase(0,1);
    }

#ifdef HAVE_SAMBA
    // smb exception handling: test if we are in a samba share...
    // if not - change filename so that we can get a proper error message
    GnomeVFSURI *dir_uri = gnome_cmd_dir_get_uri (dir);

    if (strcmp (gnome_vfs_uri_get_scheme (dir_uri), "smb")==0 && g_path_is_absolute (filename.c_str()))
    {
        string mime_type = stringify (gnome_vfs_get_mime_type (gnome_vfs_uri_to_string (dir_uri, GNOME_VFS_URI_HIDE_PASSWORD)));

        if (mime_type=="x-directory/normal" && !gnome_vfs_uri_has_parent (dir_uri))
            filename.erase(0,1);
    }
    gnome_vfs_uri_unref (dir_uri);
#endif

    GSList *uri_list = NULL;

    if (g_path_is_absolute (filename.c_str()))
        while (filename.compare("/")!=0)
        {
            uri_list = g_slist_prepend (uri_list, gnome_cmd_dir_get_absolute_path_uri (dir, filename));
            stringify (filename, g_path_get_dirname (filename.c_str()));
        }
    else
        while (filename.compare(".")!=0)        // support for mkdir -p
        {
            uri_list = g_slist_prepend (uri_list, gnome_cmd_dir_get_child_uri (dir, filename.c_str()));
            stringify (filename, g_path_get_dirname (filename.c_str()));
        }

    return uri_list;
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
                    GnomeVFSURI *dir_uri = gnome_cmd_dir_get_uri (dir);
                    gboolean new_dir_focused = FALSE;

                    // the list of uri's to be created
                    GSList *uri_list = make_uri_list (dir, filename);

                    GnomeVFSResult result = GNOME_VFS_OK;

                    guint perm = ((GNOME_VFS_PERM_USER_ALL | GNOME_VFS_PERM_GROUP_ALL | GNOME_VFS_PERM_OTHER_ALL) & ~gnome_cmd_data.umask ) | GNOME_VFS_PERM_USER_WRITE | GNOME_VFS_PERM_USER_EXEC;

                    for (GSList *i = uri_list; i; i = g_slist_next (i))
                    {
                        GnomeVFSURI *mkdir_uri = (GnomeVFSURI *) i->data;

                        result = gnome_vfs_make_directory_for_uri (mkdir_uri, perm);

                        if (result!=GNOME_VFS_OK)
                        {
                            string dirname = stringify (gnome_vfs_uri_extract_short_name (mkdir_uri));
                            gnome_cmd_show_message (GTK_WINDOW (dialog), dirname, gnome_vfs_result_to_string (result));
                            g_signal_stop_emission_by_name (dialog, "response");
                            break;
                        }

                        // focus the created directory (if possible)
                        if (gnome_vfs_uri_equal (gnome_vfs_uri_get_parent (mkdir_uri), dir_uri) == 1 && !new_dir_focused)
                        {
                            string focus_filename = stringify (gnome_vfs_uri_extract_short_name (mkdir_uri));
                            string mkdir_uri_str = stringify (gnome_vfs_uri_to_string (mkdir_uri, GNOME_VFS_URI_HIDE_PASSWORD));

                            gnome_cmd_dir_file_created (dir, mkdir_uri_str.c_str());
                            main_win->fs(ACTIVE)->file_list()->focus_file(focus_filename.c_str(), TRUE);
                            new_dir_focused = TRUE;
                        }
                    }

                    for (GSList *i = uri_list; i; i = g_slist_next (i))
                        gnome_vfs_uri_unref ((GnomeVFSURI *) i->data);

                    g_slist_free (uri_list);
                    gnome_vfs_uri_unref (dir_uri);
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
#if GTK_CHECK_VERSION (2, 14, 0)
    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
#endif

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

    // HIG defaults
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_box_set_spacing (GTK_BOX (content_area), 2);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    gtk_box_set_spacing (GTK_BOX (content_area),6);
#else
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area),6);
#endif
    GtkWidget *table, *label, *entry;

    table = gtk_table_new (3, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (table), 5);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_container_add (GTK_CONTAINER (content_area), table);
#else
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), table);
#endif

    label = gtk_label_new_with_mnemonic (_("Directory name:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

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
    gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 0, 1);

#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_widget_show_all (content_area);
#else
    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);
#endif

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), dir);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return result==GTK_RESPONSE_OK;
}
