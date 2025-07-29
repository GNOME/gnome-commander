/**
 * @file gnome-cmd-file-list.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 * @copyright (C) 2024 Andrey Kutejko\n
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-path.h"

using namespace std;


#define LIST_CLICKED_SIGNAL     "list-clicked"
#define FILES_CHANGED_SIGNAL    "files-changed"
#define DIR_CHANGED_SIGNAL      "dir-changed"
#define CON_CHANGED_SIGNAL      "con-changed"
#define FILE_ACTIVATED_SIGNAL   "file-activated"
#define CMDLINE_APPEND_SIGNAL   "cmdline-append"
#define CMDLINE_EXECUTE_SIGNAL  "cmdline-execute"


extern "C" void gnome_cmd_file_list_sort (GnomeCmdFileList *fl);
extern "C" gboolean gnome_cmd_file_list_insert_file (GnomeCmdFileList *fl, GnomeCmdFile *f);
extern "C" gboolean gnome_cmd_file_list_remove_file (GnomeCmdFileList *fl, GnomeCmdFile *f);
extern "C" gboolean gnome_cmd_file_list_has_file (GnomeCmdFileList *fl, GnomeCmdFile *f);
extern "C" void gnome_cmd_file_list_update_file (GnomeCmdFileList *fl, GnomeCmdFile *f);


enum DataColumns {
    DATA_COLUMN_FILE = GnomeCmdFileList::NUM_COLUMNS,
    DATA_COLUMN_ICON_NAME,
    DATA_COLUMN_SELECTED,
    DATA_COLUMN_SIZE,
    NUM_DATA_COLUMNS,
};

const gint FILE_COLUMN = GnomeCmdFileList::NUM_COLUMNS;


struct GnomeCmdFileListPrivate
{
    GnomeCmdCon *con;
    GnomeCmdDir *cwd;     // current working dir
    GnomeCmdDir *lwd;     // last working dir
    GnomeCmdDir *connected_dir;
};


static GnomeCmdFileListPrivate* file_list_priv (GnomeCmdFileList *fl);


GnomeCmdFileListPrivate* file_list_priv (GnomeCmdFileList *fl)
{
    return (GnomeCmdFileListPrivate *) g_object_get_data (G_OBJECT (fl), "priv");
}

/*******************************
 * Callbacks
 *******************************/

static void on_dir_file_created (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (gnome_cmd_file_list_insert_file (fl, f))
        g_signal_emit_by_name (fl, FILES_CHANGED_SIGNAL);
}


static void on_dir_file_deleted (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    auto priv = file_list_priv (fl);

    if (priv->cwd == dir)
        if (gnome_cmd_file_list_remove_file (fl, f))
            g_signal_emit_by_name (fl, FILES_CHANGED_SIGNAL);
}


static void on_dir_file_changed (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (gnome_cmd_file_list_has_file (fl, f))
    {
        gnome_cmd_file_list_update_file (fl, f);
        g_signal_emit_by_name (fl, FILES_CHANGED_SIGNAL);
    }
}


static void on_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (gnome_cmd_file_list_has_file (fl, f))
    {
        // f->invalidate_metadata(TAG_FILE);    // FIXME: should be handled in GnomeCmdDir, not here
        gnome_cmd_file_list_update_file (fl, f);

        GnomeCmdFileList::ColumnID sort_col = (GnomeCmdFileList::ColumnID) gnome_cmd_file_list_get_sort_column (fl);

        if (sort_col==GnomeCmdFileList::COLUMN_NAME || sort_col==GnomeCmdFileList::COLUMN_EXT)
            gnome_cmd_file_list_sort (fl);
    }
}


static void on_dir_list_ok (GnomeCmdDir *dir, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_ok\n");

    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    auto priv = file_list_priv (fl);

    if (gtk_widget_get_realized (GTK_WIDGET (fl)))
    {
        // gtk_widget_set_sensitive (*fl, TRUE);
        gtk_widget_set_cursor (*fl, nullptr);
    }

    if (priv->connected_dir!=dir)
    {
        if (priv->connected_dir)
        {
            g_signal_handlers_disconnect_by_func (priv->connected_dir, (gpointer) on_dir_file_created, fl);
            g_signal_handlers_disconnect_by_func (priv->connected_dir, (gpointer) on_dir_file_deleted, fl);
            g_signal_handlers_disconnect_by_func (priv->connected_dir, (gpointer) on_dir_file_changed, fl);
            g_signal_handlers_disconnect_by_func (priv->connected_dir, (gpointer) on_dir_file_renamed, fl);
        }

        g_signal_connect (dir, "file-created", G_CALLBACK (on_dir_file_created), fl);
        g_signal_connect (dir, "file-deleted", G_CALLBACK (on_dir_file_deleted), fl);
        g_signal_connect (dir, "file-changed", G_CALLBACK (on_dir_file_changed), fl);
        g_signal_connect (dir, "file-renamed", G_CALLBACK (on_dir_file_renamed), fl);

        priv->connected_dir = dir;
    }

    g_signal_emit_by_name (fl, DIR_CHANGED_SIGNAL, dir);

    DEBUG('l', "returning from on_dir_list_ok\n");
}


static gboolean set_home_connection (GnomeCmdFileList *fl)
{
    g_printerr ("Setting home connection\n");
    fl->set_connection(get_home_con ());

    return FALSE;
}


static void on_directory_deleted (GnomeCmdDir *dir, GnomeCmdFileList *fl)
{
    auto parentDir = gnome_cmd_dir_get_existing_parent(dir);
    auto parentDirPath = gnome_cmd_path_get_path (gnome_cmd_dir_get_path (parentDir));
    fl->goto_directory(parentDirPath);
    g_free (parentDirPath);
}


/**
 * Handles an error which occured when directory listing failed.
 * We expect that the error which should be reported is stored in the GnomeCmdDir object.
 * The error location is freed afterwards.
 */
static void on_dir_list_failed (GnomeCmdDir *dir, GError *error, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_failed\n");
    auto priv = file_list_priv (fl);

    if (error)
        gnome_cmd_show_message (GTK_WINDOW (gtk_widget_get_root (*fl)), _("Directory listing failed."), error->message);

    g_signal_handlers_disconnect_matched (priv->cwd, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, fl);
    priv->connected_dir = nullptr;
    g_object_unref (priv->cwd);
    gtk_widget_set_cursor (*fl, nullptr);
    // gtk_widget_set_sensitive (*fl, TRUE);

    if (priv->lwd && priv->con == gnome_cmd_file_get_connection (GNOME_CMD_FILE (priv->lwd)))
    {
        g_signal_handlers_disconnect_by_func (priv->cwd, (gpointer) on_directory_deleted, fl);

        priv->cwd = priv->lwd;
        g_signal_connect (priv->cwd, "list-ok", G_CALLBACK (on_dir_list_ok), fl);
        g_signal_connect (priv->cwd, "list-failed", G_CALLBACK (on_dir_list_failed), fl);
        g_signal_connect (priv->cwd, "dir-deleted", G_CALLBACK (on_directory_deleted), fl);
        priv->lwd = nullptr;
    }
    else
        g_timeout_add (1, (GSourceFunc) set_home_connection, fl);
}


static void on_connection_close (GnomeCmdCon *con, GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);
    if (con == priv->con)
        fl->set_connection(get_home_con());
}


/*******************************
 * Gtk class implementation
 *******************************/

extern "C" void gnome_cmd_file_list_finalize (GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);
    if (priv->con)
        g_signal_handlers_disconnect_by_func (priv->con, (gpointer) on_connection_close, fl);

    g_clear_object (&priv->con);

    g_clear_object (&priv->cwd);
    g_clear_object (&priv->lwd);
}


extern "C" void gnome_cmd_file_list_init (GnomeCmdFileList *fl)
{
    auto priv = g_new0 (GnomeCmdFileListPrivate, 1);
    g_object_set_data_full (G_OBJECT (fl), "priv", priv, g_free);
}


/***********************************
 * Public functions
 ***********************************/

extern "C" void open_connection_r (GnomeCmdFileList *fl, GtkWindow *parent_window, GnomeCmdCon *con);

void GnomeCmdFileList::set_connection (GnomeCmdCon *new_con, GnomeCmdDir *start_dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (new_con));
    auto priv = file_list_priv (this);

    if (priv->con == new_con)
    {
        if (!gnome_cmd_con_should_remember_dir (new_con))
            set_directory (gnome_cmd_con_get_default_dir (new_con));
        else
            if (start_dir)
                set_directory (start_dir);
        return;
    }

    if (!gnome_cmd_con_is_open (new_con))
    {
        GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (this)));
        open_connection_r (this, parent_window, new_con);
        return;
    }

    if (priv->con)
        g_signal_handlers_disconnect_by_func (priv->con, (gpointer) on_connection_close, this);


    if (priv->lwd)
    {
        gnome_cmd_dir_cancel_monitoring (priv->lwd);
        g_object_unref (priv->lwd);
        priv->lwd = nullptr;
    }
    if (priv->cwd)
    {
        g_signal_handlers_disconnect_by_func (priv->cwd, (gpointer) on_directory_deleted, this);
        gnome_cmd_dir_cancel_monitoring (priv->cwd);
        g_object_unref (priv->cwd);
        priv->cwd = nullptr;
    }

    g_set_object (&priv->con, new_con);

    if (!start_dir)
        start_dir = gnome_cmd_con_get_default_dir (priv->con);

    g_signal_connect (priv->con, "close", G_CALLBACK (on_connection_close), this);

    g_signal_emit_by_name (this, CON_CHANGED_SIGNAL, priv->con);

    set_directory(start_dir);
}


void GnomeCmdFileList::set_directory(GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    auto priv = file_list_priv (this);

    if (priv->cwd == dir)
        return;

    if (gtk_widget_get_realized (GTK_WIDGET (this)) && gnome_cmd_dir_get_state (dir) != GnomeCmdDir::STATE_LISTED)
    {
        // gtk_widget_set_sensitive (*this, FALSE);
        set_cursor_busy_for_widget (*this);
    }

    g_object_ref (dir);

    if (priv->lwd && priv->lwd != dir)
        g_object_unref (priv->lwd);

    if (priv->cwd)
    {
        priv->lwd = priv->cwd;
        gnome_cmd_dir_cancel_monitoring (priv->lwd);
        g_signal_handlers_disconnect_matched (priv->lwd, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        if (gnome_cmd_file_is_local (GNOME_CMD_FILE (priv->lwd)) && !gnome_cmd_dir_is_monitored (priv->lwd) && gnome_cmd_dir_needs_mtime_update (priv->lwd))
            gnome_cmd_dir_update_mtime (priv->lwd);
        g_signal_handlers_disconnect_by_func (priv->cwd, (gpointer) on_directory_deleted, this);
    }

    priv->cwd = dir;
    g_signal_connect (dir, "list-ok", G_CALLBACK (on_dir_list_ok), this);
    g_signal_connect (dir, "list-failed", G_CALLBACK (on_dir_list_failed), this);
    g_signal_connect (dir, "dir-deleted", G_CALLBACK (on_directory_deleted), this);

    switch (gnome_cmd_dir_get_state (dir))
    {
        case GnomeCmdDir::STATE_EMPTY:
            gnome_cmd_dir_list_files (GTK_WINDOW (gtk_widget_get_root (*this)), dir, TRUE);
            break;

        case GnomeCmdDir::STATE_LISTING:
        case GnomeCmdDir::STATE_CANCELING:
            break;

        case GnomeCmdDir::STATE_LISTED:
            // check if the dir has up-to-date file list; if not and it's a local dir - relist it
            if (gnome_cmd_file_is_local (GNOME_CMD_FILE (dir)) && !gnome_cmd_dir_is_monitored (dir) && gnome_cmd_dir_update_mtime (dir))
                gnome_cmd_dir_relist_files (GTK_WINDOW (gtk_widget_get_root (*this)), dir, TRUE);
            else
                on_dir_list_ok (dir, this);
            break;

        default:
            break;
    }

    gnome_cmd_dir_start_monitoring (dir);
}


void GnomeCmdFileList::goto_directory(const gchar *in_dir)
{
    g_return_if_fail (in_dir != nullptr);
    auto priv = file_list_priv (this);

    GnomeCmdDir *new_dir = nullptr;
    gchar *focus_dir = nullptr;
    gchar *dir;

    if (g_str_has_prefix (in_dir, "~"))
    {
        strlen(in_dir) > 1
            ? dir = g_strdup_printf("%s%s" , g_get_home_dir(), in_dir+1)
            : dir = g_strdup_printf("%s" , g_get_home_dir());
    }
    else
        dir = unquote_if_needed (in_dir);

    if (strcmp (dir, "..") == 0)
    {
        // let's get the parent directory
        new_dir = gnome_cmd_dir_get_parent (priv->cwd);
        if (!new_dir)
        {
            g_free (dir);
            return;
        }
        focus_dir = strdup(GNOME_CMD_FILE (priv->cwd)->get_name());
    }
    else
    {
        // check if it's an absolute address or not
        if (dir[0] == '/')
            new_dir = gnome_cmd_dir_new (priv->con, gnome_cmd_con_create_path (priv->con, dir));
        else
            if (g_str_has_prefix (dir, "\\\\"))
            {
                GnomeCmdPath *path = gnome_cmd_con_create_path (get_smb_con (), dir);
                if (path)
                    new_dir = gnome_cmd_dir_new (get_smb_con (), path);
            }
            else
                new_dir = gnome_cmd_dir_get_child (priv->cwd, dir);
    }

    if (new_dir)
        set_directory(new_dir);

    // focus the current dir when going back to the parent dir
    if (focus_dir)
    {
        gnome_cmd_file_list_focus_file (this, focus_dir, FALSE);
        g_free(focus_dir);
    }

    g_free (dir);
}

// FFI
GnomeCmdCon *gnome_cmd_file_list_get_connection(GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);
    return priv->con;
}

GnomeCmdDir *gnome_cmd_file_list_get_directory(GnomeCmdFileList *fl)
{
    auto priv = file_list_priv (fl);
    return priv->cwd;
}

void gnome_cmd_file_list_set_directory(GnomeCmdFileList *fl, GnomeCmdDir *dir)
{
    fl->set_directory(dir);
}

void gnome_cmd_file_list_set_connection(GnomeCmdFileList *fl, GnomeCmdCon *con, GnomeCmdDir *start_dir)
{
    fl->set_connection(con, start_dir);
}

void gnome_cmd_file_list_goto_directory(GnomeCmdFileList *fl, const gchar *dir)
{
    fl->goto_directory(dir);
}
