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
#include <errno.h>

#include "gnome-cmd-includes.h"
#include "utils.h"
#include "owner.h"
#include "imageloader.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-file-props-dialog.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-xfer.h"
#include "tags/gnome-cmd-tags.h"
#include "intviewer/libgviewer.h"

using namespace std;

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

struct GnomeCmdFilePrivate
{
    Handle *dir_handle;
    GTimeVal last_update;
    gint ref_cnt;
    GnomeVFSFileSize tree_size;
};


inline gboolean has_parent_dir (GnomeCmdFile *f)
{
    return f->priv->dir_handle && f->priv->dir_handle->ref;
}


inline GnomeCmdDir *get_parent_dir (GnomeCmdFile *f)
{
    g_return_val_if_fail (f->priv->dir_handle != NULL, NULL);

    return GNOME_CMD_DIR (f->priv->dir_handle->ref);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdFile *file = GNOME_CMD_FILE (object);

    delete file->metadata;

    if (file->info->name[0] != '.')
        DEBUG ('f', "file destroying 0x%p %s\n", file, file->info->name);

    g_free (file->collate_key);
    gnome_vfs_file_info_unref (file->info);
    if (file->priv->dir_handle)
        handle_unref (file->priv->dir_handle);

    if (DEBUG_ENABLED ('c'))
    {
        all_files = g_list_remove (all_files, file);
        deleted_files_cnt++;
    }

    g_free (file->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdFileClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

    parent_class = (GnomeCmdFileInfoClass *) gtk_type_class (gnome_cmd_file_info_get_type ());

    object_class->destroy = destroy;
}


static void init (GnomeCmdFile *file)
{
    file->info = NULL;
    file->collate_key = NULL;

    file->priv = g_new0 (GnomeCmdFilePrivate, 1);
    file->priv->dir_handle = NULL;

    file->priv->tree_size = -1;

    if (DEBUG_ENABLED ('c'))
    {
        all_files = g_list_append (all_files, file);
        created_files_cnt++;
    }

    file->priv->last_update.tv_sec = 0;
    file->priv->last_update.tv_usec = 0;

}


/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_file_get_type ()
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


GnomeCmdFile *gnome_cmd_file_new (GnomeVFSFileInfo *info, GnomeCmdDir *dir)
{
    GnomeCmdFile *f = (GnomeCmdFile *) gtk_type_new (gnome_cmd_file_get_type ());

    gnome_cmd_file_setup (f, info, dir);

    return f;
}


GnomeCmdFile *gnome_cmd_file_new_from_uri (GnomeVFSURI *uri)
{
    g_return_val_if_fail (uri != NULL, NULL);

    const GnomeVFSFileInfoOptions infoOpts = (GnomeVFSFileInfoOptions) (GNOME_VFS_FILE_INFO_FOLLOW_LINKS|GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();

    if (gnome_vfs_get_file_info_uri (uri, info, infoOpts) != GNOME_VFS_OK)
    {
        gnome_vfs_file_info_unref (info);
        return NULL;
    }

    GnomeVFSURI *parent = gnome_vfs_uri_get_parent (uri);
    GnomeCmdPath *path = gnome_cmd_plain_path_new (gnome_vfs_uri_get_path (parent));
    GnomeCmdDir *dir = gnome_cmd_dir_new (get_home_con(), path);

    gnome_vfs_uri_unref (parent);
    gtk_object_unref (GTK_OBJECT (path));

    return gnome_cmd_file_new (info, dir);
}


void gnome_cmd_file_invalidate_metadata (GnomeCmdFile *f)
{
    g_return_if_fail (f != NULL);

    delete f->metadata;
    f->metadata = NULL;
}


void gnome_cmd_file_setup (GnomeCmdFile *f, GnomeVFSFileInfo *info, GnomeCmdDir *dir)
{
    g_return_if_fail (f != NULL);

    f->info = info;
    GNOME_CMD_FILE_INFO (f)->info = info;

    gchar *utf8_name;

    if (!gnome_cmd_data.case_sens_sort)
    {
        gchar *s = get_utf8 (info->name);
        utf8_name = g_utf8_casefold (s, -1);
        g_free (s);
    }
    else
        utf8_name = get_utf8 (info->name);

    f->collate_key = g_utf8_collate_key_for_filename (utf8_name, -1);
    g_free (utf8_name);

    if (dir)
    {
        f->priv->dir_handle = gnome_cmd_dir_get_handle (dir);
        handle_ref (f->priv->dir_handle);

        GNOME_CMD_FILE_INFO (f)->uri = gnome_cmd_dir_get_child_uri (dir, f->info->name);
        gnome_vfs_uri_ref (GNOME_CMD_FILE_INFO (f)->uri);
    }

    gnome_vfs_file_info_ref (f->info);
}


GnomeCmdFile *gnome_cmd_file_ref (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);

    file->priv->ref_cnt++;

    if (file->priv->ref_cnt == 1)
        gtk_object_ref (GTK_OBJECT (file));

    char c = GNOME_CMD_IS_DIR (file) ? 'd' : 'f';

    DEBUG (c, "refing: 0x%p %s to %d\n", file, file->info->name, file->priv->ref_cnt);

    return file;
}


void gnome_cmd_file_unref (GnomeCmdFile *file)
{
    g_return_if_fail (file != NULL);

    file->priv->ref_cnt--;

    char c = GNOME_CMD_IS_DIR (file) ? 'd' : 'f';

    DEBUG (c, "un-refing: 0x%p %s to %d\n", file, file->info->name, file->priv->ref_cnt);
    if (file->priv->ref_cnt < 1)
        gtk_object_destroy (GTK_OBJECT (file));
}


GnomeVFSResult gnome_cmd_file_chmod (GnomeCmdFile *file, GnomeVFSFilePermissions perm)
{
    g_return_val_if_fail (file != NULL, GNOME_VFS_ERROR_CORRUPTED_DATA);
    g_return_val_if_fail (file->info != NULL, GNOME_VFS_ERROR_CORRUPTED_DATA);

    file->info->permissions = perm;
    GnomeVFSURI *uri = gnome_cmd_file_get_uri (file);
    GnomeVFSResult ret = gnome_vfs_set_file_info_uri (uri, file->info, GNOME_VFS_SET_FILE_INFO_PERMISSIONS);
    gnome_vfs_uri_unref (uri);

    if (has_parent_dir (file))
    {
        GnomeCmdDir *dir = get_parent_dir (file);
        gchar *uri_str = gnome_cmd_file_get_uri_str (file);
        gnome_cmd_dir_file_changed (dir, uri_str);
        g_free (uri_str);
    }

    return ret;
}


GnomeVFSResult gnome_cmd_file_chown (GnomeCmdFile *file, uid_t uid, gid_t gid)
{
    g_return_val_if_fail (file != NULL, GNOME_VFS_ERROR_CORRUPTED_DATA);
    g_return_val_if_fail (file->info != NULL, GNOME_VFS_ERROR_CORRUPTED_DATA);

    if (uid != -1)
        file->info->uid = uid;
    file->info->gid = gid;

    GnomeVFSURI *uri = gnome_cmd_file_get_uri (file);
    GnomeVFSResult ret = gnome_vfs_set_file_info_uri (uri, file->info, GNOME_VFS_SET_FILE_INFO_OWNER);
    gnome_vfs_uri_unref (uri);

    if (has_parent_dir (file))
    {
        GnomeCmdDir *dir = get_parent_dir (file);
        gchar *uri_str = gnome_cmd_file_get_uri_str (file);
        gnome_cmd_dir_file_changed (dir, uri_str);
        g_free (uri_str);
    }

    return ret;
}


GnomeVFSResult gnome_cmd_file_rename (GnomeCmdFile *f, const gchar *new_name)
{
    g_return_val_if_fail (f, GNOME_VFS_ERROR_CORRUPTED_DATA);
    g_return_val_if_fail (f->info, GNOME_VFS_ERROR_CORRUPTED_DATA);

    GnomeVFSFileInfo *new_info = gnome_vfs_file_info_new ();
    g_return_val_if_fail (new_info, GNOME_VFS_ERROR_CORRUPTED_DATA);

    new_info->name = const_cast<gchar *> (new_name);

    GnomeVFSURI *uri = gnome_cmd_file_get_uri (f);
    GnomeVFSResult result = gnome_vfs_set_file_info_uri (uri, new_info, GNOME_VFS_SET_FILE_INFO_NAME);
    gnome_vfs_uri_unref (uri);

    if (result==GNOME_VFS_OK)       //  re-read GnomeVFSFileInfo for the new MIME type
    {
        const GnomeVFSFileInfoOptions infoOpts = (GnomeVFSFileInfoOptions) (GNOME_VFS_FILE_INFO_FOLLOW_LINKS|GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
        uri = gnome_cmd_file_get_uri (f, new_name);
        result = gnome_vfs_get_file_info_uri (uri, new_info, infoOpts);
        gnome_vfs_uri_unref (uri);
    }

    if (result==GNOME_VFS_OK && has_parent_dir (f))
    {
        gchar *old_uri_str = gnome_cmd_file_get_uri_str (f);

        gnome_cmd_file_update_info (f, new_info);
        gnome_cmd_dir_file_renamed (get_parent_dir (f), f, old_uri_str);
        if (GNOME_CMD_IS_DIR (f))
            gnome_cmd_dir_update_path (GNOME_CMD_DIR (f));
    }

    return result;
}


gchar *gnome_cmd_file_get_quoted_name (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    return quote_if_needed (file->info->name);
}


gchar *gnome_cmd_file_get_path (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    if (strcmp (f->info->name, G_DIR_SEPARATOR_S) == 0)
        return g_strdup (G_DIR_SEPARATOR_S);

    GnomeCmdPath *path;
    gchar *path_str;

    if (!has_parent_dir (f))
    {
        if (GNOME_CMD_IS_DIR (f))
        {
            path = gnome_cmd_dir_get_path (GNOME_CMD_DIR (f));
            return g_strdup (gnome_cmd_path_get_path (path));
        }
        g_assert ("Non directory file without owning directory");
    }

    path = gnome_cmd_path_get_child (gnome_cmd_dir_get_path (get_parent_dir (f)), f->info->name);
    path_str = g_strdup (gnome_cmd_path_get_path (path));
    gtk_object_destroy (GTK_OBJECT (path));

    return path_str;
}


gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *f)
{
    GnomeVFSURI *uri = gnome_cmd_file_get_uri (f);
    gchar *path = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (uri), NULL);

    gnome_vfs_uri_unref (uri);

    return path;
}


gchar *gnome_cmd_file_get_quoted_real_path (GnomeCmdFile *f)
{
    gchar *path = gnome_cmd_file_get_real_path (f);
    gchar *ret = path ? quote_if_needed (path) : NULL;

    g_free (path);

    return ret;
}


gchar *gnome_cmd_file_get_dirname (GnomeCmdFile *f)
{
    GnomeVFSURI *uri = gnome_cmd_file_get_uri (f);
    gchar *path = gnome_vfs_uri_extract_dirname (uri);

    gnome_vfs_uri_unref (uri);

    return path;
}


gchar *gnome_cmd_file_get_unescaped_dirname (GnomeCmdFile *f)
{
    GnomeVFSURI *uri = gnome_cmd_file_get_uri (f);
    gchar *path = gnome_vfs_uri_extract_dirname (uri);
    gnome_vfs_uri_unref (uri);
    gchar *unescaped_path = gnome_vfs_unescape_string (path, NULL);
    g_free (path);

    return unescaped_path;
}


GnomeVFSURI *gnome_cmd_file_get_uri (GnomeCmdFile *f, const gchar *name)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), NULL);

    if (!has_parent_dir (f))
    {
        if (GNOME_CMD_IS_DIR (f))
        {
            GnomeCmdPath *path = gnome_cmd_dir_get_path (GNOME_CMD_DIR (f));
            GnomeCmdCon *con = gnome_cmd_dir_get_connection (GNOME_CMD_DIR (f));
            return gnome_cmd_con_create_uri (con, path);
        }
        else
            g_assert ("Non directory file without owning directory");
    }

    return gnome_cmd_dir_get_child_uri (get_parent_dir (f), name ? name : f->info->name);
}


gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f, GnomeVFSURIHideOptions hide_options)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), NULL);

    GnomeVFSURI *uri = gnome_cmd_file_get_uri (f);
    gchar *uri_str = gnome_vfs_uri_to_string (uri, hide_options);
    gnome_vfs_uri_unref (uri);

    return uri_str;
}


const gchar *gnome_cmd_file_get_extension (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    if (file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        return NULL;

    const char *s = strrchr (file->info->name, '.');        // does NOT work on UTF-8 strings, should be (MUCH SLOWER):
    // const char *s = g_utf8_strrchr (file->info->name, -1, '.');

    return s ? s+1 : NULL;
}


const gchar *gnome_cmd_file_get_owner (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    if (GNOME_VFS_FILE_INFO_LOCAL (file->info))
        return gcmd_owner.get_name_by_uid(file->info->uid);
    else
    {
        static gchar owner_str[MAX_OWNER_LENGTH];
        g_snprintf (owner_str, MAX_OWNER_LENGTH, "%d", file->info->uid);
        return owner_str;
    }
}


const gchar *gnome_cmd_file_get_group (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    if (GNOME_VFS_FILE_INFO_LOCAL (file->info))
        return gcmd_owner.get_name_by_gid(file->info->gid);
    else
    {
        static gchar group_str[MAX_GROUP_LENGTH];
        g_snprintf (group_str, MAX_GROUP_LENGTH, "%d", file->info->gid);
        return group_str;
    }
}


inline const gchar *date2string (time_t date, gboolean overide_disp_setting)
{
    return time2string (date, overide_disp_setting?"%c":gnome_cmd_data_get_date_format ());
}


const gchar *gnome_cmd_file_get_adate (GnomeCmdFile *file, gboolean overide_disp_setting)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    return date2string (file->info->atime, overide_disp_setting);
}


const gchar *gnome_cmd_file_get_mdate (GnomeCmdFile *file, gboolean overide_disp_setting)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    return date2string (file->info->mtime, overide_disp_setting);
}


const gchar *gnome_cmd_file_get_cdate (GnomeCmdFile *file, gboolean overide_disp_setting)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    return date2string (file->info->ctime, overide_disp_setting);
}


const gchar *gnome_cmd_file_get_size (GnomeCmdFile *file)
{
    static gchar dir_indicator[] = "<DIR> ";

    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    if (file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        return dir_indicator;

    return size2string (file->info->size, gnome_cmd_data.size_disp_mode);
}


GnomeVFSFileSize gnome_cmd_file_get_tree_size (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, 0);

    if (file->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
        return file->info->size;

    if (strcmp (file->info->name, "..") == 0)
        return 0;

    if (file->priv->tree_size != -1)
        return file->priv->tree_size;

    GnomeVFSURI *uri = gnome_cmd_file_get_uri (file);
    file->priv->tree_size = calc_tree_size (uri);
    gnome_vfs_uri_unref (uri);

    return file->priv->tree_size;
}


const gchar *gnome_cmd_file_get_tree_size_as_str (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    if (file->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
        return gnome_cmd_file_get_size (file);

    if (strcmp (file->info->name, "..") == 0)
        return gnome_cmd_file_get_size (file);

    return size2string (gnome_cmd_file_get_tree_size (file), gnome_cmd_data.size_disp_mode);
}


const gchar *gnome_cmd_file_get_perm (GnomeCmdFile *file)
{
    static gchar perm_str[MAX_PERM_LENGTH];

    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    perm2string (file->info->permissions, perm_str, MAX_PERM_LENGTH);
    return perm_str;
}


const gchar *gnome_cmd_file_get_type_string (GnomeCmdFile *f)
{
    static gchar type_str[MAX_TYPE_LENGTH];

    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    type2string (f->info->type, type_str, MAX_TYPE_LENGTH);
    return type_str;
}


const gchar *gnome_cmd_file_get_type_desc (GnomeCmdFile *f)
{
    static const gchar *type_strings[] = {
        N_("Unknown file type"),
        N_("Regular file"),
        N_("Directory"),
        N_("FIFO"),
        N_("UNIX Socket"),
        N_("Character device"),
        N_("Block device"),
        N_("Symbolic link")
    };

    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    if (!f->info->symlink_name)
        return type_strings[f->info->type];

    return type_strings[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK];
}


gboolean gnome_cmd_file_get_type_pixmap_and_mask (GnomeCmdFile *f, GdkPixmap **pixmap, GdkBitmap **mask)
{
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    return IMAGE_get_pixmap_and_mask (f->info->type, f->info->mime_type, f->info->symlink_name != NULL, pixmap, mask);
}


GdkPixmap *gnome_cmd_file_get_type_pixmap (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    GdkPixmap *pm;
    GdkBitmap *mask;

    IMAGE_get_pixmap_and_mask (f->info->type, f->info->mime_type, f->info->symlink_name != NULL, &pm, &mask);

    return pm;
}


GdkBitmap *gnome_cmd_file_get_type_mask (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    GdkPixmap *pm;
    GdkBitmap *mask;

    IMAGE_get_pixmap_and_mask (f->info->type, f->info->mime_type, f->info->symlink_name != NULL, &pm, &mask);

    return mask;
}


const gchar *gnome_cmd_file_get_mime_type (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    return gnome_vfs_file_info_get_mime_type (f->info);
}


const gchar *gnome_cmd_file_get_mime_type_desc (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    return f->info->mime_type ? gnome_vfs_mime_get_description (f->info->mime_type) : NULL;
}


gboolean gnome_cmd_file_has_mime_type (GnomeCmdFile *f, const gchar *mime_type)
{
    g_return_val_if_fail (f != NULL, FALSE);
    g_return_val_if_fail (f->info != NULL, FALSE);
    g_return_val_if_fail (f->info->mime_type != NULL, FALSE);
    g_return_val_if_fail (mime_type != NULL, FALSE);

    return strcmp (f->info->mime_type, mime_type) == 0;
}


gboolean gnome_cmd_file_mime_begins_with (GnomeCmdFile *f, const gchar *mime_type_start)
{
    g_return_val_if_fail (f != NULL, FALSE);
    g_return_val_if_fail (f->info != NULL, FALSE);
    g_return_val_if_fail (f->info->mime_type != NULL, FALSE);
    g_return_val_if_fail (mime_type_start != NULL, FALSE);

    return strncmp (f->info->mime_type, mime_type_start, strlen(mime_type_start)) == 0;
}


void gnome_cmd_file_show_properties (GnomeCmdFile *f)
{
    GtkWidget *dialog = gnome_cmd_file_props_dialog_create (f);
    if (!dialog) return;

    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}


inline void do_view_file (GnomeCmdFile *f, gint internal_viewer=-1)
{
    if (internal_viewer==-1)
        internal_viewer = gnome_cmd_data.use_internal_viewer;

    switch (internal_viewer)
    {
        case TRUE : {
                        GtkWidget *viewer = gviewer_window_file_view (f);
                        gtk_widget_show (viewer);
                        gdk_window_set_icon (viewer->window, NULL,
                                             IMAGE_get_pixmap (PIXMAP_INTERNAL_VIEWER),
                                             IMAGE_get_mask (PIXMAP_INTERNAL_VIEWER));
                    }
                    break;

        case FALSE: {
                        gchar *filename = gnome_cmd_file_get_quoted_real_path (f);
                        gchar *command = g_strdup_printf (gnome_cmd_data.get_viewer(), filename);
                        run_command (command, FALSE);
                        g_free (filename);
                    }
                    break;

        default: break;
    }
}


static void on_file_downloaded_for_view (GnomeVFSURI *uri)
{
    GnomeCmdFile *f = gnome_cmd_file_new_from_uri (uri);

    if (!f)
        return;

    do_view_file (f);
    gnome_cmd_file_unref (f);
}


void gnome_cmd_file_view (GnomeCmdFile *f, gint internal_viewer)
{
    g_return_if_fail (f != NULL);
    g_return_if_fail (has_parent_dir (f));

    // If the file is local there is no need to download it
    if (gnome_cmd_file_is_local (f))
    {
        do_view_file (f, internal_viewer);
        return;
    }

    // The file is remote, let's download it to a temporary file first
    gchar *path_str = get_temp_download_filepath (gnome_cmd_file_get_name (f));
    if (!path_str)  return;

    GnomeCmdPath *path = gnome_cmd_plain_path_new (path_str);
    GnomeVFSURI *src_uri = gnome_cmd_file_get_uri (f);
    GnomeVFSURI *dest_uri = gnome_cmd_con_create_uri (get_home_con (), path);

    g_printerr ("Copying to: %s\n", path_str);
    gtk_object_destroy (GTK_OBJECT (path));
    g_free (path_str);

    gnome_cmd_xfer_tmp_download (src_uri,
                                 dest_uri,
                                 GNOME_VFS_XFER_FOLLOW_LINKS,
                                 GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
                                 GTK_SIGNAL_FUNC (on_file_downloaded_for_view),
                                 dest_uri);
}


void gnome_cmd_file_edit (GnomeCmdFile *f)
{
    g_return_if_fail (f != NULL);

    if (!gnome_cmd_file_is_local (f))
        return;

    gchar *fpath = gnome_cmd_file_get_quoted_real_path (f);
    gchar *dpath = gnome_cmd_file_get_unescaped_dirname (f);
    gchar *command = g_strdup_printf (gnome_cmd_data.get_editor(), fpath);

    run_command_indir (command, dpath, FALSE);

    g_free (command);
    g_free (dpath);
    g_free (fpath);
}


void gnome_cmd_file_show_cap_cut (GnomeCmdFile *f)
{
}


void gnome_cmd_file_show_cap_copy (GnomeCmdFile *f)
{
}


void gnome_cmd_file_show_cap_paste (GnomeCmdFile *f)
{
}


void gnome_cmd_file_update_info (GnomeCmdFile *f, GnomeVFSFileInfo *info)
{
    g_return_if_fail (f != NULL);
    g_return_if_fail (info != NULL);

    g_free (f->collate_key);
    gnome_vfs_file_info_unref (f->info);
    gnome_vfs_file_info_ref (info);
    f->info = info;

    gchar *utf8_name;

    if (!gnome_cmd_data.case_sens_sort)
    {
        gchar *s = get_utf8 (info->name);
        utf8_name = g_utf8_casefold (s, -1);
        g_free (s);
    }
    else
        utf8_name = get_utf8 (info->name);

    f->collate_key = g_utf8_collate_key_for_filename (utf8_name, -1);
    g_free (utf8_name);
}


gboolean gnome_cmd_file_is_local (GnomeCmdFile *f)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), FALSE);

    return gnome_cmd_dir_is_local (get_parent_dir (f));
}


gboolean gnome_cmd_file_is_executable (GnomeCmdFile *f)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), FALSE);

    if (f->info->type != GNOME_VFS_FILE_TYPE_REGULAR)
        return FALSE;

    if (!gnome_cmd_file_is_local (f))
        return FALSE;

    if (gcmd_owner.uid() == f->info->uid
        && f->info->permissions & GNOME_VFS_PERM_USER_EXEC)
        return TRUE;

    if (gcmd_owner.gid() == f->info->gid
        && f->info->permissions & GNOME_VFS_PERM_GROUP_EXEC)
        return TRUE;

    if (f->info->permissions & GNOME_VFS_PERM_OTHER_EXEC)
        return TRUE;

    return FALSE;
}


void gnome_cmd_file_is_deleted (GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));

    if (has_parent_dir (f))
    {
        gchar *uri_str = gnome_cmd_file_get_uri_str (f);
        gnome_cmd_dir_file_deleted (get_parent_dir (f), uri_str);
        g_free (uri_str);
    }
}


void gnome_cmd_file_execute (GnomeCmdFile *f)
{
    gchar *fpath = gnome_cmd_file_get_real_path (f);
    gchar *dpath = g_path_get_dirname (fpath);
    gchar *cmd = g_strdup_printf ("./%s", f->info->name);

    run_command_indir (cmd, dpath, app_needs_terminal (f));

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

GList *gnome_cmd_file_list_copy (GList *files)
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

void gnome_cmd_file_list_free (GList *files)
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

void gnome_cmd_file_list_ref (GList *files)
{
    g_return_if_fail (files != NULL);

    g_list_foreach (files, (GFunc) gnome_cmd_file_ref, NULL);
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

void gnome_cmd_file_list_unref (GList *files)
{
    g_return_if_fail (files != NULL);

    g_list_foreach (files, (GFunc) gnome_cmd_file_unref, NULL);
}


GnomeCmdDir *gnome_cmd_file_get_parent_dir (GnomeCmdFile *f)
{
    return get_parent_dir (f);
}


inline gulong tv2ms (const GTimeVal &t)
{
    return t.tv_sec*1000 + t.tv_usec/1000;
}


gboolean gnome_cmd_file_needs_update (GnomeCmdFile *file)
{
    GTimeVal t;

    g_get_current_time (&t);

    if (tv2ms (t) - tv2ms (file->priv->last_update) > gnome_cmd_data.gui_update_rate)
    {
        file->priv->last_update = t;
        return TRUE;
    }

    return FALSE;
}


void gnome_cmd_file_invalidate_tree_size (GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));

    f->priv->tree_size = -1;
}


gboolean gnome_cmd_file_has_tree_size (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, FALSE);

    return f->priv->tree_size != -1;
}
