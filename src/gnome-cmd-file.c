/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2005 Marcus Bjurman

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
#include <errno.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-dir.h"
#include "utils.h"
#include "owner.h"
#include "imageloader.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-file-props-dialog.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-xfer.h"

#define MAX_TYPE_LENGTH 2
#define MAX_NAME_LENGTH 128
#define MAX_OWNER_LENGTH 128
#define MAX_GROUP_LENGTH 128
#define MAX_PERM_LENGTH 10
#define MAX_DATE_LENGTH 64
#define MAX_SIZE_LENGTH 32

gint created_files_cnt = 0;
gint deleted_files_cnt = 0;
GList *all_files = NULL;

static GnomeCmdFileInfoClass *parent_class = NULL;

struct _GnomeCmdFilePrivate
{
	Handle *dir_handle;
	GTimeVal last_update;
	gint ref_cnt;
};


static gboolean
has_parent_dir (GnomeCmdFile *finfo)
{
	return finfo->priv->dir_handle && finfo->priv->dir_handle->ref;
}


static GnomeCmdDir *
get_parent_dir (GnomeCmdFile *finfo)
{
	g_return_val_if_fail (finfo->priv->dir_handle != NULL, NULL);
	
	return GNOME_CMD_DIR (finfo->priv->dir_handle->ref);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	GnomeCmdFile *file = GNOME_CMD_FILE (object);

	if (file->info->name[0] != '.')
		DEBUG ('f', "file destroying 0x%x %s\n", (guint)file, file->info->name);
	gnome_vfs_file_info_unref (file->info);
	if (file->priv->dir_handle)
		handle_unref (file->priv->dir_handle);

	if (DEBUG_ENABLED ('c')) {
		all_files = g_list_remove (all_files, file);
		deleted_files_cnt++;
	}

	g_free (file->priv);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdFileClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	parent_class = gtk_type_class (gnome_cmd_file_info_get_type ());

	object_class->destroy = destroy;
}

static void
init (GnomeCmdFile *file)
{
	file->info = NULL;
	
	file->priv = g_new0 (GnomeCmdFilePrivate, 1);
	file->priv->dir_handle = NULL;

	if (DEBUG_ENABLED ('c')) {
		all_files = g_list_append (all_files, file);	
		created_files_cnt++;
	}

	file->priv->last_update.tv_sec = 0;
	file->priv->last_update.tv_usec = 0;
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_file_get_type         (void)
{
	static GtkType type = 0;

	if (type == 0)
	{
		GtkTypeInfo info =
		{
			"GnomeCmdFile",
			sizeof (GnomeCmdFile),
			sizeof (GnomeCmdFileClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gnome_cmd_file_info_get_type (), &info);
	}
	return type;
}


GnomeCmdFile *
gnome_cmd_file_new (GnomeVFSFileInfo *info, GnomeCmdDir *dir)
{
	GnomeCmdFile *finfo = gtk_type_new (gnome_cmd_file_get_type ());

	gnome_cmd_file_setup (finfo, info, dir);
	
	return finfo;
}


void
gnome_cmd_file_setup (GnomeCmdFile *finfo, GnomeVFSFileInfo *info, GnomeCmdDir *dir)
{
	g_return_if_fail (finfo != NULL);
	
	finfo->info = info;
	GNOME_CMD_FILE_INFO (finfo)->info = info;
	
	if (dir) {
		finfo->priv->dir_handle = gnome_cmd_dir_get_handle (dir);
		handle_ref (finfo->priv->dir_handle);

		GNOME_CMD_FILE_INFO (finfo)->uri = gnome_cmd_dir_get_child_uri (
			dir, finfo->info->name);
		gnome_vfs_uri_ref (GNOME_CMD_FILE_INFO (finfo)->uri);
	}

	gnome_vfs_file_info_ref (finfo->info);
}


void
gnome_cmd_file_ref (GnomeCmdFile *file)
{
	char c;
	
	g_return_if_fail (file != NULL);

	file->priv->ref_cnt++;

	if (file->priv->ref_cnt == 1) {
		gtk_object_ref (GTK_OBJECT (file));
	}

	if (GNOME_CMD_IS_DIR (file)) c = 'd';
	else c = 'f';
	
	DEBUG (c, "refing: 0x%x %s to %d\n",
		   (guint)file, file->info->name, file->priv->ref_cnt);
}


void
gnome_cmd_file_unref (GnomeCmdFile *file)
{
	char c;
	
	g_return_if_fail (file != NULL);
	
	file->priv->ref_cnt--;
	
	if (GNOME_CMD_IS_DIR (file)) c = 'd';
	else c = 'f';
	
	DEBUG (c, "un-refing: 0x%x %s to %d\n",
		   (guint)file, file->info->name, file->priv->ref_cnt);
	if (file->priv->ref_cnt < 1)
		gtk_object_destroy (GTK_OBJECT (file));
}


GnomeVFSResult
gnome_cmd_file_chmod (GnomeCmdFile *file, GnomeVFSFilePermissions perm)
{
	GnomeVFSResult ret;
	GnomeVFSURI *uri;

	g_return_val_if_fail (file != NULL, 0);
	g_return_val_if_fail (file->info != NULL, 0);

	file->info->permissions = perm;
	uri = gnome_cmd_file_get_uri (file);
	ret = gnome_vfs_set_file_info_uri (
		uri,
		file->info,
		GNOME_VFS_SET_FILE_INFO_PERMISSIONS);
	gnome_vfs_uri_unref (uri);

	if (has_parent_dir (file)) {
		GnomeCmdDir *dir = get_parent_dir (file);
		gnome_cmd_dir_file_changed (dir, file->info->name);
	}

	return ret;
}


GnomeVFSResult
gnome_cmd_file_chown (GnomeCmdFile *file, uid_t uid, gid_t gid)
{
	GnomeVFSResult ret;
	GnomeVFSURI *uri;

	g_return_val_if_fail (file != NULL, 0);
	g_return_val_if_fail (file->info != NULL, 0);

	if (uid != -1)
		file->info->uid = uid;
	file->info->gid = gid;
	
	uri = gnome_cmd_file_get_uri (file);
	ret = gnome_vfs_set_file_info_uri (
		uri,
		file->info,
		GNOME_VFS_SET_FILE_INFO_OWNER);
	gnome_vfs_uri_unref (uri);

	if (has_parent_dir (file)) {
		GnomeCmdDir *dir = get_parent_dir (file);
		gnome_cmd_dir_file_changed (dir, file->info->name);
	}

	return ret;
}


GnomeVFSResult
gnome_cmd_file_rename (GnomeCmdFile *finfo, const gchar *new_name)
{
	GnomeVFSFileInfo *new_info;
	GnomeVFSResult result;
	GnomeVFSURI *uri;

	g_return_val_if_fail (finfo, FALSE);
	g_return_val_if_fail (finfo->info, FALSE);

	new_info = gnome_vfs_file_info_dup (finfo->info);
	g_return_val_if_fail (new_info, FALSE);

	g_free (new_info->name);
	new_info->name = g_strdup (new_name);
	
	uri = gnome_cmd_file_get_uri (finfo);
	result = gnome_vfs_set_file_info_uri (
		uri,
		new_info,
		GNOME_VFS_SET_FILE_INFO_NAME);
	gnome_vfs_uri_unref (uri);

	if (result == GNOME_VFS_OK && has_parent_dir (finfo)) {
		gnome_cmd_file_update_info (finfo, new_info);
		gnome_cmd_dir_file_renamed (get_parent_dir (finfo), finfo);
		if (GNOME_CMD_IS_DIR (finfo))
			gnome_cmd_dir_update_path (GNOME_CMD_DIR (finfo));
	}

	return result;
}


const gchar *
gnome_cmd_file_get_name (GnomeCmdFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	return file->info->name;
}


gchar *
gnome_cmd_file_get_quoted_name (GnomeCmdFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	return quote_if_needed (file->info->name);
}


gchar *
gnome_cmd_file_get_path (GnomeCmdFile *finfo)
{
	gchar *path_str;
	GnomeCmdPath *path;
	
	g_return_val_if_fail (finfo != NULL, NULL);
	g_return_val_if_fail (finfo->info != NULL, NULL);

	if (strcmp (finfo->info->name, "/") == 0)
		return g_strdup ("/");
	
	if (!has_parent_dir (finfo)) {
		if (GNOME_CMD_IS_DIR (finfo)) {
			path = gnome_cmd_dir_get_path (GNOME_CMD_DIR (finfo));
			return g_strdup (gnome_cmd_path_get_path (path));
		}
		g_assert ("Non directory file without owning directory");
	}

	path = gnome_cmd_path_get_child (
		gnome_cmd_dir_get_path (get_parent_dir (finfo)), finfo->info->name);
	path_str = g_strdup (gnome_cmd_path_get_path (path));
	gtk_object_destroy (GTK_OBJECT (path));

	return path_str;
}


gchar *
gnome_cmd_file_get_real_path (GnomeCmdFile *finfo)
{
	GnomeVFSURI *uri = gnome_cmd_file_get_uri (finfo);
	gchar *path = g_strdup (gnome_vfs_uri_get_path (uri));

	gnome_vfs_uri_unref (uri);
	
	return path;

}


gchar *
gnome_cmd_file_get_quoted_real_path (GnomeCmdFile *finfo)
{
	gchar *ret = NULL;
	gchar *path;

	path = gnome_cmd_file_get_real_path (finfo);	
	if (path) {
		ret = quote_if_needed (path);
		g_free (path);
	}

	return ret;
}


GnomeVFSURI *
gnome_cmd_file_get_uri (GnomeCmdFile *finfo)
{
	g_return_val_if_fail (GNOME_CMD_IS_FILE (finfo), NULL);

	if (!has_parent_dir (finfo)) {
		if (GNOME_CMD_IS_DIR (finfo)) {
			GnomeCmdCon *con;
			GnomeCmdPath *path;	
			path = gnome_cmd_dir_get_path (GNOME_CMD_DIR (finfo));
			con = gnome_cmd_dir_get_connection (GNOME_CMD_DIR (finfo));
			return gnome_cmd_con_create_uri (con, path);
		}
		else
			g_assert ("Non directory file without owning directory");
	}
	
	return gnome_cmd_dir_get_child_uri (get_parent_dir (finfo), finfo->info->name);
}


gchar *
gnome_cmd_file_get_uri_str (GnomeCmdFile *finfo)
{
	gchar *uri_str;
	GnomeVFSURI *uri;
	
	g_return_val_if_fail (GNOME_CMD_IS_FILE (finfo), NULL);

	uri = gnome_cmd_file_get_uri (finfo);
	uri_str = gnome_vfs_uri_to_string (uri, 0);
	gnome_vfs_uri_unref (uri);
	
	return uri_str;
}


const gchar *
gnome_cmd_file_get_extension (GnomeCmdFile *file)
{
	gint i, len;
	
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);

	if (file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
		return NULL;

	len = strlen (file->info->name);
	for ( i=len ; i>0 ; i-- ) {
		if (file->info->name[i] == '.')
			return &file->info->name[i+1];
	}

	return NULL;
}


const gchar *
gnome_cmd_file_get_owner (GnomeCmdFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	if (GNOME_VFS_FILE_INFO_LOCAL (file->info))
	{
		user_t *owner = OWNER_get_user_by_uid (file->info->uid);
		return owner->name;
	}
	else
	{
		static gchar owner_str[MAX_OWNER_LENGTH];
		g_snprintf (owner_str, MAX_OWNER_LENGTH, "%d", file->info->uid);
		return owner_str;
	}
}


const gchar *
gnome_cmd_file_get_group (GnomeCmdFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	if (GNOME_VFS_FILE_INFO_LOCAL (file->info))
	{
		group_t *group = OWNER_get_group_by_gid (file->info->gid);
		return group->name;
	}
	else
	{
		static gchar group_str[MAX_GROUP_LENGTH];
		g_snprintf (group_str, MAX_GROUP_LENGTH, "%d", file->info->gid);
		return group_str;
	}
}


static const gchar *
date2string (time_t date, gboolean overide_disp_setting)
{
	return time2string (date, overide_disp_setting?"%c":gnome_cmd_data_get_date_format ());
}




const gchar *
gnome_cmd_file_get_adate (GnomeCmdFile *file,
						  gboolean overide_disp_setting)
{
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	return date2string (file->info->atime, overide_disp_setting);
}


const gchar *
gnome_cmd_file_get_mdate (GnomeCmdFile *file,
						  gboolean overide_disp_setting)
{
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	return date2string (file->info->mtime, overide_disp_setting);
}


const gchar *
gnome_cmd_file_get_cdate (GnomeCmdFile *file,
						  gboolean overide_disp_setting)
{
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	return date2string (file->info->ctime, overide_disp_setting);
}


const gchar *
gnome_cmd_file_get_size (GnomeCmdFile *file)
{
	static gchar dir_indicator[] = "<DIR> ";
	
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	if (file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
		return dir_indicator;

	return size2string (file->info->size, gnome_cmd_data_get_size_disp_mode ());
}


const gchar *
gnome_cmd_file_get_tree_size (GnomeCmdFile *file)
{
	const gchar *size;
	GnomeVFSURI *uri;
	
	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);

	if (file->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
		return gnome_cmd_file_get_size (file);

	if (strcmp (file->info->name, "..") == 0)
		return gnome_cmd_file_get_size (file);

	uri = gnome_cmd_file_get_uri (file);
	size = size2string (calc_tree_size (uri), gnome_cmd_data_get_size_disp_mode ());
	gnome_vfs_uri_unref (uri);
	
	return size;
}


const gchar *
gnome_cmd_file_get_perm (GnomeCmdFile *file)
{
	static gchar perm_str[MAX_PERM_LENGTH];

	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->info != NULL, NULL);
	
	perm2string (file->info->permissions, perm_str, MAX_PERM_LENGTH);
	return perm_str;
}


const gchar *
gnome_cmd_file_get_type_string (GnomeCmdFile *finfo)
{
	static gchar type_str[MAX_TYPE_LENGTH];
	
	g_return_val_if_fail (finfo != NULL, NULL);
	g_return_val_if_fail (finfo->info != NULL, NULL);
	
	type2string (finfo->info->type, type_str, MAX_TYPE_LENGTH);
	return type_str;
}


const gchar *
gnome_cmd_file_get_type_desc (GnomeCmdFile *finfo)
{
	gchar *type_strings[] = {
		_("Unknown file type"),
		_("Regular file"),
		_("Directory"),
		_("FIFO"),
		_("UNIX Socket"),
		_("Character device"),
		_("Block device"),
		_("Symbolic link")
	};

	g_return_val_if_fail (finfo != NULL, NULL);
	g_return_val_if_fail (finfo->info != NULL, NULL);

	if (!finfo->info->symlink_name)	
		return type_strings[finfo->info->type];
	return type_strings[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK];
}


GdkPixmap *
gnome_cmd_file_get_type_pixmap (GnomeCmdFile *finfo)
{
	GdkPixmap *pm;
	GdkBitmap *mask;
	
	g_return_val_if_fail (finfo != NULL, NULL);
	g_return_val_if_fail (finfo->info != NULL, NULL);

	IMAGE_get_pixmap_and_mask (finfo->info->type,
							   finfo->info->mime_type,
							   finfo->info->symlink_name != NULL,
							   &pm, &mask);
	return pm;
}


GdkBitmap *
gnome_cmd_file_get_type_mask (GnomeCmdFile *finfo)
{
	GdkPixmap *pm;
	GdkBitmap *mask;
	
	g_return_val_if_fail (finfo != NULL, NULL);
	g_return_val_if_fail (finfo->info != NULL, NULL);

	IMAGE_get_pixmap_and_mask (finfo->info->type,
							   finfo->info->mime_type,
							   finfo->info->symlink_name != NULL,
							   &pm, &mask);
	return mask;
}


const gchar *
gnome_cmd_file_get_mime_type (GnomeCmdFile *finfo)
{
	g_return_val_if_fail (finfo != NULL, NULL);
	g_return_val_if_fail (finfo->info != NULL, NULL);

	return gnome_vfs_file_info_get_mime_type (finfo->info);
}


const gchar *
gnome_cmd_file_get_mime_type_desc (GnomeCmdFile *finfo)
{
	g_return_val_if_fail (finfo != NULL, NULL);
	g_return_val_if_fail (finfo->info != NULL, NULL);
	
	return gnome_vfs_mime_get_description (finfo->info->mime_type);
}


gboolean
gnome_cmd_file_has_mime_type (GnomeCmdFile *finfo, const gchar *mime_type)
{
	g_return_val_if_fail (finfo != NULL, FALSE);
	g_return_val_if_fail (finfo->info != NULL, FALSE);
	g_return_val_if_fail (finfo->info->mime_type != NULL, FALSE);
	g_return_val_if_fail (mime_type != NULL, FALSE);

	return strcmp (finfo->info->mime_type, mime_type) == 0;
}


gboolean gnome_cmd_file_mime_begins_with (GnomeCmdFile *finfo,
										  const gchar *mime_type_start)
{
	g_return_val_if_fail (finfo != NULL, FALSE);
	g_return_val_if_fail (finfo->info != NULL, FALSE);
	g_return_val_if_fail (finfo->info->mime_type != NULL, FALSE);
	g_return_val_if_fail (mime_type_start != NULL, FALSE);

	return strncmp (finfo->info->mime_type, mime_type_start, strlen(mime_type_start)) == 0;
}


void
gnome_cmd_file_show_properties (GnomeCmdFile *finfo)
{
	GtkWidget *dialog = gnome_cmd_file_props_dialog_create (finfo);
	if (!dialog) return;
	gtk_widget_ref (dialog);
	gtk_widget_show (dialog);
}


static void
do_view_file (const gchar *path)
{
	gchar *command;
	gchar *arg;

	arg = g_shell_quote (path);
	command = g_strdup_printf (gnome_cmd_data_get_viewer (), arg);
	run_command (command, FALSE);

	g_free (arg);
	g_free (command);
}


static void
on_file_downloaded_for_view (gchar *path)
{
	do_view_file (path);
	
	g_free (path);
}


void
gnome_cmd_file_view (GnomeCmdFile *finfo)
{
	gchar *path_str;
	GnomeCmdPath *path;
	GnomeVFSURI *src_uri, *dest_uri;
	GnomeCmdCon *con;

	g_return_if_fail (finfo != NULL);
	g_return_if_fail (has_parent_dir (finfo));

	/* If the file is local there is no need to download it */
	if (gnome_cmd_dir_is_local (get_parent_dir (finfo))) {
		gchar *fpath = gnome_cmd_file_get_real_path (finfo);
		do_view_file (fpath);
		g_free (fpath);
		return;
	}

	/* The file is remote lets download it to a temporary file first */
	path_str = get_temp_download_filepath (gnome_cmd_file_get_name (finfo));
	if (!path_str) return;

	path = gnome_cmd_plain_path_new (path_str);
	src_uri = gnome_cmd_file_get_uri (finfo);
	con = gnome_cmd_con_list_get_home (
		gnome_cmd_data_get_con_list ());
	dest_uri = gnome_cmd_con_create_uri (con, path);

	g_printerr ("Copying to: %s\n", path_str);
	gtk_object_destroy (GTK_OBJECT (path));
	
	gnome_cmd_xfer_tmp_download (src_uri,
								 dest_uri,
								 GNOME_VFS_XFER_FOLLOW_LINKS,
								 GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
								 GTK_SIGNAL_FUNC (on_file_downloaded_for_view),
								 path_str);
}


void
gnome_cmd_file_edit (GnomeCmdFile *finfo)
{
	gchar *command, *fpath, *arg;
	
	g_return_if_fail (finfo != NULL);

	fpath = gnome_cmd_file_get_real_path (finfo);
	arg = g_shell_quote (fpath);
	command = g_strdup_printf (gnome_cmd_data_get_editor (), arg);
	run_command (command, FALSE);	
	g_free (command);
	g_free (fpath);
	g_free (arg);
}


void
gnome_cmd_file_show_cap_cut (GnomeCmdFile *finfo)
{
}


void
gnome_cmd_file_show_cap_copy (GnomeCmdFile *finfo)
{
}


void
gnome_cmd_file_show_cap_paste (GnomeCmdFile *finfo)
{
}


void
gnome_cmd_file_update_info (GnomeCmdFile *finfo, GnomeVFSFileInfo *info)
{
	g_return_if_fail (finfo != NULL);
	g_return_if_fail (info != NULL);
	
	gnome_vfs_file_info_unref (finfo->info);
	gnome_vfs_file_info_ref (info);
	finfo->info = info;
}


gboolean
gnome_cmd_file_is_local (GnomeCmdFile *finfo)
{
	g_return_val_if_fail (GNOME_CMD_IS_FILE (finfo), FALSE);

	return gnome_cmd_dir_is_local (get_parent_dir (finfo));
}

gboolean
gnome_cmd_file_is_executable (GnomeCmdFile *finfo)
{
	user_t *user;
	
	g_return_val_if_fail (GNOME_CMD_IS_FILE (finfo), FALSE);

	if (finfo->info->type != GNOME_VFS_FILE_TYPE_REGULAR)
		return FALSE;

	if (!gnome_cmd_file_is_local (finfo))
		return FALSE;

	user = OWNER_get_program_user ();
	if (!user)
		return FALSE;
	
	if (user->uid == finfo->info->uid
		&& finfo->info->permissions & GNOME_VFS_PERM_USER_EXEC)
		return TRUE;

	if (user->gid == finfo->info->gid
		&& finfo->info->permissions & GNOME_VFS_PERM_GROUP_EXEC)
		return TRUE;

	if (finfo->info->permissions & GNOME_VFS_PERM_OTHER_EXEC)
		return TRUE;

	return FALSE;
}


void
gnome_cmd_file_is_deleted (GnomeCmdFile *finfo)
{
	g_return_if_fail (GNOME_CMD_IS_FILE (finfo));

	if (has_parent_dir (finfo)) {
		gchar *uri_str = gnome_cmd_file_get_uri_str (finfo);
		gnome_cmd_dir_file_deleted (get_parent_dir (finfo), uri_str);
		g_free (uri_str);
	}
}


void
gnome_cmd_file_execute (GnomeCmdFile *finfo)
{
	gchar *fpath, *dpath, *cmd;

	fpath = gnome_cmd_file_get_real_path (finfo);
	dpath = g_dirname (fpath);
	cmd = g_strdup_printf ("./%s", finfo->info->name);
	run_command_indir (cmd, dpath,
				 app_needs_terminal (finfo));
	g_free (fpath);
	g_free (dpath);
	g_free (cmd);
}



/******************************************************************************
*
*   Function: gnome_cmd_file_list_copy
*
*   Purpose: Refs all files in the passed list and return a copy of that list
*
*   Params: 
*
*   Returns: A copy of the passed list with all files ref'ed
*
*   Statuses: 
*
******************************************************************************/

GList *
gnome_cmd_file_list_copy (GList *files)
{
	g_return_val_if_fail (files != NULL, NULL);

	gnome_cmd_file_list_ref (files);
	return g_list_copy (files);
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_free
*
*   Purpose: Unrefs all files in the passed list and then frees the list
*
*   Params: 
*
*   Returns: 
*
*   Statuses: 
*
******************************************************************************/

void
gnome_cmd_file_list_free (GList *files)
{
	if (!files) return;

	gnome_cmd_file_list_unref (files);
	g_list_free (files);
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_ref
*
*   Purpose: Refs all files in the passed list
*
*   Params: 
*
*   Returns: 
*
*   Statuses: 
*
******************************************************************************/

void
gnome_cmd_file_list_ref (GList *files)
{
	g_return_if_fail (files != NULL);

	g_list_foreach (files, (GFunc)gnome_cmd_file_ref, NULL);
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_unref
*
*   Purpose: Unrefs all files in the passed list
*
*   Params: 
*
*   Returns: 
*
*   Statuses: 
*
******************************************************************************/

void
gnome_cmd_file_list_unref (GList *files)
{
	g_return_if_fail (files != NULL);

	g_list_foreach (files, (GFunc)gnome_cmd_file_unref, NULL);
}


static gulong
tv2ms (GTimeVal *t)
{
	return t->tv_sec * 1000 + t->tv_usec/1000;
}


gboolean gnome_cmd_file_needs_update (GnomeCmdFile *file)
{
	GTimeVal t;

	g_get_current_time (&t);

	if (tv2ms (&t) - tv2ms (&file->priv->last_update) >
		gnome_cmd_data_get_gui_update_rate ()) {
		file->priv->last_update.tv_sec = t.tv_sec;
		file->priv->last_update.tv_usec = t.tv_usec;
		return TRUE;
	}

	return FALSE;
}
