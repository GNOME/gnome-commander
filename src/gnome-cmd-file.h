/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak

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


class GnomeCmdFileMetadata;

struct GnomeCmdFile
{
    GnomeCmdFileInfo parent;

    class Private;

    Private *priv;

    GnomeVFSFileInfo *info;
    gboolean is_dotdot;
    gchar *collate_key;                 // necessary for proper sorting of UTF-8 encoded file names
    GnomeCmdFileMetadata *metadata;

    void invalidate_metadata();

    gchar *get_path();
    gchar *get_dirname();
    gchar *get_unescaped_dirname();
    GnomeVFSURI *get_uri(const gchar *name=NULL);

    char *get_collation_fname() const    {  return collate_key ? collate_key : info->name;  }

    const gchar *get_type_string();
    const gchar *get_type_desc();
    gboolean get_type_pixmap_and_mask(GdkPixmap **pixmap, GdkBitmap **mask);

    GnomeVFSResult chmod(GnomeVFSFilePermissions perm);
    GnomeVFSResult chown(uid_t uid, gid_t gid);
    GnomeVFSResult rename(const gchar *new_name);

    void update_info(GnomeVFSFileInfo *info);
    gboolean is_local();
    gboolean is_executable();
    void is_deleted();
    void execute();

    gboolean needs_update();

    void invalidate_tree_size();
    gboolean has_tree_size();

    GnomeVFSMimeApplication *get_default_application();
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

GnomeCmdFile *gnome_cmd_file_ref (GnomeCmdFile *f);
void gnome_cmd_file_unref (GnomeCmdFile *f);

inline gchar *gnome_cmd_file_get_name (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    return f->info->name;
}

gchar *gnome_cmd_file_get_quoted_name (GnomeCmdFile *f);
gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *f);
gchar *gnome_cmd_file_get_quoted_real_path (GnomeCmdFile *f);
gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f, GnomeVFSURIHideOptions hide_options=GNOME_VFS_URI_HIDE_NONE);

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

inline GnomeVFSMimeApplication *GnomeCmdFile::get_default_application()
{
    return info->mime_type ? gnome_vfs_mime_get_default_application (info->mime_type) : NULL;
}

#endif // __GNOME_CMD_FILE_H__
