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
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-patternsel-dialog.h"
#include "gnome-cmd-mkdir-dialog.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-list-popmenu.h"
#include "history.h"
#include "cap.h"
#include "utils.h"
#include "gnome-cmd-make-copy-dialog.h"

#define FS_PBAR_MAX 50

static GtkTargetEntry drop_types [] = {
	{ TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
	{ TARGET_URL_TYPE, 0, TARGET_URL }
};

static GtkVBoxClass *parent_class = NULL;

struct _GnomeCmdFileSelectorPrivate {
	GtkWidget *filter_box;
	
	gboolean active;
	gboolean realized;
	gboolean selection_lock;
	gboolean sel_first_file;
	GnomeCmdCon *con;
	GnomeCmdDir *cwd, *lwd; // current & last working dir
	GnomeCmdDir *connected_dir;
	GList *old_btns;
	History *dir_history;
	GnomeCmdFile *sym_file;
	GtkWidget *con_open_dialog;
	GtkWidget *con_open_dialog_label;
	GtkWidget *con_open_dialog_pbar;
	GnomeCmdCon *con_opening;
	
	gboolean autoscroll_dir;
	guint autoscroll_timeout;
	gint autoscroll_y;
};

enum {
  CHANGED_DIR,
  LAST_SIGNAL
};

static guint file_selector_signals[LAST_SIGNAL] = { 0 };

/*******************************
 * Utility functions
 *******************************/

static void
set_same_directory (GnomeCmdFileSelector *fs)
{
	GnomeCmdFileSelector *other;
	GnomeCmdCon *con;
	GnomeCmdDir *dir;

	other = gnome_cmd_main_win_get_inactive_fs (main_win);
	con = gnome_cmd_file_selector_get_connection (other);
	dir = gnome_cmd_file_selector_get_directory (other);
	if (fs->priv->con != con)
		gnome_cmd_file_selector_set_connection (fs, con, dir);
	else
		gnome_cmd_file_selector_set_directory (fs, dir);
}


static void
show_list_popup (GnomeCmdFileSelector *fs)
{
	/* create the popup menu */
	GtkWidget *menu = gnome_cmd_list_popmenu_new (fs);
	gtk_widget_ref (menu);

	gtk_menu_popup (GTK_MENU(menu), NULL, NULL,
					NULL, fs,
					3, GDK_CURRENT_TIME);
}


static void
show_selected_dir_tree_size (GnomeCmdFileSelector *fs)
{
	GnomeCmdFile *finfo;
	
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	finfo = gnome_cmd_file_list_get_selected_file (fs->list);
	gnome_cmd_file_list_show_dir_size (fs->list, finfo);
}


static void
show_dir_tree_sizes (GnomeCmdFileSelector *fs)
{
	GList *files;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	files = gnome_cmd_file_list_get_all_files (fs->list);
	while (files) {
		gnome_cmd_file_list_show_dir_size (fs->list, (GnomeCmdFile*)files->data);
		files = files->next;
	}
}


static void
update_selected_files_label (GnomeCmdFileSelector *fs)
{
	GList *all_files;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	all_files = gnome_cmd_file_list_get_all_files (fs->list);
	if (!all_files)
		return;
	
	if (g_list_length (all_files) >= 0)
	{
		GList *sel_files, *tmp;
		gchar *info_str, *sel_str, *total_str;		
		GnomeVFSFileSize sel_kb, sel_bytes = 0;
		GnomeVFSFileSize total_kb, total_bytes = 0;
		gint num_files = 0;
		gint num_sel_files = 0;

		tmp = all_files;
		while (tmp)
		{
			GnomeCmdFile *finfo = (GnomeCmdFile*)tmp->data;
			if (finfo->info->type == GNOME_VFS_FILE_TYPE_REGULAR) {
				total_bytes += finfo->info->size;
				num_files++;
			}
			tmp = tmp->next;
		}

		sel_files = gnome_cmd_file_list_get_marked_files (fs->list);
		tmp = sel_files;
		while (tmp)
		{
			GnomeCmdFile *finfo = (GnomeCmdFile*)tmp->data;
			if (finfo->info->type == GNOME_VFS_FILE_TYPE_REGULAR) {
				sel_bytes += finfo->info->size;
				num_sel_files++;
			}
			tmp = tmp->next;
		}

		sel_kb = sel_bytes / 1024;
		total_kb = total_bytes / 1024;

		sel_str = g_strdup (size2string (sel_kb, GNOME_CMD_SIZE_DISP_MODE_GROUPED));
		total_str = g_strdup (size2string (total_kb, GNOME_CMD_SIZE_DISP_MODE_GROUPED));

		info_str = g_strdup_printf (
			ngettext("%s of %s kB in %d of %d file selected",
					 "%s of %s kB in %d of %d files selected",
					 num_sel_files),
			sel_str, total_str, num_sel_files, num_files);
		
		gtk_label_set_text (GTK_LABEL (fs->info_label), info_str);
		
		if (sel_files)
			g_list_free (sel_files);

		g_free (sel_str);
		g_free (total_str);
		g_free (info_str);
	}
}


static void
set_connection (GnomeCmdFileSelector *fs, GnomeCmdCon *con, GnomeCmdDir *dir)
{
	fs->priv->con = con;
	fs->priv->dir_history = gnome_cmd_con_get_dir_history (con);

	if (fs->priv->lwd) {
		gnome_cmd_dir_cancel_monitoring (fs->priv->lwd);
		gnome_cmd_dir_unref (fs->priv->lwd);
		fs->priv->lwd = NULL;
	}
	if (fs->priv->cwd) {
		gnome_cmd_dir_cancel_monitoring (fs->priv->cwd);
		gnome_cmd_dir_unref (fs->priv->cwd);
		fs->priv->cwd = NULL;
	}
	
	if (dir) {
		gnome_cmd_file_selector_set_directory (fs, dir);
	}
	else if (gnome_cmd_con_should_remember_dir (con)) {
		dir = gnome_cmd_con_get_cwd (con);
		gnome_cmd_file_selector_set_directory (fs, dir);
	}
	else {
		dir = gnome_cmd_con_get_default_dir (con);
		gnome_cmd_file_selector_set_directory (fs, dir);
	}

	gnome_cmd_combo_select_data (GNOME_CMD_COMBO (fs->con_combo), con);
}


static gboolean
file_is_wanted (GnomeCmdFileSelector *fs, GnomeVFSFileInfo *info)
{
		if (info->type == GNOME_VFS_FILE_TYPE_UNKNOWN
			&& gnome_cmd_data_get_type_filter (GNOME_VFS_FILE_TYPE_UNKNOWN))
			return FALSE;
		if (info->type == GNOME_VFS_FILE_TYPE_REGULAR
			&& gnome_cmd_data_get_type_filter (GNOME_VFS_FILE_TYPE_REGULAR))
			return FALSE;
		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY
			&& gnome_cmd_data_get_type_filter (GNOME_VFS_FILE_TYPE_DIRECTORY))
			return FALSE;
		if (info->type == GNOME_VFS_FILE_TYPE_FIFO
			&& gnome_cmd_data_get_type_filter (GNOME_VFS_FILE_TYPE_FIFO))
			return FALSE;
		if (info->type == GNOME_VFS_FILE_TYPE_SOCKET
			&& gnome_cmd_data_get_type_filter (GNOME_VFS_FILE_TYPE_SOCKET))
			return FALSE;
		if (info->type == GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE
			&& gnome_cmd_data_get_type_filter (GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE))
			return FALSE;
		if (info->type == GNOME_VFS_FILE_TYPE_BLOCK_DEVICE
			&& gnome_cmd_data_get_type_filter (GNOME_VFS_FILE_TYPE_BLOCK_DEVICE))
			return FALSE;
		if ((info->flags == GNOME_VFS_FILE_FLAGS_SYMLINK
			 || info->symlink_name != NULL)
			&& gnome_cmd_data_get_type_filter (GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK))
			return FALSE;
		if (strcmp (info->name, ".") == 0)
			return FALSE;
		if (strcmp (info->name, "..") == 0)
			return FALSE;
		if (info->name[0] == '.' 
			&& gnome_cmd_data_get_hidden_filter ())
			return FALSE;
		if (gnome_cmd_data_get_backup_filter () &&
			patlist_matches (gnome_cmd_data_get_backup_pattern_list (), info->name))
			return FALSE;

		return TRUE;
}


static GnomeCmdFile *
create_parent_dir_file (GnomeCmdDir *dir)
{
	GnomeCmdFile *finfo;
	GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
	memset (info, '\0', sizeof (GnomeVFSFileInfo));
	info->name = g_strdup ("..");
	info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
	info->mime_type = g_strdup ("x-directory/normal");
	info->size = 0;
	info->refcount = 1;
	info->valid_fields = GNOME_VFS_FILE_INFO_FIELDS_TYPE
		| GNOME_VFS_FILE_INFO_FIELDS_SIZE
		| GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
	finfo = gnome_cmd_file_new (info, dir);
	return finfo;
}


static void
update_files (GnomeCmdFileSelector *fs)
{
	GList *list;
	GList *list2 = NULL;
	GnomeCmdDir *dir;
	gchar *path;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	dir = gnome_cmd_file_selector_get_directory (fs);
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

//	if (gnome_cmd_con_open_is_needed (fs->priv->con))
//		gnome_cmd_con_open (fs->priv->con);

	gnome_cmd_dir_get_files (dir, &list);
		
//	if (!list) {
//		create_error_dialog (_("Failed to load directory"));
//		return;
//	}

	/* sort out the files to show */
	while (list != NULL) {
		GnomeCmdFile *finfo = GNOME_CMD_FILE (list->data);

		if (file_is_wanted (fs, finfo->info))
			list2 = g_list_append (list2, finfo);
		
		list = list->next;
	}

	/* Create a parent dir file (..) if appropriate */
	path = gnome_cmd_file_get_path (GNOME_CMD_FILE (dir));
	if (path && strcmp (path, "/") != 0)
		list2 = g_list_append (list2, create_parent_dir_file (dir));
	g_free (path);

	gnome_cmd_file_list_show_files (fs->list, list2, TRUE);
	gnome_cmd_clist_set_voffset (GNOME_CMD_CLIST (fs->list), fs->priv->cwd->voffset);
	
	if (fs->priv->realized)
		update_selected_files_label (fs);
	if (fs->priv->active)
		gnome_cmd_file_list_select_row (fs->list, 0);

	if (list2)
		g_list_free (list2);
}


static void update_direntry (GnomeCmdFileSelector *fs)
{
	gchar *tmp;
	
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	tmp = gnome_cmd_dir_get_display_path (fs->priv->cwd);
	g_return_if_fail (tmp != NULL);
	
	gnome_cmd_dir_indicator_set_dir (
		GNOME_CMD_DIR_INDICATOR (fs->dir_indicator), tmp);

	g_free (tmp);
}


void
gnome_cmd_file_list_show_make_copy_dialog (GnomeCmdFileSelector *fs)
{
	GnomeCmdFile *finfo;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	finfo = gnome_cmd_file_list_get_selected_file (
		GNOME_CMD_FILE_LIST (fs->list));

	if (GNOME_CMD_IS_FILE (finfo)) {
		GtkWidget *dialog = gnome_cmd_make_copy_dialog_new (finfo, fs->priv->cwd);
		
		gtk_widget_ref (dialog);
		gtk_widget_show (dialog);
	}
}


/******************************************************
 * DnD functions
 **/

static void
restore_drag_indicator (GnomeCmdFileSelector *fs)
{
	gnome_cmd_clist_set_drag_row (GNOME_CMD_CLIST (fs->list), -1);
}


static void
unref_uri_list (GList *list)
{
	g_return_if_fail (list != NULL);
	
	g_list_foreach (list, (GFunc)gnome_vfs_uri_unref, NULL);
}


static void
drag_data_received (GtkWidget          *widget,
					GdkDragContext     *context,
					gint                x,
					gint                y,
					GtkSelectionData   *selection_data,
					guint               info,
					guint32             time,
					GnomeCmdFileSelector *fs)
{
	GnomeCmdFile *finfo;
	GtkCList *clist;
	GnomeCmdDir *to, *cwd;
	GList *uri_list = NULL;
	gchar *to_fn = NULL;
	int row;
	GnomeVFSXferOptions xferOptions;

	clist = GTK_CLIST (fs->list);

	
	/* Find out what operation to perform
	 *
	 */
	if (context->action == GDK_ACTION_MOVE)
		xferOptions = GNOME_VFS_XFER_REMOVESOURCE;
	else if (context->action == GDK_ACTION_COPY)
		xferOptions = GNOME_VFS_XFER_RECURSIVE;
	else {
		warn_print ("Unknown context->action in drag_data_received\n");
		return;
	}


	/* Find the row that the file was dropped on
	 *
	 */
	y -= (clist->column_title_area.height - GTK_CONTAINER (clist)->border_width);
	if (y < 0) return;
	row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->list), x, y);

	/* Transform the drag data to a list with uris
	 *
	 */
	uri_list = strings_to_uris (selection_data->data);

	if (g_list_length (uri_list) == 1) {
		GnomeVFSURI *uri = (GnomeVFSURI*)uri_list->data;
		to_fn = gnome_vfs_unescape_string (gnome_vfs_uri_extract_short_name (uri), 0);
	}

	finfo = gnome_cmd_file_list_get_file_at_row (fs->list, row);
	cwd = gnome_cmd_file_selector_get_directory (fs);
	
	if (finfo && finfo->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		/* The drop was over a directory in the list, which means that the 
		 * xfer should be done to that directory instead of the current one in the list 
		 */
		if (strcmp (finfo->info->name, "..") == 0 )
			to = gnome_cmd_dir_get_parent (cwd);
		else
			to = gnome_cmd_dir_get_child (cwd, finfo->info->name);
	}
	else
		to = cwd;

	g_return_if_fail (GNOME_CMD_IS_DIR (to));

	gnome_cmd_dir_ref (to);

	/* Start the xfer
	 * 
	 */
	gnome_cmd_xfer_uris_start (uri_list,
							   to,
							   NULL,
							   NULL,
							   to_fn, 
							   xferOptions,
							   GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
							   GTK_SIGNAL_FUNC (unref_uri_list),
							   uri_list);
}


static void
drag_begin (GtkWidget *widget, GdkDragContext *context, GnomeCmdFileSelector *fs)
{
}


static void
drag_end (GtkWidget *widget, GdkDragContext *context, GnomeCmdFileSelector *fs)
{
	restore_drag_indicator (fs);
}


static void
drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, GnomeCmdFileSelector *fs)
{
	if (fs->priv->autoscroll_timeout > 0) {
		gtk_timeout_remove (fs->priv->autoscroll_timeout);
		fs->priv->autoscroll_timeout = 0;
	}
	
	restore_drag_indicator (fs);
}


static void
drag_data_delete (GtkWidget *widget,
				  GdkDragContext *drag_context,
				  GnomeCmdFileSelector *fs)
{
	GnomeCmdDir *dir;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	dir = gnome_cmd_file_selector_get_directory (fs);
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
}


static gboolean
do_scroll (GnomeCmdFileSelector *fs)
{
	gint w, h;
	gint focus_row, top_row, bottom_row;
	gint row_count;
	guint offset;
	gint row_height;
	GtkCList *clist = GTK_CLIST (fs->list);

	gdk_window_get_size (GTK_WIDGET (clist)->window, &w, &h);
	
	offset = (0-clist->voffset);
	row_height = gnome_cmd_data_get_list_row_height();
	row_count = clist->rows;
	focus_row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->list), 1, fs->priv->autoscroll_y);
	top_row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->list), 1, 0);
	bottom_row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->list), 1, h);

	if (!fs->priv->autoscroll_dir) {
		if (focus_row > 0) {
			gtk_clist_moveto (clist, top_row-1, 0, 0, 0);
		}
		else
			return FALSE;
	}
	else {	
		if (focus_row < row_count) {
			gtk_clist_moveto (clist, top_row+1, 0, 0, 0);
		}
		else
			return FALSE;
	}

	return TRUE;
}


static void
autoscroll_if_appropriate (GnomeCmdFileSelector *fs, gint x, gint y)
{
	gint w, h;
	gint smin, smax;
	guint offset;
	GtkCList *clist = GTK_CLIST (fs->list);

	if (y < 0) return;
	
	gdk_window_get_size (GTK_WIDGET (clist)->window, &w, &h);
	
	offset = (0-clist->voffset);
	smin = h/8;
	smax = h-smin;
	
	if (y < smin) {
		if (fs->priv->autoscroll_timeout) return;		
		fs->priv->autoscroll_dir = FALSE;
		fs->priv->autoscroll_y = y;
		fs->priv->autoscroll_timeout =
			gtk_timeout_add (gnome_cmd_data_get_gui_update_rate (), (GtkFunction)do_scroll, fs);
	}
	else if (y > smax) {
		if (fs->priv->autoscroll_timeout) return;		
		fs->priv->autoscroll_dir = TRUE;
		fs->priv->autoscroll_y = y;
		fs->priv->autoscroll_timeout =
			gtk_timeout_add (gnome_cmd_data_get_gui_update_rate (), (GtkFunction)do_scroll, fs);
	}
	else {
		if (fs->priv->autoscroll_timeout > 0) {
			gtk_timeout_remove (fs->priv->autoscroll_timeout);
			fs->priv->autoscroll_timeout = 0;
		}
	}
}


static gboolean
drag_motion (GtkWidget *widget,
			 GdkDragContext *context,
			 gint x, gint y,
			 guint time,
			 GnomeCmdFileSelector *fs)
{
	gint row;
	GtkCList *clist;
	
	gdk_drag_status (context, context->suggested_action, time);

	clist = GTK_CLIST (fs->list);
	
	y -= (clist->column_title_area.height - GTK_CONTAINER (clist)->border_width);

	row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->list), x, y);

	if (row > -1) {
		GnomeCmdFile *finfo = gnome_cmd_file_list_get_file_at_row (fs->list, row);
		if (finfo->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
			row = -1;

		gnome_cmd_clist_set_drag_row (GNOME_CMD_CLIST (clist), row);
	}

	autoscroll_if_appropriate (fs, x, y);
	
	return FALSE;
}


static void
init_dnd (GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));


	/* Set up drag source */

	gtk_signal_connect (GTK_OBJECT (fs->list), "drag_begin",
						GTK_SIGNAL_FUNC (drag_begin), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "drag_end",
						GTK_SIGNAL_FUNC (drag_end), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "drag_leave",
						GTK_SIGNAL_FUNC (drag_leave), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "drag_data_delete",
						GTK_SIGNAL_FUNC (drag_data_delete), fs);

	/* Set up drag destination */

	gtk_drag_dest_set (GTK_WIDGET (fs->list),
					   GTK_DEST_DEFAULT_DROP,
					   drop_types, ELEMENTS (drop_types),
					   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

	gtk_signal_connect (GTK_OBJECT (fs->list), "drag_motion",
						GTK_SIGNAL_FUNC (drag_motion), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "drag_leave",
						GTK_SIGNAL_FUNC (drag_leave), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "drag_data_received",
						GTK_SIGNAL_FUNC (drag_data_received), fs);
}


static void
update_dir_combo (GnomeCmdFileSelector *fs)
{/*
	GList *tmp;

	if (!fs->priv->dir_history) return;
	
	tmp = fs->priv->dir_history->ents;

	gnome_cmd_combo_clear (GNOME_CMD_COMBO (fs->dir_combo));
	
	while (tmp) {
		gchar *text[2];

		text[0] = (gchar*)tmp->data;
		text[1] = NULL;
		
		gnome_cmd_combo_append (GNOME_CMD_COMBO (fs->dir_combo), text, tmp->data);
		tmp = tmp->next;
	}

	if (fs->priv->dir_history->ents && fs->priv->dir_history->pos)
		gtk_clist_select_row (
			GTK_CLIST (GNOME_CMD_COMBO (fs->dir_combo)->list),
			g_list_index (fs->priv->dir_history->ents, fs->priv->dir_history->pos->data),
			0);*/
}


static void
update_vol_label (GnomeCmdFileSelector *fs)

{
	gchar *s;
	GnomeVFSFileSize free_space;
	GnomeVFSResult res;
	
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (GNOME_CMD_IS_CON (fs->priv->con));

	if (gnome_cmd_con_can_show_free_space (fs->priv->con)) {
		GnomeVFSURI *uri = gnome_cmd_file_get_uri (GNOME_CMD_FILE (fs->priv->cwd));
		res = gnome_vfs_get_volume_free_space (uri, &free_space);
		gnome_vfs_uri_unref (uri);
	
		if (res == GNOME_VFS_OK) {
			gchar *sfree = gnome_vfs_format_file_size_for_display (free_space);
			s = g_strdup_printf (_("%s Free"), sfree);
			g_free (sfree);
		}
		else
			s = g_strdup ("Unknown disk usage");
	}
	else
		s = g_strdup ("");

	gtk_label_set_text (GTK_LABEL (fs->vol_label), s);

	g_free (s);
}


static void
goto_directory (GnomeCmdFileSelector *fs,
				const gchar *in_dir)
{
	GnomeCmdDir *cur_dir, *new_dir = NULL;
	const gchar *focus_dir = NULL;
	gchar *dir;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (in_dir != NULL);

	cur_dir = gnome_cmd_file_selector_get_directory (fs);
	g_return_if_fail (GNOME_CMD_IS_DIR (cur_dir));

	if (strcmp (in_dir, "~") == 0)
		dir = g_strdup (g_get_home_dir ());
	else
		dir = unquote_if_needed (in_dir);
	
	if (strcmp (dir, "..") == 0) {
		/* lets get the parent directory */
		new_dir = gnome_cmd_dir_get_parent (cur_dir);
		if (!new_dir) {
			g_free (dir);
			return;
		}
		focus_dir = gnome_cmd_file_get_name (GNOME_CMD_FILE (cur_dir));
	}
	else {
		/* check if it's an absolute address or not */
		if (dir[0] == '/') {
			new_dir = gnome_cmd_dir_new (
				fs->priv->con, gnome_cmd_con_create_path (fs->priv->con, dir));
		}
		else if (strncmp (dir, "\\\\", 2) == 0) {
			GnomeCmdPath *path = gnome_cmd_con_create_path (get_smb_con (), dir);
			if (path)
				new_dir = gnome_cmd_dir_new (get_smb_con (), path);
		}
		else
			new_dir = gnome_cmd_dir_get_child (cur_dir, dir);			
	}

	if (new_dir)
		gnome_cmd_file_selector_set_directory (fs, new_dir);
	
	/* focus the current dir when going back to the parent dir */
	if (focus_dir)
		gnome_cmd_file_list_focus_file (fs->list, focus_dir, FALSE);

	g_free (dir);
}


static void
do_file_specific_action                  (GnomeCmdFileSelector *fs,
										  GnomeCmdFile *finfo)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	if (finfo->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		if (strcmp (finfo->info->name, "..") == 0)
			goto_directory (fs, "..");
		else
			gnome_cmd_file_selector_set_directory (fs, GNOME_CMD_DIR (finfo));
	}
}


static gboolean
file_is_in_list (GnomeCmdFileSelector *fs, GnomeCmdFile *finfo)
{
	return g_list_index (gnome_cmd_file_list_get_all_files (fs->list), finfo) != -1;
}


static void
add_file_to_cmdline (GnomeCmdFileSelector *fs, gboolean fullpath)
{	
	GnomeCmdFile *finfo;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	
	finfo = gnome_cmd_file_list_get_selected_file (fs->list);
	
	if (finfo && gnome_cmd_data_get_cmdline_visibility ()) {
		gchar *text;

		if (fullpath)
			text = gnome_cmd_file_get_quoted_real_path (finfo);
		else
			text = gnome_cmd_file_get_quoted_name (finfo);
		
		gnome_cmd_cmdline_append_text (
			gnome_cmd_main_win_get_cmdline (main_win), text);
		gnome_cmd_cmdline_focus (
			gnome_cmd_main_win_get_cmdline (main_win));
		g_free (text);
	}
}


static void
add_cwd_to_cmdline (GnomeCmdFileSelector *fs)
{	
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	
	if (gnome_cmd_data_get_cmdline_visibility ()) {
		gchar *dpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (fs->priv->cwd));
		gnome_cmd_cmdline_append_text (
			gnome_cmd_main_win_get_cmdline (main_win), dpath);
		g_free (dpath);
			
		gnome_cmd_cmdline_focus (
			gnome_cmd_main_win_get_cmdline (main_win));
	}
}



/*******************************
 * Callbacks
 *******************************/

static void
on_con_list_list_changed (GnomeCmdConList *con_list, GnomeCmdFileSelector *fs)
{
	gnome_cmd_file_selector_update_connections (fs);
}


static void
on_dir_file_created (GnomeCmdDir *dir, GnomeCmdFile *finfo, GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	g_return_if_fail (GNOME_CMD_IS_FILE (finfo));
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	if (!file_is_wanted (fs, finfo->info))
		return;

	gnome_cmd_file_list_insert_file (fs->list, finfo);
	update_selected_files_label (fs);
}


static void
on_dir_file_deleted (GnomeCmdDir *dir, GnomeCmdFile *finfo, GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	g_return_if_fail (GNOME_CMD_IS_FILE (finfo));
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	
	if (fs->priv->cwd == dir && file_is_in_list (fs, finfo)) {
		gnome_cmd_file_list_remove_file (fs->list, finfo);
		update_selected_files_label (fs);
	}
}


static void
on_dir_file_changed (GnomeCmdDir *dir, GnomeCmdFile *finfo, GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	g_return_if_fail (GNOME_CMD_IS_FILE (finfo));
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	if (file_is_in_list (fs, finfo)) {
		gnome_cmd_file_list_update_file (GNOME_CMD_FILE_LIST (fs->list), finfo);
		update_selected_files_label (fs);
	}
}


static void
on_con_combo_item_selected              (GnomeCmdCombo *con_combo,
										 GnomeCmdCon *con,
										 GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (GNOME_CMD_IS_CON (con));

	gnome_cmd_main_win_switch_fs (main_win, fs);
	gnome_cmd_file_selector_set_connection (fs, con, NULL);
}


static void
on_combo_popwin_hidden              (GnomeCmdCombo *combo,
									 GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_COMBO (combo));
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	gnome_cmd_main_win_refocus (main_win);
}


static void
on_con_btn_clicked                      (GtkButton *button,
										 GnomeCmdFileSelector *fs)
{
	GnomeCmdCon *con = gtk_object_get_data (GTK_OBJECT (button), "con");

	g_return_if_fail (GNOME_CMD_IS_CON (con));
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	gnome_cmd_main_win_switch_fs (main_win, fs);
	gnome_cmd_file_selector_set_connection (fs, con, NULL);
}


static void
create_con_buttons (GnomeCmdFileSelector *fs)
{
	GList *l;
	static GtkTooltips *tooltips = NULL;

	if (!gnome_cmd_data_get_conbuttons_visibility ())
		return;
	
	l = fs->priv->old_btns;
	while (l) {
		gtk_object_destroy (GTK_OBJECT (l->data));
		l = l->next;
	}
	g_list_free (fs->priv->old_btns);
	fs->priv->old_btns = NULL;

	if (!tooltips) {
		tooltips = gtk_tooltips_new ();
	}

	l = gnome_cmd_con_list_get_all (gnome_cmd_data_get_con_list ());
	while (l) {
		GtkWidget *btn, *label;
		GtkWidget *hbox;
		GnomeCmdPixmap *pm;
		GnomeCmdCon *con = GNOME_CMD_CON (l->data);

		if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
			&& !GNOME_CMD_IS_CON_SMB (con)) {
			l = l->next;
			continue;
		}
		pm = gnome_cmd_con_get_go_pixmap (con);
		
		btn = create_styled_button (NULL);
		gtk_object_set_data (GTK_OBJECT (btn), "con", con);
		gtk_signal_connect (GTK_OBJECT (btn), "clicked",
							(GtkSignalFunc)on_con_btn_clicked, fs);
		gtk_box_pack_start (GTK_BOX (fs->con_btns_hbox), btn, FALSE, FALSE, 0);
		GTK_WIDGET_UNSET_FLAGS (btn, GTK_CAN_FOCUS);
		fs->priv->old_btns = g_list_append (fs->priv->old_btns, btn);
		gtk_tooltips_set_tip (tooltips, btn,
							  gnome_cmd_con_get_go_text (con), NULL);

		hbox = gtk_hbox_new (FALSE, 1);
		gtk_widget_ref (hbox);
		gtk_object_set_data_full (GTK_OBJECT (fs), "con-hbox", hbox,
								  (GtkDestroyNotify) gtk_widget_unref);
		gtk_widget_show (hbox);		

		if (pm) {
			GtkWidget *pixmap = gtk_pixmap_new (pm->pixmap, pm->mask);
			if (pixmap) {
				gtk_widget_ref (pixmap);
				gtk_object_set_data_full (GTK_OBJECT (fs), "con-pixmap", pixmap,
										  (GtkDestroyNotify) gtk_widget_unref);
				gtk_widget_show (pixmap);
				gtk_box_pack_start (GTK_BOX (hbox), pixmap, TRUE, TRUE, 0);
			}
		}

		if (!gnome_cmd_data_get_device_only_icon () || !pm) {
			label = gtk_label_new (gnome_cmd_con_get_alias (con));
			gtk_widget_ref (label);
			gtk_object_set_data_full (GTK_OBJECT (fs), "con-label", label,
									  (GtkDestroyNotify) gtk_widget_unref);
			gtk_widget_show (label);
			gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
		}

		gtk_container_add (GTK_CONTAINER (btn), hbox);

		l = l->next;
	}
}


static void
on_realize                               (GnomeCmdFileSelector *fs,
										  gpointer user_data)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	
	fs->priv->realized = TRUE;
	
	create_con_buttons (fs);
	gnome_cmd_file_selector_update_connections (fs);
}


static void
on_list_file_clicked (GnomeCmdFileList *fl, GnomeCmdFile *finfo,
					  GdkEventButton *event, GnomeCmdFileSelector *fs)
{
	if (event->type == GDK_2BUTTON_PRESS) {
		if (event->button == 1)
			do_file_specific_action (fs, finfo);
	}
}


static void
on_list_list_clicked (GnomeCmdFileList *fl, GdkEventButton *event,
					  GnomeCmdFileSelector *fs)
{
	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 1 || event->button == 3)
			gnome_cmd_main_win_switch_fs (main_win, fs);
		else if (event->button == 2)
			goto_directory (fs, "..");
	}
}


static void
on_list_empty_space_clicked (GnomeCmdFileList *fl, GdkEventButton *event,
							 GnomeCmdFileSelector *fs)
{
	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 3)
			show_list_popup (fs);
	}
}


static void
on_list_selection_changed (GnomeCmdFileList *fl, GnomeCmdFileSelector *fs)
{
	update_selected_files_label (fs);
}


static void
on_dir_list_ok (GnomeCmdDir *dir, GList *files, GnomeCmdFileSelector *fs)
{
	GnomeCmdCon *con;
	
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	DEBUG('l', "on_dir_list_ok\n");
	
	if (fs->priv->realized) {	
		gtk_widget_set_sensitive (GTK_WIDGET (fs), TRUE);
		set_cursor_default_for_widget (GTK_WIDGET (fs));
		gtk_widget_grab_focus (GTK_WIDGET (fs->list));
	}

	if (fs->priv->connected_dir != dir) {
		if (fs->priv->connected_dir != NULL) {
			gtk_signal_disconnect_by_func (
				GTK_OBJECT (fs->priv->connected_dir),
				GTK_SIGNAL_FUNC (on_dir_file_created), fs);
			gtk_signal_disconnect_by_func (
				GTK_OBJECT (fs->priv->connected_dir),
				GTK_SIGNAL_FUNC (on_dir_file_deleted), fs);
			gtk_signal_disconnect_by_func (
				GTK_OBJECT (fs->priv->connected_dir),
				GTK_SIGNAL_FUNC (on_dir_file_changed), fs);
		}
		
		gtk_signal_connect (GTK_OBJECT (dir), "file-created",
							GTK_SIGNAL_FUNC (on_dir_file_created), fs);
		gtk_signal_connect (GTK_OBJECT (dir), "file-deleted",
							GTK_SIGNAL_FUNC (on_dir_file_deleted), fs);
		gtk_signal_connect (GTK_OBJECT (dir), "file-changed",
							GTK_SIGNAL_FUNC (on_dir_file_changed), fs);
		fs->priv->connected_dir = dir;
	}

	con = gnome_cmd_file_selector_get_connection (fs);
	gnome_cmd_con_set_cwd (con, dir);

	if (fs->priv->dir_history && !fs->priv->dir_history->lock) {
		gchar *fpath = gnome_cmd_file_get_path (GNOME_CMD_FILE (dir));
		history_add (fs->priv->dir_history, fpath);
		g_free (fpath);
		update_dir_combo (fs);		
	}
	
	gtk_signal_emit (GTK_OBJECT (fs),
					 file_selector_signals[CHANGED_DIR], dir);

	update_direntry (fs);
	update_vol_label (fs);

	if (fs->priv->cwd != dir) return;
	
	fs->priv->sel_first_file = FALSE;
	update_files (fs);
	fs->priv->sel_first_file = TRUE;

	if (!fs->priv->active) {
		GTK_CLIST (fs->list)->focus_row = -1;
		gtk_clist_unselect_all (GTK_CLIST (fs->list));
	}

	if (fs->priv->sel_first_file && fs->priv->active)
		gtk_clist_select_row (GTK_CLIST (fs->list), 0, 0);
	
	update_selected_files_label (fs);

	DEBUG('l', "returning from on_dir_list_ok\n");
}


static gboolean
set_home_connection (GnomeCmdFileSelector *fs)
{
	g_printerr ("Setting home connection\n");
	gnome_cmd_file_selector_set_connection (
		fs,
		gnome_cmd_con_list_get_home (gnome_cmd_data_get_con_list ()),
		NULL);
		
	return FALSE;
}


static void
on_dir_list_failed (GnomeCmdDir *dir, GnomeVFSResult result, GnomeCmdFileSelector *fs)
{
	DEBUG('l', "on_dir_list_failed\n");

	if (result != GNOME_VFS_OK) {
		gchar *msg = g_strdup_printf (
			"List failed: %s\n", gnome_vfs_result_to_string (result));
		create_error_dialog (msg);
		g_free (msg);
	}

	gtk_signal_disconnect_by_data (GTK_OBJECT (fs->priv->cwd), fs);
	fs->priv->connected_dir = NULL;
	gnome_cmd_dir_unref (fs->priv->cwd);
	set_cursor_default_for_widget (GTK_WIDGET (fs));
	gtk_widget_set_sensitive (GTK_WIDGET (fs), TRUE);

	if (fs->priv->lwd
		&& fs->priv->con == gnome_cmd_dir_get_connection (fs->priv->lwd)) {
		fs->priv->cwd = fs->priv->lwd;
		gtk_signal_connect (GTK_OBJECT (fs->priv->cwd), "list-ok",
							GTK_SIGNAL_FUNC (on_dir_list_ok), fs);
		gtk_signal_connect (GTK_OBJECT (fs->priv->cwd), "list-failed",
							GTK_SIGNAL_FUNC (on_dir_list_failed), fs);
		fs->priv->lwd = NULL;
	}
	else {
		g_timeout_add (1, (GtkFunction)set_home_connection, fs);
	}
}


/* This function should only be called for input made when the file-selector
 * was focused
 */
static gboolean
on_list_key_pressed                      (GtkCList *clist,
										  GdkEventKey *event,
										  GnomeCmdFileSelector *fs)
{
	gboolean ret = FALSE;

	if (gnome_cmd_file_list_keypressed (fs->list, event))
		ret = TRUE;
	else if (gnome_cmd_file_selector_keypressed (fs, event))
		ret = TRUE;
	else if (gnome_cmd_main_win_keypressed (main_win, event))
		ret = TRUE;

	if (ret) {
		stop_kp (GTK_OBJECT (clist));
		return TRUE;
	}

	return FALSE;
}


static gboolean
on_list_key_pressed_private              (GtkCList *clist,
										  GdkEventKey *event,
										  GnomeCmdFileSelector *fs)
{
	if (state_is_blank (event->state) || state_is_shift (event->state)) {
		if ((event->keyval >= GDK_A && event->keyval <= GDK_Z)
			|| (event->keyval >= GDK_a && event->keyval <= GDK_z)
			|| event->keyval == GDK_period) {
			static gchar text[2];

			if (!gnome_cmd_data_get_cmdline_visibility ()) {
				gnome_cmd_file_list_show_quicksearch (fs->list, (gchar)event->keyval);
			}
			else {
				text[0] = event->keyval;
				text[1] = '\0';
				gnome_cmd_cmdline_append_text (
					gnome_cmd_main_win_get_cmdline (main_win), text);
				gnome_cmd_cmdline_focus (
					gnome_cmd_main_win_get_cmdline (main_win));
			}
			return TRUE;
		}
	}

	return FALSE;
}


static void
on_root_btn_clicked                      (GtkButton *button,
										  GnomeCmdFileSelector *fs)
{
	gnome_cmd_main_win_switch_fs (main_win, fs);
	goto_directory (fs, "/");
}


static void
on_parent_btn_clicked                    (GtkButton *button,
										  GnomeCmdFileSelector *fs)
{
	gnome_cmd_main_win_switch_fs (main_win, fs);
	goto_directory (fs, "..");
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	GnomeCmdFileSelector *fs;

	fs = GNOME_CMD_FILE_SELECTOR (object);

	gnome_cmd_dir_unref (fs->priv->cwd);	
	g_free (fs->priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
map (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
		GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdFileSelectorClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	parent_class = gtk_type_class (gtk_vbox_get_type ());

	file_selector_signals[CHANGED_DIR] =
		gtk_signal_new ("changed_dir",
			GTK_RUN_LAST,
		    G_OBJECT_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GnomeCmdFileSelectorClass, changed_dir),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE,
			1, GTK_TYPE_POINTER);
  
	object_class->destroy = destroy;
	widget_class->map = map;
	class->changed_dir = NULL;
}

static void
init (GnomeCmdFileSelector *fs)
{
	GtkVBox *vbox;
	GtkWidget *padding;

	fs->priv = g_new0 (GnomeCmdFileSelectorPrivate, 1);
	fs->priv->realized = FALSE;
	fs->priv->cwd = NULL;
	fs->priv->lwd = NULL;
	fs->priv->connected_dir = NULL;
	fs->priv->old_btns = NULL;
	fs->priv->selection_lock = FALSE;
	fs->priv->sel_first_file = TRUE;
	fs->priv->dir_history = NULL;
	fs->priv->active = FALSE;
	fs->priv->sym_file = NULL;
	fs->priv->con = NULL;
	fs->priv->con_open_dialog = NULL;
	fs->priv->con_open_dialog_label = NULL;
	fs->priv->con_opening = NULL;
	
	vbox = GTK_VBOX (fs);

	/* create the box used for packing the dir_combo and buttons */
	gnome_cmd_file_selector_update_conbuttons_visibility (fs);

	/* create the box used for packing the con_combo and information */
	fs->con_hbox = create_hbox (GTK_WIDGET (fs), FALSE, 2);

	/* create the list */
	fs->list_widget = gnome_cmd_file_list_new ();
	gtk_widget_ref (fs->list_widget);
	gtk_object_set_data_full (GTK_OBJECT (fs), "list_widget", fs->list_widget,
							  (GtkDestroyNotify) gtk_widget_unref);
	fs->list = GNOME_CMD_FILE_LIST (fs->list_widget);
	gnome_cmd_file_list_show_column (fs->list, FILE_LIST_COLUMN_DIR, FALSE);

	/* create the connection combo */
	fs->con_combo = gnome_cmd_combo_new (2, 1, NULL);
	gtk_widget_ref (fs->con_combo);
	gtk_object_set_data_full (GTK_OBJECT (fs),
							  "con_combo", fs->con_combo,
							  (GtkDestroyNotify) gtk_widget_unref);
	gtk_clist_set_row_height (GTK_CLIST (GNOME_CMD_COMBO (fs->con_combo)->list), 20);
	gtk_entry_set_editable (GTK_ENTRY (GNOME_CMD_COMBO (fs->con_combo)->entry), FALSE);
	gtk_clist_set_column_width (
		GTK_CLIST (GNOME_CMD_COMBO (fs->con_combo)->list), 0, 20);	
	gtk_clist_set_column_width (
		GTK_CLIST (GNOME_CMD_COMBO (fs->con_combo)->list), 1, 60);
	GTK_WIDGET_UNSET_FLAGS (GNOME_CMD_COMBO (fs->con_combo)->button, GTK_CAN_FOCUS);

	/* Create the free space on volume label */
	fs->vol_label = gtk_label_new ("");
	gtk_widget_ref (fs->vol_label);
	gtk_object_set_data_full (GTK_OBJECT (fs), "vol_label", fs->vol_label,
							  (GtkDestroyNotify) gtk_widget_unref);
	gtk_misc_set_alignment (GTK_MISC (fs->vol_label), 1, 0.5);

	/* create the root button */
	fs->root_btn = create_styled_pixmap_button (
		NULL, IMAGE_get_gnome_cmd_pixmap (PIXMAP_ROOT_DIR));
	gtk_object_set_data_full (GTK_OBJECT (fs),
							  "root_btn", fs->root_btn,
							  (GtkDestroyNotify) gtk_widget_unref);
	GTK_WIDGET_UNSET_FLAGS (fs->root_btn, GTK_CAN_FOCUS);

	/* create the parent dir button */
	fs->parent_btn = create_styled_pixmap_button (
		NULL, IMAGE_get_gnome_cmd_pixmap (PIXMAP_PARENT_DIR));
	gtk_object_set_data_full (GTK_OBJECT (fs),
							  "parent_btn", fs->parent_btn,
							  (GtkDestroyNotify) gtk_widget_unref);
	GTK_WIDGET_UNSET_FLAGS (fs->parent_btn, GTK_CAN_FOCUS);

	/* create the directory indicator */
	fs->dir_indicator = gnome_cmd_dir_indicator_new (fs);
	gtk_widget_ref (fs->dir_indicator);
	gtk_object_set_data_full (GTK_OBJECT (fs),
							  "dir_indicator", fs->dir_indicator,
							  (GtkDestroyNotify) gtk_widget_unref);
	
	/* create the scrollwindow that we'll place the list in */
	fs->scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_ref (fs->scrolledwindow);
	gtk_object_set_data_full (GTK_OBJECT (fs),
							  "scrolledwindow", fs->scrolledwindow,
							  (GtkDestroyNotify) gtk_widget_unref);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (fs->scrolledwindow),
									GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
	/* create the info label */
	fs->info_label = gtk_label_new ("not initialized");
	gtk_widget_ref (fs->info_label);
	gtk_object_set_data_full (GTK_OBJECT (fs), "infolabel", fs->info_label,
							  (GtkDestroyNotify) gtk_widget_unref);
	gtk_misc_set_alignment (GTK_MISC (fs->info_label), 0.0f, 0.5f);

	/* pack the widgets */
	gtk_box_pack_start (GTK_BOX (fs), fs->con_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), fs->dir_indicator, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (fs->scrolledwindow), fs->list_widget);
	gtk_box_pack_start (GTK_BOX (vbox), fs->scrolledwindow, TRUE, TRUE, 0);
	padding = create_hbox (GTK_WIDGET (fs), FALSE, 6);	
	gtk_box_pack_start (GTK_BOX (vbox), padding, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (padding), fs->info_label, FALSE, TRUE, 6);
	gtk_box_pack_start (GTK_BOX (fs->con_hbox), fs->con_combo, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (fs->con_hbox), fs->vol_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (fs->con_hbox), fs->root_btn, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (fs->con_hbox), fs->parent_btn, FALSE, FALSE, 0);


	/* initialize dnd */
	init_dnd (fs);
	
	/* connect signals */
	gtk_signal_connect (GTK_OBJECT (fs), "realize",
						GTK_SIGNAL_FUNC (on_realize), fs);
	
	gtk_signal_connect (GTK_OBJECT (fs->con_combo), "item-selected",
						GTK_SIGNAL_FUNC (on_con_combo_item_selected), fs);
	gtk_signal_connect (GTK_OBJECT (fs->con_combo), "popwin-hidden",
						GTK_SIGNAL_FUNC (on_combo_popwin_hidden), fs);

	gtk_signal_connect (GTK_OBJECT (fs->list), "file-clicked",
						GTK_SIGNAL_FUNC (on_list_file_clicked), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "list-clicked",
						GTK_SIGNAL_FUNC (on_list_list_clicked), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "empty-space-clicked",
						GTK_SIGNAL_FUNC (on_list_empty_space_clicked), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "selection_changed",
						GTK_SIGNAL_FUNC (on_list_selection_changed), fs);
	
	gtk_signal_connect (GTK_OBJECT (fs->list), "key-press-event",
						GTK_SIGNAL_FUNC (on_list_key_pressed), fs);
	gtk_signal_connect (GTK_OBJECT (fs->list), "key-press-event",
						GTK_SIGNAL_FUNC (on_list_key_pressed_private), fs);
	
	gtk_signal_connect (GTK_OBJECT (fs->root_btn), "clicked",
						GTK_SIGNAL_FUNC (on_root_btn_clicked), fs);
	gtk_signal_connect (GTK_OBJECT (fs->parent_btn), "clicked",
						GTK_SIGNAL_FUNC (on_parent_btn_clicked), fs);

	gtk_signal_connect (GTK_OBJECT (gnome_cmd_data_get_con_list ()), "list-changed",
						GTK_SIGNAL_FUNC (on_con_list_list_changed), fs);

	 
	/* show the widgets */
	gtk_widget_show (GTK_WIDGET (vbox));
	gtk_widget_show (fs->con_hbox);
	gtk_widget_show (fs->dir_indicator);
	gtk_widget_show (fs->root_btn);
	gtk_widget_show (fs->parent_btn);
	gtk_widget_show (fs->scrolledwindow);
	gtk_widget_show (fs->vol_label);
	gtk_widget_show (fs->con_combo);
	gtk_widget_show (fs->list_widget);
	gtk_widget_show (fs->info_label);

	gnome_cmd_file_selector_update_style (fs);
}






/***********************************
 * Public functions
 ***********************************/


GtkType
gnome_cmd_file_selector_get_type         (void)
{
	static GtkType fs_type = 0;

	if (fs_type == 0)
	{
		GtkTypeInfo fs_info =
		{
			"GnomeCmdFileSelector",
			sizeof (GnomeCmdFileSelector),
			sizeof (GnomeCmdFileSelectorClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		fs_type = gtk_type_unique (gtk_vbox_get_type (), &fs_info);
	}
	return fs_type;
}


GtkWidget *
gnome_cmd_file_selector_new              (void)
{
	GnomeCmdFileSelector *fs;

	fs = gtk_type_new (gnome_cmd_file_selector_get_type ());

	return GTK_WIDGET (fs);
}


GnomeCmdDir*
gnome_cmd_file_selector_get_directory    (GnomeCmdFileSelector *fs)
{
	g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), NULL);
	
	return fs->priv->cwd;
}


void
gnome_cmd_file_selector_reload           (GnomeCmdFileSelector *fs)
{
	GnomeCmdDir *dir;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	gnome_cmd_file_list_unselect_all (fs->list);
	
	dir = gnome_cmd_file_selector_get_directory (fs);
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	gnome_cmd_dir_relist_files (
		dir, gnome_cmd_con_needs_list_visprog (fs->priv->con));
}


void
gnome_cmd_file_selector_start_editor      (GnomeCmdFileSelector *fs)
{
	gint i;
	gint l;
	gchar *cmd, *dpath;
	
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (GNOME_CMD_IS_DIR (fs->priv->cwd));
	g_return_if_fail (GNOME_CMD_IS_CON (fs->priv->con));

	if (!gnome_cmd_con_is_local (fs->priv->con))
		return;

	/* create a command with an empty argument to the editor */
	cmd = g_strdup (gnome_cmd_data_get_editor ());
	l = strlen(cmd);
	for ( i=0 ; i<l ; i++ ) {
		if (cmd[i] == ' ') {
			cmd[i] = '\0';
			break;
		}
	}

	dpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (fs->priv->cwd));
	run_command_indir (cmd, dpath, FALSE);
	g_free (dpath);
}


void
gnome_cmd_file_selector_back             (GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	if (!fs->priv->dir_history) return;
	
	if (history_can_back (fs->priv->dir_history)) {
		fs->priv->dir_history->lock = TRUE;
		goto_directory (fs, history_back (fs->priv->dir_history));
		fs->priv->dir_history->lock = FALSE;
	}
}


void
gnome_cmd_file_selector_forward           (GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	if (!fs->priv->dir_history) return;

	if (history_can_forward (fs->priv->dir_history)) {
		fs->priv->dir_history->lock = TRUE;
		goto_directory (fs, history_forward (fs->priv->dir_history));
		fs->priv->dir_history->lock = FALSE;
	}
}


gboolean
gnome_cmd_file_selector_can_back (GnomeCmdFileSelector *fs)
{
	g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), FALSE);
	if (!fs->priv->dir_history) return FALSE;
	
	return history_can_back (fs->priv->dir_history);
}


gboolean
gnome_cmd_file_selector_can_forward (GnomeCmdFileSelector *fs)
{
	g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), FALSE);
	if (!fs->priv->dir_history) return FALSE;

	return history_can_forward (fs->priv->dir_history);
}


void
gnome_cmd_file_selector_set_directory (GnomeCmdFileSelector *fs, GnomeCmdDir *dir)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));

	if (fs->priv->cwd == dir)
		return;

	gnome_cmd_dir_ref (dir);
	
	if (fs->priv->lwd && fs->priv->lwd != dir) {
		gnome_cmd_dir_cancel_monitoring (fs->priv->lwd);
		gnome_cmd_dir_unref (fs->priv->lwd);
	}

	if (fs->priv->cwd) {
		fs->priv->lwd = fs->priv->cwd;
		gtk_signal_disconnect_by_data (GTK_OBJECT (fs->priv->lwd), fs);
		fs->priv->connected_dir = NULL;
		fs->priv->lwd->voffset = gnome_cmd_clist_get_voffset (GNOME_CMD_CLIST (fs->list));
	}

	fs->priv->cwd = dir;
	
	if (fs->priv->realized) {	
		gtk_widget_set_sensitive (GTK_WIDGET (fs), FALSE);
		set_cursor_busy_for_widget (GTK_WIDGET (fs));
	}

	gtk_signal_connect (GTK_OBJECT (dir), "list-ok",
						GTK_SIGNAL_FUNC (on_dir_list_ok), fs);
	gtk_signal_connect (GTK_OBJECT (dir), "list-failed",
						GTK_SIGNAL_FUNC (on_dir_list_failed), fs);

	gnome_cmd_dir_list_files (
		dir, gnome_cmd_con_needs_list_visprog (fs->priv->con));
	gnome_cmd_dir_start_monitoring (dir);
}


void
gnome_cmd_file_selector_goto_directory   (GnomeCmdFileSelector *fs,
										  const gchar *dir)
{
	goto_directory (fs, dir);
}


void
gnome_cmd_file_selector_set_active       (GnomeCmdFileSelector *fs,
										  gboolean value)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	
	fs->priv->active = value;	
	
	if (value) {
		gtk_widget_grab_focus (fs->list_widget);
		gnome_cmd_file_list_select_row (
			fs->list, GTK_CLIST (fs->list)->focus_row);
	}
 	else
		gtk_clist_unselect_all (GTK_CLIST (fs->list));

	gnome_cmd_dir_indicator_set_active (
		GNOME_CMD_DIR_INDICATOR (fs->dir_indicator), value);
}


static void
on_con_open_cancel (GtkButton *button, GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (fs->priv->con_opening != NULL);
	g_return_if_fail (fs->priv->con_opening->state == CON_STATE_OPENING);
	
	DEBUG('m', "on_con_open_cancel\n");
	gnome_cmd_con_cancel_open (fs->priv->con_opening);
	
	gtk_widget_destroy (fs->priv->con_open_dialog);
	fs->priv->con_open_dialog = NULL;
	fs->priv->con_opening = NULL;
}


static gboolean
update_con_open_progress (GnomeCmdFileSelector *fs)
{
	const gchar *msg;

	if (!fs->priv->con_open_dialog)
		return FALSE;

	msg = gnome_cmd_con_get_open_msg (fs->priv->con_opening);
	gtk_label_set_text (GTK_LABEL (fs->priv->con_open_dialog_label), msg);
	progress_bar_update (fs->priv->con_open_dialog_pbar, FS_PBAR_MAX);
		
	return TRUE;
}


static void
on_con_open_done (GnomeCmdCon *con, GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (fs->priv->con_opening != NULL);
	g_return_if_fail (fs->priv->con_opening == con);
	g_return_if_fail (fs->priv->con_open_dialog != NULL);
	
	DEBUG('m', "on_con_open_done\n");
	gtk_signal_disconnect_by_data (GTK_OBJECT (con), fs);
	
	set_connection (fs, con, NULL);

	gtk_widget_destroy (fs->priv->con_open_dialog);
	fs->priv->con_open_dialog = NULL;
	fs->priv->con_opening = NULL;
}


static void
on_con_open_failed (GnomeCmdCon *con, const gchar *emsg, GnomeVFSResult result,
					GnomeCmdFileSelector *fs)
{
	gchar *s;
	
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (fs->priv->con_opening != NULL);
	g_return_if_fail (fs->priv->con_opening == con);
	g_return_if_fail (fs->priv->con_open_dialog != NULL);

	DEBUG('m', "on_con_open_failed\n");
	gtk_signal_disconnect_by_data (GTK_OBJECT (con), fs);

	if (emsg)
		s = g_strdup (emsg);
	else
		s = g_strdup_printf (_("Failed to open connection: %s\n"),
							 gnome_vfs_result_to_string (result));
	
	if (result != GNOME_VFS_OK || emsg != NULL)
		create_error_dialog (s);

	g_free (s);
	
	gtk_widget_destroy (fs->priv->con_open_dialog);
	fs->priv->con_open_dialog = NULL;
	fs->priv->con_opening = NULL;
}


static void
create_con_open_progress_dialog (GnomeCmdFileSelector *fs)
{
	GtkWidget *vbox;
	
	fs->priv->con_open_dialog = gnome_cmd_dialog_new (NULL);
	gtk_widget_ref (fs->priv->con_open_dialog);

	gnome_cmd_dialog_add_button (
		GNOME_CMD_DIALOG (fs->priv->con_open_dialog),
		GNOME_STOCK_BUTTON_CANCEL,
		GTK_SIGNAL_FUNC (on_con_open_cancel), fs);

	vbox = create_vbox (fs->priv->con_open_dialog, FALSE, 0);
	
	fs->priv->con_open_dialog_label = create_label (
		fs->priv->con_open_dialog, "");

	fs->priv->con_open_dialog_pbar = create_progress_bar (
		fs->priv->con_open_dialog);
	gtk_progress_set_show_text (GTK_PROGRESS (fs->priv->con_open_dialog_pbar), FALSE);
	gtk_progress_set_activity_mode (GTK_PROGRESS (fs->priv->con_open_dialog_pbar), TRUE);
	gtk_progress_configure (GTK_PROGRESS (fs->priv->con_open_dialog_pbar), 0, 0, FS_PBAR_MAX);

	gtk_box_pack_start (GTK_BOX (vbox), fs->priv->con_open_dialog_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), fs->priv->con_open_dialog_pbar, FALSE, TRUE, 0);
	
	gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (fs->priv->con_open_dialog), vbox);

	gnome_cmd_dialog_set_transient_for (
		GNOME_CMD_DIALOG (fs->priv->con_open_dialog),
		GTK_WINDOW (main_win));

	gtk_widget_show_all (fs->priv->con_open_dialog);
}


void
gnome_cmd_file_selector_set_connection (GnomeCmdFileSelector *fs,
										GnomeCmdCon *con,
										GnomeCmdDir *dir)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	g_return_if_fail (GNOME_CMD_IS_CON (con));

	if (fs->priv->con == con) {
		if (!gnome_cmd_con_should_remember_dir (con))
			gnome_cmd_file_selector_set_directory (
				fs,
				gnome_cmd_con_get_default_dir (con));
		return;
	}

	if (!gnome_cmd_con_is_open (con)) {
		gtk_signal_connect (GTK_OBJECT (con), "open-done",
							GTK_SIGNAL_FUNC (on_con_open_done), fs);
		gtk_signal_connect (GTK_OBJECT (con), "open-failed",
							GTK_SIGNAL_FUNC (on_con_open_failed), fs);
		fs->priv->con_opening = con;

		create_con_open_progress_dialog (fs);
		gtk_timeout_add (gnome_cmd_data_get_gui_update_rate (), (GtkFunction)update_con_open_progress, fs);
		
		gnome_cmd_con_open (con);
	}
	else
		set_connection (fs, con, dir);
}


void
gnome_cmd_file_selector_update_connections (GnomeCmdFileSelector *fs)
{
	GList *l;
	gboolean found_my_con = FALSE;
	GnomeCmdConList *con_list;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	
	if (!fs->priv->realized)
		return;
	
	fs->priv->selection_lock = TRUE;

	gnome_cmd_combo_clear (GNOME_CMD_COMBO (fs->con_combo));
	GNOME_CMD_COMBO (fs->con_combo)->highest_pixmap = 20;
	GNOME_CMD_COMBO (fs->con_combo)->widest_pixmap = 20;
	gtk_clist_set_row_height (GTK_CLIST (GNOME_CMD_COMBO (fs->con_combo)->list), 20);
	gtk_clist_set_column_width (GTK_CLIST (GNOME_CMD_COMBO (fs->con_combo)->list), 0, 20);

	con_list = gnome_cmd_data_get_con_list ();
	l = gnome_cmd_con_list_get_all (con_list);
	while (l)
	{
		gint row;
		gchar *text[3];
		GnomeCmdPixmap *pixmap;
		GnomeCmdCon *con = (GnomeCmdCon*)l->data;

		if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
			&& !GNOME_CMD_IS_CON_SMB (con)) {
			l = l->next;
			continue;
		}
		
		if (con == fs->priv->con)
			found_my_con = TRUE;
		
		text[0] = NULL;
		text[1] = (gchar*)gnome_cmd_con_get_alias (con);
		text[2] = NULL;

		pixmap = gnome_cmd_con_get_go_pixmap (con);

		row = gnome_cmd_combo_append (GNOME_CMD_COMBO (fs->con_combo), text, con);
		if (pixmap)
			gnome_cmd_combo_set_pixmap (GNOME_CMD_COMBO (fs->con_combo), row, 0, pixmap);
		
		l = l->next;
	}

	fs->priv->selection_lock = FALSE;

	// If the connection is no longer available use the home connection
	if (!found_my_con)
		gnome_cmd_file_selector_set_connection (
			fs, gnome_cmd_con_list_get_home (con_list), NULL);
	else {
		gnome_cmd_combo_select_data (GNOME_CMD_COMBO (fs->con_combo), fs->priv->con);
	}

	create_con_buttons (fs);
}


GnomeCmdCon *
gnome_cmd_file_selector_get_connection (GnomeCmdFileSelector *fs)
{
	g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), NULL);
	
	return fs->priv->con;
}


void
gnome_cmd_file_selector_update_style (GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	gnome_cmd_combo_update_style (GNOME_CMD_COMBO (fs->con_combo));
	gnome_cmd_file_list_update_style (fs->list);

	if (fs->priv->realized)
		update_files (fs);
	
	gnome_cmd_file_list_show_column (
		fs->list, FILE_LIST_COLUMN_EXT,
		gnome_cmd_data_get_ext_disp_mode () != GNOME_CMD_EXT_DISP_WITH_FNAME);
	
	create_con_buttons (fs);
	gnome_cmd_file_selector_update_connections (fs);
}


void
gnome_cmd_file_selector_show_mkdir_dialog (GnomeCmdFileSelector *fs)
{
	GtkWidget *dialog;
	GnomeCmdDir *dir;

	dir = gnome_cmd_file_selector_get_directory (fs);
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	
	dialog = gnome_cmd_mkdir_dialog_new (dir);
	g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));

	gtk_widget_ref (dialog);
	gtk_widget_show (dialog);
}


static gboolean
on_new_textfile_ok (GnomeCmdStringDialog *string_dialog,
					const gchar **values,
					GnomeCmdFileSelector *fs)
{
	gchar *cmd, *dpath;
	gchar *filepath;
	const gchar *filename = values[0];
	GnomeCmdDir *dir;

	g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), TRUE);

	if (!filename) {
		gnome_cmd_string_dialog_set_error_desc (
			string_dialog, g_strdup (_("No filename entered")));
		return FALSE;
	}

	dir = gnome_cmd_file_selector_get_directory (fs);
	g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), TRUE);

	dpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (dir));
	filepath = g_build_path ("/", dpath, filename, NULL);
	g_free (dpath);
	g_return_val_if_fail (filepath, TRUE);
	
	cmd = g_strdup_printf (gnome_cmd_data_get_editor (), filepath);
	g_return_val_if_fail (cmd, TRUE);

	run_command (cmd, FALSE);
	
	return TRUE;
}


static gboolean
on_create_symlink_ok (GnomeCmdStringDialog *string_dialog,
					  const gchar **values,
					  GnomeCmdFileSelector *fs)
{
	const gchar *name = values[0];
	GnomeVFSResult result;
	GnomeVFSURI *uri;

	g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), TRUE);
	g_return_val_if_fail (fs->priv->sym_file != NULL, TRUE);

	/* dont create any symlink if no name was passed or cancel was selected */
	if (name == NULL || strcmp (name, "") == 0) {
		gnome_cmd_string_dialog_set_error_desc (
			string_dialog, g_strdup (_("No filename given")));
		return FALSE;
	}

	uri = gnome_cmd_dir_get_child_uri (fs->priv->cwd, name);

	result = gnome_vfs_create_symbolic_link (
		uri, gnome_cmd_file_get_uri_str (fs->priv->sym_file));

	gnome_vfs_uri_unref (uri);
	
	if (result == GNOME_VFS_OK)
		return TRUE;
	
	gnome_cmd_string_dialog_set_error_desc (
		string_dialog, g_strdup (gnome_vfs_result_to_string (result)));

	return FALSE;
}


void
gnome_cmd_file_selector_show_new_textfile_dialog (GnomeCmdFileSelector *fs)
{
	const gchar *labels[] = {_("Filename:")};
	GtkWidget *dialog;
	
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	dialog = gnome_cmd_string_dialog_new (
		_("New Text File"), labels, 1,
		(GnomeCmdStringDialogCallback)on_new_textfile_ok, fs);
	g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));

	gtk_widget_ref (dialog);
	gtk_widget_show (dialog);
}


void
gnome_cmd_file_selector_cap_paste (GnomeCmdFileSelector *fs)
{
	GnomeCmdDir *dir;

	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
	dir = gnome_cmd_file_selector_get_directory (fs);
	g_return_if_fail (GNOME_CMD_IS_DIR (dir));
	
	cap_paste_files (dir);
}


gboolean
gnome_cmd_file_selector_keypressed (GnomeCmdFileSelector *fs,
									GdkEventKey *event)
{
	GnomeCmdFile *finfo;
	
	g_return_val_if_fail (event != NULL, FALSE);
	g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), FALSE);

	if (state_is_ctrl_shift (event->state)) {
		switch (event->keyval)
		{
			case GDK_Return:
			case GDK_KP_Enter:
				add_file_to_cmdline (fs, TRUE);
				return TRUE;
		}
	}
	else if (state_is_shift (event->state)) {
		switch (event->keyval)
		{
			case GDK_F5:
				gnome_cmd_file_list_show_make_copy_dialog (fs);
				return TRUE;
				
			case GDK_F4:
				gnome_cmd_file_selector_start_editor (fs);
				return TRUE;
		}
	}
	else if (state_is_alt (event->state)) {
		switch (event->keyval)
		{
			case GDK_Down:
				gnome_cmd_dir_indicator_show_history (
					GNOME_CMD_DIR_INDICATOR (fs->dir_indicator));
				return TRUE;

			case GDK_Left:
				gnome_cmd_file_selector_back (fs);
				stop_kp(GTK_OBJECT(fs->list));
				return TRUE;
				
			case GDK_Right:
				gnome_cmd_file_selector_forward (fs);
				stop_kp(GTK_OBJECT(fs->list));
				return TRUE;
		}
	}
	else if (state_is_alt_shift (event->state)) {
		switch (event->keyval)
		{
			case GDK_Return:
			case GDK_KP_Enter:
				show_dir_tree_sizes (fs);
				return TRUE;
		}		
	}
	else if (state_is_ctrl (event->state)) {
		switch (event->keyval)
		{
			case GDK_V:
			case GDK_v:
				gnome_cmd_file_selector_cap_paste (fs);
				return TRUE;

			case GDK_R:
			case GDK_r:
				gnome_cmd_file_selector_reload (fs);
				return TRUE;
				
			case GDK_P:
			case GDK_p:
				add_cwd_to_cmdline (fs);
				return TRUE;

			case GDK_Page_Up:
				goto_directory (fs, "..");
				return TRUE;
				
			case GDK_Page_Down:
				finfo = gnome_cmd_file_list_get_selected_file (fs->list);
				if (finfo && finfo->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
					do_file_specific_action (fs, finfo);
				return TRUE;
				
			case GDK_Return:
			case GDK_KP_Enter:
				add_file_to_cmdline (fs, FALSE);
				return TRUE;

			case GDK_period:
				set_same_directory (fs);
				return TRUE;
		}
	}
	else if (state_is_blank (event->state)) {
		switch (event->keyval)
		{
			case GDK_space:
				set_cursor_busy ();
				gnome_cmd_file_list_toggle (fs->list);
				show_selected_dir_tree_size (fs);
				stop_kp (GTK_OBJECT (fs->list));
				set_cursor_default ();
				return TRUE;
			
			case GDK_Left:
			case GDK_KP_Left:
			case GDK_BackSpace:
				goto_directory (fs, "..");
				return TRUE;

			case GDK_Right:
			case GDK_KP_Right:			
				finfo = gnome_cmd_file_list_get_selected_file (fs->list);
				if (finfo && finfo->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
					do_file_specific_action (fs, finfo);
				stop_kp (GTK_OBJECT (fs->list));
				return TRUE;
		
			case GDK_Return:
			case GDK_KP_Enter:
				if (gnome_cmd_data_get_cmdline_visibility ()
					&& gnome_cmd_cmdline_is_empty (gnome_cmd_main_win_get_cmdline (main_win)))
					gnome_cmd_cmdline_exec (gnome_cmd_main_win_get_cmdline (main_win));
				else
					do_file_specific_action (
						fs, gnome_cmd_file_list_get_focused_file (fs->list));
				return TRUE;

			case GDK_Escape:
				if (gnome_cmd_data_get_cmdline_visibility ())
					gnome_cmd_cmdline_set_text (
						gnome_cmd_main_win_get_cmdline (main_win), "");
				return TRUE;
		}
	}

	return FALSE;
}


void
gnome_cmd_file_selector_create_symlink (GnomeCmdFileSelector *fs, GnomeCmdFile *finfo)
{
	gchar *text, *fname;
	const gchar *labels[] = {_("Symlink name:")};
	GtkWidget *dialog;

	fname = get_utf8 (gnome_cmd_file_get_name (finfo));
	text = g_strdup_printf ("Symlink to %s", fname);
	g_free (fname);
	dialog = gnome_cmd_string_dialog_new (_("Create Symbolic Link"),
										  labels,
										  1,
										  (GnomeCmdStringDialogCallback)on_create_symlink_ok,
										  fs);
	gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, text);
	g_free (text);
	fs->priv->sym_file = finfo;
	gtk_widget_show (dialog);
}


void
gnome_cmd_file_selector_update_conbuttons_visibility (GnomeCmdFileSelector *fs)
{
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	if (!gnome_cmd_data_get_conbuttons_visibility ()) {
		if (fs->con_btns_hbox) {
			gtk_object_destroy (GTK_OBJECT (fs->con_btns_hbox));
			fs->con_btns_hbox = NULL;
		}
	}
	else {
		if (!fs->con_btns_hbox) {
			fs->con_btns_hbox = create_hbox (GTK_WIDGET (fs), FALSE, 2);
			gtk_box_pack_start (GTK_BOX (fs), fs->con_btns_hbox, FALSE, FALSE, 0);
			gtk_box_reorder_child (GTK_BOX (fs), fs->con_btns_hbox, 0);
			gtk_widget_show (fs->con_btns_hbox);
			create_con_buttons (fs);
		}
	}
}


static void
on_filter_box_close (GtkButton *btn, GnomeCmdFileSelector *fs)
{
	if (!fs->priv->filter_box) return;
	
	gtk_widget_destroy (fs->priv->filter_box);
	fs->priv->filter_box = NULL;
}


gboolean
on_filter_box_keypressed (GtkEntry *entry, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
	if (state_is_blank (event->state)) {
		if (event->keyval == GDK_Escape) {
			on_filter_box_close (NULL, fs);
			return TRUE;
		}
	}

	return FALSE;
}


void
gnome_cmd_file_selector_show_filter (GnomeCmdFileSelector *fs, gchar c)
{
	GtkWidget *close_btn, *entry, *label, *parent;
	
	g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

	parent = GTK_WIDGET (fs);

	if (fs->priv->filter_box) return;

	fs->priv->filter_box = create_hbox (parent, FALSE, 0);
	label = create_label (parent, _("Filter:"));
	entry = create_entry (parent, "entry", "");
	close_btn = create_button_with_data (
		GTK_WIDGET (main_win), "x",
		GTK_SIGNAL_FUNC (on_filter_box_close), fs);
  
	gtk_signal_connect (GTK_OBJECT (entry), "key-press-event",
			GTK_SIGNAL_FUNC (on_filter_box_keypressed), fs);
	gtk_box_pack_start (GTK_BOX (fs->priv->filter_box), label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (fs->priv->filter_box), entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (fs->priv->filter_box), close_btn, FALSE, TRUE, 0);
	
	gtk_box_pack_start (GTK_BOX (fs), fs->priv->filter_box, FALSE, TRUE, 0);

	gtk_widget_grab_focus (entry);
}
