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
#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "utils.h"
#include "dirlist.h"


#define FILES_PER_NOTIFICATION 50
#define LIST_PRIORITY 0


static void
on_files_listed (GnomeVFSAsyncHandle *handle,
				 GnomeVFSResult result,
				 GList *list,
				 guint entries_read,
				 GnomeCmdDir *dir)
{
	DEBUG('l', "on_files_listed\n");
	
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		DEBUG ('l', "Directory listing failed, %s\n", gnome_vfs_result_to_string (result));	
		dir->state = DIR_STATE_EMPTY;
		dir->list_result = result;
	}	
	
	if (entries_read > 0 && list != NULL) {
		g_list_foreach (list, (GFunc)gnome_vfs_file_info_ref, NULL);
		dir->infolist = g_list_concat (dir->infolist, g_list_copy (list));
		dir->list_counter += entries_read;
		DEBUG ('l', "files listed: %d\n", dir->list_counter);
	}
	
	if (result == GNOME_VFS_ERROR_EOF) {
		dir->state = DIR_STATE_LISTED;
		dir->list_result = GNOME_VFS_OK;
		DEBUG('l', "All files listed\n");
	}
}


static gboolean
update_list_progress (GnomeCmdDir *dir)
{
	DEBUG ('l', "Checking list progress...\n");

	if (dir->state == DIR_STATE_LISTING) {
		gchar *msg = g_strdup_printf ("%d files listed", dir->list_counter);
		gtk_label_set_text (GTK_LABEL (dir->label), msg);
		progress_bar_update (dir->pbar, 50);
		DEBUG('l', "%s\n", msg);
		g_free (msg);
		return TRUE;
	}
	
	DEBUG ('l', "calling list_done func\n");
	dir->done_func (dir, dir->infolist, dir->list_result);
	return FALSE;
}


static void
visprog_list (GnomeCmdDir *dir)
{
	gchar *uri_str;
	GnomeVFSURI *uri;
	GnomeVFSFileInfoOptions infoOpts;

	DEBUG('l', "visprog_list\n");

	infoOpts = GNOME_VFS_FILE_INFO_FOLLOW_LINKS
		| GNOME_VFS_FILE_INFO_GET_MIME_TYPE;

	uri = gnome_cmd_file_get_uri (GNOME_CMD_FILE (dir));
	uri_str = gnome_vfs_uri_to_string (uri, 0);
	DEBUG('l', "visprog_list: %s\n", uri_str);
	g_free (uri_str);
	
	gnome_vfs_async_load_directory_uri (
		&dir->list_handle,
		uri,
		infoOpts,
		FILES_PER_NOTIFICATION,
		LIST_PRIORITY,
		(GnomeVFSAsyncDirectoryLoadCallback)on_files_listed,
		dir);

	gtk_timeout_add (gnome_cmd_data_get_gui_update_rate (), (GtkFunction)update_list_progress, dir);
}


static void
blocking_list (GnomeCmdDir *dir)
{
	gchar *uri_str;
	GnomeVFSFileInfoOptions infoOpts;

	DEBUG('l', "blocking_list\n");
	
	infoOpts = GNOME_VFS_FILE_INFO_FOLLOW_LINKS
		| GNOME_VFS_FILE_INFO_GET_MIME_TYPE;

	uri_str = gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir));
	DEBUG('l', "blocking_list: %s\n", uri_str);

	dir->list_result = gnome_vfs_directory_list_load (
		&dir->infolist,
		uri_str,
		infoOpts);

	g_free (uri_str);

	if (dir->list_result == GNOME_VFS_OK) {
		dir->state = DIR_STATE_LISTED;
		dir->done_func (dir, dir->infolist, GNOME_VFS_OK);
	}
	else {
		dir->state = DIR_STATE_EMPTY;
		dir->done_func (dir, NULL, dir->list_result);
	}
}


void
dirlist_list (GnomeCmdDir *dir, gboolean visprog)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	dir->infolist = NULL;
	dir->list_handle = NULL;
	dir->list_counter = 0;
	dir->list_result = GNOME_VFS_OK;
	dir->state = DIR_STATE_LISTING;
	
	if (!visprog) {
		blocking_list (dir);
		return;
	}

	visprog_list (dir);
}


void
dirlist_cancel (GnomeCmdDir *dir)
{
	dir->state = DIR_STATE_EMPTY;
	dir->list_result = GNOME_VFS_OK;

	DEBUG('l', "Calling async_cancel\n");
	gnome_vfs_async_cancel (dir->list_handle);
}
