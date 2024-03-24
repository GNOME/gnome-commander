/**
 * @file gnome-cmd-file.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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

#include <config.h>
#include <errno.h>

#include "gnome-cmd-includes.h"
#include "utils.h"
#include "gnome-cmd-owner.h"
#include "imageloader.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-xfer.h"
#include "tags/gnome-cmd-tags.h"
#include "intviewer/libgviewer.h"
#include "dialogs/gnome-cmd-file-props-dialog.h"
#include "gnome-cmd-user-actions.h"

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
GList *all_files = nullptr;

struct GnomeCmdFile::Private
{
    Handle *dir_handle;
    GTimeVal last_update;
    gint ref_cnt;
    guint64 tree_size;
};


G_DEFINE_TYPE (GnomeCmdFile, gnome_cmd_file, GNOME_CMD_TYPE_FILE_BASE)


inline gboolean has_parent_dir (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, false);
    g_return_val_if_fail (f->priv != nullptr, false);

    return f->priv->dir_handle && f->priv->dir_handle->ref;
}


inline GnomeCmdDir *get_parent_dir (GnomeCmdFile *f)
{
    g_return_val_if_fail (f->priv->dir_handle != nullptr, nullptr);

    return GNOME_CMD_DIR (f->priv->dir_handle->ref);
}


static void gnome_cmd_file_init (GnomeCmdFile *f)
{
    // f->info = nullptr;
    // f->collate_key = nullptr;

    f->priv = g_new0 (GnomeCmdFile::Private, 1);

    // f->priv->dir_handle = nullptr;

    // f->priv->last_update.tv_sec = 0;
    // f->priv->last_update.tv_usec = 0;
    f->priv->tree_size = -1;

    if (DEBUG_ENABLED ('c'))
    {
        all_files = g_list_append (all_files, f);
        created_files_cnt++;
    }
}


static void gnome_cmd_file_finalize (GObject *object)
{
    GnomeCmdFile *f = GNOME_CMD_FILE (object);

    delete f->metadata;

    if (f->get_file_info() && strcmp(g_file_info_get_display_name(f->get_file_info()), "..") != 0)
        DEBUG ('f', "file destroying %p %s\n", f, g_file_info_get_display_name(f->get_file_info()));

    g_free (f->collate_key);
    g_object_unref(f->get_file_info());
    if (f->priv->dir_handle)
        handle_unref (f->priv->dir_handle);

    if (DEBUG_ENABLED ('c'))
    {
        all_files = g_list_remove (all_files, f);
        deleted_files_cnt++;
    }

    g_free (f->priv);

    G_OBJECT_CLASS (gnome_cmd_file_parent_class)->finalize (object);
}


static void gnome_cmd_file_class_init (GnomeCmdFileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_file_finalize;
}


/***********************************
 * Public functions
 ***********************************/

GnomeCmdFile *gnome_cmd_file_new (const gchar *local_full_path)
{
    g_return_val_if_fail (local_full_path != nullptr, nullptr);

    auto gFile = g_file_new_for_path(local_full_path);
    GnomeCmdFile *f = gnome_cmd_file_new_from_gfile (gFile);

    return f;
}


GnomeCmdFile *gnome_cmd_file_new (GFileInfo *gFileInfo, GnomeCmdDir *dir)
{
    auto gnomeCmdFile = static_cast<GnomeCmdFile*> (g_object_new (GNOME_CMD_TYPE_FILE, nullptr));

    gnome_cmd_file_setup (G_OBJECT(gnomeCmdFile), gFileInfo, dir);

    if(!gnomeCmdFile->get_file())
    {
        g_object_unref(gnomeCmdFile);
        return nullptr;
    }

    return gnomeCmdFile;
}


GnomeCmdFile *gnome_cmd_file_new_from_gfile (GFile *gFile)
{
    g_return_val_if_fail (gFile != nullptr, nullptr);
    g_return_val_if_fail (G_IS_FILE(gFile), nullptr);
    GError *error = nullptr;

    auto gFileInfo = g_file_query_info(gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &error);
    if (!gFileInfo || error)
    {
        g_object_unref (gFileInfo);
        g_critical ("gnome_cmd_file_new_from_gfile error: %s\n", error->message);
        g_error_free (error);
        return nullptr;
    }

    auto gFileParent = g_file_get_parent(gFile);
    if (gFileParent)
    {
        auto gFileParentPath = g_file_get_path(gFileParent);
        GnomeCmdDir *dir = gnome_cmd_dir_new (get_home_con(), new GnomeCmdPlainPath(gFileParentPath));
        g_free(gFileParentPath);
        g_object_unref(gFileParent);
        return gnome_cmd_file_new (gFileInfo, dir);
    }

    auto gFileParentPath = g_file_get_path(gFileParent);
    GnomeCmdDir *dir = gnome_cmd_dir_new (get_home_con(), new GnomeCmdPlainPath(G_DIR_SEPARATOR_S));
    g_free(gFileParentPath);
    g_object_unref(gFileParent);
    return gnome_cmd_file_new (gFileInfo, dir);
}


void GnomeCmdFile::invalidate_metadata()
{
    delete metadata;
    metadata = nullptr;
}


gboolean gnome_cmd_file_setup (GObject *gObject, GFile *gFile, GError **error)
{
    g_return_val_if_fail (gObject != nullptr, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE(gObject), FALSE);
    g_return_val_if_fail (gFile != nullptr, FALSE);

    GError *errorTmp = nullptr;
    auto gnomeCmdFile = (GnomeCmdFile*) gObject;

    GNOME_CMD_FILE_BASE (gnomeCmdFile)->gFileInfo = g_file_query_info(gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &errorTmp);
    if (errorTmp)
    {
        g_propagate_error(error, errorTmp);
        return FALSE;
    }

    auto filename = g_file_info_get_attribute_string (gnomeCmdFile->get_file_info(), G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

    // check if file is '..'
    gnomeCmdFile->is_dotdot = g_file_info_get_attribute_uint32 (gnomeCmdFile->get_file_info(), G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
                              && g_strcmp0(filename, "..") == 0;

    auto utf8Name = gnome_cmd_data.options.case_sens_sort
        ? g_strdup(filename)
        : g_utf8_casefold (filename, -1);
    gnomeCmdFile->collate_key = g_utf8_collate_key_for_filename (utf8Name, -1);
    g_free (utf8Name);

    GNOME_CMD_FILE_BASE (gnomeCmdFile)->gFile = gFile;
    return TRUE;
}


void gnome_cmd_file_setup (GObject *gObject, GFileInfo *gFileInfo, GnomeCmdDir *parentDir)
{
    g_return_if_fail (gObject != nullptr);
    g_return_if_fail (GNOME_CMD_IS_FILE(gObject));
    g_return_if_fail (gFileInfo != nullptr);

    auto gnomeCmdFile = (GnomeCmdFile*) gObject;
    GNOME_CMD_FILE_BASE (gnomeCmdFile)->gFileInfo = gFileInfo;

    auto filename = g_file_info_get_attribute_string (gFileInfo, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

    // check if file is '..'
    gnomeCmdFile->is_dotdot = g_file_info_get_attribute_uint32 (gFileInfo, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
                              && g_strcmp0(filename, "..") == 0;

    auto utf8Name = gnome_cmd_data.options.case_sens_sort
        ? g_strdup(filename)
        : g_utf8_casefold (filename, -1);
    gnomeCmdFile->collate_key = g_utf8_collate_key_for_filename (utf8Name, -1);
    g_free (utf8Name);

    if (parentDir)
    {
        gnomeCmdFile->priv->dir_handle = gnome_cmd_dir_get_handle (parentDir);
        handle_ref (gnomeCmdFile->priv->dir_handle);
    }

    auto pathString = gnomeCmdFile->GetPathStringThroughParent();
    if (pathString)
    {
        auto con = gnome_cmd_dir_get_connection(parentDir ? parentDir : GNOME_CMD_DIR(gObject));
        auto gUri = g_uri_build(G_URI_FLAGS_NONE,
                                con && con->scheme ? con->scheme : "file",
                                nullptr,
                                con ? con->hostname : nullptr,
                                con ? con->port : -1,
                                pathString,
                                nullptr,
                                nullptr);

        auto uriString = g_uri_to_string(gUri);
        auto gFileFinal = g_file_new_for_uri (uriString);
        GNOME_CMD_FILE_BASE (gnomeCmdFile)->gFile = gFileFinal;
        g_free(uriString);
        g_free(pathString);
    }
    // EVERY GnomeCmdFile instance must have a gFile reference
    if (!gnomeCmdFile->get_file())
    {
        g_object_unref(gnomeCmdFile->get_file_info());
        return;
    }
}


GnomeCmdFile *GnomeCmdFile::ref()
{
    priv->ref_cnt++;

    if (priv->ref_cnt == 1)
        g_object_ref (this);

    char c = GNOME_CMD_IS_DIR (this) ? 'd' : 'f';

    DEBUG (c, "refing: %p %s to %d\n", this, g_file_info_get_name(get_file_info()), priv->ref_cnt);

    return this;
}


void GnomeCmdFile::unref()
{
    priv->ref_cnt--;

    char c = GNOME_CMD_IS_DIR (this) ? 'd' : 'f';

    DEBUG (c, "un-refing: %p %s to %d\n", this, g_file_info_get_display_name(get_file_info()), priv->ref_cnt);
    if (priv->ref_cnt < 1)
        g_object_unref (this);
}


gboolean GnomeCmdFile::chmod(guint32 permissions, GError **error)
{
    GError *tmp_error = nullptr;

    auto gFileInfoPerms = g_file_query_info(get_file(),
                                            G_FILE_ATTRIBUTE_UNIX_MODE,
                                            G_FILE_QUERY_INFO_NONE,
                                            nullptr,
                                            &tmp_error);
    if (tmp_error)
    {
        g_propagate_error (error, tmp_error);
        return false;
    }

    tmp_error = nullptr;

    g_file_info_set_attribute_uint32(gFileInfoPerms,
                                     G_FILE_ATTRIBUTE_UNIX_MODE,
                                     permissions);

    g_file_set_attributes_from_info(get_file(),
                                    gFileInfoPerms,
                                    G_FILE_QUERY_INFO_NONE,
                                    nullptr,
                                    &tmp_error);
    if (tmp_error)
    {
        g_object_unref(gFileInfoPerms);
        g_propagate_error (error, tmp_error);
        return false;
    }

    g_object_unref(gFileInfoPerms);

    if (has_parent_dir (this))
    {
        GnomeCmdDir *dir = ::get_parent_dir (this);
        gchar *uri_str = get_uri_str();
        gnome_cmd_dir_file_changed (dir, uri_str);
        g_free (uri_str);
    }
    return true;
}


gboolean GnomeCmdFile::chown(uid_t uid, gid_t gid, GError **error)
{
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    GError *tmp_error = nullptr;

    auto gFileInfoMods = g_file_query_info(get_file(),
                                           G_FILE_ATTRIBUTE_UNIX_UID "," G_FILE_ATTRIBUTE_UNIX_GID,
                                           G_FILE_QUERY_INFO_NONE,
                                           nullptr,
                                           &tmp_error);
    if (tmp_error)
    {
        g_propagate_error (error, tmp_error);
        return false;
    }

    tmp_error = nullptr;

    if (uid != (uid_t) -1)
    {
        g_file_info_set_attribute_uint32(gFileInfoMods,
                                         G_FILE_ATTRIBUTE_UNIX_UID,
                                         uid);
    }

    g_file_info_set_attribute_uint32(gFileInfoMods,
                                     G_FILE_ATTRIBUTE_UNIX_GID,
                                     gid);

    g_file_set_attributes_from_info(get_file(),
                                    gFileInfoMods,
                                    G_FILE_QUERY_INFO_NONE,
                                    nullptr,
                                    &tmp_error);
    if (tmp_error)
    {
        g_object_unref(gFileInfoMods);
        g_propagate_error (error, tmp_error);
        return false;
    }

    g_object_unref(gFileInfoMods);

    if (has_parent_dir (this))
    {
        GnomeCmdDir *dir = ::get_parent_dir (this);
        gchar *uri_str = get_uri_str();
        gnome_cmd_dir_file_changed (dir, uri_str);
        g_free (uri_str);
    }
    return true;
}


gboolean GnomeCmdFile::rename(const gchar *new_name, GError **error)
{
    g_return_val_if_fail (get_file_info(), false);

    gchar *old_uri_str = get_uri_str();

    GError *tmp_error;
    tmp_error = nullptr;

    auto newgFile = g_file_set_display_name (this->get_file(), new_name, nullptr, &tmp_error);

    if (tmp_error || newgFile == nullptr)
    {
        g_message ("rename to \"%s\" failed: %s", new_name, tmp_error->message);
        g_propagate_error (error, tmp_error);
        return FALSE;
    }

    g_object_unref(GNOME_CMD_FILE_BASE (this)->gFile);
    GNOME_CMD_FILE_BASE (this)->gFile = newgFile;

    auto gFileInfoNew = g_file_query_info(newgFile,
                                          "*",
                                          G_FILE_QUERY_INFO_NONE,
                                          nullptr,
                                          &tmp_error);
    if (tmp_error)
    {
        g_propagate_error (error, tmp_error);
        return false;
    }

    if (has_parent_dir (this))
    {
        update_gFileInfo(gFileInfoNew);
        gnome_cmd_dir_file_renamed (::get_parent_dir (this), this, old_uri_str);
        if (GNOME_CMD_IS_DIR (this))
            gnome_cmd_dir_update_path (GNOME_CMD_DIR (this));
    }

    g_free(old_uri_str);

    return true;
}


gchar *GnomeCmdFile::get_quoted_name()
{
    g_return_val_if_fail (get_file_info() != nullptr, nullptr);

    return quote_if_needed (g_file_info_get_display_name(get_file_info()));
}


gchar *GnomeCmdFile::GetPathStringThroughParent()
{
    g_return_val_if_fail (get_file_info() != nullptr, nullptr);

    auto filename = g_file_info_get_name (this->get_file_info());

    if (!filename)
        return nullptr;

    GnomeCmdPath *path;
    gchar *path_str;

    if (!has_parent_dir (this))
    {
        if (GNOME_CMD_IS_DIR (this))
        {
            path = gnome_cmd_dir_get_path (GNOME_CMD_DIR (this));
            return g_strdup (path->get_path());
        }
        g_assert ("Non directory file without owning directory");
    }

    path = gnome_cmd_dir_get_path (::get_parent_dir (this))->get_child(filename);
    if (!path)
    {
        return nullptr;
    }

    path_str = g_strdup (path->get_path());
    delete path;

    return path_str;
}


//ToDo: Try to remove usage of this method.
gchar *GnomeCmdFile::get_real_path()
{
    auto gFileTmp = get_gfile();

    if (!gFileTmp)
        return nullptr;

    gchar *path = g_file_get_path (gFileTmp);
    g_object_unref (gFileTmp);

    return path;
}


gchar *GnomeCmdFile::get_quoted_real_path()
{
    gchar *path = get_real_path();

    if (!path)
        return nullptr;

    gchar *ret = quote_if_needed (path);

    g_free (path);

    return ret;
}


gchar *GnomeCmdFile::get_dirname()
{
    auto gFileTmp = get_gfile();
    auto gFileParent = g_file_get_parent(gFileTmp);
    if (!gFileParent)
    {
        g_object_unref(gFileTmp);
        return nullptr;
    }
    gchar *path = g_file_get_path (gFileParent);
    g_object_unref (gFileTmp);
    g_object_unref (gFileParent);

    return path;
}


GAppInfo *GnomeCmdFile::GetAppInfoForContentType()
{
    auto contentTypeString = GetContentType();
    GAppInfo *appInfo = nullptr;

    if (g_file_has_uri_scheme(this->get_file(), "file"))
        appInfo = g_app_info_get_default_for_type (contentTypeString, false);
    else
        appInfo = g_app_info_get_default_for_type (contentTypeString, true);

    g_free(contentTypeString);

    return appInfo;
}


gboolean GnomeCmdFile::GetGfileAttributeBoolean(const char *attribute)
{
    return get_gfile_attribute_boolean(this->get_file_info(), attribute);
}


gchar *GnomeCmdFile::GetGfileAttributeString(const char *attribute)
{
    return get_gfile_attribute_string(this->get_file_info(), attribute);
}


guint32 GnomeCmdFile::GetGfileAttributeUInt32(const char *attribute)
{
    return get_gfile_attribute_uint32(this->get_file_info(), attribute);
}


guint64 GnomeCmdFile::GetGfileAttributeUInt64(const char *attribute)
{
    return get_gfile_attribute_uint64(this->get_file_info(), attribute);
}


gchar *GnomeCmdFile::GetContentType()
{
    auto contentType = GetGfileAttributeString (G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

    if (!contentType)
        contentType = GetGfileAttributeString(G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);

    return contentType;
}


gchar *GnomeCmdFile::GetDefaultApplicationNameString()
{
    auto contentType = GetContentType();

    if (!contentType)
        return nullptr;

    auto appInfo = g_app_info_get_default_for_type (contentType, false);

    g_free(contentType);
    return g_strdup(g_app_info_get_name (appInfo));
}


gchar *GnomeCmdFile::get_default_application_action_label(GAppInfo *gAppInfo)
{
    gchar *escaped_app_name = get_default_application_name (gAppInfo);
    if (escaped_app_name == nullptr)
    {
        return g_strdup (_("_Open"));
    }

    gchar *retval = g_strdup_printf (_("_Open with “%s”"), escaped_app_name);
    g_free (escaped_app_name);

    return retval;
}


gchar *GnomeCmdFile::get_default_application_name(GAppInfo *gAppInfo)
{
    gchar *escaped_app_name = string_double_underscores (g_app_info_get_name (gAppInfo));

    return escaped_app_name;
}


GFile *GnomeCmdFile::get_gfile(const gchar *name)
{
    if (!has_parent_dir (this))
    {
        if (GNOME_CMD_IS_DIR (this))
        {
            GnomeCmdPath *path = gnome_cmd_dir_get_path (GNOME_CMD_DIR (this));
            GnomeCmdCon *con = gnome_cmd_dir_get_connection (GNOME_CMD_DIR (this));
            return gnome_cmd_con_create_gfile (con, path);
        }
        else
            g_assert ("Non directory file without owning directory");
    }
    if (!get_file())
        return nullptr;

    auto filename = g_file_get_basename(get_file());
    auto childGFile = gnome_cmd_dir_get_child_gfile (::get_parent_dir (this), name ? name : filename);
    g_free(filename);
    return childGFile;
}


gchar *GnomeCmdFile::get_uri_str()
{
    return g_file_get_uri(this->get_file());
}


const gchar *GnomeCmdFile::get_extension()
{
    g_return_val_if_fail (get_file_info() != nullptr, nullptr);

    if (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        return nullptr;

    const char *s = strrchr (g_file_info_get_name(get_file_info()), '.');
    return s ? s+1 : nullptr;
}


const gchar *GnomeCmdFile::get_owner()
{
    g_return_val_if_fail (get_file_info() != nullptr, nullptr);

    const gchar *ownerString = nullptr;

    ownerString = g_file_info_get_attribute_string(get_file_info(), G_FILE_ATTRIBUTE_OWNER_USER);

    if (!ownerString)
    {
        static gchar owner_str[MAX_OWNER_LENGTH];
        g_snprintf (owner_str, MAX_OWNER_LENGTH, "%d",
            g_file_info_get_attribute_uint32(get_file_info(), G_FILE_ATTRIBUTE_UNIX_UID));
        return owner_str;
    }

    return ownerString;
}


const gchar *GnomeCmdFile::get_group()
{
    g_return_val_if_fail (get_file_info() != nullptr, nullptr);

    const gchar *groupString = nullptr;

    groupString = g_file_info_get_attribute_string (get_file_info(), G_FILE_ATTRIBUTE_OWNER_GROUP);

    if (!groupString)
    {
        static gchar owner_str[MAX_OWNER_LENGTH];
        g_snprintf (owner_str, MAX_OWNER_LENGTH, "%d",
            g_file_info_get_attribute_uint32 (get_file_info(), G_FILE_ATTRIBUTE_UNIX_GID));
        return owner_str;
    }

    return groupString;
}


inline const gchar *date2string (GDateTime *date, gboolean overide_disp_setting)
{
    if (!date) return nullptr;

    return time2string (date, overide_disp_setting ? "%c" : gnome_cmd_data.options.date_format);
}


#ifdef GLIB_2_70
const gchar *GnomeCmdFile::get_adate(gboolean overide_disp_setting)
{
    g_return_val_if_fail (get_file_info() != nullptr, nullptr);

    return date2string (g_file_info_get_access_date_time(get_file_info()), overide_disp_setting);
}
#endif

const gchar *GnomeCmdFile::get_mdate(gboolean overide_disp_setting)
{
    g_return_val_if_fail (get_file_info() != nullptr, nullptr);

    return date2string (g_file_info_get_modification_date_time (get_file_info()), overide_disp_setting);
}


const gchar *GnomeCmdFile::get_size()
{
    static gchar dir_indicator[] = "<DIR> ";

    if (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        return dir_indicator;

    auto size = GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_STANDARD_SIZE);
    return size2string (size, gnome_cmd_data.options.size_disp_mode);
}


guint64 GnomeCmdFile::get_tree_size()
{
    if (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY)
        return g_file_info_get_attribute_uint64 (get_file_info(), G_FILE_ATTRIBUTE_STANDARD_SIZE);

    if (is_dotdot)
        return 0;

    priv->tree_size = calc_tree_size (nullptr);

    return priv->tree_size;
}


guint64 GnomeCmdFile::calc_tree_size (gulong *count)
{
    g_return_val_if_fail (this->get_file() != NULL, -1);

    guint64 size = 0;

    if (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
    {
        GError *error;
        error = nullptr;

        g_file_measure_disk_usage (this->get_file(),
                           G_FILE_MEASURE_NONE,
                           nullptr,
                           nullptr,
                           nullptr,
                           &size,
                           nullptr,
                           nullptr,
                           &error);

        if (error)
        {
            g_message ("calc_tree_size: g_file_measure_disk_usage failed: %s", error->message);
            g_error_free (error);
        }
    }
    else
    {
        size += GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_SIZE);
        if (count!=NULL) {
            (*count)++;
        }
    }
    return size;
}


const gchar *GnomeCmdFile::get_tree_size_as_str()
{
    if (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY)
        return get_size();

    if (is_dotdot)
        return get_size();

    return size2string (get_tree_size(), gnome_cmd_data.options.size_disp_mode);
}


const gchar *GnomeCmdFile::get_perm()
{
    static gchar perm_str[MAX_PERM_LENGTH];

    perm2string (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_MODE) & 0xFFF, perm_str, MAX_PERM_LENGTH);
    return perm_str;
}


const gchar *GnomeCmdFile::get_type_string()
{
    static gchar type_str[MAX_TYPE_LENGTH];

    type2string (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE), type_str, MAX_TYPE_LENGTH);
    return type_str;
}


gboolean GnomeCmdFile::get_type_pixmap_and_mask(GdkPixmap **pixmap, GdkBitmap **mask)
{
    g_return_val_if_fail (get_file_info() != nullptr, FALSE);

    return IMAGE_get_pixmap_and_mask (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE),
                                      is_dotdot ? nullptr : g_file_info_get_content_type (get_file_info()),
                                      is_dotdot ? false : g_file_info_get_is_symlink (get_file_info()),
                                      pixmap,
                                      mask);
}


gboolean GnomeCmdFile::has_content_type(const gchar *contentType)
{
    g_return_val_if_fail (contentType != nullptr, FALSE);

    auto actualContentType = GetGfileAttributeString(G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

    if (!actualContentType)
    {
        return false;
    }

    auto compareValue = strcmp (actualContentType, contentType);
    g_free(actualContentType);

    return compareValue == 0;
}


gboolean GnomeCmdFile::content_type_begins_with(const gchar *contentTypeStart)
{
    g_return_val_if_fail (contentTypeStart != nullptr, FALSE);

    auto actualContentType = GetGfileAttributeString(G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

    if (!actualContentType)
    {
        return false;
    }

    auto compareValue = strncmp (actualContentType, contentTypeStart, strlen(contentTypeStart));
    g_free(actualContentType);

    return compareValue == 0;
}


void gnome_cmd_file_show_properties (GnomeCmdFile *f)
{
    GtkWidget *dialog = gnome_cmd_file_props_dialog_create (f);
    if (!dialog) return;

    g_object_ref (dialog);
    gtk_widget_show (dialog);
}


static void view_file_with_internal_viewer (GFile *gFile)
{
    auto gnomeCmdFile = gnome_cmd_file_new_from_gfile (gFile);

    if (!gnomeCmdFile)
        return;

    GtkWidget *viewer = gviewer_window_file_view (gnomeCmdFile);
    gtk_widget_show (viewer);
    gdk_window_set_icon (gtk_widget_get_window (viewer), nullptr,
                         IMAGE_get_pixmap (PIXMAP_INTERNAL_VIEWER),
                         IMAGE_get_mask (PIXMAP_INTERNAL_VIEWER));

    gnomeCmdFile->unref();
}


void gnome_cmd_file_view_internal(GnomeCmdFile *f)
{
    // If the file is local there is no need to download it
    if (f->is_local())
    {
        view_file_with_internal_viewer (f->get_file());
        return;
    }
    else
    {
        // The file is remote, let's download it to a temporary file first
        gchar *path_str = get_temp_download_filepath (f->get_name());
        if (!path_str)  return;

        GnomeCmdPlainPath path(path_str);
        auto srcGFile = f->get_gfile();
        auto destGFile = gnome_cmd_con_create_gfile (get_home_con (), &path);
        auto gFile = gnome_cmd_con_create_gfile (get_home_con (), &path);

        g_printerr ("Copying to: %s\n", path_str);
        g_free (path_str);

        gnome_cmd_tmp_download (g_list_append (nullptr, srcGFile),
                                g_list_append (nullptr, destGFile),
                                G_FILE_COPY_OVERWRITE,
                                G_CALLBACK (view_file_with_internal_viewer),
                                gFile);
    }
}

void gnome_cmd_file_view_external(GnomeCmdFile *f)
{
    string command;
    if (parse_command(&command, gnome_cmd_data.options.viewer) == 0)
    {
        DEBUG ('g', "Edit file command is not valid.\n");
        gnome_cmd_show_message (*main_win, _("No valid command given."));
        return;
    }
    else
    {
        gint     argc;
        gchar  **argv  = nullptr;
        GError  *error = nullptr;
        DEBUG ('g', "Edit file: %s\n", command.c_str());
        g_shell_parse_argv (command.c_str(), &argc, &argv, nullptr);
        if (!g_spawn_async (nullptr, argv, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &error))
        gnome_cmd_error_message (_("Unable to execute command."), error);
        g_strfreev (argv);
    }
}

void gnome_cmd_file_view (GnomeCmdFile *f)
{
    g_return_if_fail (f != nullptr);
    g_return_if_fail (has_parent_dir (f));

    switch (gnome_cmd_data.options.use_internal_viewer)
    {
        case TRUE:
        {
            gnome_cmd_file_view_internal(f);
            break;
        }
        case FALSE:
        {
            gnome_cmd_file_view_external(f);
            break;
        }
        default:
            break;
    }
}


void gnome_cmd_file_edit (GnomeCmdFile *f)
{
    g_return_if_fail (f != nullptr);

    if (!f->is_local())
        return;

    gchar *fpath = f->get_quoted_real_path();
    auto parentDir = g_file_get_parent(f->get_file());
    gchar *dpath = g_file_get_parse_name (parentDir);
    g_object_unref(parentDir);
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    gchar *command = g_strdup_printf (gnome_cmd_data.options.editor, fpath);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    run_command_indir (command, dpath, FALSE);

    g_free (command);
    g_free (dpath);
    g_free (fpath);
}


void GnomeCmdFile::update_gFileInfo(GFileInfo *gFileInfo_new)
{
    g_return_if_fail (gFileInfo_new != nullptr);
    g_return_if_fail (G_IS_FILE_INFO(gFileInfo_new));

    g_free (collate_key);
    g_object_unref (this->parent.gFileInfo);
    g_object_ref (gFileInfo_new);
    this->parent.gFileInfo = gFileInfo_new;

    gchar *filename;

    if (!gnome_cmd_data.options.case_sens_sort)
    {
        auto filenameWithCase = g_file_info_get_display_name(gFileInfo_new);
        filename = g_utf8_casefold (filenameWithCase, -1);
    }
    else
        filename = g_strdup(g_file_info_get_display_name(gFileInfo_new));

    collate_key = g_utf8_collate_key_for_filename (filename, -1);
    g_free(filename);
}


gboolean GnomeCmdFile::is_local()
{
    if (!has_parent_dir (this))
    {
        if (GNOME_CMD_IS_DIR (this))
            return gnome_cmd_dir_is_local (GNOME_CMD_DIR (this));
        g_assert ("Non directory file without owning directory");
    }

    return gnome_cmd_dir_is_local (::get_parent_dir (this));
}


gboolean GnomeCmdFile::is_executable()
{
    if (!is_local())
        return FALSE;

    if (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_REGULAR)
        return FALSE;

    if (GetGfileAttributeBoolean(G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
        return TRUE;

    if (gcmd_owner.gid() == GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_GID)
                            && GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_MODE) & GNOME_CMD_PERM_GROUP_EXEC)
        return TRUE;

    if (GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_MODE) & GNOME_CMD_PERM_OTHER_EXEC)
        return TRUE;

    return FALSE;
}


void GnomeCmdFile::is_deleted()
{
    if (has_parent_dir (this))
    {
        gchar *uri_str = get_uri_str();
        gnome_cmd_dir_file_deleted (::get_parent_dir (this), uri_str);
        g_free (uri_str);
    }
}


void GnomeCmdFile::execute()
{
    gchar *fpath = get_real_path();
    gchar *dpath = g_path_get_dirname (fpath);
    gchar *cmd = g_strdup_printf ("./%s", this->get_quoted_name());

    run_command_indir (cmd, dpath, app_needs_terminal (this));

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
    g_return_val_if_fail (files != nullptr, nullptr);

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


GnomeCmdDir *GnomeCmdFile::get_parent_dir()
{
    return ::get_parent_dir (this);
}


inline gulong tv2ms (const GTimeVal &t)
{
    return t.tv_sec*1000 + t.tv_usec/1000;
}


gboolean GnomeCmdFile::needs_update()
{
    GTimeVal t;

    g_get_current_time (&t);

    if (tv2ms (t) - tv2ms (priv->last_update) > gnome_cmd_data.gui_update_rate)
    {
        priv->last_update = t;
        return TRUE;
    }

    return FALSE;
}


void GnomeCmdFile::invalidate_tree_size()
{
    priv->tree_size = -1;
}


gboolean GnomeCmdFile::has_tree_size()
{
    return priv->tree_size != (guint64)-1;
}
