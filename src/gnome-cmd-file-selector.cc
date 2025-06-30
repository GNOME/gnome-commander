/**
 * @file gnome-cmd-file-selector.cc
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-cmdline.h"
#include "history.h"
#include "utils.h"
#include "widget-factory.h"

using namespace std;


struct GnomeCmdFileSelectorPrivate
{
    GnomeCmdFileList *list;

    gboolean select_connection_in_progress {FALSE};
};


#define DIR_CHANGED_SIGNAL          "dir-changed"
#define LIST_CLICKED_SIGNAL         "list-clicked"
#define ACTIVATE_REQUEST_SIGNAL     "activate-request"

/*******************************
 * Utility functions
 *******************************/

struct GnomeCmdFileMetadataService;

extern "C" gboolean mime_exec_file (GtkWindow *parent_window, GnomeCmdFile *f);
extern "C" GtkDropDown *gnome_cmd_file_selector_connection_dropdown (GnomeCmdFileSelector *fs);
extern "C" GtkWidget *gnome_cmd_file_selector_get_dir_indicator (GnomeCmdFileSelector *fs);
extern "C" GtkNotebook *gnome_cmd_file_selector_get_notebook (GnomeCmdFileSelector *fs);
extern "C" void gnome_cmd_file_selector_update_selected_files_label (GnomeCmdFileSelector *fs);
extern "C" void gnome_cmd_file_selector_update_show_tabs (GnomeCmdFileSelector *fs);
extern "C" void gnome_cmd_file_selector_update_vol_label (GnomeCmdFileSelector *fs);


static GnomeCmdFileSelectorPrivate *file_selector_priv (GnomeCmdFileSelector *fs)
{
    return (GnomeCmdFileSelectorPrivate *) g_object_get_data (G_OBJECT (fs), "priv");
}


inline void GnomeCmdFileSelector::update_direntry()
{
    GnomeCmdDir *dir = get_directory();

    if (!dir)
        return;

    auto dir_indicator = gnome_cmd_file_selector_get_dir_indicator (this);
    g_object_set (dir_indicator, "directory", dir, nullptr);
}


void GnomeCmdFileSelector::do_file_specific_action (GnomeCmdFileList *fl, GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (f != nullptr);

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
    {
        if (!gnome_cmd_file_selector_is_tab_locked (this, fl))
        {
            fl->invalidate_tree_size();

            if (gnome_cmd_file_is_dotdot (f))
                fl->goto_directory("..");
            else
                fl->set_directory(GNOME_CMD_DIR (f));
        }
        else
            new_tab(gnome_cmd_file_is_dotdot (f) ? gnome_cmd_dir_get_parent (gnome_cmd_file_list_get_directory (fl)) : GNOME_CMD_DIR (f));
    }
    else
    {
        GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (this)));
        mime_exec_file (parent_window, f);
    }
}


/*******************************
 * Callbacks
 *******************************/

static void on_con_combo_item_selected (GtkDropDown *con_dropdown, GParamSpec *pspec, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    auto priv = file_selector_priv (fs);

    if (priv->select_connection_in_progress)
        return;

    GnomeCmdCon *con = GNOME_CMD_CON (gtk_drop_down_get_selected_item (con_dropdown));
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    GdkModifierType mask = get_modifiers_state();

    if (mask & GDK_CONTROL_MASK || gnome_cmd_file_selector_is_current_tab_locked (fs))
        fs->new_tab(gnome_cmd_con_get_default_dir (con));
    else
        fs->set_connection(con, gnome_cmd_con_get_default_dir (con));

    g_signal_emit_by_name (fs, ACTIVATE_REQUEST_SIGNAL);
}


static gboolean select_connection (GnomeCmdFileSelector *fs, GnomeCmdCon *con)
{
    auto priv = file_selector_priv (fs);

    GtkDropDown *con_dropdown = gnome_cmd_file_selector_connection_dropdown (fs);
    GListModel *store = gtk_drop_down_get_model (con_dropdown);
    for (guint position = 0; position < g_list_model_get_n_items (store); ++position)
    {
        GnomeCmdCon *item = GNOME_CMD_CON (g_list_model_get_item (store, position));
        if (item == con)
        {
            priv->select_connection_in_progress = TRUE;
            gtk_drop_down_set_selected (con_dropdown, position);
            priv->select_connection_in_progress = FALSE;
            return TRUE;
        }
    }
    return FALSE;
}


extern "C" void on_notebook_switch_page (GnomeCmdFileSelector *fs, guint n)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    auto priv = file_selector_priv (fs);

    GnomeCmdDir *prev_dir = fs->get_directory();
    GnomeCmdCon *prev_con = fs->get_connection();

    priv->list = fs->file_list(n);
    fs->update_direntry();
    gnome_cmd_file_selector_update_selected_files_label (fs);
    gnome_cmd_file_selector_update_vol_label (fs);
    gnome_cmd_file_selector_update_style (fs);

    if (prev_dir!=fs->get_directory())
        g_signal_emit_by_name (fs, DIR_CHANGED_SIGNAL, fs->get_directory());

    if (prev_con!=fs->get_connection())
        select_connection (fs, fs->get_connection());
}


static void on_list_list_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, GnomeCmdFileSelector *fs)
{
    MiddleMouseButtonMode middle_mouse_button_mode;

    switch (event->button)
    {
        case 1:
        case 3:
            g_signal_emit_by_name (fs, LIST_CLICKED_SIGNAL);
            break;

        case 2:
            g_object_get (fl, "middle-mouse-button-mode", &middle_mouse_button_mode, nullptr);
            if (middle_mouse_button_mode == MIDDLE_BUTTON_GOES_UP_DIR)
            {
                if (gnome_cmd_file_selector_is_tab_locked (fs, fl))
                    fs->new_tab(gnome_cmd_dir_get_parent (gnome_cmd_file_list_get_directory (fl)));
                else
                    fl->goto_directory("..");
            }
            else
            {
                if (event->file && gnome_cmd_file_is_dotdot (event->file))
                    fs->new_tab(gnome_cmd_dir_get_parent (gnome_cmd_file_list_get_directory (fl)));
                else
                    fs->new_tab(event->file && event->file->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
                                     ? GNOME_CMD_DIR (event->file)
                                     : gnome_cmd_file_list_get_directory (fl));
            }
            break;

        case 6:
        case 8:
            gnome_cmd_file_selector_back(fs);
            break;

        case 7:
        case 9:
            gnome_cmd_file_selector_forward(fs);
            break;

        default:
            break;
    }
}


static void on_list_con_changed (GnomeCmdFileList *fl, GnomeCmdCon *con, GnomeCmdFileSelector *fs)
{
    select_connection (fs, con);
}


static void on_list_dir_changed (GnomeCmdFileList *fl, GnomeCmdDir *dir, GnomeCmdFileSelector *fs)
{
    gchar *fpath = GNOME_CMD_FILE (dir)->GetPathStringThroughParent();
    gnome_cmd_con_dir_history_add (gnome_cmd_file_list_get_connection (fl), fpath);
    g_free (fpath);

    if (fs->file_list()!=fl)  return;

    fs->update_direntry();
    gnome_cmd_file_selector_update_vol_label (fs);

    if (gnome_cmd_file_list_get_directory (fl) != dir)  return;

    gnome_cmd_file_selector_update_tab_label (fs, fl);
    gnome_cmd_file_selector_update_files (fs);
    gnome_cmd_file_selector_update_selected_files_label (fs);

    g_signal_emit_by_name (fs, DIR_CHANGED_SIGNAL, dir);
}


static void on_list_files_changed (GnomeCmdFileList *fl, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->file_list()==fl)
        gnome_cmd_file_selector_update_selected_files_label (fs);
}


static void on_list_file_activated (GnomeCmdFileList *fl, GnomeCmdFile *f, GnomeCmdFileSelector *fs)
{
    fs->do_file_specific_action (fl, f);
}


static void on_list_cmdline_append (GnomeCmdFileList *fl, const gchar *text, GnomeCmdFileSelector *fs)
{
    GnomeCmdCmdline *cmdline;
    g_object_get (G_OBJECT (fs), "command-line", &cmdline, nullptr);
    if (gtk_widget_is_visible (GTK_WIDGET (cmdline)))
    {
        gnome_cmd_cmdline_append_text (cmdline, text);
        gtk_widget_grab_focus (GTK_WIDGET (cmdline));
    }
}


static gboolean on_list_cmdline_execute (GnomeCmdFileList *fl, GnomeCmdFileSelector *fs)
{
    GnomeCmdCmdline *cmdline;
    g_object_get (G_OBJECT (fs), "command-line", &cmdline, nullptr);
    if (gtk_widget_is_visible (GTK_WIDGET (cmdline)) && !gnome_cmd_cmdline_is_empty (cmdline))
    {
        gnome_cmd_cmdline_exec (cmdline);
        return TRUE;
    }
    return FALSE;
}


/*******************************
 * Gtk class implementation
 *******************************/

extern "C" void gnome_cmd_file_selector_init (GnomeCmdFileSelector *fs)
{
    auto priv = g_new0(GnomeCmdFileSelectorPrivate, 1);
    g_object_set_data_full (G_OBJECT (fs), "priv", priv, g_free);

    priv->list = nullptr;

    GtkDropDown *con_dropdown = gnome_cmd_file_selector_connection_dropdown (fs);
    g_signal_connect (con_dropdown, "notify::selected-item", G_CALLBACK (on_con_combo_item_selected), fs);
}

/***********************************
 * Public functions
 ***********************************/

void GnomeCmdFileSelector::update_connections()
{
    if (!gtk_widget_get_realized (GTK_WIDGET (this)))
        return;

    // If the connection is no longer available use the home connection
    if (!select_connection (this, get_connection()))
        set_connection (get_home_con ());
}


extern "C" GType gnome_cmd_tab_label_get_type();


GtkWidget *GnomeCmdFileSelector::new_tab(GnomeCmdDir *dir, GnomeCmdFileList::ColumnID sort_col, GtkSortType sort_order, gboolean locked, gboolean activate, gboolean grab_focus)
{
    auto priv = file_selector_priv (this);

    GnomeCmdFileMetadataService *file_metadata_service;
    g_object_get (G_OBJECT (this), "file-metadata-service", &file_metadata_service, nullptr);

    // create the list
    GnomeCmdFileList *fl = (GnomeCmdFileList *) g_object_new (GNOME_CMD_TYPE_FILE_LIST,
        "file-metadata-service", file_metadata_service,
        nullptr);
    gnome_cmd_file_list_set_sorting (fl, sort_col, sort_order);

    if (activate)
        priv->list = fl;               //  ... update GnomeCmdFileSelector::list to point at newly created tab

    gnome_cmd_file_selector_set_tab_locked (this, fl, locked);
    fl->update_style();

    // hide dir column
    fl->show_column(GnomeCmdFileList::COLUMN_DIR, FALSE);

    GtkWidget *tab_label = GTK_WIDGET (g_object_new (gnome_cmd_tab_label_get_type(), nullptr));

    GtkNotebook *notebook = gnome_cmd_file_selector_get_notebook (this);
    gint n = gtk_notebook_append_page (notebook, GTK_WIDGET (fl), tab_label);
    gnome_cmd_file_selector_update_show_tabs (this);

    gtk_notebook_set_tab_reorderable (notebook, GTK_WIDGET (fl), TRUE);

    g_signal_connect (fl, "con-changed", G_CALLBACK (on_list_con_changed), this);
    g_signal_connect (fl, "dir-changed", G_CALLBACK (on_list_dir_changed), this);
    g_signal_connect (fl, "files-changed", G_CALLBACK (on_list_files_changed), this);
    g_signal_connect (fl, "file-activated", G_CALLBACK (on_list_file_activated), this);
    g_signal_connect (fl, "cmdline-append", G_CALLBACK (on_list_cmdline_append), this);
    g_signal_connect (fl, "cmdline-execute", G_CALLBACK (on_list_cmdline_execute), this);

    if (dir)
        fl->set_connection(gnome_cmd_file_get_connection (GNOME_CMD_FILE (dir)), dir);

    gnome_cmd_file_selector_update_tab_label (this, fl);

    if (activate)
    {
        gtk_notebook_set_current_page (notebook, n);
        if (grab_focus)
            gtk_widget_grab_focus (*fl);
    }

    g_signal_connect (fl, "list-clicked", G_CALLBACK (on_list_list_clicked), this);

    return GTK_WIDGET (fl);
}


GnomeCmdFileList *GnomeCmdFileSelector::file_list()
{
    return gnome_cmd_file_selector_file_list (this);
}


GnomeCmdFileList *GnomeCmdFileSelector::file_list(gint n)
{
    return gnome_cmd_file_selector_file_list_nth (this, n);
}


// FFI
GnomeCmdFileList *gnome_cmd_file_selector_file_list (GnomeCmdFileSelector *fs)
{
    auto priv = file_selector_priv (fs);
    return priv->list;
}

GnomeCmdFileList *gnome_cmd_file_selector_file_list_nth (GnomeCmdFileSelector *fs, gint n)
{
    GtkNotebook *notebook = gnome_cmd_file_selector_get_notebook (fs);
    auto page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), n);
    return GNOME_CMD_FILE_LIST (page);
}

GtkWidget *gnome_cmd_file_selector_new_tab_full (GnomeCmdFileSelector *fs, GnomeCmdDir *dir, gint sort_col, gint sort_order, gboolean locked, gboolean activate, gboolean grab_focus)
{
    return fs->new_tab(dir, (GnomeCmdFileList::ColumnID) sort_col, (GtkSortType) sort_order, locked, activate, grab_focus);
}

gboolean gnome_cmd_file_selector_is_tab_locked (GnomeCmdFileSelector *fs, GnomeCmdFileList *fl)
{
    return g_object_get_data (G_OBJECT (fl), "file-list-locked") != nullptr;
}

extern "C" gboolean gnome_cmd_file_selector_is_current_tab_locked (GnomeCmdFileSelector *fs)
{
    return gnome_cmd_file_selector_is_tab_locked (fs, gnome_cmd_file_selector_file_list (fs));
}

void gnome_cmd_file_selector_set_tab_locked (GnomeCmdFileSelector *fs, GnomeCmdFileList *fl, gboolean lock)
{
    g_object_set_data (G_OBJECT (fl), "file-list-locked", (gpointer) lock);
}

void gnome_cmd_file_selector_update_connections (GnomeCmdFileSelector *fs)
{
    fs->update_connections();
}

extern "C" void gnome_cmd_file_selector_do_file_specific_action (GnomeCmdFileSelector *fs, GnomeCmdFileList *fl, GnomeCmdFile *f)
{
    fs->do_file_specific_action(fl, f);
}
