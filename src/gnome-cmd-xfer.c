/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2004 Marcus Bjurman

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
#include <unistd.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-xfer-progress-win.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "utils.h"


#define XFER_PRIORITY 0



typedef struct
{
	GnomeVFSXferOptions xferOptions;
	GnomeVFSAsyncHandle *handle;
	
	// Source and target uri's. The first src_uri should be transfered to the
	// first dest_uri and so on...
	GList *src_uri_list;
	GList *dest_uri_list;

	GnomeCmdDir *to_dir;
	GnomeCmdFileList *src_fl;
	GList *src_files;

	// Used for showing the progress
	GnomeCmdXferProgressWin *win;
	GnomeVFSXferPhase cur_phase;
	GnomeVFSXferPhase prev_phase;
	GnomeVFSXferProgressStatus prev_status;
	gulong cur_file;
	gulong prev_file;
	gulong files_total;
	gfloat prev_totalprog;
	gboolean first_time;
	gchar *cur_file_name;

	GnomeVFSFileSize bytes_total;
	GnomeVFSFileSize file_size;
	GnomeVFSFileSize bytes_copied;
	GnomeVFSFileSize total_bytes_copied;
	
	GFunc on_completed_func;
	gpointer on_completed_data;

	gboolean done;
	gboolean aborted;
	
} XferData;


static void
free_xfer_data (XferData *data)
{
	GList *tmp_list;
	GnomeVFSURI *uri;

	if (data->on_completed_func)
		data->on_completed_func (data->on_completed_data, NULL);

	g_list_free (data->src_uri_list);

	/* free the list with target uris */
	tmp_list = data->dest_uri_list;
	while (tmp_list)
	{
		uri = (GnomeVFSURI*)tmp_list->data;
		gnome_vfs_uri_unref (uri);
		tmp_list = tmp_list->next;
	}
	g_list_free (data->dest_uri_list);

	g_free (data);
}


static XferData *
create_xfer_data (GnomeVFSXferOptions xferOptions, GList *src_uri_list, GList *dest_uri_list,
				  GnomeCmdDir *to_dir, GnomeCmdFileList *src_fl, GList *src_files,
				  GFunc on_completed_func, gpointer on_completed_data)
{
	XferData *data = g_new0 (XferData, 1);

	data->xferOptions = xferOptions;
	data->src_uri_list = src_uri_list;
	data->dest_uri_list = dest_uri_list;
	data->to_dir = to_dir;
	data->src_fl = src_fl;
	data->src_files = src_files;
	data->win = NULL;
	data->cur_file_name = NULL;
	data->prev_status = GNOME_VFS_XFER_PROGRESS_STATUS_OK;
	data->cur_phase = -1;
	data->prev_phase = -1;
	data->cur_file = -1;
	data->prev_file = -1;
	data->files_total = 0;
	data->prev_totalprog = (gfloat)0.00;
	data->first_time = TRUE;
	data->on_completed_func = on_completed_func;
	data->on_completed_data = on_completed_data;
	data->done = FALSE;
	data->aborted = FALSE;

	return data;
}


static gint
xfer_callback (GnomeVFSXferProgressInfo *info, gpointer user_data)
{
	return 1;
}


static gint
async_xfer_callback (GnomeVFSAsyncHandle *handle,
					 GnomeVFSXferProgressInfo *info,
					 XferData *data)
{
	data->cur_phase = info->phase;
	data->cur_file = info->file_index;
	data->files_total = info->files_total;
	data->bytes_total = info->bytes_total;
	data->file_size = info->file_size;
	data->bytes_copied = info->bytes_copied;
	data->total_bytes_copied = info->total_bytes_copied;

	if (data->aborted)
		return 0;

	if (info->source_name != NULL) {
		if (data->cur_file_name
			&& strcmp (data->cur_file_name, info->source_name) != 0) {
			g_free (data->cur_file_name);
			data->cur_file_name = NULL;
		}

		if (!data->cur_file_name)
			data->cur_file_name = g_strdup (info->source_name);
	}
	
	if (info->status == GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE) {
		gchar *t = gnome_vfs_get_local_path_from_uri (info->target_name);
		gchar *fn = get_utf8 (t);
		gchar *msg = g_strdup_printf (_("The file \"%s\" already exists.\n\nDo you want to overwrite it?\n"), fn);
		gint ret;

		gdk_threads_enter ();
		ret = run_simple_dialog (
			GTK_WIDGET (main_win), TRUE, GTK_MESSAGE_ERROR, msg, " ",
			_("Abort"), _("Replace"), _("Replace All"), _("Skip"), _("Skip All"), NULL);
		g_free (msg);
		g_free (fn);
		g_free (t);
		data->prev_status = GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE;
		gdk_threads_leave ();
		return ret;
	}
	else if (info->status == GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR
			 && data->prev_status != GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE) {
		const gchar *error = gnome_vfs_result_to_string (info->vfs_status);
		gchar *t = gnome_vfs_get_local_path_from_uri (info->target_name);
		gchar *fn = get_utf8 (t);
		gchar *msg = g_strdup_printf (
			_("Error while copying to %s\n\n%s"), fn, error);
		gint ret;

		gdk_threads_leave ();
		ret = run_simple_dialog (
			GTK_WIDGET (main_win), TRUE, GTK_MESSAGE_ERROR, msg, _("Xfer problem"),
			_("Abort"), _("Retry"), _("Skip"), NULL);
		g_free (msg);
		g_free (fn);
		g_free (t);
		data->prev_status = GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR;
		gdk_threads_leave ();
		return ret;
	}

	if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
		gnome_cmd_main_win_focus_file_lists (main_win);
		data->done = TRUE;
	}

	data->prev_status = info->status;
	
	return 1;
}


static gboolean
update_xfer_gui_func (XferData *data)
{
	if (data->win && data->win->cancel_pressed) {
		data->aborted = TRUE;

		if (data->on_completed_func)
			data->on_completed_func (data->on_completed_data, NULL);

		gtk_widget_destroy (GTK_WIDGET (data->win));
		return FALSE;
	}

	if (data->cur_phase == GNOME_VFS_XFER_PHASE_COPYING) {
		gfloat total_diff=0;

		if (data->prev_phase != GNOME_VFS_XFER_PHASE_COPYING) {
			gnome_cmd_xfer_progress_win_set_action (data->win, _("copying..."));
			data->prev_file = -1;
		}
		
		if (data->prev_file != data->cur_file) {
			gchar *t = str_uri_basename (data->cur_file_name);
			gchar *fn = get_utf8 (t);
			gchar *msg = g_strdup_printf (
				_("[file %ld of %ld] \"%s\""),
				data->cur_file, data->files_total, fn);
			
			gnome_cmd_xfer_progress_win_set_msg (data->win, msg);

			data->prev_file = data->cur_file;
			
			if (msg) g_free (msg);
			if (fn) g_free (fn);
			if (t) g_free (t);
		}

		if (data->bytes_total > 0) {
			gfloat total_prog;
			
			total_prog = (gfloat)((gdouble)data->total_bytes_copied / (gdouble)data->bytes_total);
			total_diff = total_prog - data->prev_totalprog;

			if ((total_diff > (gfloat)0.01 && total_prog >= 0.0 && total_prog <= 1.0)
				|| data->first_time) {
				data->first_time = FALSE;
				gnome_cmd_xfer_progress_win_set_total_progress (
					data->win, data->total_bytes_copied, data->bytes_total);
				while (gtk_events_pending ())
					gtk_main_iteration_do (FALSE);
			}
		}
		
	}

	if (data->done) {
		/* Remove files from the source file-list when a move
		   operation has finished */
		if (data->xferOptions & GNOME_VFS_XFER_REMOVESOURCE) {
			g_printerr ("src_fl: %x\n", (guint)data->src_fl);
			if (data->src_fl && data->src_files)
				gnome_cmd_file_list_remove_files (
					data->src_fl, data->src_files);
		}

		/* Only update the files if needed */
		if (data->to_dir) {
			gnome_cmd_dir_relist_files (data->to_dir, FALSE);
			gnome_cmd_main_win_focus_file_lists (main_win);
			gnome_cmd_dir_unref (data->to_dir);
			data->to_dir = NULL;
		}
		
		if (data->win) {
			gtk_widget_destroy (GTK_WIDGET (data->win));
			data->win = NULL;
		}

		free_xfer_data (data);
		
		return FALSE;
	}
	
	data->prev_phase = data->cur_phase;
	
	return TRUE;
}


static gboolean
uri_is_parent_to_dir (GnomeVFSURI *uri, GnomeCmdDir *dir)
{
	GnomeVFSURI *dir_uri;
	gboolean ret;

	dir_uri = gnome_cmd_file_get_uri (GNOME_CMD_FILE (dir));

	ret = gnome_vfs_uri_is_parent (uri, dir_uri, TRUE);
	gnome_vfs_uri_unref (dir_uri);
	
	return ret;
}


static gchar *
remove_basename (gchar *in)
{
	gint i;
	gchar *out = g_strdup (in);

	for ( i=strlen(out)-1 ; i>0 ; i-- ) {
		if (out[i] == '/') {
			out[i] = '\0';
			return out;
		}
	}

	return NULL;
}


static gboolean
file_is_already_in_dir (GnomeVFSURI *uri, GnomeCmdDir *dir) {
	gchar *uri_str, *tmp;
	gchar *dir_uri_str;
	gboolean ret;

	tmp = gnome_vfs_uri_to_string (uri, 0);
	uri_str = remove_basename (tmp);
	dir_uri_str = gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir));

	ret = (strcmp (uri_str, dir_uri_str) == 0);

	g_free (uri_str);
	g_free (dir_uri_str);
	g_free (tmp);

	return ret;
}


void
gnome_cmd_xfer_uris_start (GList *src_uri_list,
						   GnomeCmdDir *to_dir,
						   GnomeCmdFileList *src_fl,
						   GList *src_files,
						   gchar *dest_fn,
						   GnomeVFSXferOptions xferOptions,
						   GnomeVFSXferOverwriteMode xferOverwriteMode,
						   GtkSignalFunc on_completed_func,
						   gpointer on_completed_data)
{
	GnomeVFSURI *src_uri, *dest_uri;
	GnomeVFSResult result;
	gint num_files;
	XferData *data;
	GList *tmp;

	g_return_if_fail (src_uri_list != NULL);
	g_return_if_fail (GNOME_CMD_IS_DIR (to_dir));

	/*
	 * Sanity check
	 */
	tmp = src_uri_list;
	while (tmp) {
		src_uri = (GnomeVFSURI*)tmp->data;
		if (uri_is_parent_to_dir (src_uri, to_dir)) {
			create_error_dialog (_("Copying a directory into it self is a bad idea.\nThe whole operation was canceled."));
			return;
		}
		if (file_is_already_in_dir (src_uri, to_dir)) {
			if (dest_fn && strcmp (
				dest_fn, gnome_vfs_uri_extract_short_name (src_uri)) == 0) {
				DEBUG ('x', "Copying a file to the same directory as it's already in, is not permitted\n");
				return;
			}
		}
		
		tmp = tmp->next;
	}	
	
	data = create_xfer_data (
		xferOptions, src_uri_list, NULL,
		to_dir, src_fl, src_files,
		(GFunc)on_completed_func, on_completed_data);
	
	num_files = g_list_length (src_uri_list);

	if (g_list_length (src_uri_list) == 1 && dest_fn != NULL) {
		dest_uri = gnome_cmd_dir_get_child_uri (to_dir, dest_fn);

		data->dest_uri_list = g_list_append (data->dest_uri_list, dest_uri);
	}
	else {
		while (src_uri_list) {
			gchar *basename;
			src_uri = (GnomeVFSURI*)src_uri_list->data;
			basename = gnome_vfs_unescape_string (
				gnome_vfs_uri_extract_short_name (src_uri), 0);

			dest_uri = gnome_cmd_dir_get_child_uri (to_dir, basename);
			g_free (basename);
		
			data->dest_uri_list = g_list_append (data->dest_uri_list, dest_uri);
				
			src_uri_list = src_uri_list->next;
		}
	}

	if (dest_fn != NULL)
		g_free (dest_fn);
	
	data->win = GNOME_CMD_XFER_PROGRESS_WIN (gnome_cmd_xfer_progress_win_new ());
	gtk_widget_ref (GTK_WIDGET (data->win));
	gtk_window_set_title (GTK_WINDOW (data->win), _("preparing..."));
	gtk_widget_show (GTK_WIDGET (data->win));
	
	/* start the transfer */
	result = gnome_vfs_async_xfer (
		&data->handle,
		data->src_uri_list,
		data->dest_uri_list,
		xferOptions,
		GNOME_VFS_XFER_ERROR_MODE_QUERY,
		xferOverwriteMode,
		XFER_PRIORITY,
		(GnomeVFSAsyncXferProgressCallback)async_xfer_callback,
		data,
		xfer_callback,
		data);

	gtk_timeout_add (
		gnome_cmd_data_get_gui_update_rate (),
		(GtkFunction)update_xfer_gui_func,
		data);
}


void
gnome_cmd_xfer_start (GList *src_files,
					  GnomeCmdDir *to_dir,
					  GnomeCmdFileList *src_fl,
					  gchar *dest_fn,
					  GnomeVFSXferOptions xferOptions,
					  GnomeVFSXferOverwriteMode xferOverwriteMode,
					  GtkSignalFunc on_completed_func,
					  gpointer on_completed_data)
{
	GList *src_uri_list = NULL;

	g_return_if_fail (src_files != NULL);
	g_return_if_fail (GNOME_CMD_IS_DIR (to_dir));

	src_uri_list = file_list_to_uri_list (src_files);

	gnome_cmd_xfer_uris_start (src_uri_list,
							   to_dir,
							   src_fl,
							   src_files,
							   dest_fn,
							   xferOptions,
							   xferOverwriteMode,
							   on_completed_func,
							   on_completed_data);
}


void
gnome_cmd_xfer_tmp_download (GnomeVFSURI *src_uri,
							 GnomeVFSURI *dest_uri,
							 GnomeVFSXferOptions xferOptions,
							 GnomeVFSXferOverwriteMode xferOverwriteMode,
							 GtkSignalFunc on_completed_func,
							 gpointer on_completed_data)
{
	g_return_if_fail (src_uri != NULL);
	g_return_if_fail (dest_uri != NULL);
	
	gnome_cmd_xfer_tmp_download_multiple (
		g_list_append (NULL, src_uri),
		g_list_append (NULL, dest_uri),
		xferOptions,
		xferOverwriteMode,
		on_completed_func,
		on_completed_data);
		
}
							 

void
gnome_cmd_xfer_tmp_download_multiple (GList *src_uri_list,
									  GList *dest_uri_list,
									  GnomeVFSXferOptions xferOptions,
									  GnomeVFSXferOverwriteMode xferOverwriteMode,
									  GtkSignalFunc on_completed_func,
									  gpointer on_completed_data)
{
	GnomeVFSResult result;
	XferData *data;

	g_return_if_fail (src_uri_list != NULL);
	g_return_if_fail (dest_uri_list != NULL);
	
	data = create_xfer_data (
		xferOptions, src_uri_list, dest_uri_list,
		NULL, NULL, NULL,
		(GFunc)on_completed_func, on_completed_data);
	
	data->win = GNOME_CMD_XFER_PROGRESS_WIN (gnome_cmd_xfer_progress_win_new ());
	gtk_window_set_title (GTK_WINDOW (data->win), _("downloading to /tmp"));
	gtk_widget_show (GTK_WIDGET (data->win));

	/* start the transfer */
	result = gnome_vfs_async_xfer (
		&data->handle,
		data->src_uri_list,
		data->dest_uri_list,
		xferOptions,
		GNOME_VFS_XFER_ERROR_MODE_ABORT,
		xferOverwriteMode,
		XFER_PRIORITY,
		(GnomeVFSAsyncXferProgressCallback)async_xfer_callback,
		data,
		xfer_callback,
		data);

	gtk_timeout_add (
		gnome_cmd_data_get_gui_update_rate (),
		(GtkFunction)update_xfer_gui_func,
		data);
}

