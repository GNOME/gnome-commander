/** 
 * @file gnome-cmd-dir.cc
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
#include "gnome-cmd-dir.h"
#include "gnome-cmd-main-win.h"
#ifdef HAVE_SAMBA
#include "gnome-cmd-con-smb.h"
#endif
#include "gnome-cmd-data.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-file-collection.h"
#include "dirlist.h"
#include "utils.h"

using namespace std;


#define DIR_PBAR_MAX 50

int created_dirs_cnt = 0;
int deleted_dirs_cnt = 0;

GList *all_dirs = NULL;

enum
{
    FILE_CREATED,
    FILE_DELETED,
    FILE_CHANGED,
    FILE_RENAMED,
    LIST_OK,
    LIST_FAILED,
    LAST_SIGNAL
};

struct GnomeCmdDirPrivate
{
    GnomeVFSAsyncHandle *list_handle;

    gint ref_cnt;
    GList *files;
    GnomeCmdFileCollection *file_collection;
    GnomeVFSResult last_result;
    GnomeCmdCon *con;
    GnomeCmdPath *path;

    gboolean lock;
    gboolean needs_mtime_update;

    Handle *handle;
    GnomeVFSMonitorHandle *monitor_handle;
    gint monitor_users;
};


G_DEFINE_TYPE (GnomeCmdDir, gnome_cmd_dir, GNOME_CMD_TYPE_FILE)


static guint signals[LAST_SIGNAL] = { 0 };


static void monitor_callback (GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, GnomeCmdDir *dir)
{
    switch (event_type)
    {
        case GNOME_VFS_MONITOR_EVENT_CHANGED:
            DEBUG('n', "GNOME_VFS_MONITOR_EVENT_CHANGED for %s\n", info_uri);
            gnome_cmd_dir_file_changed (dir, info_uri);
            break;
        case GNOME_VFS_MONITOR_EVENT_DELETED:
            DEBUG('n', "GNOME_VFS_MONITOR_EVENT_DELETED for %s\n", info_uri);
            gnome_cmd_dir_file_deleted (dir, info_uri);
            break;
        case GNOME_VFS_MONITOR_EVENT_CREATED:
            DEBUG('n', "GNOME_VFS_MONITOR_EVENT_CREATED for %s\n", info_uri);
            gnome_cmd_dir_file_created (dir, info_uri);
            break;
        case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED:
        case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING:
        case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING:
            break;

        default:
            DEBUG('n', "Unknown monitor event %d\n", event_type);
    }
}


static void gnome_cmd_dir_init (GnomeCmdDir *dir)
{
    // dir->voffset = 0;
    // dir->dialog = NULL;
    dir->state = GnomeCmdDir::STATE_EMPTY;

    dir->priv = g_new0 (GnomeCmdDirPrivate, 1);

    dir->priv->handle = handle_new (dir);
    // dir->priv->monitor_handle = NULL;
    // dir->priv->monitor_users = 0;
    // dir->priv->files = NULL;
    dir->priv->file_collection = new GnomeCmdFileCollection;

    if (DEBUG_ENABLED ('c'))
    {
        created_dirs_cnt++;
        all_dirs = g_list_append (all_dirs, dir);
    }
}


static void gnome_cmd_dir_finalize (GObject *object)
{
    GnomeCmdDir *dir = GNOME_CMD_DIR (object);

    DEBUG ('d', "dir destroying 0x%p %s\n", dir, dir->priv->path->get_path());

    gnome_cmd_con_remove_from_cache (dir->priv->con, dir);

    delete dir->priv->file_collection;
    delete dir->priv->path;

    dir->priv->handle->ref = NULL;
    handle_unref (dir->priv->handle);

    g_free (dir->priv);

    if (DEBUG_ENABLED ('c'))
    {
        all_dirs = g_list_remove (all_dirs, dir);
        deleted_dirs_cnt++;
    }

    G_OBJECT_CLASS (gnome_cmd_dir_parent_class)->finalize (object);
}


static void gnome_cmd_dir_class_init (GnomeCmdDirClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals[FILE_CREATED] =
        g_signal_new ("file-created",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_created),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILE_DELETED] =
        g_signal_new ("file-deleted",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_deleted),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILE_CHANGED] =
        g_signal_new ("file-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILE_RENAMED] =
        g_signal_new ("file-renamed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, file_renamed),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[LIST_OK] =
        g_signal_new ("list-ok",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, list_ok),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[LIST_FAILED] =
        g_signal_new ("list-failed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirClass, list_failed),
            NULL, NULL,
            g_cclosure_marshal_VOID__INT,
            G_TYPE_NONE,
            1, G_TYPE_INT);

    object_class->finalize = gnome_cmd_dir_finalize;
    klass->file_created = NULL;
    klass->file_deleted = NULL;
    klass->file_changed = NULL;
    klass->file_renamed = NULL;
    klass->list_ok = NULL;
    klass->list_failed = NULL;
}


/***********************************
 * Public functions
 ***********************************/

GnomeCmdDir *gnome_cmd_dir_new_from_info (GnomeVFSFileInfo *info, GnomeCmdDir *parent)
{
    g_return_val_if_fail (info != NULL, NULL);
    g_return_val_if_fail (GNOME_CMD_IS_DIR (parent), NULL);

    GnomeCmdCon *con = gnome_cmd_dir_get_connection (parent);
    GnomeCmdPath *path =  gnome_cmd_dir_get_path (parent)->get_child(info->name);

    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, path);
    gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);

    GnomeCmdDir *dir = gnome_cmd_con_cache_lookup (gnome_cmd_dir_get_connection (parent), uri_str);
    g_free (uri_str);
    gnome_vfs_uri_unref (uri);

    if (dir)
    {
        delete path;
        GNOME_CMD_FILE (dir)->update_info(info);
        return dir;
    }

    dir = (GnomeCmdDir *) g_object_new (GNOME_CMD_TYPE_DIR, NULL);
    gnome_cmd_file_setup (GNOME_CMD_FILE (dir), info, parent);

    dir->priv->con = con;
    gnome_cmd_dir_set_path (dir, path);
    dir->priv->needs_mtime_update = FALSE;

    gnome_cmd_con_add_to_cache (gnome_cmd_dir_get_connection (parent), dir);

    return dir;
}


GnomeCmdDir *gnome_cmd_dir_new_with_con (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    g_return_val_if_fail (con->base_info != NULL, NULL);

    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, con->base_path);
    gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);

    GnomeCmdDir *dir = gnome_cmd_con_cache_lookup (con, uri_str);
    if (dir)
    {
        g_free (uri_str);
        gnome_vfs_uri_unref (uri);
        GNOME_CMD_FILE (dir)->update_info(con->base_info);
        return dir;
    }

    dir = (GnomeCmdDir *) g_object_new (GNOME_CMD_TYPE_DIR, NULL);
    gnome_cmd_file_setup (GNOME_CMD_FILE (dir), con->base_info, NULL);

    dir->priv->con = con;
    gnome_cmd_dir_set_path (dir, con->base_path->clone());
    dir->priv->needs_mtime_update = FALSE;

    gnome_cmd_con_add_to_cache (con, dir);

    return dir;
}


GnomeCmdDir *gnome_cmd_dir_new (GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    g_return_val_if_fail (path!=NULL, NULL);

    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, path);
    if (!uri) return NULL;

    gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);

    GnomeCmdDir *dir = gnome_cmd_con_cache_lookup (con, uri_str);
    if (dir)
    {
        g_free (uri_str);
        return dir;
    }

    GnomeVFSFileInfoOptions infoOpts = (GnomeVFSFileInfoOptions) (GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
                                                                  GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
                                                                  GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
    GnomeVFSResult res = gnome_vfs_get_file_info_uri (uri, info, infoOpts);

    if (res == GNOME_VFS_OK)
    {
        dir = (GnomeCmdDir *) g_object_new (GNOME_CMD_TYPE_DIR, NULL);
        gnome_cmd_file_setup (GNOME_CMD_FILE (dir), info, NULL);

        dir->priv->con = con;
        gnome_cmd_dir_set_path (dir, path);
        dir->priv->needs_mtime_update = FALSE;

        gnome_cmd_con_add_to_cache (con, dir);
    }
    else
    {
        gnome_cmd_show_message (*main_win, path->get_display_path(), gnome_vfs_result_to_string (res));
        gnome_vfs_file_info_unref (info);
    }

    gnome_vfs_uri_unref (uri);
    g_free (uri_str);

    return dir;
}


GnomeCmdDir *gnome_cmd_dir_get_parent (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    GnomeCmdPath *path = dir->priv->path->get_parent();

    return path ? gnome_cmd_dir_new (dir->priv->con, path) : NULL;
}


GnomeCmdDir *gnome_cmd_dir_get_child (GnomeCmdDir *dir, const gchar *child)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    GnomeCmdPath *path = dir->priv->path->get_child(child);

    return path ? gnome_cmd_dir_new (dir->priv->con, path) : NULL;
}


GnomeCmdCon *gnome_cmd_dir_get_connection (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    return dir->priv->con;
}


Handle *gnome_cmd_dir_get_handle (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    return dir->priv->handle;
}


GList *gnome_cmd_dir_get_files (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    return dir->priv->files;
}


static GList *create_file_list (GnomeCmdDir *dir, GList *info_list)
{
    GList *file_list = NULL;

    // create a new list with GnomeCmdFile objects

    for (GList *i = info_list; i; i = i->next)
    {
        GnomeVFSFileInfo *info = (GnomeVFSFileInfo *) i->data;

        if (info && info->name)
        {

            if (strcmp (info->name, ".") == 0 || strcmp (info->name, "..") == 0)
            {
                gnome_vfs_file_info_unref (info);
                continue;
            }

#ifdef HAVE_SAMBA
            GnomeCmdCon *con = gnome_cmd_dir_get_connection (dir);
            if (GNOME_CMD_IS_CON_SMB (con)
                && info->mime_type
                && (strcmp (info->mime_type, "application/x-gnome-app-info") == 0 ||
                    strcmp (info->mime_type, "application/x-desktop") == 0)
                && strcmp (info->name, ".directory"))
            {
                // This is a hack to make samba workgroups etc
                // look like normal directories
                info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
                // Determining smb MIME type: workgroup or server
                gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

                info->mime_type = strcmp (uri_str, "smb:///") == 0 ? g_strdup ("x-directory/smb-workgroup") :
                                                                     g_strdup ("x-directory/smb-server");
            }
#endif

            GnomeCmdFile *f = info->type == GNOME_VFS_FILE_TYPE_DIRECTORY ? GNOME_CMD_FILE (gnome_cmd_dir_new_from_info (info, dir)) :
                                                                            gnome_cmd_file_new (info, dir);

            gnome_cmd_file_ref (f);
            file_list = g_list_append (file_list, f);
        }
    }

    return file_list;
}


static void on_list_done (GnomeCmdDir *dir, GList *infolist, GnomeVFSResult result)
{
    if (dir->state == GnomeCmdDir::STATE_LISTED)
    {
        DEBUG('l', "File listing succeded\n");

        if (!dir->priv->file_collection->empty())
            dir->priv->file_collection->clear();

        dir->priv->files = create_file_list (dir, infolist);
        dir->priv->file_collection->add(dir->priv->files);
        g_list_free (infolist);

        if (dir->dialog)
        {
            gtk_widget_destroy (dir->dialog);
            dir->dialog = NULL;
        }
        dir->priv->lock = FALSE;
        dir->priv->last_result = GNOME_VFS_OK;

        DEBUG('l', "Emitting 'list-ok' signal\n");
        g_signal_emit (dir, signals[LIST_OK], 0, dir->priv->files);
    }
    else if (dir->state == GnomeCmdDir::STATE_EMPTY)
    {
        DEBUG('l', "File listing failed: %s\n", gnome_vfs_result_to_string (result));

        if (dir->dialog)
        {
            gtk_widget_destroy (dir->dialog);
            dir->dialog = NULL;
        }

        dir->priv->last_result = result;
        dir->priv->lock = FALSE;

        DEBUG('l', "Emitting 'list-failed' signal\n");
        g_signal_emit (dir, signals[LIST_FAILED], 0, result);
    }
}


static void on_dir_list_cancel (GtkButton *btn, GnomeCmdDir *dir)
{
    if (dir->state == GnomeCmdDir::STATE_LISTING)
    {
        DEBUG('l', "on_dir_list_cancel\n");
        dirlist_cancel (dir);

        gtk_widget_destroy (dir->dialog);
        dir->dialog = NULL;
    }
}


static void create_list_progress_dialog (GnomeCmdDir *dir)
{
    dir->dialog = gnome_cmd_dialog_new (NULL);
    g_object_ref (dir->dialog);

    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dir->dialog),
        GTK_STOCK_CANCEL,
        GTK_SIGNAL_FUNC (on_dir_list_cancel), dir);

    GtkWidget *vbox = create_vbox (dir->dialog, FALSE, 0);

    dir->label = create_label (dir->dialog, _("Waiting for file list"));

    dir->pbar = create_progress_bar (dir->dialog);
    gtk_progress_set_show_text (GTK_PROGRESS (dir->pbar), FALSE);
    gtk_progress_set_activity_mode (GTK_PROGRESS (dir->pbar), TRUE);
    gtk_progress_configure (GTK_PROGRESS (dir->pbar), 0, 0, DIR_PBAR_MAX);

    gtk_box_pack_start (GTK_BOX (vbox), dir->label, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), dir->pbar, FALSE, TRUE, 0);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dir->dialog), vbox);
    gtk_window_set_transient_for (GTK_WINDOW (dir->dialog), *main_win);

    gtk_widget_show_all (dir->dialog);
}


void gnome_cmd_dir_relist_files (GnomeCmdDir *dir, gboolean visprog)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    if (dir->priv->lock) return;
    dir->priv->lock = TRUE;

    dir->done_func = (DirListDoneFunc) on_list_done;

    if (visprog)
        create_list_progress_dialog (dir);

    dirlist_list (dir, visprog);
}


void gnome_cmd_dir_list_files (GnomeCmdDir *dir, gboolean visprog)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    if (!dir->priv->files || gnome_cmd_dir_is_local (dir))
    {
        DEBUG ('l', "relisting files for 0x%x %s %d\n",
               dir,
               GNOME_CMD_FILE (dir)->get_path(),
               visprog);
        gnome_cmd_dir_relist_files (dir, visprog);
    }
    else
        g_signal_emit (dir, signals[LIST_OK], 0, dir->priv->files);
}


GnomeCmdPath *gnome_cmd_dir_get_path (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    return dir->priv->path;
}


void gnome_cmd_dir_set_path (GnomeCmdDir *dir, GnomeCmdPath *path)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    delete dir->priv->path;

    dir->priv->path = path;
}


void gnome_cmd_dir_update_path (GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    GnomeCmdDir *parent = gnome_cmd_dir_get_parent (dir);
    if (!parent)
        return;

    GnomeCmdPath *path = gnome_cmd_dir_get_path (parent)->get_child(GNOME_CMD_FILE (dir)->get_name());
    if (path)
        gnome_cmd_dir_set_path (dir, path);
}


gchar *gnome_cmd_dir_get_display_path (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    return g_strdup (dir->priv->path->get_display_path());
}


GnomeVFSURI *gnome_cmd_dir_get_uri (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    return gnome_cmd_dir_get_child_uri(dir, ".");
}


gchar *gnome_cmd_dir_get_uri_str (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    GnomeVFSURI *uri = gnome_cmd_dir_get_uri (dir);
    gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
    gnome_vfs_uri_unref (uri);

    return uri_str;
}


GnomeVFSURI *gnome_cmd_dir_get_child_uri (GnomeCmdDir *dir, const gchar *filename)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    GnomeCmdPath *path = dir->priv->path->get_child(filename);
    GnomeVFSURI *uri = gnome_cmd_con_create_uri (dir->priv->con, path);
    delete path;

    return uri;
}


gchar *gnome_cmd_dir_get_child_uri_str (GnomeCmdDir *dir, const gchar *filename)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    GnomeVFSURI *uri = gnome_cmd_dir_get_child_uri (dir, filename);
    gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
    gnome_vfs_uri_unref (uri);

    return uri_str;
}


GnomeVFSURI *gnome_cmd_dir_get_absolute_path_uri (GnomeCmdDir *dir, string absolute_filename)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

#ifdef HAVE_SAMBA
    // include workgroups and shares for smb uris
    GnomeVFSURI *dir_uri = gnome_cmd_dir_get_uri (dir);

    if (strcmp (gnome_vfs_uri_get_scheme (dir_uri), "smb") == 0)
    {
        gchar *mime_type = gnome_vfs_get_mime_type (gnome_vfs_uri_to_string (dir_uri, GNOME_VFS_URI_HIDE_PASSWORD));
        while (strcmp (mime_type, "x-directory/normal") == 0)
        {
            g_free (mime_type);

            GnomeVFSURI *tmp_dir_uri = gnome_vfs_uri_get_parent (dir_uri);
            gnome_vfs_uri_unref (dir_uri);
            dir_uri = gnome_vfs_uri_dup (tmp_dir_uri);
            mime_type = gnome_vfs_get_mime_type (gnome_vfs_uri_to_string (dir_uri, GNOME_VFS_URI_HIDE_PASSWORD));
        }

        g_free (mime_type);

        gchar *server_and_share = gnome_vfs_uri_to_string (dir_uri, GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);
        stringify (absolute_filename, g_build_filename (server_and_share, absolute_filename.c_str(), NULL));
        g_free (server_and_share);
    }

    gnome_vfs_uri_unref (dir_uri);
#endif

    GnomeCmdPath *path = gnome_cmd_con_create_path (dir->priv->con, absolute_filename.c_str());
    GnomeVFSURI *uri = gnome_cmd_con_create_uri (dir->priv->con, path);

    delete path;

    return uri;
}


inline gboolean file_already_exists (GnomeCmdDir *dir, const gchar *uri_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), TRUE);
    g_return_val_if_fail (uri_str != NULL, TRUE);

    return dir->priv->file_collection->find(uri_str) != NULL;
}


// A file has been created. Create a new GnomeCmdFile object for that file
void gnome_cmd_dir_file_created (GnomeCmdDir *dir, const gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (uri_str != NULL);

    if (file_already_exists (dir, uri_str))
        return;

    GnomeVFSURI *uri = gnome_vfs_uri_new (uri_str);
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
    GnomeVFSFileInfoOptions infoOpts = (GnomeVFSFileInfoOptions) (GNOME_VFS_FILE_INFO_FOLLOW_LINKS|GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
    GnomeVFSResult res = gnome_vfs_get_file_info_uri (uri, info, infoOpts);
    gnome_vfs_uri_unref (uri);

    GnomeCmdFile *f;

    if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        f = GNOME_CMD_FILE (gnome_cmd_dir_new_from_info (info, dir));
    else
        f = gnome_cmd_file_new (info, dir);

    dir->priv->file_collection->add(f);
    dir->priv->files = dir->priv->file_collection->get_list();

    dir->priv->needs_mtime_update = TRUE;

    g_signal_emit (dir, signals[FILE_CREATED], 0, f);
}


// A file has been deleted. Remove the corresponding GnomeCmdFile
void gnome_cmd_dir_file_deleted (GnomeCmdDir *dir, const gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (uri_str != NULL);

    GnomeCmdFile *f = dir->priv->file_collection->find(uri_str);

    if (!GNOME_CMD_IS_FILE (f))
        return;

    dir->priv->needs_mtime_update = TRUE;

    g_signal_emit (dir, signals[FILE_DELETED], 0, f);

    dir->priv->file_collection->remove(uri_str);
    dir->priv->files = dir->priv->file_collection->get_list();
}


// A file has been changed. Find the corresponding GnomeCmdFile, update its GnomeVFSFileInfo
void gnome_cmd_dir_file_changed (GnomeCmdDir *dir, const gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (uri_str != NULL);

    GnomeCmdFile *f = dir->priv->file_collection->find(uri_str);

    g_return_if_fail (GNOME_CMD_IS_FILE (f));

    GnomeVFSURI *uri = f->get_uri();
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
    GnomeVFSResult res = gnome_vfs_get_file_info_uri (uri, info, GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
    gnome_vfs_uri_unref (uri);

    dir->priv->needs_mtime_update = TRUE;

    f->update_info(info);
    f->invalidate_metadata();
    g_signal_emit (dir, signals[FILE_CHANGED], 0, f);
}


void gnome_cmd_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *f, const gchar *old_uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (GNOME_CMD_IS_FILE (f));
    g_return_if_fail (old_uri_str!=NULL);

    if (GNOME_CMD_IS_DIR (f))
        gnome_cmd_con_remove_from_cache (dir->priv->con, old_uri_str);

    dir->priv->needs_mtime_update = TRUE;

    dir->priv->file_collection->remove(old_uri_str);
    dir->priv->file_collection->add(f);
    g_signal_emit (dir, signals[FILE_RENAMED], 0, f);
}


gboolean gnome_cmd_dir_uses_fam (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);

    return FALSE;
}


void gnome_cmd_dir_start_monitoring (GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    GnomeVFSResult result;

    if (dir->priv->monitor_users == 0)
    {
        gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

        result = gnome_vfs_monitor_add (
            &dir->priv->monitor_handle,
            uri_str,
            GNOME_VFS_MONITOR_DIRECTORY,
            (GnomeVFSMonitorCallback) monitor_callback,
            dir);

        if (result == GNOME_VFS_OK)
            DEBUG('n', "Added monitor to 0x%x %s\n", dir, uri_str);
        else
            DEBUG ('n', "Failed to add monitor to 0x%p %s: %s\n", dir, uri_str, gnome_vfs_result_to_string (result));

        g_free (uri_str);
    }

    dir->priv->monitor_users++;
}


void gnome_cmd_dir_cancel_monitoring (GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    if (dir->priv->monitor_users < 1) return;

    dir->priv->monitor_users--;

    if (dir->priv->monitor_users == 0)
    {
        if (dir->priv->monitor_handle)
        {
            GnomeVFSResult result = gnome_vfs_monitor_cancel (dir->priv->monitor_handle);
            if (result == GNOME_VFS_OK)
                DEBUG('n', "Removed monitor from 0x%p %s\n",
                      dir, GNOME_CMD_FILE (dir)->get_uri_str());
            else
                DEBUG('n', "Failed to remove monitor from 0x%p %s: %s\n",
                      dir, GNOME_CMD_FILE (dir)->get_uri_str(),
                      gnome_vfs_result_to_string (result));

            dir->priv->monitor_handle = NULL;
        }
    }
}


gboolean gnome_cmd_dir_is_monitored (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);

    return dir->priv->monitor_users > 0;
}


gboolean gnome_cmd_dir_is_local (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);

    return gnome_cmd_con_is_local (dir->priv->con);
}


gboolean gnome_cmd_dir_update_mtime (GnomeCmdDir *dir)
{
    // this function also determines if cached dir is up-to-date (FALSE=yes)
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);

    // assume cache is updated
    gboolean returnValue = FALSE;
    GnomeVFSURI *uri = gnome_cmd_dir_get_uri (dir);
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
    GnomeVFSResult res = gnome_vfs_get_file_info_uri (uri, info, (GnomeVFSFileInfoOptions)
                                            (GNOME_VFS_FILE_INFO_FOLLOW_LINKS | GNOME_VFS_FILE_INFO_NAME_ONLY));
    if (res != GNOME_VFS_OK || GNOME_CMD_FILE(dir)->info->mtime != info->mtime)
    {
        // cache is not up-to-date
        GNOME_CMD_FILE (dir)->info->mtime = info->mtime;
        returnValue = TRUE;
    }

    gnome_vfs_file_info_unref (info);
    gnome_vfs_uri_unref (uri);

    // after this function we are sure dir's mtime is up-to-date
    dir->priv->needs_mtime_update = FALSE;

    return returnValue;
}


gboolean gnome_cmd_dir_needs_mtime_update (GnomeCmdDir *dir)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);

    return dir->priv->needs_mtime_update;
}
