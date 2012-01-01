/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak

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

#define GNOME_CMD_TYPE_FILE              (gnome_cmd_file_get_type ())
#define GNOME_CMD_FILE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE, GnomeCmdFile))
#define GNOME_CMD_FILE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE, GnomeCmdFileClass))
#define GNOME_CMD_IS_FILE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE))
#define GNOME_CMD_IS_FILE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE))
#define GNOME_CMD_FILE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE, GnomeCmdFileClass))


GType gnome_cmd_file_get_type ();


class GnomeCmdFileMetadata;

struct GnomeCmdDir;


struct GnomeCmdFile
{
    GnomeCmdFileInfo parent;

    class Private;

    Private *priv;

    GnomeVFSFileInfo *info;
    gboolean is_dotdot;
    gchar *collate_key;                 // necessary for proper sorting of UTF-8 encoded file names
    GnomeCmdFileMetadata *metadata;

    GnomeCmdFile *ref();
    void unref();

    void invalidate_metadata();

    gchar *get_name();
    gchar *get_quoted_name();
    gchar *get_path();
    gchar *get_real_path();
    gchar *get_quoted_real_path();
    gchar *get_dirname();
    gchar *get_unescaped_dirname();
    GnomeVFSURI *get_uri(const gchar *name=NULL);
    gchar *get_uri_str(GnomeVFSURIHideOptions hide_options=GNOME_VFS_URI_HIDE_NONE);

    char *get_collation_fname() const    {  return collate_key ? collate_key : info->name;  }

    const gchar *get_extension();
    const gchar *get_owner();
    const gchar *get_group();
    const gchar *get_adate(gboolean overide_disp_setting);
    const gchar *get_mdate(gboolean overide_disp_setting);
    const gchar *get_cdate(gboolean overide_disp_setting);
    const gchar *get_size();
    GnomeVFSFileSize get_tree_size();
    const gchar *get_tree_size_as_str();
    const gchar *get_perm();
    const gchar *get_mime_type();
    const gchar *get_mime_type_desc();
    gboolean has_mime_type(const gchar *mime_type);
    gboolean mime_begins_with(const gchar *mime_type_start);

    GnomeCmdDir *get_parent_dir();

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


inline gchar *GnomeCmdFile::get_name()
{
    g_return_val_if_fail (info != NULL, NULL);
    return info->name;
}

GnomeCmdFile *gnome_cmd_file_new_from_uri (GnomeVFSURI *uri);
GnomeCmdFile *gnome_cmd_file_new (const gchar *local_full_path);
GnomeCmdFile *gnome_cmd_file_new (GnomeVFSFileInfo *info, GnomeCmdDir *dir);
void gnome_cmd_file_setup (GnomeCmdFile *f, GnomeVFSFileInfo *info, GnomeCmdDir *dir);

inline GnomeCmdFile *gnome_cmd_file_ref (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->ref();
}

inline void gnome_cmd_file_unref (GnomeCmdFile *f)
{
    g_return_if_fail (f != NULL);
    f->unref();
}

inline gchar *gnome_cmd_file_get_name (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_name();
}

inline gchar *gnome_cmd_file_get_quoted_name (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_quoted_name();
}

inline gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_real_path();
}

inline gchar *gnome_cmd_file_get_quoted_real_path (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_quoted_real_path();
}

inline gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f, GnomeVFSURIHideOptions hide_options=GNOME_VFS_URI_HIDE_NONE)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_uri_str(hide_options);
}

void gnome_cmd_file_show_properties (GnomeCmdFile *f);
void gnome_cmd_file_show_chown_dialog (GList *files);
void gnome_cmd_file_show_chmod_dialog (GList *files);
void gnome_cmd_file_view (GnomeCmdFile *f, gint internal_viewer);
void gnome_cmd_file_edit (GnomeCmdFile *f);
void gnome_cmd_file_show_cap_cut (GnomeCmdFile *f);
void gnome_cmd_file_show_cap_copy (GnomeCmdFile *f);
void gnome_cmd_file_show_cap_paste (GnomeCmdFile *f);

GList *gnome_cmd_file_list_copy (GList *files);
void gnome_cmd_file_list_free (GList *files);

inline void gnome_cmd_file_list_ref (GList *files)
{
    g_list_foreach (files, (GFunc) gnome_cmd_file_ref, NULL);
}

inline void gnome_cmd_file_list_unref (GList *files)
{
    g_list_foreach (files, (GFunc) gnome_cmd_file_unref, NULL);
}

inline const gchar *GnomeCmdFile::get_mime_type()
{
    g_return_val_if_fail (info != NULL, NULL);
    return gnome_vfs_file_info_get_mime_type (info);
}

inline const gchar *GnomeCmdFile::get_mime_type_desc()
{
    g_return_val_if_fail (info != NULL, NULL);
    return info->mime_type ? gnome_vfs_mime_get_description (info->mime_type) : NULL;
}

inline GnomeVFSMimeApplication *GnomeCmdFile::get_default_application()
{
    return info->mime_type ? gnome_vfs_mime_get_default_application (info->mime_type) : NULL;
}

#endif // __GNOME_CMD_FILE_H__
