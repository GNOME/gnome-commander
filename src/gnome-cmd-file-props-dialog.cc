/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#include "gnome-cmd-file-props-dialog.h"
#include "gnome-cmd-chown-component.h"
#include "gnome-cmd-chmod-component.h"
#include "gnome-cmd-data.h"
#include "utils.h"
#include "imageloader.h"

using namespace std;


typedef struct {
    GtkWidget *dialog;
    GnomeCmdFile *finfo;
    GThread *thread;
    GMutex *mutex;
    gboolean count_done;
    gchar *msg;

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


static const gchar *
get_size_disp_string (GnomeVFSFileSize size)
{
    static gchar s[64];
    GnomeCmdSizeDispMode mode = gnome_cmd_data_get_size_disp_mode ();

    if (mode == GNOME_CMD_SIZE_DISP_MODE_POWERED)
        return create_nice_size_str (size);

    snprintf (s, sizeof (s), ngettext("%s byte","%s bytes",size), size2string (size, mode));
    return s;
}


static void
calc_tree_size_r (GnomeCmdFilePropsDialogPrivate *data, GnomeVFSURI *uri)
{
    GnomeVFSFileInfoOptions infoOpts = GNOME_VFS_FILE_INFO_DEFAULT;
    GnomeVFSResult result;
    GList *list = NULL, *tmp;
    gchar *uri_str;

    if (data->stop)
        g_thread_exit (NULL);

    if (!uri) return;
    uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
    if (!uri_str) return;

    result = gnome_vfs_directory_list_load (&list, uri_str, infoOpts);

    if (result != GNOME_VFS_OK) return;
    if (!list) return;

    for (tmp = list; tmp; tmp = tmp->next)
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

    for (tmp = list; tmp; tmp = tmp->next)
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


static void
calc_tree_size_func (GnomeCmdFilePropsDialogPrivate *data)
{
    calc_tree_size_r (data, data->uri);

    data->count_done = TRUE;
}


/*
 * Tells the thread to exit and then waits for it to do so.
 */
static gboolean
join_thread_func (GnomeCmdFilePropsDialogPrivate *data)
{
    if (data->thread)
        g_thread_join (data->thread);

    gnome_cmd_file_unref (data->finfo);
    g_free (data);
    return FALSE;
}


static void
on_dialog_destroy (GtkDialog *dialog, GnomeCmdFilePropsDialogPrivate *data)
{
    data->stop = TRUE;

    gtk_timeout_add (1, (GtkFunction)join_thread_func, data);
}


static gboolean
update_count_status (GnomeCmdFilePropsDialogPrivate *data)
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


static void
do_calc_tree_size (GnomeCmdFilePropsDialogPrivate *data)
{
    g_return_if_fail (data != NULL);

    data->stop = FALSE;
    data->size = 0;
    data->count_done = FALSE;

    data->thread = g_thread_create ((PthreadFunc)calc_tree_size_func, data, TRUE, NULL);

    gtk_timeout_add (gnome_cmd_data_get_gui_update_rate (), (GtkFunction)update_count_status, data);
}


static void
on_change_default_app (GtkButton *btn, GnomeCmdFilePropsDialogPrivate *data)
{

    edit_mimetypes (data->finfo->info->mime_type, TRUE);

    GnomeVFSMimeApplication *vfs_app = gnome_vfs_mime_get_default_application (data->finfo->info->mime_type);

    gchar *appname = vfs_app ? vfs_app->name : g_strdup (_("No default application registered"));

    gtk_label_set_text (GTK_LABEL (data->app_label), appname);

    if (vfs_app)
        gnome_vfs_mime_application_free (vfs_app);
}


//----------------------------------------------------------------------------------


static void
on_dialog_ok (GtkButton *btn, GnomeCmdFilePropsDialogPrivate *data)
{
    GnomeVFSResult result = GNOME_VFS_OK;

    const gchar *filename = gtk_entry_get_text (GTK_ENTRY (data->filename_entry));
    if (strcmp (filename, gnome_cmd_file_get_name (data->finfo)) != 0) {
        result = gnome_cmd_file_rename (data->finfo, filename);
    }

    if (result == GNOME_VFS_OK) {
        GnomeVFSFilePermissions perms = gnome_cmd_chmod_component_get_perms (
            GNOME_CMD_CHMOD_COMPONENT (data->chmod_component));
        if (perms != data->finfo->info->permissions) {
            result = gnome_cmd_file_chmod (data->finfo, perms);
        }
    }


    if (result == GNOME_VFS_OK) {
        uid_t uid = gnome_cmd_chown_component_get_owner (
            GNOME_CMD_CHOWN_COMPONENT (data->chown_component));
        gid_t gid = gnome_cmd_chown_component_get_group (
            GNOME_CMD_CHOWN_COMPONENT (data->chown_component));

        if (uid == data->finfo->info->uid)
            uid = -1;
        if (gid == data->finfo->info->gid)
            gid = -1;

        if (uid != -1 || gid != -1) {
            result = gnome_cmd_file_chown (data->finfo, uid, gid);
        }
    }

    if (result != GNOME_VFS_OK) {
        create_error_dialog (gnome_vfs_result_to_string (result));
        return;
    }

    gtk_widget_destroy (data->dialog);
}


static void
on_dialog_cancel (GtkButton *btn, GnomeCmdFilePropsDialogPrivate *data)
{
    gtk_widget_destroy (data->dialog);
}


static void on_dialog_help (GtkButton *button, GnomeCmdFilePropsDialogPrivate *data)
{
    gnome_cmd_help_display("gnome-commander.xml");
}


static void
add_sep (GtkWidget *table, gint y)
{
    GtkWidget *hsep;

    hsep = create_hsep (table);
    gtk_table_attach (GTK_TABLE (table), hsep,
                      0, 2, y, y+1, GTK_FILL, GTK_FILL, 0, 0);
}


static GtkWidget*
create_properties_tab (GnomeCmdFilePropsDialogPrivate *data)
{
    gint y = 0;
    GtkWidget *dialog = data->dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *space_frame;
    GtkWidget *hbox;
    GtkWidget *btn;
    gchar *fname;
    GnomeVFSMimeApplication *vfs_app;

    space_frame = create_space_frame (dialog, 6);

    table = create_table (dialog, 6, 3);
    gtk_container_add (GTK_CONTAINER (space_frame), table);

    label = create_bold_label (dialog, _("Filename:"));
    table_add (table, label, 0, y, GTK_FILL);

    fname = get_utf8 (gnome_cmd_file_get_name (data->finfo));
    data->filename_entry = create_entry (
        dialog, "filename_entry", fname);
    g_free (fname);
    table_add (table, data->filename_entry, 1, y++, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));
    gtk_editable_set_position (GTK_EDITABLE (data->filename_entry), 0);

    if (data->finfo->info->symlink_name) {
        label = create_bold_label (dialog, _("Symlink target:"));
        table_add (table, label, 0, y, GTK_FILL);

        label = create_label (dialog, data->finfo->info->symlink_name);
        table_add (table, label, 1, y++, GTK_FILL);
    }

    add_sep (table, y++);

    label = create_bold_label (dialog, _("Type:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, gnome_cmd_file_get_mime_type_desc (data->finfo));
    table_add (table, label, 1, y++, GTK_FILL);


    label = create_bold_label (dialog, _("MIME Type:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, gnome_cmd_file_get_mime_type (data->finfo));
    table_add (table, label, 1, y++, GTK_FILL);


    if (data->finfo->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY) {
        label = create_bold_label (dialog, _("Opens with:"));
        table_add (table, label, 0, y, GTK_FILL);

        vfs_app = gnome_vfs_mime_get_default_application (data->finfo->info->mime_type);
        if (vfs_app) {
            data->app_label = label = create_label (dialog, vfs_app->name);
            gnome_vfs_mime_application_free (vfs_app);
        }
        else
            label = create_label (dialog, _("No default application registered"));
        btn = create_button_with_data (
            dialog, _("_Change"), GTK_SIGNAL_FUNC (on_change_default_app), data);
        hbox = create_hbox (dialog, FALSE, 6);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
        label = create_label (dialog, " ");
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
        table_add (table, hbox, 1, y++, GTK_FILL);
    }

    add_sep (table, y++);


    label = create_bold_label (dialog, _("Accessed:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, gnome_cmd_file_get_adate (data->finfo, TRUE));
    table_add (table, label, 1, y++, GTK_FILL);


    label = create_bold_label (dialog, _("Modified:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, gnome_cmd_file_get_mdate (data->finfo, TRUE));
    table_add (table, label, 1, y++, GTK_FILL);


    add_sep (table, y++);


    label = create_bold_label (dialog, _("Size:"));
    table_add (table, label, 0, y, GTK_FILL);

    label = create_label (dialog, get_size_disp_string (data->finfo->info->size));
    table_add (table, label, 1, y++, GTK_FILL);
    if (data->finfo->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        do_calc_tree_size (data);

    data->size_label = label;

    return space_frame;
}


static GtkWidget*
create_permissions_tab (GnomeCmdFilePropsDialogPrivate *data)
{
    GtkWidget *vbox;
    GtkWidget *cat;
    GtkWidget *space_frame;

    space_frame = create_space_frame (data->dialog, 6);

    vbox = create_vbox (data->dialog, FALSE, 6);
    gtk_container_add (GTK_CONTAINER (space_frame), vbox);

    data->chown_component = gnome_cmd_chown_component_new ();
    gtk_widget_ref (data->chown_component);
    gtk_object_set_data_full (GTK_OBJECT (data->dialog),
                              "chown_component", data->chown_component,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (data->chown_component);
    gnome_cmd_chown_component_set (
        GNOME_CMD_CHOWN_COMPONENT (data->chown_component),
        data->finfo->info->uid, data->finfo->info->gid);

    cat = create_category (data->dialog, data->chown_component, _("Owner and group"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, TRUE, TRUE, 0);


    data->chmod_component = gnome_cmd_chmod_component_new ((GnomeVFSFilePermissions) 0);
    gtk_widget_ref (data->chmod_component);
    gtk_object_set_data_full (GTK_OBJECT (data->dialog),
                              "chmod_component", data->chmod_component,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (data->chmod_component);
    gnome_cmd_chmod_component_set_perms (
        GNOME_CMD_CHMOD_COMPONENT (data->chmod_component),
        data->finfo->info->permissions);

    cat = create_category (data->dialog, data->chmod_component, _("Access permissions"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, TRUE, TRUE, 0);

    return space_frame;
}


GtkWidget*
gnome_cmd_file_props_dialog_create (GnomeCmdFile *finfo)
{
    GtkWidget *dialog;
    GtkWidget *notebook;
    GnomeCmdFilePropsDialogPrivate *data;

    g_return_val_if_fail (finfo != NULL, NULL);
    g_return_val_if_fail (finfo->info != NULL, NULL);

    if (strcmp (finfo->info->name, "..") == 0)
        return NULL;

    data = g_new (GnomeCmdFilePropsDialogPrivate, 1);
    data->thread = 0;

    dialog = gnome_cmd_dialog_new (_("File Properties"));
    gtk_signal_connect (GTK_OBJECT (dialog), "destroy", (GtkSignalFunc)on_dialog_destroy, data);

    data->dialog = GTK_WIDGET (dialog);
    data->finfo = finfo;
    data->uri = gnome_cmd_file_get_uri (finfo);
    data->mutex = g_mutex_new ();
    data->msg = NULL;
    gnome_cmd_file_ref (finfo);

    notebook = gtk_notebook_new ();
    gtk_widget_ref (notebook);
    gtk_object_set_data_full (GTK_OBJECT (dialog), "notebook", notebook,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (notebook);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), notebook);

    gtk_container_add (GTK_CONTAINER (notebook),
                       create_properties_tab (data));
    gtk_container_add (GTK_CONTAINER (notebook),
                       create_permissions_tab (data));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0),
        gtk_label_new (_("Properties")));
    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1),
        gtk_label_new (_("Permissions")));

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_HELP, GTK_SIGNAL_FUNC (on_dialog_help), data);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_CANCEL, GTK_SIGNAL_FUNC (on_dialog_cancel), data);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_OK, GTK_SIGNAL_FUNC (on_dialog_ok), data);

    return dialog;
}

