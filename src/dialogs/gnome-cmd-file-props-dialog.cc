/**
 * @file gnome-cmd-file-props-dialog.cc
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
#include "gnome-cmd-dir.h"
#include "gnome-cmd-con-device.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-chown-component.h"
#include "gnome-cmd-chmod-component.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-treeview.h"
#include "utils.h"
#include "imageloader.h"
#include "tags/gnome-cmd-tags.h"
#include "dialogs/gnome-cmd-file-props-dialog.h"

using namespace std;


struct GnomeCmdFilePropsDialogPrivate
{
    GtkWidget *dialog;
    GnomeCmdFile *f;
    GCancellable* cancellable;
    gboolean count_done;
    guint updater_proc_id;

    GtkWidget *notebook;
    GtkWidget *copy_button;

    // Properties tab stuff
    guint64 size;
    GFile *gFile;
    GtkWidget *filename_entry;
    GtkWidget *size_label;
    GtkWidget *app_label;

    // Permissions tab stuff
    GtkWidget *chown_component;
    GtkWidget *chmod_component;

};


static void on_dialog_destroy (GtkDialog *dialog, GnomeCmdFilePropsDialogPrivate *data)
{
    data->f->unref();

    if (data->updater_proc_id)
        g_source_remove (data->updater_proc_id);

    if (data->cancellable)
        g_cancellable_cancel (data->cancellable);
    else
        g_free (data);
}


static gboolean update_count_status (GnomeCmdFilePropsDialogPrivate *data)
{
    if (data->size_label && data->size != 0)
    {
        gchar *msg = create_nice_size_str (data->size);
        gtk_label_set_text (GTK_LABEL (data->size_label), msg);
        g_free(msg);
    }

    if (data->count_done)
    {
        data->updater_proc_id = 0;
        // Returning FALSE here stops the timeout callbacks
        return FALSE;
    }

    return TRUE;
}


static void g_file_measure_disk_usage_async_callback(GObject *unused, GAsyncResult *result, gpointer user_data)
{
    g_return_if_fail (user_data != nullptr);

    auto data = (GnomeCmdFilePropsDialogPrivate *) user_data;
    GError *error = nullptr;

    g_file_measure_disk_usage_finish (data->gFile, result, &(data->size), nullptr, nullptr, &error);

    if (error)
    {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            // async task was cancelled
            g_error_free(error);
            g_object_unref (data->cancellable);
            g_free (data);
            return;
        }

        g_message("g_file_measure_disk_usage_finish error: %s\n", error->message);
        g_error_free(error);
    }

    if (data->cancellable)
    {
        g_object_unref (data->cancellable);
        data->cancellable = nullptr;
    }

    data->count_done = TRUE;
}


static void progress_callback (gboolean reporting,
                                 guint64 current_size,
                                 guint64 num_dirs,
                                 guint64 num_files,
                                 gpointer user_data)
{
    auto data = (GnomeCmdFilePropsDialogPrivate *) user_data;

    data->size = current_size;
}


static void do_calc_tree_size (GnomeCmdFilePropsDialogPrivate *data)
{
    g_return_if_fail (data != nullptr);

    data->size = 0;
    data->count_done = FALSE;
    data->cancellable = g_cancellable_new();

    g_file_measure_disk_usage_async (data->gFile,
                                 G_FILE_MEASURE_NONE,
                                 G_PRIORITY_LOW,
                                 data->cancellable,
                                 progress_callback,
                                 data,
                                 g_file_measure_disk_usage_async_callback,
                                 data);

    data->updater_proc_id = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_count_status, data);
}


//----------------------------------------------------------------------------------


static void on_dialog_ok (GtkButton *btn, GnomeCmdFilePropsDialogPrivate *data)
{
    GError *error;
    error = nullptr;

    gboolean retValue = true;

    const gchar *filename = gtk_entry_get_text (GTK_ENTRY (data->filename_entry));

    if (strcmp (filename, data->f->get_name()) != 0)
    {
        retValue = data->f->rename(filename, &error);

        if (retValue)
            main_win->fs(ACTIVE)->file_list()->focus_file(filename, TRUE);
    }

    if (retValue)
    {
        auto perms = gnome_cmd_chmod_component_get_perms (GNOME_CMD_CHMOD_COMPONENT (data->chmod_component));

        if (perms != (get_gfile_attribute_uint32(data->f->get_file(), G_FILE_ATTRIBUTE_UNIX_MODE) & 0xFFF ))
            retValue = data->f->chmod(perms, &error);
    }

    if (retValue)
    {
        uid_t uid = gnome_cmd_chown_component_get_owner (GNOME_CMD_CHOWN_COMPONENT (data->chown_component));
        gid_t gid = gnome_cmd_chown_component_get_group (GNOME_CMD_CHOWN_COMPONENT (data->chown_component));

        if (   uid != get_gfile_attribute_uint32(data->f->get_file(), G_FILE_ATTRIBUTE_UNIX_UID)
            || gid != get_gfile_attribute_uint32(data->f->get_file(), G_FILE_ATTRIBUTE_UNIX_GID))
        {
            retValue = data->f->chown(uid, gid, &error);
        }
    }

    if (!retValue && error != nullptr)
    {
        gnome_cmd_show_message (nullptr, filename, error->message);
        g_error_free(error);
        return;
    }

    gtk_widget_destroy (data->dialog);
}


static void on_dialog_cancel (GtkButton *btn, GnomeCmdFilePropsDialogPrivate *data)
{
    gtk_widget_destroy (data->dialog);
}


static void on_copy_clipboard (GtkButton *button, GnomeCmdFilePropsDialogPrivate *data)
{
    g_return_if_fail (data != nullptr);

    string s;

    for (GnomeCmdFileMetadata::METADATA_COLL::const_iterator i=data->f->metadata->begin(); i!=data->f->metadata->end(); ++i)
        for (set<string>::const_iterator j=i->second.begin(); j!=i->second.end(); ++j)
        {
            s += gcmd_tags_get_name(i->first);
            s += '\t';
            s += gcmd_tags_get_title(i->first);
            s += '\t';
            s += *j;
            s += '\n';
        }

    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), s.data(), s.size());
}


static void on_dialog_help (GtkButton *button, GnomeCmdFilePropsDialogPrivate *data)
{
    switch (gtk_notebook_get_current_page ((GtkNotebook *) data->notebook))
    {
        case 0:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-file-properties");
            break;
        case 1:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-permissions");
            break;
        case 2:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-advanced-rename-metadata-tags");
            break;

        default:
            break;
    }
}


static void on_notebook_page_change (GtkNotebook *notebook, gpointer page, guint page_num, GnomeCmdFilePropsDialogPrivate *data)
{
    gtk_widget_set_sensitive (data->copy_button, page_num==2);
}


inline void add_sep (GtkWidget *grid, gint y)
{
    gtk_grid_attach (GTK_GRID (grid), create_hsep (grid), 0, y, 2, 1);
}


static void add_tag (GtkWidget *dialog, GtkWidget *grid, gint &y, GnomeCmdFileMetadata &metadata, GnomeCmdTag tag, const gchar *appended_text=nullptr)
{
    if (!metadata.has_tag (tag))
        return;

    GtkWidget *label;

    string title = gcmd_tags_get_title (tag);
    title += ':';

    label = create_bold_label (dialog, title.c_str());
    gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

    string value = truncate (metadata[tag],120);

    if (appended_text)
        value += appended_text;

    label = create_label (dialog, value.c_str());
    gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);
}


inline void add_width_height_tag (GtkWidget *dialog, GtkWidget *grid, gint &y, GnomeCmdFileMetadata &metadata)
{
    if (!metadata.has_tag (TAG_IMAGE_WIDTH) || !metadata.has_tag (TAG_IMAGE_HEIGHT))
        return;

    GtkWidget *label;

    label = create_bold_label (dialog, _("Image:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

    string value = metadata[TAG_IMAGE_WIDTH];
    value += " x ";
    value += metadata[TAG_IMAGE_HEIGHT];

    label = create_label (dialog, value.c_str());
    gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);
}


static GtkWidget *create_properties_tab (GnomeCmdFilePropsDialogPrivate *data)
{
    gint y = 0;
    GtkWidget *dialog = data->dialog;
    GtkWidget *grid;
    GtkWidget *label;

    GtkWidget *space_frame = create_space_frame (dialog, 6);

    grid = create_grid (dialog);
    gtk_container_add (GTK_CONTAINER (space_frame), grid);

    label = create_bold_label (dialog, GNOME_CMD_IS_DIR (data->f) ? _("Directory name:") : _("File name:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

    data->filename_entry = create_entry (dialog, "filename_entry", g_file_info_get_edit_name (data->f->get_file_info()));
    gtk_widget_set_hexpand (data->filename_entry, TRUE);
    gtk_grid_attach (GTK_GRID (grid), data->filename_entry, 1, y++, 1, 1);
    gtk_editable_set_position (GTK_EDITABLE (data->filename_entry), 0);

    if (g_file_info_get_is_symlink(data->f->get_file_info()))
    {
        label = create_bold_label (dialog, _("Symlink target:"));
        gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

        label = create_label (dialog, g_file_info_get_symlink_target (data->f->get_file_info()));
        gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);
    }

    add_sep (grid, y++);

    if (data->f->is_local())
    {
        GnomeCmdDir *dir = GNOME_CMD_IS_DIR (data->f) ? gnome_cmd_dir_get_parent (GNOME_CMD_DIR (data->f)) : data->f->get_parent_dir();
        GnomeCmdCon *con = dir ? gnome_cmd_dir_get_connection (dir) : nullptr;
        gchar *location = data->f->get_dirname();

        label = create_bold_label (dialog, _("Location:"));
        gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

        label = create_label (dialog, location);
        gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);

        g_free (location);

        label = create_bold_label (dialog, _("Volume:"));
        gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

        if (GNOME_CMD_IS_CON_DEVICE (con))
        {
            if (auto *gMount = gnome_cmd_con_device_get_gmount (GNOME_CMD_CON_DEVICE (con)))
            {
                gchar *dev_uuid = g_mount_get_uuid (gMount);

                gchar *s = g_strdup_printf ("%s (%s)", gnome_cmd_con_get_alias (con), dev_uuid);

                g_free (dev_uuid);

                label = create_label (dialog, s);

                g_free (s);
            }
            else
                label = create_label (dialog, gnome_cmd_con_get_alias (con));
        }
        else
            label = create_label (dialog, gnome_cmd_con_get_alias (con));

        gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);

        if (dir && gnome_cmd_con_can_show_free_space (con))
            if (gchar *free_space = gnome_cmd_dir_get_free_space (dir))
            {
                label = create_bold_label (dialog, _("Free space:"));
                gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

                label = create_label (dialog, free_space);
                gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);

                g_free (free_space);
            }

        add_sep (grid, y++);
    }

    label = create_bold_label (dialog, _("Content Type:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

    auto contentTypeString = data->f->GetGfileAttributeString(G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
    label = create_label (dialog, contentTypeString);
    gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);
    g_free(contentTypeString);

    if (data->f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY)
    {
        GtkWidget *hbox;

        label = create_bold_label (dialog, _("Opens with:"));
        gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

        auto default_application_string = data->f->GetDefaultApplicationNameString();

        if (default_application_string)
        {
            data->app_label = label = create_label (dialog, default_application_string);
            g_free (default_application_string);
        }
        else
            label = create_label (dialog, _("No default application registered"));

        hbox = create_hbox (dialog, FALSE, 6);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
        label = create_label (dialog, " ");
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
        gtk_grid_attach (GTK_GRID (grid), hbox, 1, y++, 1, 1);
    }

    add_sep (grid, y++);

    label = create_bold_label (dialog, _("Modified:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

    label = create_label (dialog, data->f->get_mdate(TRUE));
    gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);

#ifdef GLIB_2_70
    label = create_bold_label (dialog, _("Accessed:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

    label = create_label (dialog, data->f->get_adate(TRUE));
    gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);
#endif

    add_sep (grid, y++);


    label = create_bold_label (dialog, _("Size:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, y, 1, 1);

    gchar *s = create_nice_size_str (g_file_info_get_size (data->f->get_file_info()));
    label = create_label (dialog, s);
    gtk_grid_attach (GTK_GRID (grid), label, 1, y++, 1, 1);
    g_free (s);
    if (data->f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        do_calc_tree_size (data);

    data->size_label = label;

    if (data->f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_SPECIAL)
        gcmd_tags_bulk_load (data->f);

    if (data->f->metadata)
    {
        add_tag (dialog, grid, y, *data->f->metadata, TAG_FILE_DESCRIPTION);
        add_tag (dialog, grid, y, *data->f->metadata, TAG_FILE_PUBLISHER);
        add_tag (dialog, grid, y, *data->f->metadata, TAG_DOC_TITLE);
        add_tag (dialog, grid, y, *data->f->metadata, TAG_DOC_PAGECOUNT);
        add_width_height_tag (dialog, grid, y, *data->f->metadata);
        add_tag (dialog, grid, y, *data->f->metadata, TAG_AUDIO_ALBUMARTIST);
        add_tag (dialog, grid, y, *data->f->metadata, TAG_AUDIO_TITLE);
        add_tag (dialog, grid, y, *data->f->metadata, TAG_AUDIO_BITRATE, " kbps");
        add_tag (dialog, grid, y, *data->f->metadata, TAG_AUDIO_DURATIONMMSS);
    }

    return space_frame;
}


inline GtkWidget *create_permissions_tab (GnomeCmdFilePropsDialogPrivate *data)
{
    GtkWidget *vbox = create_vbox (data->dialog, FALSE, 6);

    GtkWidget *space_frame = create_space_frame (data->dialog, 6);
    gtk_container_add (GTK_CONTAINER (space_frame), vbox);

    data->chown_component = gnome_cmd_chown_component_new ();
    g_object_ref (data->chown_component);
    g_object_set_data_full (G_OBJECT (data->dialog), "chown_component", data->chown_component, g_object_unref);
    gtk_widget_show (data->chown_component);
    gnome_cmd_chown_component_set (GNOME_CMD_CHOWN_COMPONENT (data->chown_component),
        get_gfile_attribute_uint32(data->f->get_file(), G_FILE_ATTRIBUTE_UNIX_UID),
        get_gfile_attribute_uint32(data->f->get_file(), G_FILE_ATTRIBUTE_UNIX_GID));

    GtkWidget *cat = create_category (data->dialog, data->chown_component, _("Owner and group"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);


    data->chmod_component = gnome_cmd_chmod_component_new (0);
    g_object_ref (data->chmod_component);
    g_object_set_data_full (G_OBJECT (data->dialog), "chmod_component", data->chmod_component, g_object_unref);
    gtk_widget_show (data->chmod_component);
    gnome_cmd_chmod_component_set_perms (GNOME_CMD_CHMOD_COMPONENT (data->chmod_component),
        get_gfile_attribute_uint32(data->f->get_file(), G_FILE_ATTRIBUTE_UNIX_MODE) & 0xFFF);

    cat = create_category (data->dialog, data->chmod_component, _("Access permissions"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    return space_frame;
}


enum
{
    COL_TAG,
    COL_TYPE,
    COL_NAME,
    COL_VALUE,
    COL_DESC,
    NUM_COLS
} ;


static GtkTreeModel *create_and_fill_model (GnomeCmdFile *f)
{
    GtkTreeStore *treestore = gtk_tree_store_new (NUM_COLS,
                                                  G_TYPE_UINT,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING);

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_SPECIAL || !gcmd_tags_bulk_load (f))
        return GTK_TREE_MODEL (treestore);

    GnomeCmdTagClass prev_tagclass = TAG_NONE_CLASS;

    GtkTreeIter toplevel;

    for (GnomeCmdFileMetadata::METADATA_COLL::const_iterator i=f->metadata->begin(); i!=f->metadata->end(); ++i)
    {
        const GnomeCmdTag t = i->first;
        GnomeCmdTagClass curr_tagclass = gcmd_tags_get_class(t);

        if (curr_tagclass==TAG_NONE_CLASS)
            continue;

        if (prev_tagclass!=curr_tagclass)
        {
            gtk_tree_store_append (treestore, &toplevel, nullptr);
            gtk_tree_store_set (treestore, &toplevel,
                                COL_TAG, TAG_NONE,
                                COL_TYPE, gcmd_tags_get_class_name(t),
                                -1);
        }

        for (set<string>::const_iterator j=i->second.begin(); j!=i->second.end(); ++j)
        {
            GtkTreeIter child;

            gtk_tree_store_append (treestore, &child, &toplevel);
            gtk_tree_store_set (treestore, &child,
                                COL_TAG, t,
                                COL_NAME, gcmd_tags_get_title(t),
                                COL_VALUE, j->c_str(),
                                COL_DESC, gcmd_tags_get_description(t),
                                -1);
        }

        prev_tagclass = curr_tagclass;
    }

    return GTK_TREE_MODEL (treestore);
}


static GtkWidget *create_view_and_model (GnomeCmdFile *f)
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "enable-search", TRUE,
                  "search-column", COL_VALUE,
                  nullptr);

    GtkCellRenderer *renderer = nullptr;
    GtkTreeViewColumn *col = nullptr;

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_TYPE, _("Type"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Metadata namespace"));

    g_object_set (renderer,
                  "weight-set", TRUE,
                  "weight", PANGO_WEIGHT_BOLD,
                  nullptr);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), COL_NAME, _("Name"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Tag name"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), COL_VALUE, _("Value"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Tag value"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_DESC, _("Description"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Metadata tag description"));

    g_object_set (renderer,
                  "foreground-set", TRUE,
                  "foreground", "DarkGray",
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  nullptr);

    GtkTreeModel *model = create_and_fill_model (f);

    gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

    g_object_unref (model);          // destroy model automatically with view

    gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)), GTK_SELECTION_NONE);

    return view;
}


inline GtkWidget *create_metadata_tab (GnomeCmdFilePropsDialogPrivate *data)
{
    GtkWidget *vbox = create_vbox (data->dialog, FALSE, 1);

    GtkWidget *space_frame = create_space_frame (data->dialog, 1);
    gtk_container_add (GTK_CONTAINER (space_frame), vbox);

    GtkWidget *scrolledwindow = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 10);

    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (scrolledwindow), TRUE, TRUE, 0);

    GtkWidget *view = create_view_and_model (data->f);

    gtk_container_add (GTK_CONTAINER (scrolledwindow), view);

    gtk_widget_show_all (vbox);

    return space_frame;
}


GtkWidget *gnome_cmd_file_props_dialog_create (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, nullptr);
    g_return_val_if_fail (f->get_file_info() != nullptr, nullptr);

    if (f->is_dotdot)
        return nullptr;

    GnomeCmdFilePropsDialogPrivate *data = g_new0 (GnomeCmdFilePropsDialogPrivate, 1);

    GtkWidget *dialog = gnome_cmd_dialog_new (_("File Properties"));
    g_signal_connect (dialog, "destroy", G_CALLBACK (on_dialog_destroy), data);
    gtk_window_set_resizable(GTK_WINDOW (dialog), TRUE);

    GtkWidget *notebook = gtk_notebook_new ();

    data->dialog = GTK_WIDGET (dialog);
    data->f = f;
    data->gFile = f->get_gfile();
    data->notebook = notebook;
    f->ref();

    g_object_ref (notebook);
    g_object_set_data_full (G_OBJECT (dialog), "notebook", notebook, g_object_unref);
    gtk_widget_show (notebook);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), notebook);

    gtk_container_add (GTK_CONTAINER (notebook), create_properties_tab (data));
    gtk_container_add (GTK_CONTAINER (notebook), create_permissions_tab (data));
    gtk_container_add (GTK_CONTAINER (notebook), create_metadata_tab (data));

    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), gtk_label_new (_("Properties")));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1), gtk_label_new (_("Permissions")));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2), gtk_label_new (_("Metadata")));

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_Help"), G_CALLBACK (on_dialog_help), data);
    data->copy_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("Co_py"), G_CALLBACK (on_copy_clipboard), data);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_Cancel"), G_CALLBACK (on_dialog_cancel), data);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_OK"), G_CALLBACK (on_dialog_ok), data);

    gtk_widget_set_sensitive (data->copy_button, gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook))==2);

    g_signal_connect (G_OBJECT (notebook), "switch-page", G_CALLBACK (on_notebook_page_change), data);

    return dialog;
}
