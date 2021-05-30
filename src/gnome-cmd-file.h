/**
 * @file gnome-cmd-file.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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

#pragma once

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
    GnomeCmdFileBase parent;

    class Private;

    Private *priv;

    GnomeVFSFileInfo *info;
    GFile *gFile;
    GFileInfo *gFileInfo;
    gboolean is_dotdot;
    gchar *collate_key;                 // necessary for proper sorting of UTF-8 encoded file names
    GnomeCmdFileMetadata *metadata;

    GnomeCmdFile *ref();
    void unref();

    void invalidate_metadata();

    const gchar *get_name();
    gchar *get_quoted_name();
    gchar *get_path();
    gchar *get_real_path();
    gchar *get_quoted_real_path();
    gchar *get_dirname();
    gchar *get_unescaped_dirname();

    GnomeVFSURI *get_uri(const gchar *name=NULL);
    gchar *get_uri_str();

    char *get_collation_fname() const    {  return collate_key ? collate_key : info->name;  }

    const gchar *get_extension();
    const gchar *get_owner();
    const gchar *get_group();
#ifdef GLIB_2_70
    const gchar *get_adate(gboolean overide_disp_setting);
#endif
    const gchar *get_mdate(gboolean overide_disp_setting);
    const gchar *get_size();
    guint64 get_tree_size();
    guint64 calc_tree_size (gulong *count);
    const gchar *get_tree_size_as_str();
    const gchar *get_perm();
    gboolean has_mime_type(const gchar *mime_type);
    gboolean mime_begins_with(const gchar *mime_type_start);

    GnomeCmdDir *get_parent_dir();

    const gchar *get_type_string();
    gboolean get_type_pixmap_and_mask(GdkPixmap **pixmap, GdkBitmap **mask);

    void chmod(guint32 permissions);
    gboolean chown(uid_t uid, gid_t gid);
    GnomeVFSResult rename(const gchar *new_name);

    void update_info(GnomeVFSFileInfo *info);
    void update_gFileInfo(GFileInfo *gFileInfo);
    gboolean is_local();
    gboolean is_executable();
    void is_deleted();
    void execute();

    gboolean needs_update();

    void invalidate_tree_size();
    gboolean has_tree_size();

    guint32 GetGfileAttributeUInt32(const char *attribute);
    guint64 GetGfileAttributeUInt64(const char *attribute);
    gchar *GetGfileAttributeString(const char *attribute);
    gchar *get_default_application_name_string();
    gchar *get_default_application_action_label(GAppInfo *gAppInfo);
    gchar *get_default_application_name(GAppInfo *gAppInfo);
    GAppInfo *GetAppInfoForContentType();
    GnomeVFSMimeApplication *get_default_gnome_vfs_app_for_mime_type();
};

struct GnomeCmdFileClass
{
    GnomeCmdFileBaseClass parent_class;
};


inline const gchar *GnomeCmdFile::get_name()
{
    g_return_val_if_fail (gFileInfo != NULL, NULL);
    return g_file_info_get_display_name(gFileInfo);
}

GnomeCmdFile *gnome_cmd_file_new_from_uri (GnomeVFSURI *uri);
GnomeCmdFile *gnome_cmd_file_new (const gchar *local_full_path);
GnomeCmdFile *gnome_cmd_file_new (GnomeVFSFileInfo *info, GnomeCmdDir *dir);
GnomeCmdFile *gnome_cmd_file_new (GFileInfo *gFileInfo, GnomeCmdDir *dir);
void gnome_cmd_file_setup (GnomeCmdFile *gnomeCmdFile, GnomeVFSFileInfo *info, GnomeCmdDir *dir);
void gnome_cmd_file_setup (GnomeCmdFile *gnomeCmdFile, GFileInfo *gFileInfo, GnomeCmdDir *dir);


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

inline const gchar *gnome_cmd_file_get_name (GnomeCmdFile *f)
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

inline gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_uri_str();
}

void gnome_cmd_file_show_properties (GnomeCmdFile *f);
void gnome_cmd_file_show_chown_dialog (GList *files);
void gnome_cmd_file_show_chmod_dialog (GList *files);
void gnome_cmd_file_view (GnomeCmdFile *f, gint internal_viewer);
void gnome_cmd_file_edit (GnomeCmdFile *f);

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
