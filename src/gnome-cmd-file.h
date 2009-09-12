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

#ifndef __GNOME_CMD_FILE_H__
#define __GNOME_CMD_FILE_H__

#define GNOME_CMD_FILE(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_file_get_type (), GnomeCmdFile)
#define GNOME_CMD_FILE_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_file_get_type (), GnomeCmdFileClass)
#define GNOME_CMD_IS_FILE(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_file_get_type ())


struct GnomeCmdFilePrivate;
class GnomeCmdFileMetadata;

struct GnomeCmdFile
{
    GnomeCmdFileInfo parent;

    GnomeVFSFileInfo *info;
    gchar *collate_key;                 // necessary for proper sorting of UTF-8 encoded file names
    GnomeCmdFilePrivate *priv;
    GnomeCmdFileMetadata *metadata;
};

struct GnomeCmdFileClass
{
    GnomeCmdFileInfoClass parent_class;
};


struct GnomeCmdDir;


GtkType gnome_cmd_file_get_type ();

GnomeCmdFile *gnome_cmd_file_new_from_uri (const gchar *local_full_path);
GnomeCmdFile *gnome_cmd_file_new (GnomeVFSFileInfo *info, GnomeCmdDir *dir);
void gnome_cmd_file_setup (GnomeCmdFile *f, GnomeVFSFileInfo *info, GnomeCmdDir *dir);

void gnome_cmd_file_invalidate_metadata (GnomeCmdFile *f);

GnomeCmdFile *gnome_cmd_file_ref (GnomeCmdFile *f);
void gnome_cmd_file_unref (GnomeCmdFile *f);

GnomeVFSResult gnome_cmd_file_chmod (GnomeCmdFile *f, GnomeVFSFilePermissions perm);
GnomeVFSResult gnome_cmd_file_chown (GnomeCmdFile *f, uid_t uid, gid_t gid);
GnomeVFSResult gnome_cmd_file_rename (GnomeCmdFile *f, const gchar *new_name);

inline gchar *gnome_cmd_file_get_name (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    return file->info->name;
}

gchar *gnome_cmd_file_get_quoted_name (GnomeCmdFile *file);

gchar *gnome_cmd_file_get_path (GnomeCmdFile *f);
gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *f);
gchar *gnome_cmd_file_get_quoted_real_path (GnomeCmdFile *f);
gchar *gnome_cmd_file_get_dirname (GnomeCmdFile *f);
gchar *gnome_cmd_file_get_unescaped_dirname (GnomeCmdFile *f);
GnomeVFSURI *gnome_cmd_file_get_uri (GnomeCmdFile *f, const gchar *name=NULL);
gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f, GnomeVFSURIHideOptions hide_options=GNOME_VFS_URI_HIDE_NONE);

inline char *gnome_cmd_file_get_collation_fname (GnomeCmdFile *f)
{
    return f->collate_key ? f->collate_key : f->info->name;
}

const gchar *gnome_cmd_file_get_extension (GnomeCmdFile *f);
const gchar *gnome_cmd_file_get_owner (GnomeCmdFile *f);
const gchar *gnome_cmd_file_get_group (GnomeCmdFile *f);
const gchar *gnome_cmd_file_get_adate (GnomeCmdFile *f, gboolean overide_disp_setting);
const gchar *gnome_cmd_file_get_mdate (GnomeCmdFile *f, gboolean overide_disp_setting);
const gchar *gnome_cmd_file_get_cdate (GnomeCmdFile *f, gboolean overide_disp_setting);
const gchar *gnome_cmd_file_get_size (GnomeCmdFile *f);
GnomeVFSFileSize gnome_cmd_file_get_tree_size (GnomeCmdFile *f);
const gchar *gnome_cmd_file_get_tree_size_as_str (GnomeCmdFile *f);
const gchar *gnome_cmd_file_get_perm (GnomeCmdFile *f);
const gchar *gnome_cmd_file_get_mime_type_desc (GnomeCmdFile *f);
const gchar *gnome_cmd_file_get_mime_type (GnomeCmdFile *f);
gboolean gnome_cmd_file_has_mime_type (GnomeCmdFile *f, const gchar *mime_type);
gboolean gnome_cmd_file_mime_begins_with (GnomeCmdFile *f, const gchar *mime_type_start);

const gchar *gnome_cmd_file_get_type_string (GnomeCmdFile *f);
const gchar *gnome_cmd_file_get_type_desc (GnomeCmdFile *f);
gboolean gnome_cmd_file_get_type_pixmap_and_mask (GnomeCmdFile *f, GdkPixmap **pixmap, GdkBitmap **mask);
GdkPixmap *gnome_cmd_file_get_type_pixmap (GnomeCmdFile *f);
GdkBitmap *gnome_cmd_file_get_type_mask (GnomeCmdFile *f);

void gnome_cmd_file_show_properties (GnomeCmdFile *f);
void gnome_cmd_file_show_chown_dialog (GList *files);
void gnome_cmd_file_show_chmod_dialog (GList *files);
void gnome_cmd_file_view (GnomeCmdFile *f, gint internal_viewer);
void gnome_cmd_file_edit (GnomeCmdFile *f);
void gnome_cmd_file_show_cap_cut (GnomeCmdFile *f);
void gnome_cmd_file_show_cap_copy (GnomeCmdFile *f);
void gnome_cmd_file_show_cap_paste (GnomeCmdFile *f);

// FIXME: These names suck when we have a class called GnomeCmdFileList...
GList *gnome_cmd_file_list_copy (GList *files);
void gnome_cmd_file_list_free (GList *files);
void gnome_cmd_file_list_ref (GList *files);
void gnome_cmd_file_list_unref (GList *files);

GnomeCmdDir *gnome_cmd_file_get_parent_dir (GnomeCmdFile *f);

void gnome_cmd_file_update_info (GnomeCmdFile *f, GnomeVFSFileInfo *info);
gboolean gnome_cmd_file_is_local (GnomeCmdFile *f);
gboolean gnome_cmd_file_is_executable (GnomeCmdFile *f);
void gnome_cmd_file_is_deleted (GnomeCmdFile *f);
void gnome_cmd_file_execute (GnomeCmdFile *f);

gboolean gnome_cmd_file_needs_update (GnomeCmdFile *f);

//misc tree size functions
void gnome_cmd_file_invalidate_tree_size (GnomeCmdFile *f);
gboolean gnome_cmd_file_has_tree_size (GnomeCmdFile *f);

#endif // __GNOME_CMD_FILE_H__
