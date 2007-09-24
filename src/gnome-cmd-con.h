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

#ifndef __GNOME_CMD_CON_H__
#define __GNOME_CMD_CON_H__

#define GNOME_CMD_CON(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_con_get_type (), GnomeCmdCon)
#define GNOME_CMD_CON_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_con_get_type (), GnomeCmdConClass)
#define GNOME_CMD_IS_CON(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_con_get_type ())
#define GNOME_CMD_CON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_CMD_CON, GnomeCmdConClass))

typedef struct _GnomeCmdCon GnomeCmdCon;
typedef struct _GnomeCmdConClass GnomeCmdConClass;
typedef struct _GnomeCmdConPrivate GnomeCmdConPrivate;

#include "gnome-cmd-path.h"
#include "gnome-cmd-pixmap.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-dir-pool.h"
#include "history.h"
#include "gnome-cmd-types.h"

typedef enum
{
    CON_STATE_CLOSED,
    CON_STATE_OPEN,
    CON_STATE_OPENING,
    CON_STATE_CANCELLING
} ConState;

typedef enum
{
    CON_OPEN_OK,
    CON_OPEN_FAILED,
    CON_OPEN_CANCELLED,
    CON_OPEN_IN_PROGRESS,
    CON_OPEN_NOT_STARTED
} ConOpenResult;

struct _GnomeCmdCon
{
    GtkObject parent;

    gchar            *alias;
    gchar            *open_msg;
    GnomeCmdPath     *base_path;
    GnomeVFSFileInfo *base_info;
    gboolean         should_remember_dir;
    gboolean         needs_open_visprog;
    gboolean         needs_list_visprog;
    gboolean         can_show_free_space;
    ConState         state;
    gboolean         is_local;
    gboolean         is_closeable;
    gchar            *go_text;
    gchar            *go_tooltip;
    GnomeCmdPixmap   *go_pixmap;
    gchar            *open_text;
    gchar            *open_tooltip;
    GnomeCmdPixmap   *open_pixmap;
    gchar            *close_text;
    gchar            *close_tooltip;
    GnomeCmdPixmap   *close_pixmap;

    ConOpenResult    open_result;
    GnomeVFSResult   open_failed_reason;
    gchar            *open_failed_msg;

    GnomeCmdConPrivate *priv;
};

struct _GnomeCmdConClass
{
    GtkObjectClass parent_class;

    /* signals */
    void (* updated) (GnomeCmdCon *con);
    void (* open_done) (GnomeCmdCon *con);
    void (* open_failed) (GnomeCmdCon *con, const gchar *msg, GnomeVFSResult result);

    /* virtual functions */
    void (* open) (GnomeCmdCon *con);
    void (* cancel_open) (GnomeCmdCon *con);
    gboolean (* close) (GnomeCmdCon *con);
    gboolean (* open_is_needed) (GnomeCmdCon *con);
    GnomeVFSURI *(* create_uri) (GnomeCmdCon *con, GnomeCmdPath *path);
    GnomeCmdPath *(* create_path) (GnomeCmdCon *con, const gchar *path_str);
};


GtkType gnome_cmd_con_get_type (void);


void gnome_cmd_con_open (GnomeCmdCon *con);

inline gboolean gnome_cmd_con_is_open (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->state == CON_STATE_OPEN;
}

void gnome_cmd_con_cancel_open (GnomeCmdCon *con);

gboolean gnome_cmd_con_close (GnomeCmdCon *con);

inline gboolean gnome_cmd_con_open_is_needed (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->open_is_needed (con);
}

GnomeVFSURI *gnome_cmd_con_create_uri (GnomeCmdCon *con, GnomeCmdPath *path);

GnomeCmdPath *gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str);

inline const gchar *gnome_cmd_con_get_open_msg (GnomeCmdCon *con)
{
    return con->open_msg;
}

inline const gchar *gnome_cmd_con_get_alias (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->alias;
}

void gnome_cmd_con_set_cwd (GnomeCmdCon *con, GnomeCmdDir *dir);

GnomeCmdDir *gnome_cmd_con_get_cwd (GnomeCmdCon *con);

GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con);

void gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir);

GnomeCmdDir *gnome_cmd_con_get_root_dir (GnomeCmdCon *con);
void gnome_cmd_con_set_root_dir (GnomeCmdCon *con, GnomeCmdDir *dir);

inline gboolean gnome_cmd_con_should_remember_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->should_remember_dir;
}

inline gboolean gnome_cmd_con_needs_open_visprog (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->needs_open_visprog;
}

inline gboolean gnome_cmd_con_needs_list_visprog (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->needs_list_visprog;
}

inline gboolean gnome_cmd_con_can_show_free_space (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->can_show_free_space;
}

inline gboolean gnome_cmd_con_is_local (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->is_local;
}

inline gboolean gnome_cmd_con_is_closeable (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->is_closeable;
}

History *gnome_cmd_con_get_dir_history (GnomeCmdCon *con);

inline const gchar *gnome_cmd_con_get_go_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->go_text;
}

inline const gchar *gnome_cmd_con_get_open_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_text;
}

inline const gchar *gnome_cmd_con_get_close_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->close_text;
}

inline const gchar *gnome_cmd_con_get_go_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->go_tooltip;
}

inline const gchar *gnome_cmd_con_get_open_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_tooltip;
}

inline const gchar *gnome_cmd_con_get_close_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->close_tooltip;
}

inline GnomeCmdPixmap *gnome_cmd_con_get_go_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->go_pixmap;
}

inline GnomeCmdPixmap *gnome_cmd_con_get_open_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_pixmap;
}

inline GnomeCmdPixmap *gnome_cmd_con_get_close_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->close_pixmap;
}

GnomeCmdBookmarkGroup *gnome_cmd_con_get_bookmarks (GnomeCmdCon *con);

void gnome_cmd_con_set_bookmarks (GnomeCmdCon *con, GnomeCmdBookmarkGroup *bookmarks);

GnomeCmdDirPool *gnome_cmd_con_get_dir_pool (GnomeCmdCon *con);

void gnome_cmd_con_updated (GnomeCmdCon *con);

GnomeVFSResult gnome_cmd_con_get_path_target_type (GnomeCmdCon *con, const gchar *path, GnomeVFSFileType *type);

GnomeVFSResult gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str);

void gnome_cmd_con_add_to_cache (GnomeCmdCon *con, GnomeCmdDir *dir);

void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, GnomeCmdDir *dir);

GnomeCmdDir *gnome_cmd_con_cache_lookup (GnomeCmdCon *con, const gchar *uri);

#endif // __GNOME_CMD_CON_H__
