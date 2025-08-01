/**
 * @file gnome-cmd-file.cc
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

#include <config.h>
#include <errno.h>

#include "gnome-cmd-includes.h"
#include "utils.h"
#include "gnome-cmd-path.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-xfer.h"
#include "intviewer/libgviewer.h"

using namespace std;


#define MAX_NAME_LENGTH 128
#define MAX_OWNER_LENGTH 128
#define MAX_GROUP_LENGTH 128
#define MAX_PERM_LENGTH 10
#define MAX_DATE_LENGTH 64
#define MAX_SIZE_LENGTH 32

struct GnomeCmdFilePrivate
{
    GFile *file;
    GFileInfo *file_info;
    gboolean is_dotdot;
    GnomeCmdDir *parent_dir;
    GTimeVal last_update;
};


static void gnome_cmd_file_descriptor_init (GnomeCmdFileDescriptorInterface *iface);
static GFile *gnome_cmd_file_get_file (GnomeCmdFileDescriptor *fd);
static GFileInfo *gnome_cmd_file_get_file_info (GnomeCmdFileDescriptor *fd);
static GnomeCmdCon *file_get_connection (GnomeCmdFile *f);


G_DEFINE_TYPE_WITH_CODE (GnomeCmdFile, gnome_cmd_file, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (GNOME_CMD_TYPE_FILE_DESCRIPTOR, gnome_cmd_file_descriptor_init)
                        G_ADD_PRIVATE (GnomeCmdFile))

static GnomeCmdFilePrivate *file_private(GnomeCmdFile *file)
{
    return (GnomeCmdFilePrivate *) gnome_cmd_file_get_instance_private (file);
}


static void gnome_cmd_file_descriptor_init (GnomeCmdFileDescriptorInterface *iface)
{
    iface->get_file = gnome_cmd_file_get_file;
    iface->get_file_info = gnome_cmd_file_get_file_info;
}


inline gboolean has_parent_dir (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, false);
    auto priv = file_private (f);

    return priv->parent_dir != nullptr;
}


inline GnomeCmdDir *get_parent_dir (GnomeCmdFile *f)
{
    auto priv = file_private (f);
    return priv->parent_dir;
}


static void gnome_cmd_file_init (GnomeCmdFile *f)
{
}


static void gnome_cmd_file_dispose (GObject *object)
{
    GnomeCmdFile *f = GNOME_CMD_FILE (object);
    auto priv = file_private (f);

    g_clear_object (&priv->parent_dir);

    G_OBJECT_CLASS (gnome_cmd_file_parent_class)->dispose (object);
}


static void gnome_cmd_file_finalize (GObject *object)
{
    GnomeCmdFile *f = GNOME_CMD_FILE (object);

    if (f->get_file_info() && strcmp(g_file_info_get_display_name(f->get_file_info()), "..") != 0)
        DEBUG ('f', "file destroying %p %s\n", f, g_file_info_get_display_name(f->get_file_info()));

    G_OBJECT_CLASS (gnome_cmd_file_parent_class)->finalize (object);
}


static void gnome_cmd_file_class_init (GnomeCmdFileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = gnome_cmd_file_dispose;
    object_class->finalize = gnome_cmd_file_finalize;

    klass->get_connection = file_get_connection;
}


/***********************************
 * Public functions
 ***********************************/

GnomeCmdFile *gnome_cmd_file_new_full (GFileInfo *gFileInfo, GFile *gFile, GnomeCmdDir *dir)
{
    g_return_val_if_fail (gFileInfo != nullptr, nullptr);
    g_return_val_if_fail (gFile != nullptr, nullptr);
    g_return_val_if_fail (dir != nullptr, nullptr);

    auto gnomeCmdFile = static_cast<GnomeCmdFile*> (g_object_new (GNOME_CMD_TYPE_FILE, nullptr));
    auto priv = file_private (gnomeCmdFile);

    priv->file_info = gFileInfo;
    priv->file = gFile;
    priv->parent_dir = g_object_ref (dir);

    auto filename = g_file_info_get_attribute_string (gFileInfo, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

    // check if file is '..'
    priv->is_dotdot = g_file_info_get_attribute_uint32 (gFileInfo, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
                              && g_strcmp0(filename, "..") == 0;

    return gnomeCmdFile;
}


GnomeCmdFile *gnome_cmd_file_new (GFileInfo *gFileInfo, GnomeCmdDir *dir)
{
    g_return_val_if_fail (gFileInfo != nullptr, nullptr);
    g_return_val_if_fail (dir != nullptr, nullptr);

    GFile *gFile = g_file_get_child (
        gnome_cmd_file_descriptor_get_file (GNOME_CMD_FILE_DESCRIPTOR (dir)),
        g_file_info_get_name (gFileInfo));

    g_return_val_if_fail (gFile != nullptr, nullptr);

    return gnome_cmd_file_new_full (gFileInfo, gFile, dir);
}


GnomeCmdFile *gnome_cmd_file_new_from_path (const gchar *local_full_path)
{
    g_return_val_if_fail (local_full_path != nullptr, nullptr);

    auto gFile = g_file_new_for_path(local_full_path);

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

    GnomeCmdCon *con = get_home_con ();
    GnomeCmdDir *dir;

    auto gFileParent = g_file_get_parent(gFile);
    if (gFileParent)
    {
        auto gFileParentPath = g_file_get_path(gFileParent);
        dir = gnome_cmd_dir_new (con, gnome_cmd_plain_path_new (gFileParentPath));
        g_free(gFileParentPath);
        g_object_unref(gFileParent);
    }
    else
    {
        dir = gnome_cmd_dir_new (con, gnome_cmd_plain_path_new (G_DIR_SEPARATOR_S));
    }

    return gnome_cmd_file_new_full (gFileInfo, gFile, dir);
}


gboolean gnome_cmd_file_setup (GObject *gObject, GFile *gFile, GError **error)
{
    g_return_val_if_fail (gObject != nullptr, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE(gObject), FALSE);
    g_return_val_if_fail (gFile != nullptr, FALSE);

    GError *errorTmp = nullptr;
    auto gnomeCmdFile = (GnomeCmdFile*) gObject;
    auto priv = file_private (gnomeCmdFile);

    priv->file_info = g_file_query_info(gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &errorTmp);
    if (errorTmp)
    {
        g_propagate_error(error, errorTmp);
        return FALSE;
    }

    auto filename = g_file_info_get_attribute_string (gnomeCmdFile->get_file_info(), G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

    // check if file is '..'
    priv->is_dotdot = g_file_info_get_attribute_uint32 (gnomeCmdFile->get_file_info(), G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
                              && g_strcmp0(filename, "..") == 0;

    priv->file = gFile;
    return TRUE;
}


GnomeCmdFile *GnomeCmdFile::ref()
{
    g_object_ref (this);
    return this;
}


void GnomeCmdFile::unref()
{
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
    auto priv = file_private (this);
    g_return_val_if_fail (priv->file_info, false);

    gchar *old_uri_str = get_uri_str();

    GError *tmp_error;
    tmp_error = nullptr;

    auto newgFile = g_file_set_display_name (priv->file, new_name, nullptr, &tmp_error);

    if (tmp_error || newgFile == nullptr)
    {
        g_message ("rename to \"%s\" failed: %s", new_name, tmp_error->message);
        g_propagate_error (error, tmp_error);
        return FALSE;
    }

    g_set_object (&priv->file, newgFile);

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


GnomeCmdPath *GnomeCmdFile::GetPathThroughParent()
{
    g_return_val_if_fail (get_file_info() != nullptr, nullptr);

    auto filename = g_file_info_get_name (this->get_file_info());

    if (!filename)
        return nullptr;

    if (!has_parent_dir (this))
    {
        if (GNOME_CMD_IS_DIR (this))
        {
            return gnome_cmd_path_clone (gnome_cmd_dir_get_path (GNOME_CMD_DIR (this)));
        }
        g_assert ("Non directory file without owning directory");
    }

    return gnome_cmd_path_get_child (gnome_cmd_dir_get_path (::get_parent_dir (this)), filename);
}


gchar *GnomeCmdFile::GetPathStringThroughParent()
{
    GnomeCmdPath *path = GetPathThroughParent();

    if (!path)
        return nullptr;

    gchar *path_str = gnome_cmd_path_get_path (path);
    gnome_cmd_path_free (path);

    return path_str;
}


//ToDo: Try to remove usage of this method.
gchar *GnomeCmdFile::get_real_path()
{
    auto gFileTmp = get_file();

    if (!gFileTmp)
        return nullptr;

    gchar *path = g_file_get_path (gFileTmp);

    return path;
}


GnomeCmdPath *gnome_cmd_file_get_path_through_parent (GnomeCmdFile *f)
{
    return f->GetPathThroughParent();
}


gchar *gnome_cmd_file_get_path_string_through_parent (GnomeCmdFile *f)
{
    return f->GetPathStringThroughParent();
}


gchar *GnomeCmdFile::get_dirname()
{
    auto gFileTmp = get_file();
    auto gFileParent = g_file_get_parent(gFileTmp);
    if (!gFileParent)
        return nullptr;

    gchar *path = g_file_get_path (gFileParent);
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


gchar *GnomeCmdFile::get_uri_str()
{
    return g_file_get_uri(this->get_file());
}


void GnomeCmdFile::update_gFileInfo(GFileInfo *gFileInfo_new)
{
    g_return_if_fail (gFileInfo_new != nullptr);
    g_return_if_fail (G_IS_FILE_INFO(gFileInfo_new));
    auto priv = file_private (this);

    g_set_object (&priv->file_info, gFileInfo_new);
}


void GnomeCmdFile::set_deleted()
{
    if (has_parent_dir (this))
    {
        gchar *uri_str = get_uri_str();
        gnome_cmd_dir_file_deleted (::get_parent_dir (this), uri_str);
        g_free (uri_str);
    }
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
    auto priv = file_private (this);

    GTimeVal t;

    g_get_current_time (&t);

    if (tv2ms (t) - tv2ms (priv->last_update) > gui_update_rate())
    {
        priv->last_update = t;
        return TRUE;
    }

    return FALSE;
}

GFile *gnome_cmd_file_get_file (GnomeCmdFileDescriptor *fd)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (fd), NULL);
    auto priv = file_private (GNOME_CMD_FILE (fd));
    return priv->file;
}

GFileInfo *gnome_cmd_file_get_file_info (GnomeCmdFileDescriptor *fd)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (fd), NULL);
    auto priv = file_private (GNOME_CMD_FILE (fd));
    return priv->file_info;
}

const gchar *gnome_cmd_file_get_name (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_name();
}

gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_real_path();
}

gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    return f->get_uri_str();
}

GnomeCmdCon *file_get_connection (GnomeCmdFile *f)
{
    return gnome_cmd_file_get_connection (GNOME_CMD_FILE (f->get_parent_dir()));
}

GnomeCmdCon *gnome_cmd_file_get_connection (GnomeCmdFile *f)
{
    GnomeCmdFileClass *klass = GNOME_CMD_FILE_GET_CLASS (f);
    return klass->get_connection (f);
}

gboolean gnome_cmd_file_is_local (GnomeCmdFile *f)
{
    GnomeCmdCon *con = gnome_cmd_file_get_connection (f);
    return gnome_cmd_con_is_local (con);
}

gboolean gnome_cmd_file_rename(GnomeCmdFile *f, const gchar *new_name, GError **error)
{
    return f->rename(new_name, error);
}

gboolean gnome_cmd_file_chown(GnomeCmdFile *f, uid_t uid, gid_t gid, GError **error)
{
    return f->chown(uid, gid, error);
}

gboolean gnome_cmd_file_chmod(GnomeCmdFile *f, guint32 permissions, GError **error)
{
    return f->chmod(permissions, error);
}

gchar *gnome_cmd_file_get_free_space (GnomeCmdFile *f)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), NULL);

    GError *error = nullptr;
    auto gFileInfo = g_file_query_filesystem_info (f->get_file(),
                              G_FILE_ATTRIBUTE_FILESYSTEM_FREE,
                              nullptr,
                              &error);
    if (error)
    {
        g_warning("Could not g_file_query_filesystem_info %s: %s\n",
            g_file_peek_path(f->get_file()), error->message);
        g_error_free(error);
        return nullptr;
    }

    auto freeSpace = g_file_info_get_attribute_uint64(gFileInfo, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

    g_object_unref(gFileInfo);

    return g_format_size (freeSpace);
}

gboolean gnome_cmd_file_is_dotdot(GnomeCmdFile *f)
{
    auto priv = file_private (f);
    return priv->is_dotdot;
}

void gnome_cmd_file_set_deleted(GnomeCmdFile *f)
{
    f->set_deleted();
}

GnomeCmdFile *gnome_cmd_file_new_dotdot (GnomeCmdDir *dir)
{
    auto info = g_file_info_new ();

    g_file_info_set_name(info, "..");
    g_file_info_set_display_name(info, "..");
    g_file_info_set_file_type(info, G_FILE_TYPE_DIRECTORY);
    g_file_info_set_size(info, 0);

    auto gnomeCmdFile = gnome_cmd_file_new (info, dir);
    auto priv = file_private (gnomeCmdFile);
    priv->is_dotdot = true;
    return gnomeCmdFile;
}

GnomeCmdDir *gnome_cmd_file_get_parent_dir(GnomeCmdFile *f)
{
    return f->get_parent_dir();
}

gboolean gnome_cmd_file_needs_update(GnomeCmdFile *f)
{
    return f->needs_update();
}
