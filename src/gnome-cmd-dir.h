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

#ifndef __GNOME_CMD_DIR_H__
#define __GNOME_CMD_DIR_H__

#define GNOME_CMD_DIR(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_dir_get_type (), GnomeCmdDir)
#define GNOME_CMD_DIR_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_dir_get_type (), GnomeCmdDirClass)
#define GNOME_CMD_IS_DIR(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_dir_get_type ())


typedef struct _GnomeCmdDir         GnomeCmdDir;
typedef struct _GnomeCmdDirPrivate  GnomeCmdDirPrivate;
typedef struct _GnomeCmdDirClass    GnomeCmdDirClass;

typedef void (* DirListDoneFunc) (GnomeCmdDir *dir,
                                  GList *files,
                                  GnomeVFSResult result);

#include "gnome-cmd-file.h"
#include "gnome-cmd-con.h"
#include "handle.h"

G_BEGIN_DECLS

typedef enum {
    DIR_STATE_EMPTY,
    DIR_STATE_LISTED,
    DIR_STATE_LISTING,
    DIR_STATE_CANCELING
} DirState;

struct _GnomeCmdDir
{
    GnomeCmdFile parent;

    gint voffset;
    GList *infolist;
    GnomeVFSAsyncHandle *list_handle;
    GnomeVFSResult list_result;
    gint list_counter;
    DirState state;

    DirListDoneFunc done_func;

    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *pbar;

    GnomeCmdDirPrivate *priv;
};


struct _GnomeCmdDirClass
{
    GnomeCmdFileClass parent_class;

    void (* file_created)       (GnomeCmdDir *dir,
                                 GnomeCmdFile *file);
    void (* file_deleted)       (GnomeCmdDir *dir,
                                 GnomeCmdFile *file);
    void (* file_changed)       (GnomeCmdDir *dir,
                                 GnomeCmdFile *file);
    void (* list_ok)            (GnomeCmdDir *dir,
                                 GList *files);
    void (* list_failed)        (GnomeCmdDir *dir,
                                 GnomeVFSResult result);
};

GtkType gnome_cmd_dir_get_type (void);

GnomeCmdDir *gnome_cmd_dir_new_from_info (GnomeVFSFileInfo *info,
                                          GnomeCmdDir *parent);
GnomeCmdDir *gnome_cmd_dir_new_with_con (GnomeVFSFileInfo *info,
                                         GnomeCmdPath *path,
                                         GnomeCmdCon *con);
GnomeCmdDir *gnome_cmd_dir_new (GnomeCmdCon *con,
                                GnomeCmdPath *path);
GnomeCmdDir *gnome_cmd_dir_get_parent (GnomeCmdDir *dir);
GnomeCmdDir *gnome_cmd_dir_get_child (GnomeCmdDir *dir, const gchar *child);
GnomeCmdCon *gnome_cmd_dir_get_connection (GnomeCmdDir *dir);
Handle *gnome_cmd_dir_get_handle (GnomeCmdDir *dir);


void gnome_cmd_dir_ref (GnomeCmdDir *dir);
void gnome_cmd_dir_unref (GnomeCmdDir *dir);

GnomeVFSResult gnome_cmd_dir_get_files (GnomeCmdDir *dir, GList **files);
void gnome_cmd_dir_relist_files (GnomeCmdDir *dir, gboolean visprog);
void gnome_cmd_dir_list_files (GnomeCmdDir *dir, gboolean visprog);

GnomeCmdPath *gnome_cmd_dir_get_path (GnomeCmdDir *dir);
void gnome_cmd_dir_set_path (GnomeCmdDir *dir, GnomeCmdPath *path);
void gnome_cmd_dir_update_path (GnomeCmdDir *dir);
gchar *gnome_cmd_dir_get_display_path (GnomeCmdDir *dir);

GnomeVFSURI *gnome_cmd_dir_get_uri (GnomeCmdDir *dir);
gchar       *gnome_cmd_dir_get_uri_str (GnomeCmdDir *dir);

GnomeVFSURI *gnome_cmd_dir_get_child_uri (GnomeCmdDir *dir, const gchar *filename);
gchar       *gnome_cmd_dir_get_child_uri_str (GnomeCmdDir *dir, const gchar *filename);

void gnome_cmd_dir_file_created (GnomeCmdDir *dir, const gchar *filename);
void gnome_cmd_dir_file_deleted (GnomeCmdDir *dir, const gchar *filename);
void gnome_cmd_dir_file_changed (GnomeCmdDir *dir, const gchar *filename);
void gnome_cmd_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *finfo);

gboolean gnome_cmd_dir_uses_fam (GnomeCmdDir *dir);
void gnome_cmd_dir_start_monitoring (GnomeCmdDir *dir);
void gnome_cmd_dir_cancel_monitoring (GnomeCmdDir *dir);
gboolean gnome_cmd_dir_is_local (GnomeCmdDir *dir);
void gnome_cmd_dir_set_content_changed (GnomeCmdDir *dir);

G_END_DECLS

#endif // __GNOME_CMD_DIR_H__
