/**
 * @file gnome-cmd-file.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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


extern "C" GType gnome_cmd_file_get_type ();


struct GnomeCmdDir;
struct GnomeCmdCon;
struct GnomeCmdPath;


struct GnomeCmdFile
{
    GObject parent;

    struct Private;

    Private *priv;

    GFile *get_file() { return gnome_cmd_file_descriptor_get_file (GNOME_CMD_FILE_DESCRIPTOR (this)); }
    GFileInfo *get_file_info() { return gnome_cmd_file_descriptor_get_file_info (GNOME_CMD_FILE_DESCRIPTOR (this)); }
    gboolean is_dotdot;

    GnomeCmdFile *ref();
    void unref();

    const gchar *get_name();
    gchar *get_quoted_name();
    GnomeCmdPath *GetPathThroughParent();
    gchar *GetPathStringThroughParent();
    gchar *get_real_path();
    gchar *get_quoted_real_path();
    gchar *get_dirname();
    gchar *get_unescaped_dirname();

    gchar *get_uri_str();

    const gchar *get_extension();
    const gchar *get_owner();
    const gchar *get_group();
    const gchar *get_adate(gboolean overide_disp_setting);
    const gchar *get_mdate(gboolean overide_disp_setting);
    const gchar *get_size();
    guint64 calc_tree_size (gulong *count);
    const gchar *get_tree_size_as_str();
    const gchar *get_perm();
    gboolean has_content_type(const gchar *contentType);
    gboolean content_type_begins_with(const gchar *contentTypeStart);

    GnomeCmdDir *get_parent_dir();

    const gchar *get_type_string();
    GIcon *get_type_icon();

    gboolean chmod(guint32 permissions, GError **error);
    gboolean chown(uid_t uid, gid_t gid, GError **error);
    gboolean rename(const gchar *new_name, GError **error);

    void update_gFileInfo(GFileInfo *gFileInfo);
    void set_deleted();

    gboolean needs_update();

    void invalidate_tree_size();

    gboolean GetGfileAttributeBoolean(const char *attribute);
    guint32 GetGfileAttributeUInt32(const char *attribute);
    guint64 GetGfileAttributeUInt64(const char *attribute);
    gchar *GetGfileAttributeString(const char *attribute);
    gchar *GetDefaultApplicationNameString();
    gchar *GetContentType();
    gchar *get_default_application_action_label(GAppInfo *gAppInfo);
    gchar *get_default_application_name(GAppInfo *gAppInfo);
    GAppInfo *GetAppInfoForContentType();
};

struct GnomeCmdFileClass
{
    GObjectClass parent_class;

    /* virtual functions */
    GnomeCmdCon *(* get_connection) (GnomeCmdFile *f);
};


inline const gchar *GnomeCmdFile::get_name()
{
    g_return_val_if_fail (get_file_info () != NULL, NULL);
    return g_file_info_get_display_name (get_file_info ());
}

extern "C" GnomeCmdFile *gnome_cmd_file_new_from_path (const gchar *local_full_path);
extern "C" GnomeCmdFile *gnome_cmd_file_new (GFileInfo *gFileInfo, GnomeCmdDir *dir);
extern "C" GnomeCmdFile *gnome_cmd_file_new_full (GFileInfo *gFileInfo, GFile *gFile, GnomeCmdDir *dir);
gboolean gnome_cmd_file_setup (GObject *gObject, GFile *gFile, GError **error);

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

extern "C" const gchar *gnome_cmd_file_get_name (GnomeCmdFile *f);

extern "C" gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *f);

extern "C" GnomeCmdPath *gnome_cmd_file_get_path_through_parent (GnomeCmdFile *f);
extern "C" gchar *gnome_cmd_file_get_path_string_through_parent (GnomeCmdFile *f);

extern "C" gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f);
extern "C" gboolean gnome_cmd_file_is_local (GnomeCmdFile *f);
extern "C" GnomeCmdCon *gnome_cmd_file_get_connection (GnomeCmdFile *f);

extern "C" gchar *gnome_cmd_file_get_free_space (GnomeCmdFile *f);

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
extern "C" gboolean gnome_cmd_file_chown(GnomeCmdFile *f, uid_t uid, gid_t gid, GError **error);
extern "C" gboolean gnome_cmd_file_chmod(GnomeCmdFile *f, guint32 permissions, GError **error);

extern "C" gboolean gnome_cmd_file_is_dotdot(GnomeCmdFile *f);

extern "C" void gnome_cmd_file_set_deleted(GnomeCmdFile *f);
