/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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

#include <config.h>


#define GNOME_CMD_FILE(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_file_get_type (), GnomeCmdFile)
#define GNOME_CMD_FILE_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_file_get_type (), GnomeCmdFileClass)
#define GNOME_CMD_IS_FILE(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_file_get_type ())


typedef struct _GnomeCmdFile        GnomeCmdFile;
typedef struct _GnomeCmdFileClass   GnomeCmdFileClass;
typedef struct _GnomeCmdFilePrivate GnomeCmdFilePrivate;
typedef struct _GnomeCmdFileMetadata GnomeCmdFileMetadata;

struct _GnomeCmdFileMetadata
{
    gboolean accessed;
    gpointer metadata;
};

struct GnomeCmdFileMetadata_New;

struct _GnomeCmdFile
{
    GnomeCmdFileInfo parent;

    GnomeVFSFileInfo *info;
    GnomeCmdFilePrivate *priv;
    GnomeCmdFileMetadata_New *metadata;
#ifdef HAVE_EXIF
    GnomeCmdFileMetadata exif;
#endif
#ifdef HAVE_IPTC
    GnomeCmdFileMetadata iptc;
#endif
#ifdef HAVE_LCMS
    GnomeCmdFileMetadata icc;
#endif
#ifdef HAVE_ID3
    GnomeCmdFileMetadata id3;
#endif
#ifdef HAVE_GSF
    GnomeCmdFileMetadata gsf;
#endif
};

struct _GnomeCmdFileClass
{
    GnomeCmdFileInfoClass parent_class;
};

#include "gnome-cmd-dir.h"

G_BEGIN_DECLS

GtkType gnome_cmd_file_get_type (void);

GnomeCmdFile *gnome_cmd_file_new (GnomeVFSFileInfo *info, GnomeCmdDir *dir);
void gnome_cmd_file_setup (GnomeCmdFile *finfo, GnomeVFSFileInfo *info, GnomeCmdDir *dir);

void gnome_cmd_file_ref (GnomeCmdFile *finfo);
void gnome_cmd_file_unref (GnomeCmdFile *finfo);

GnomeVFSResult gnome_cmd_file_chmod (GnomeCmdFile *finfo, GnomeVFSFilePermissions perm);
GnomeVFSResult gnome_cmd_file_chown (GnomeCmdFile *finfo, uid_t uid, gid_t gid);
GnomeVFSResult gnome_cmd_file_rename (GnomeCmdFile *finfo, const gchar *new_name);

inline gchar *gnome_cmd_file_get_name (GnomeCmdFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);
    g_return_val_if_fail (file->info != NULL, NULL);

    return file->info->name;
}

gchar *gnome_cmd_file_get_quoted_name (GnomeCmdFile *file);

gchar *gnome_cmd_file_get_path (GnomeCmdFile *finfo);
gchar *gnome_cmd_file_get_real_path (GnomeCmdFile *finfo);
gchar *gnome_cmd_file_get_quoted_real_path (GnomeCmdFile *finfo);
GnomeVFSURI *gnome_cmd_file_get_uri (GnomeCmdFile *finfo);
gchar *gnome_cmd_file_get_uri_str (GnomeCmdFile *finfo);

const gchar *gnome_cmd_file_get_extension (GnomeCmdFile *finfo);
const gchar *gnome_cmd_file_get_owner (GnomeCmdFile *finfo);
const gchar *gnome_cmd_file_get_group (GnomeCmdFile *finfo);
const gchar *gnome_cmd_file_get_adate (GnomeCmdFile *finfo, gboolean overide_disp_setting);
const gchar *gnome_cmd_file_get_mdate (GnomeCmdFile *finfo, gboolean overide_disp_setting);
const gchar *gnome_cmd_file_get_cdate (GnomeCmdFile *finfo, gboolean overide_disp_setting);
const gchar *gnome_cmd_file_get_size (GnomeCmdFile *finfo);
GnomeVFSFileSize gnome_cmd_file_get_tree_size (GnomeCmdFile *finfo);
const gchar *gnome_cmd_file_get_tree_size_as_str (GnomeCmdFile *finfo);
const gchar *gnome_cmd_file_get_perm (GnomeCmdFile *finfo);
const gchar *gnome_cmd_file_get_mime_type_desc (GnomeCmdFile *finfo);
const gchar *gnome_cmd_file_get_mime_type (GnomeCmdFile *finfo);
gboolean gnome_cmd_file_has_mime_type (GnomeCmdFile *finfo, const gchar *mime_type);
gboolean gnome_cmd_file_mime_begins_with (GnomeCmdFile *finfo, const gchar *mime_type_start);

const gchar *gnome_cmd_file_get_type_string (GnomeCmdFile *finfo);
const gchar *gnome_cmd_file_get_type_desc (GnomeCmdFile *finfo);
GdkPixmap *gnome_cmd_file_get_type_pixmap (GnomeCmdFile *finfo);
GdkBitmap *gnome_cmd_file_get_type_mask (GnomeCmdFile *finfo);

void gnome_cmd_file_show_properties (GnomeCmdFile *finfo);
void gnome_cmd_file_show_chown_dialog (GList *files);
void gnome_cmd_file_show_chmod_dialog (GList *files);
void gnome_cmd_file_view (GnomeCmdFile *finfo, gint internal_viewer);
void gnome_cmd_file_edit (GnomeCmdFile *finfo);
void gnome_cmd_file_show_cap_cut (GnomeCmdFile *finfo);
void gnome_cmd_file_show_cap_copy (GnomeCmdFile *finfo);
void gnome_cmd_file_show_cap_paste (GnomeCmdFile *finfo);

//FIXME: These names suck when we have a class called GnomeCmdFileList...
GList *gnome_cmd_file_list_copy (GList *files);
void gnome_cmd_file_list_free (GList *files);
void gnome_cmd_file_list_ref (GList *files);
void gnome_cmd_file_list_unref (GList *files);

void gnome_cmd_file_update_info (GnomeCmdFile *finfo, GnomeVFSFileInfo *info);
gboolean gnome_cmd_file_is_local (GnomeCmdFile *finfo);
gboolean gnome_cmd_file_is_executable (GnomeCmdFile *finfo);
void gnome_cmd_file_is_deleted (GnomeCmdFile *finfo);
void gnome_cmd_file_execute (GnomeCmdFile *finfo);

gboolean gnome_cmd_file_needs_update (GnomeCmdFile *finfo);

//misc tree size functions
void gnome_cmd_file_invalidate_tree_size (GnomeCmdFile *finfo);
gboolean gnome_cmd_file_has_tree_size (GnomeCmdFile *finfo);

G_END_DECLS

#endif // __GNOME_CMD_FILE_H__
