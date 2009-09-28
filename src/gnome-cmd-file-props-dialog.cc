/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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
#include <libgnomevfs/gnome-vfs-mime-monitor.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-file-props-dialog.h"
#include "gnome-cmd-chown-component.h"
#include "gnome-cmd-chmod-component.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-treeview.h"
#include "utils.h"
#include "imageloader.h"
#include "tags/gnome-cmd-tags.h"

using namespace std;


typedef struct
{
    GtkWidget *dialog;
    GnomeCmdFile *f;
    GThread *thread;
    GMutex *mutex;
    gboolean count_done;
    gchar *msg;
    guint updater_proc_id;

    GtkWidget *notebook;
    GtkWidget *copy_button;

    // Properties tab stuff
    gboolean stop;
    GnomeVFSFileSize size;
    GnomeVFSURI *uri;
    GtkWidget *filename_entry;
    GtkWidget *size_label;
    GtkWidget *app_label;

    // Permissions tab stuff
    GtkWidget *chown_component;
    GtkWidget *chmod_component;

} GnomeCmdFilePropsDialogPrivate;


inline const gchar *get_size_disp_string (GnomeVFSFileSize size)
{
    static gchar s[64];

    if (gnome_cmd_data.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_POWERED)
        return create_nice_size_str (size);

    snprintf (s, sizeof (s), ngettext("%s byte","%s bytes",size), size2string (size, gnome_cmd_data.size_disp_mode));
    return s;
}


static void calc_tree_size_r (GnomeCmdFilePropsDialogPrivate *data, GnomeVFSURI *uri)
{
    GList *list = NULL;
    gchar *uri_str;

    if (data->stop)
        g_thread_exit (NULL);

    if (!uri) return;
    uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
    if (!uri_str) return;

    GnomeVFSResult result = gnome_vfs_directory_list_load (&list, uri_str, GNOME_VFS_FILE_INFO_DEFAULT);

    if (result != GNOME_VFS_OK) return;
    if (!list) return;

    for (GList *tmp = list; tmp; tmp = tmp->next)
    {
        GnomeVFSFileInfo *info = (GnomeVFSFileInfo *) tmp->data;

        if (strcmp (info->name, ".") != 0 && strcmp (info->name, "..") != 0)
        {
            if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
            {
                GnomeVFSURI *new_uri = gnome_vfs_uri_append_file_name (uri, info->name);
                calc_tree_size_r (data, new_uri);
                gnome_vfs_uri_unref (new_uri);
            }
            else
                data->size += info->size;
        }
    }

    for (GList *tmp = list; tmp; tmp = tmp->next)
    {
        GnomeVFSFileInfo *info = (GnomeVFSFileInfo *) tmp->data;
        gnome_vfs_file_info_unref (info);
    }

    g_list_free (list);
    g_free (uri_str);

    if (data->stop)
        g_thread_exit (NULL);

    g_mutex_lock (data->mutex);
    g_free (data->msg);
    data->msg = g_strdup (get_size_disp_string (data->size));
    g_mutex_unlock (data->mutex);
}


static void calc_tree_size_func (GnomeCmdFilePropsDialogPrivate *data)
{
    calc_tree_size_r (data, data->uri);

    data->count_done = TRUE;
}


// Tells the thread to exit and then waits for it to do so.
static gboolean join_thread_func (GnomeCmdFilePropsDialogPrivate *data)
{
    g_source_remove (data->updater_proc_id);
    
    if (data->thread)
        g_thread_join (data->thread);

    gnome_cmd_file_unref (data->f);
    g_free (data);

    return FALSE;
}


static void on_dialog_destroy (GtkDialog *dialog, GnomeCmdFilePropsDialogPrivate *data)
{
    data->stop = TRUE;

    g_timeout_add (1, (GSourceFunc) join_thread_func, data);
}


static gboolean update_count_status (GnomeCmdFilePropsDialogPrivate *data)
{
    g_mutex_lock (data->mutex);
    if (data->size_label)
        gtk_label_set_text (GTK_LABEL (data->size_label), data->msg);
    g_mutex_unlock (data->mutex);

    if (data->count_done)
    {
        // Returning FALSE here stops the timeout callbacks
        return FALSE;
    }

    return TRUE;
}


static void do_calc_tree_size (GnomeCmdFilePropsDialogPrivate *data)
{
    g_return_if_fail (data != NULL);

    data->stop = FALSE;
    data->size = 0;
    data->count_done = FALSE;

    data->thread = g_thread_create ((PthreadFunc) calc_tree_size_func, data, TRUE, NULL);

    data->updater_proc_id = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_count_status, data);
}


static void on_change_default_app (GtkButton *btn, GnomeCmdFilePropsDialogPrivate *data)
{

    edit_mimetypes (data->f->info->mime_type, TRUE);

    GnomeVFSMimeApplication *vfs_app = gnome_vfs_mime_get_default_application (data->f->info->mime_type);

    gchar *appname = vfs_app ? vfs_app->name : g_strdup (_("No default application registered"));

    gtk_label_set_text (GTK_LABEL (data->app_label), appname);

    if (vfs_app)
        gnome_vfs_mime_application_free (vfs_app);
}


//----------------------------------------------------------------------------------


static void on_dialog_ok (GtkButton *btn, GnomeCmdFilePropsDialogPrivate *data)
{
    GnomeVFSResult result = GNOME_VFS_OK;

    const gchar *filename = gtk_entry_get_text (GTK_ENTRY (data->filename_entry));

    if (strcmp (filename, gnome_cmd_file_get_name (data->f)) != 0)
    {
        result = gnome_cmd_file_rename (data->f, filename);

        if (result==GNOME_VFS_OK)
            gnome_cmd_main_win_get_fs (main_win, ACTIVE)->file_list()->focus_file(filename, TRUE);
    }

    if (result == GNOME_VFS_OK)
    {
        GnomeVFSFilePermissions perms = gnome_cmd_chmod_component_get_perms (GNOME_CMD_CHMOD_COMPONENT (data->chmod_component));

        if (perms != data->f->info->permissions)
            result = gnome_cmd_file_chmod (data->f, perms);
    }

    if (result == GNOME_VFS_OK)
    {
        uid_t uid = gnome_cmd_chown_component_get_owner (GNOME_CMD_CHOWN_COMPONENT (data->chown_component));
        gid_t gid = gnome_cmd_chown_component_get_group (GNOME_CMD_CHOWN_COMPONENT (data->chown_component));

        if (uid == data->f->info->uid)
            uid = -1;
        if (gid == data->f->info->gid)
            gid = -1;

        if (uid != -1 || gid != -1)
            result = gnome_cmd_file_chown (data->f, uid, gid);
    }

    if (result != GNOME_VFS_OK)
    {
        gnome_cmd_show_message (NULL, filename, gnome_vfs_result_to_string (result));
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
    g_return_if_fail (data != NULL);

    string s;

    for (GnomeCmdFileMetadata::METADATA_COLL::const_iterator i=data->f->metadata->begin(); i!=data->f->metadata->end(); ++i)
        for (set<std::string>::const_iterator j=i->second.begin(); j!=i->second.end(); ++j)
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


static void on_notebook_page_change (GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, GnomeCmdFilePropsDialogPrivate *data)
{
    gtk_widget_set_sensitive (data->copy_button, page_num==2);
}


inline void add_sep (GtkWidget *table, gint y)
{
    gtk_table_attach (GTK_TABLE (table), create_hsep (table), 0, 2, y, y+1, GTK_FILL, GTK_FILL, 0, 0);
}


inline void add_tag (GtkWidget *dialog, GtkWidget *table, gint &y, GnomeCmdFileMetadata &metadata, GnomeCmdTag tag, const gchar *appended_text=NULL)
{
    if (!metadata.has_tag (tag))
        return;

    GtkWidget *label;

    string title = gcmd_tags_get_title (tag);
    title += ':';

    label = create_bold_label (dialog, title.c_str());
    table_add (table, label, 0, y, GTK_FILL);

    string value = truncate (metadata[tag],120);

    if (appended_text)
        value += appended_text;

    label = create_label (dialog, value.c_str());
    table_add (table, label, 1, y++, GTK_FILL);
}


inline void add_width_height_tag (GtkWidget *dialog, GtkWidget *table, gint &y, GnomeCmdFileMetadata &metadata)
{
    if (!metadata.has_tag (TAG_IMAGE_WIDTH) || !metadata.has_tag (TAG_IMAGE_HEIGHT))
        return;

    GtkWidget *label;

    label = create_bold_label (dialog, _("Image:"));
    table_add (table, label, 0, y, GTK_FILL);

    string value = metadata[TAG_IMAGE_WIDTH];
    value += " x ";
    value += metadata[TAG_IMAGE_HEIGHT];

    label = create_label (dialog, value.c_str());
    table_add (table, label, 1, y++, GTK_FILL);
}


inline GtkWidget *create_properties_tab (GnomeCmdFilePropsDialogPrivate *data)
{
    gint y = 0;
    GtkWidget *dialog = data->dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *hbox;
    GtkWidget *btn;
    gchar *fname;

    GtkWidget *space_frame = create_space_frame (dialog, 6);

    table = create_table (dialog, 6, 3);
    gtk_container_add (GTK_CONTAINER (space_frame), table);

    label = create_bold_label (dialog, GNOME_CMD_IS_DIR (data->f) ? _("Directory name:") : _("File name:"));
    table_add (table, label, 0, y, GTK_FILL);

    fname = get_utf8 (gnome_cmd_file_get_name (data->f));
    data->filename_entry = create_entry (dialog, "filename_entry", fname);
    g_free (fname);
    table_add (table, data->filename_entry, 1, y++, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));
    gtk_editable_set_position (GTK_EDITABLE (data->filename_entry), 0);

    if (data->f->info->symlink_name)
    {
        label = create_bold_label (dialog, _("Symlink target:"));
        table_add (table, label, 0, y, GTK_FILL);

        label = create_label (dialog, data->f->info->symlink_name);
        table_add (table, label, 1, y++, GTK_FILL);
    }

    add_sep (table, y++);

    label = create_bold_label (dialog, _("Type:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, gnome_cmd_file_get_mime_type_desc (data->f));
    table_add (table, label, 1, y++, GTK_FILL);


    label = create_bold_label (dialog, _("MIME Type:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, gnome_cmd_file_get_mime_type (data->f));
    table_add (table, label, 1, y++, GTK_FILL);


    if (data->f->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
        label = create_bold_label (dialog, _("Opens with:"));
        table_add (table, label, 0, y, GTK_FILL);

        GnomeVFSMimeApplication *vfs_app = gnome_cmd_app_get_default_application (data->f);

        if (vfs_app)
        {
            data->app_label = label = create_label (dialog, vfs_app->name);
            gnome_vfs_mime_application_free (vfs_app);
        }
        else
            label = create_label (dialog, _("No default application registered"));

        btn = create_button_with_data (dialog, _("_Change"), GTK_SIGNAL_FUNC (on_change_default_app), data);
        hbox = create_hbox (dialog, FALSE, 6);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
        label = create_label (dialog, " ");
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
        table_add (table, hbox, 1, y++, GTK_FILL);
    }

    add_sep (table, y++);


    label = create_bold_label (dialog, _("Modified:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, gnome_cmd_file_get_mdate (data->f, TRUE));
    table_add (table, label, 1, y++, GTK_FILL);

    label = create_bold_label (dialog, _("Accessed:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, gnome_cmd_file_get_adate (data->f, TRUE));
    table_add (table, label, 1, y++, GTK_FILL);


    add_sep (table, y++);


    label = create_bold_label (dialog, _("Size:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, get_size_disp_string (data->f->info->size));
    table_add (table, label, 1, y++, GTK_FILL);
    if (data->f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        do_calc_tree_size (data);

    data->size_label = label;

    gcmd_tags_bulk_load (data->f);

    if (data->f->metadata)
    {
        add_tag (dialog, table, y, *data->f->metadata, TAG_FILE_DESCRIPTION);
        add_tag (dialog, table, y, *data->f->metadata, TAG_FILE_PUBLISHER);
        add_tag (dialog, table, y, *data->f->metadata, TAG_DOC_TITLE);
        add_tag (dialog, table, y, *data->f->metadata, TAG_DOC_PAGECOUNT);
        add_width_height_tag (dialog, table, y, *data->f->metadata);
        add_tag (dialog, table, y, *data->f->metadata, TAG_AUDIO_ALBUMARTIST);
        add_tag (dialog, table, y, *data->f->metadata, TAG_AUDIO_TITLE);
        add_tag (dialog, table, y, *data->f->metadata, TAG_AUDIO_BITRATE, " kbps");
        add_tag (dialog, table, y, *data->f->metadata, TAG_AUDIO_DURATIONMMSS);
    }

    return space_frame;
}


inline GtkWidget *create_permissions_tab (GnomeCmdFilePropsDialogPrivate *data)
{
    GtkWidget *vbox = create_vbox (data->dialog, FALSE, 6);

    GtkWidget *space_frame = create_space_frame (data->dialog, 6);
    gtk_container_add (GTK_CONTAINER (space_frame), vbox);

    data->chown_component = gnome_cmd_chown_component_new ();
    gtk_widget_ref (data->chown_component);
    gtk_object_set_data_full (GTK_OBJECT (data->dialog),
                              "chown_component", data->chown_component,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (data->chown_component);
    gnome_cmd_chown_component_set (GNOME_CMD_CHOWN_COMPONENT (data->chown_component),
        data->f->info->uid, data->f->info->gid);

    GtkWidget *cat = create_category (data->dialog, data->chown_component, _("Owner and group"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, TRUE, TRUE, 0);


    data->chmod_component = gnome_cmd_chmod_component_new ((GnomeVFSFilePermissions) 0);
    gtk_widget_ref (data->chmod_component);
    gtk_object_set_data_full (GTK_OBJECT (data->dialog), "chmod_component", data->chmod_component, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (data->chmod_component);
    gnome_cmd_chmod_component_set_perms (GNOME_CMD_CHMOD_COMPONENT (data->chmod_component), data->f->info->permissions);

    cat = create_category (data->dialog, data->chmod_component, _("Access permissions"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, TRUE, TRUE, 0);

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

    if (!gcmd_tags_bulk_load (f))
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
            gtk_tree_store_append (treestore, &toplevel, NULL);
            gtk_tree_store_set (treestore, &toplevel,
                                COL_TAG, TAG_NONE,
                                COL_TYPE, gcmd_tags_get_class_name(t),
                                -1);
        }

        for (set<std::string>::const_iterator j=i->second.begin(); j!=i->second.end(); ++j)
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
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    GtkTooltips *tips = gtk_tooltips_new ();

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_TYPE, _("Type"));
    gtk_tooltips_set_tip (tips, col->button, _("Metadata namespace"), NULL);

    g_object_set (renderer,
                  "weight-set", TRUE,
                  "weight", PANGO_WEIGHT_BOLD,
                  NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), COL_NAME, _("Name"));
    gtk_tooltips_set_tip (tips, col->button, _("Tag name"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), COL_VALUE, _("Value"));
    gtk_tooltips_set_tip (tips, col->button, _("Tag value"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_DESC, _("Description"));
    gtk_tooltips_set_tip (tips, col->button, _("Metadata tag description"), NULL);

    g_object_set (renderer,
                  "foreground-set", TRUE,
                  "foreground", "DarkGray",
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  NULL);

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

    GtkWidget *scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
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
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    if (strcmp (f->info->name, "..") == 0)
        return NULL;

    GnomeCmdFilePropsDialogPrivate *data = g_new0 (GnomeCmdFilePropsDialogPrivate, 1);
    // data->thread = 0;

    GtkWidget *dialog = gnome_cmd_dialog_new (_("File Properties"));
    gtk_signal_connect (GTK_OBJECT (dialog), "destroy", (GtkSignalFunc) on_dialog_destroy, data);
    gtk_window_set_resizable(GTK_WINDOW (dialog), TRUE);

    GtkWidget *notebook = gtk_notebook_new ();

    data->dialog = GTK_WIDGET (dialog);
    data->f = f;
    data->uri = gnome_cmd_file_get_uri (f);
    data->mutex = g_mutex_new ();
    data->msg = NULL;
    data->notebook = notebook;
    gnome_cmd_file_ref (f);

    gtk_widget_ref (notebook);
    gtk_object_set_data_full (GTK_OBJECT (dialog), "notebook", notebook, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (notebook);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), notebook);

    gtk_container_add (GTK_CONTAINER (notebook), create_properties_tab (data));
    gtk_container_add (GTK_CONTAINER (notebook), create_permissions_tab (data));
    gtk_container_add (GTK_CONTAINER (notebook), create_metadata_tab (data));

    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), gtk_label_new (_("Properties")));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1), gtk_label_new (_("Permissions")));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2), gtk_label_new (_("Metadata")));

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_HELP, GTK_SIGNAL_FUNC (on_dialog_help), data);
    data->copy_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_COPY, GTK_SIGNAL_FUNC (on_copy_clipboard), data);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_dialog_cancel), data);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK, GTK_SIGNAL_FUNC (on_dialog_ok), data);

    gtk_widget_set_sensitive (data->copy_button, gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook))==2);

    g_signal_connect (GTK_OBJECT (notebook), "switch-page", GTK_SIGNAL_FUNC (on_notebook_page_change), data);

    return dialog;
}
