/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2003 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 
#include <config.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-dir-pool.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-file-collection.h"
#include "dirlist.h"
#include "utils.h"

#define DIR_PBAR_MAX 50

int created_dirs_cnt = 0;
int deleted_dirs_cnt = 0;

GList *all_dirs = NULL;
GHashTable *all_dirs_map = NULL;

enum {
	FILE_CREATED,
	FILE_DELETED,
	FILE_CHANGED,
	LIST_OK,
	LIST_FAILED,
	LAST_SIGNAL
};

struct _GnomeCmdDirPrivate
{
	GnomeVFSAsyncHandle *list_handle;

	gint ref_cnt;
	GList *files;
	GnomeCmdFileCollection *file_collection;
	GnomeVFSResult last_result;
	GnomeCmdCon *con;
	GnomeCmdPath *path;
	
	gboolean lock;

	Handle *handle;
	GnomeVFSMonitorHandle *monitor_handle;
	gint monitor_users;
};

static GnomeCmdFileClass *parent_class = NULL;

static guint dir_signals[LAST_SIGNAL] = { 0 };


static void
monitor_callback (GnomeVFSMonitorHandle *handle,
				  const gchar *monitor_uri,
				  const gchar *info_uri,
				  GnomeVFSMonitorEventType event_type,
				  GnomeCmdDir *dir)
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


static void
add_dir (GnomeCmdDir *dir)
{
	gchar *uri_str = gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir));
	
	if (!all_dirs_map)
		all_dirs_map = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, NULL);

	DEBUG ('p', "ADDING 0x%x %s to the cache\n", (guint)dir, uri_str);
	g_hash_table_insert (all_dirs_map, uri_str, dir);
}


static void
remove_dir (GnomeCmdDir *dir)
{
	gchar *uri_str = gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir));
	
	DEBUG ('p', "REMOVING 0x%x %s from the cache\n", (guint)dir, uri_str);
	g_hash_table_remove (all_dirs_map, uri_str);
	g_free (uri_str);
}


static GnomeCmdDir *
get_directory (const gchar *uri_str)
{
	GnomeCmdDir *dir = NULL;
	
	if (all_dirs_map)
		dir = g_hash_table_lookup (all_dirs_map, uri_str);
	
	if (dir) {
		DEBUG ('p', "FOUND 0x%x %s in the hash-table, reusing it!\n", (guint)dir, uri_str);
		return dir;
	}
	
	DEBUG ('p', "FAILED to find %s in the hash-table\n", uri_str);
	return NULL;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	GnomeCmdDir *dir = GNOME_CMD_DIR (object);

	DEBUG ('d', "dir destroying 0x%x %s\n", (guint)dir, gnome_cmd_path_get_path (dir->priv->path));

	remove_dir (dir);
	
	gtk_object_destroy (GTK_OBJECT (dir->priv->file_collection));
	
	if (dir->priv->path)
		gtk_object_unref (GTK_OBJECT (dir->priv->path));
	
	gnome_cmd_dir_pool_remove (gnome_cmd_con_get_dir_pool (dir->priv->con), dir);

	dir->priv->handle->ref = NULL;
	handle_unref (dir->priv->handle);
	
	if (dir->priv)
		g_free (dir->priv);
	
	if (DEBUG_ENABLED ('c')) {
		all_dirs = g_list_remove (all_dirs, dir);
		deleted_dirs_cnt++;
	}
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdDirClass *class)
{
	GtkObjectClass *object_class;
	GnomeCmdFileClass *file_class;

	object_class = GTK_OBJECT_CLASS (class);
	file_class = GNOME_CMD_FILE_CLASS (class);
	parent_class = gtk_type_class (gnome_cmd_file_get_type ());

	dir_signals[FILE_CREATED] =
		gtk_signal_new ("file_created",
			GTK_RUN_LAST,
		    G_OBJECT_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GnomeCmdDirClass, file_created),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE,
			1, GTK_TYPE_POINTER);

	dir_signals[FILE_DELETED] =
		gtk_signal_new ("file_deleted",
			GTK_RUN_LAST,
		    G_OBJECT_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GnomeCmdDirClass, file_deleted),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE,
			1, GTK_TYPE_POINTER);
	
	dir_signals[FILE_CHANGED] =
		gtk_signal_new ("file_changed",
			GTK_RUN_LAST,
		    G_OBJECT_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GnomeCmdDirClass, file_changed),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE,
			1, GTK_TYPE_POINTER);
	
	dir_signals[LIST_OK] =
		gtk_signal_new ("list_ok",
			GTK_RUN_LAST,
		    G_OBJECT_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GnomeCmdDirClass, list_ok),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE,
			1, GTK_TYPE_POINTER);
	
	dir_signals[LIST_FAILED] =
		gtk_signal_new ("list_failed",
			GTK_RUN_LAST,
		    G_OBJECT_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GnomeCmdDirClass, list_failed),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE,
			1, GTK_TYPE_INT);
	
	object_class->destroy = destroy;
	class->file_created = NULL;
	class->file_deleted = NULL;
	class->file_changed = NULL;
	class->list_ok = NULL;
	class->list_failed = NULL;
}

static void
init (GnomeCmdDir *dir)
{
	dir->priv = g_new0 (GnomeCmdDirPrivate, 1);
	dir->voffset = 0;
	dir->dialog = NULL;
	dir->state = DIR_STATE_EMPTY;

	if (DEBUG_ENABLED ('c')) {
		created_dirs_cnt++;
		all_dirs = g_list_append (all_dirs, dir);
	}

	dir->priv->handle = handle_new (dir);
	dir->priv->monitor_handle = NULL;
	dir->priv->monitor_users = 0;
	dir->priv->files = NULL;
	dir->priv->file_collection = gnome_cmd_file_collection_new ();
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_dir_get_type         (void)
{
	static GtkType type = 0;

	if (type == 0)
	{
		GtkTypeInfo info =
		{
			"GnomeCmdDir",
			sizeof (GnomeCmdDir),
			sizeof (GnomeCmdDirClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gnome_cmd_file_get_type (), &info);
	}
	return type;
}


GnomeCmdDir *
gnome_cmd_dir_new_from_info (GnomeVFSFileInfo *info, GnomeCmdDir *parent)
{
	GnomeCmdDir *dir;
	GnomeCmdCon *con;
	GnomeCmdPath *path;
	GnomeVFSURI *uri;
	gchar *uri_str;

	g_return_val_if_fail (info != NULL, NULL);
	g_return_val_if_fail (GNOME_CMD_IS_DIR (parent), NULL);

	con = gnome_cmd_dir_get_connection (parent);
	path = gnome_cmd_path_get_child (
		gnome_cmd_dir_get_path (parent), info->name);

	uri = gnome_cmd_con_create_uri (con, path);
	uri_str = gnome_vfs_uri_to_string (uri, 0);

	dir = get_directory (uri_str);
	g_free (uri_str);
	gnome_vfs_uri_unref (uri);
	if (dir) {
		gtk_object_destroy (GTK_OBJECT (path));
		gnome_cmd_file_update_info (GNOME_CMD_FILE (dir), info);
		return dir;
	}
	
	dir = gtk_type_new (gnome_cmd_dir_get_type ());	
	gnome_cmd_file_setup (GNOME_CMD_FILE (dir), info, parent);

	dir->priv->con = con;
	gnome_cmd_dir_set_path (dir, path);
	gtk_object_ref (GTK_OBJECT (path));

	add_dir (dir);
	
	return dir;
}


GnomeCmdDir *
gnome_cmd_dir_new_with_con (GnomeVFSFileInfo *info,
							GnomeCmdPath *path,
							GnomeCmdCon *con)
{
	GnomeCmdDir *dir;
	GnomeVFSURI *uri;
	gchar *uri_str;

	g_return_val_if_fail (info != NULL, NULL);
	g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
	
	uri = gnome_cmd_con_create_uri (con, path);
	uri_str = gnome_vfs_uri_to_string (uri, 0);

	dir = get_directory (uri_str);
	if (dir) {
		g_free (uri_str);
		gnome_vfs_uri_unref (uri);
		gnome_cmd_file_update_info (GNOME_CMD_FILE (dir), info);
		return dir;
	}
	
	dir = gtk_type_new (gnome_cmd_dir_get_type ());	
	gnome_cmd_file_setup (GNOME_CMD_FILE (dir), info, NULL);

	dir->priv->con = con;
	gnome_cmd_dir_set_path (dir, path);
	gtk_object_ref (GTK_OBJECT (path));

	add_dir (dir);

	return dir;
}


GnomeCmdDir *
gnome_cmd_dir_new (GnomeCmdCon *con, GnomeCmdPath *path)
{
	GnomeCmdDir *dir = NULL;
	GnomeVFSFileInfo *info;
	GnomeVFSURI *uri;
	gchar *uri_str;
	GnomeVFSResult res;
	GnomeVFSFileInfoOptions infoOpts = GNOME_VFS_FILE_INFO_FOLLOW_LINKS
		| GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		| GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE;
	
	g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
	g_return_val_if_fail (GNOME_CMD_IS_PATH (path), NULL);

	uri = gnome_cmd_con_create_uri (con, path);
	if (!uri) return NULL;
	
	uri_str = gnome_vfs_uri_to_string (uri, 0);

	dir = get_directory (uri_str);
	if (dir) {
		g_free (uri_str);
		return dir;
	}
	
	info = gnome_vfs_file_info_new ();
	res = gnome_vfs_get_file_info_uri (
		uri, info, infoOpts);
	if (res == GNOME_VFS_OK) {
		dir = gtk_type_new (gnome_cmd_dir_get_type ());
		gnome_cmd_file_setup (GNOME_CMD_FILE (dir), info, NULL);
		
		dir->priv->con = con;
		gnome_cmd_dir_set_path (dir, path);
		gtk_object_ref (GTK_OBJECT (path));

		add_dir (dir);
	}
	else {
		create_error_dialog (gnome_vfs_result_to_string (res));
		gnome_vfs_file_info_unref (info);
	}
	
	gnome_vfs_uri_unref (uri);
	g_free (uri_str);

	return dir;
}


GnomeCmdDir *
gnome_cmd_dir_get_parent (GnomeCmdDir *dir)
{
	GnomeCmdPath *path;

	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);
	
	path = gnome_cmd_path_get_parent (dir->priv->path);
	if (!path)
		return NULL;	
	
	return gnome_cmd_dir_new (dir->priv->con, path);
}


GnomeCmdDir *
gnome_cmd_dir_get_child (GnomeCmdDir *dir, const gchar *child)
{
	GnomeCmdPath *path;

	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);
	
	path = gnome_cmd_path_get_child (dir->priv->path, child);
	if (!path)
		return NULL;	
	
	return gnome_cmd_dir_new (dir->priv->con, path);
}


GnomeCmdCon *
gnome_cmd_dir_get_connection (GnomeCmdDir *dir)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

	return dir->priv->con;
}


Handle *gnome_cmd_dir_get_handle (GnomeCmdDir *dir)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);
	
	return dir->priv->handle;
}


void
gnome_cmd_dir_ref (GnomeCmdDir *dir)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	gnome_cmd_file_ref (GNOME_CMD_FILE (dir));
}


void
gnome_cmd_dir_unref (GnomeCmdDir *dir)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	gnome_cmd_file_unref (GNOME_CMD_FILE (dir));
}


GnomeVFSResult
gnome_cmd_dir_get_files (GnomeCmdDir *dir, GList **files)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (files)
		*files = dir->priv->files;

	return dir->priv->last_result;
}


static GList *
create_file_list (GnomeCmdDir *dir, GList *info_list)
{
	GList *tmp, *file_list = NULL;
	
	/* create a new list with GnomeCmdFile objects */
	tmp = info_list;
	while (tmp) {
		GnomeCmdFile *finfo;
		GnomeVFSFileInfo *info = (GnomeVFSFileInfo*)tmp->data;
		GnomeCmdCon *con = gnome_cmd_dir_get_connection (dir);
		if (info && info->name) {
			if (strcmp (info->name, ".") == 0 || strcmp (info->name, "..") == 0) {
				gnome_vfs_file_info_unref (info);
				tmp = tmp->next;
				continue;
			}
			
			if (GNOME_CMD_IS_CON_SMB (con)
				&& info->mime_type
				&& !strcmp (info->mime_type, "application/x-gnome-app-info")
				&& strcmp (info->name, ".directory")) {
				// This is a hack to make samba workgroups etc 
				// look like normal directories
				info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
				info->mime_type = g_strdup ("x-directory/normal");
			}

			if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
				finfo = GNOME_CMD_FILE (gnome_cmd_dir_new_from_info (info, dir));
			else
				finfo = gnome_cmd_file_new (info, dir);

			gnome_cmd_file_ref (finfo);
			file_list = g_list_append (file_list, finfo);
		}
		tmp = tmp->next;
	}

	return file_list;
}


static void
on_list_done (GnomeCmdDir *dir, GList *infolist, GnomeVFSResult result)
{
	if (dir->state == DIR_STATE_LISTED) {
		DEBUG('l', "File listing succeded\n");
	
		if (gnome_cmd_file_collection_get_size (dir->priv->file_collection))
			gnome_cmd_file_collection_clear (dir->priv->file_collection);
		
		dir->priv->files = create_file_list (dir, infolist);
		gnome_cmd_file_collection_add_list (dir->priv->file_collection,
											dir->priv->files);
		dir->state = DIR_STATE_LISTED;
		g_list_free (infolist);
	
		if (dir->dialog) {
			gtk_widget_destroy (dir->dialog);
			dir->dialog = NULL;
		}
		dir->priv->lock = FALSE;
		dir->priv->last_result = GNOME_VFS_OK;

		DEBUG('l', "Emitting list_ok signal\n");
		gtk_signal_emit (GTK_OBJECT (dir), dir_signals[LIST_OK], dir->priv->files);
	}
	else if (dir->state == DIR_STATE_EMPTY) {
		DEBUG('l', "File listing failed: %s\n", gnome_vfs_result_to_string (result));

		if (dir->dialog) {
			gtk_widget_destroy (dir->dialog);
			dir->dialog = NULL;
		}
		
		dir->priv->last_result = result;
		dir->priv->lock = FALSE;

		DEBUG('l', "Emitting list_failed signal\n");
		gtk_signal_emit (GTK_OBJECT (dir), dir_signals[LIST_FAILED], result);
	}
}

 
static void
on_dir_list_cancel (GtkButton *btn, GnomeCmdDir *dir)
{
	if (dir->state == DIR_STATE_LISTING) {
		DEBUG('l', "on_dir_list_cancel\n");
		dirlist_cancel (dir);
	
		gtk_widget_destroy (dir->dialog);
		dir->dialog = NULL;
	}
}

 
static void
create_list_progress_dialog (GnomeCmdDir *dir)
{
	GtkWidget *vbox;
	
	dir->dialog = gnome_cmd_dialog_new (NULL);
	gtk_widget_ref (dir->dialog);

	gnome_cmd_dialog_add_button (
		GNOME_CMD_DIALOG (dir->dialog),
		GNOME_STOCK_BUTTON_CANCEL,
		GTK_SIGNAL_FUNC (on_dir_list_cancel), dir);

	vbox = create_vbox (dir->dialog, FALSE, 0);
	
	dir->label = create_label (dir->dialog, _("Waiting for file list"));

	dir->pbar = create_progress_bar (dir->dialog);
	gtk_progress_set_show_text (GTK_PROGRESS (dir->pbar), FALSE);
	gtk_progress_set_activity_mode (GTK_PROGRESS (dir->pbar), TRUE);
	gtk_progress_configure (GTK_PROGRESS (dir->pbar), 0, 0, DIR_PBAR_MAX);
	
	gtk_box_pack_start (GTK_BOX (vbox), dir->label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), dir->pbar, FALSE, TRUE, 0);
	
	gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dir->dialog), vbox);
	
	gnome_cmd_dialog_set_transient_for (
		GNOME_CMD_DIALOG (dir->dialog),
		GTK_WINDOW (main_win));

	gtk_widget_show_all (dir->dialog);
}


void
gnome_cmd_dir_relist_files (GnomeCmdDir *dir, gboolean visprog)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	if (dir->priv->lock) return;
	dir->priv->lock = TRUE;

	dir->done_func = (DirListDoneFunc)on_list_done;

	if (visprog)
		create_list_progress_dialog (dir);
	
	dirlist_list (dir, visprog);
}


void
gnome_cmd_dir_list_files (GnomeCmdDir *dir, gboolean visprog)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	if (!dir->priv->files
		|| gnome_vfs_uri_is_local (gnome_cmd_file_get_uri (GNOME_CMD_FILE (dir))))
	{
		DEBUG ('l', "relisting files for 0x%x %s %d\n",
			   dir,
			   gnome_cmd_file_get_path (GNOME_CMD_FILE (dir)),
			   visprog);
		gnome_cmd_dir_relist_files (dir, visprog);		
	}
	else {
		gtk_signal_emit (GTK_OBJECT (dir), dir_signals[LIST_OK], dir->priv->files);
	}
}


GnomeCmdPath *
gnome_cmd_dir_get_path (GnomeCmdDir *dir)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

	return dir->priv->path;
}


void
gnome_cmd_dir_set_path (GnomeCmdDir *dir, GnomeCmdPath *path)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	if (dir->priv->path)
		gtk_object_destroy (GTK_OBJECT (dir->priv->path));

	dir->priv->path = path;
	gtk_object_ref (GTK_OBJECT (path));
}


gchar *
gnome_cmd_dir_get_display_path (GnomeCmdDir *dir)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

	return g_strdup (gnome_cmd_path_get_display_path (dir->priv->path));
}


GnomeVFSURI *
gnome_cmd_dir_get_child_uri (GnomeCmdDir *dir, const gchar *filename)
{
	GnomeVFSURI *uri;
	GnomeCmdPath *path;

	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

	path = gnome_cmd_path_get_child (dir->priv->path, filename);
	uri = gnome_cmd_con_create_uri (dir->priv->con, path);
	gtk_object_destroy (GTK_OBJECT (path));

	return uri;
}


gchar *
gnome_cmd_dir_get_child_uri_str (GnomeCmdDir *dir, const gchar *filename)
{
	gchar *uri_str;
	GnomeVFSURI *uri;
	
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

	uri = gnome_cmd_dir_get_child_uri (dir, filename);
	uri_str = gnome_vfs_uri_to_string (uri, 0);
	gnome_vfs_uri_unref (uri);

	return uri_str;
}


static gboolean
file_already_exists (GnomeCmdDir *dir, const gchar *uri_str)
{
	GnomeCmdFile *finfo;

	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), TRUE);
	g_return_val_if_fail (uri_str != NULL, TRUE);

	finfo = gnome_cmd_file_collection_lookup (
		dir->priv->file_collection, uri_str);
	
	return finfo != NULL;
}


/* A file has been created. Create a new GnomeCmdFile object for that file
 */
void gnome_cmd_dir_file_created (GnomeCmdDir *dir, const gchar *uri_str)
{
	GnomeVFSURI *uri;
	GnomeVFSResult res;
	GnomeVFSFileInfo *info;
	GnomeCmdFile *finfo;
	GnomeVFSFileInfoOptions infoOpts =
		GNOME_VFS_FILE_INFO_FOLLOW_LINKS|GNOME_VFS_FILE_INFO_GET_MIME_TYPE;

	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	g_return_if_fail (uri_str != NULL);

	if (file_already_exists (dir, uri_str))
		return;
	
	uri = gnome_vfs_uri_new (uri_str);
	info = gnome_vfs_file_info_new ();	
	res = gnome_vfs_get_file_info_uri (
		uri, info, infoOpts);
	gnome_vfs_uri_unref (uri);
	
	if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) 
		finfo = GNOME_CMD_FILE (gnome_cmd_dir_new_from_info (info, dir));
	else
		finfo = gnome_cmd_file_new (info, dir);

	gnome_cmd_file_collection_add (dir->priv->file_collection, finfo);
	dir->priv->files = gnome_cmd_file_collection_get_list (
		dir->priv->file_collection);

	gtk_signal_emit (GTK_OBJECT (dir), dir_signals[FILE_CREATED], finfo);
}


/* A file has been deleted. Remove the corresponding GnomeCmdFile
 */
void gnome_cmd_dir_file_deleted (GnomeCmdDir *dir, const gchar *uri_str)
{
	GnomeCmdFile *finfo;
	
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	g_return_if_fail (uri_str != NULL);

	finfo = gnome_cmd_file_collection_lookup (
		dir->priv->file_collection, uri_str);
	g_return_if_fail (GNOME_CMD_IS_FILE (finfo));
	gtk_signal_emit (GTK_OBJECT (dir), dir_signals[FILE_DELETED], finfo);

	gnome_cmd_file_collection_remove (
		dir->priv->file_collection, finfo);
	dir->priv->files = gnome_cmd_file_collection_get_list (
		dir->priv->file_collection);
}


/* A file has been changed. Find the corresponding GnomeCmdFile, update its
 * GnomeVFSFileInfo
 */
void gnome_cmd_dir_file_changed (GnomeCmdDir *dir, const gchar *uri_str)
{
	GnomeCmdFile *finfo;
	GnomeVFSResult res;
	GnomeVFSURI *uri;
	GnomeVFSFileInfo *info;
	GnomeVFSFileInfoOptions infoOpts = GNOME_VFS_FILE_INFO_GET_MIME_TYPE;

	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	g_return_if_fail (uri_str != NULL);

	finfo = gnome_cmd_file_collection_lookup (
		dir->priv->file_collection, uri_str);
	g_return_if_fail (GNOME_CMD_IS_FILE (finfo));

	uri = gnome_cmd_file_get_uri (finfo);
	info = gnome_vfs_file_info_new ();
	res = gnome_vfs_get_file_info_uri (
		uri, info, infoOpts);
	gnome_vfs_uri_unref (uri);
			
	gnome_cmd_file_update_info (finfo, info);
	gtk_signal_emit (GTK_OBJECT (dir), dir_signals[FILE_CHANGED], finfo);
}


void gnome_cmd_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *finfo)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	g_return_if_fail (GNOME_CMD_IS_FILE (finfo));

	gtk_signal_emit (GTK_OBJECT (dir), dir_signals[FILE_CHANGED], finfo);
}


gboolean
gnome_cmd_dir_uses_fam (GnomeCmdDir *dir)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);
	
	return FALSE;
}


void
gnome_cmd_dir_start_monitoring (GnomeCmdDir *dir)
{
	GnomeVFSResult result;

	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	if (dir->priv->monitor_users == 0) {
		gchar *uri_str = gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir));	
	
		result = gnome_vfs_monitor_add (
			&dir->priv->monitor_handle,
			uri_str,
			GNOME_VFS_MONITOR_DIRECTORY,
			(GnomeVFSMonitorCallback)monitor_callback,
			dir);
	
		if (result == GNOME_VFS_OK)
			DEBUG('n', "Added monitor to 0x%x %s\n", dir, uri_str);
		else 
			DEBUG ('n', "Failed to add monitor to 0x%x %s: %s\n",
				   (guint)dir, uri_str, gnome_vfs_result_to_string (result));
		g_free (uri_str);
	}

	dir->priv->monitor_users++;
}


void
gnome_cmd_dir_cancel_monitoring (GnomeCmdDir *dir)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	if (dir->priv->monitor_users < 1) return;
	
	dir->priv->monitor_users--;

	if (dir->priv->monitor_users == 0) {
		if (dir->priv->monitor_handle) {
			GnomeVFSResult result = gnome_vfs_monitor_cancel (dir->priv->monitor_handle);
			if (result == GNOME_VFS_OK)
				DEBUG('n', "Removed monitor from 0x%x %s\n",
					  dir, gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir)));
			else
				DEBUG('n', "Failed to remove monitor from 0x%x %s: %s\n",
					  (guint)dir, gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir)),
					  gnome_vfs_result_to_string (result));
		
			dir->priv->monitor_handle = NULL;
		}
	}
}


gboolean
gnome_cmd_dir_is_local (GnomeCmdDir *dir)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), FALSE);
	
	return gnome_cmd_con_is_local (dir->priv->con);
}
