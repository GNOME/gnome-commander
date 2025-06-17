/**
 * @file gnome-cmd-main-win.cc
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
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-user-actions.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-path.h"
#include "gnome-cmd-con-list.h"
#include "imageloader.h"
#include "utils.h"
#include "widget-factory.h"

using namespace std;


struct GnomeCmdMainWinPrivate
{
    GWeakRef advrename_dlg;
    GWeakRef file_search_dlg;

    bool state_saved;
    GnomeCmdShortcuts *gcmd_shortcuts;
};


extern "C" GApplication *gnome_cmd_application;
extern "C" void gnome_cmd_main_win_load_tabs(GnomeCmdMainWin *, GApplication *);
extern "C" void gnome_cmd_main_win_save_tabs(GnomeCmdMainWin *, gboolean, gboolean);


extern "C" void gnome_cmd_main_win_update_drop_con_button (GnomeCmdMainWin *main_win, GnomeCmdFileList *fl);
extern "C" GnomeCmdFileSelector *gnome_cmd_main_win_get_fs (GnomeCmdMainWin *main_win, FileSelectorID id);
extern "C" void gnome_cmd_main_win_swap_panels (GnomeCmdMainWin *main_win);
extern "C" void gnome_cmd_main_win_show_slide_popup (GnomeCmdMainWin *main_win);


static gboolean on_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);


static GnomeCmdMainWinPrivate *gnome_cmd_main_win_priv (GnomeCmdMainWin *mw)
{
    return (GnomeCmdMainWinPrivate *) g_object_get_data (G_OBJECT (mw), "priv");
}


/*****************************************************************************
    Misc widgets callbacks
*****************************************************************************/

extern "C" void gnome_cmd_main_win_update_browse_buttons (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs == mw->fs(ACTIVE))
    {
        g_simple_action_set_enabled (
            G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (mw), "view-first")),
            gnome_cmd_file_selector_can_back(fs));
        g_simple_action_set_enabled (
            G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (mw), "view-back")),
            gnome_cmd_file_selector_can_back(fs));
        g_simple_action_set_enabled (
            G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (mw), "view-forward")),
            gnome_cmd_file_selector_can_forward(fs));
        g_simple_action_set_enabled (
            G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (mw), "view-last")),
            gnome_cmd_file_selector_can_forward(fs));
    }
}


static void on_fs_dir_change (GnomeCmdFileSelector *fs, const gchar dir, GnomeCmdMainWin *mw)
{
    gnome_cmd_main_win_update_browse_buttons (mw, fs);
    gnome_cmd_main_win_update_drop_con_button (mw, fs->file_list());
    mw->update_cmdline();
}


static void on_fs_activate_request (GnomeCmdFileSelector *fs, GnomeCmdMainWin *mw)
{
    gnome_cmd_main_win_switch_fs (mw, fs);
    mw->refocus();
}


static void toggle_action_change_state (GnomeCmdMainWin *mw, const gchar *action, bool state)
{
    g_action_change_state (
        g_action_map_lookup_action (G_ACTION_MAP (mw), action),
        g_variant_new_boolean (state));
}


/*******************************
 * Gtk class implementation
 *******************************/

extern "C" void gnome_cmd_main_win_dispose (GnomeCmdMainWin *main_win)
{
    auto priv = gnome_cmd_main_win_priv (main_win);

    if (!priv->state_saved)
    {
        gnome_cmd_shortcuts_save_to_settings (priv->gcmd_shortcuts);
        gnome_cmd_shortcuts_free (priv->gcmd_shortcuts);
        priv->gcmd_shortcuts = nullptr;

        gnome_cmd_main_win_save_tabs(main_win, gnome_cmd_data.options.save_tabs_on_exit, gnome_cmd_data.options.save_dirs_on_exit);

        gnome_cmd_data.save(main_win);
        priv->state_saved = true;
    }

    g_weak_ref_set (&priv->advrename_dlg, nullptr);
    g_weak_ref_set (&priv->file_search_dlg, nullptr);
}


extern "C" void gnome_cmd_main_win_init (GnomeCmdMainWin *mw)
{
    auto priv = g_new0 (GnomeCmdMainWinPrivate, 1);
    g_object_set_data_full (G_OBJECT (mw), "priv", priv, g_free);

    priv->advrename_dlg = { { nullptr } };
    priv->file_search_dlg = { { nullptr } };
    priv->state_saved = false;
    priv->gcmd_shortcuts = gnome_cmd_shortcuts_load_from_settings ();

    mw->update_show_toolbar();

    g_signal_connect (mw->fs(LEFT), "dir-changed", G_CALLBACK (on_fs_dir_change), mw);
    g_signal_connect (mw->fs(RIGHT), "dir-changed", G_CALLBACK (on_fs_dir_change), mw);

    g_signal_connect (mw->fs(LEFT), "activate-request", G_CALLBACK (on_fs_activate_request), mw);
    g_signal_connect (mw->fs(RIGHT), "activate-request", G_CALLBACK (on_fs_activate_request), mw);

    mw->fs(LEFT)->update_connections();
    mw->fs(RIGHT)->update_connections();

    gnome_cmd_main_win_load_tabs(mw, gnome_cmd_application);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_event_controller_set_propagation_phase (key_controller, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller (GTK_WIDGET (mw), GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_key_pressed), mw);
}


GnomeCmdFileSelector *GnomeCmdMainWin::fs(FileSelectorID id)
{
    return gnome_cmd_main_win_get_fs (this, id);
}


void GnomeCmdMainWin::update_view()
{
    update_style();
}


void GnomeCmdMainWin::update_style()
{
    auto priv = gnome_cmd_main_win_priv (this);
    g_return_if_fail (priv != NULL);

    fs(LEFT)->update_style();
    fs(RIGHT)->update_style();

    auto cmdline = gnome_cmd_main_win_get_cmdline (this);
    gnome_cmd_cmdline_update_style (cmdline);

    if (auto file_search_dlg = static_cast<GnomeCmdSearchDialog*>(g_weak_ref_get (&priv->file_search_dlg)))
    {
        gnome_cmd_search_dialog_update_style (file_search_dlg);
        g_object_unref (file_search_dlg);
    }
}


void GnomeCmdMainWin::focus_file_lists()
{
    auto priv = gnome_cmd_main_win_priv (this);

    fs(ACTIVE)->set_active(TRUE);
    fs(INACTIVE)->set_active(FALSE);
}


void GnomeCmdMainWin::refocus()
{
    gtk_widget_grab_focus (GTK_WIDGET (fs(ACTIVE)));
}


static gboolean on_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    auto mw = GNOME_CMD_MAIN_WIN (user_data);
    auto priv = gnome_cmd_main_win_priv (mw);

    if (state_is_ctrl_alt (state))
    {
        switch (keyval)
        {
            case GDK_KEY_c:
            case GDK_KEY_C:
                {
                    auto cmdline = gnome_cmd_main_win_get_cmdline (mw);
                    if (gtk_widget_is_visible (GTK_WIDGET (cmdline)) && gnome_cmd_data.options.quick_search == GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER)
                        gtk_widget_grab_focus (GTK_WIDGET (cmdline));
                }
                return TRUE;
        }
    }
    else if (state_is_alt (state))
    {
        switch (keyval)
        {
            case GDK_KEY_F8:
                {
                    auto cmdline = gnome_cmd_main_win_get_cmdline (mw);
                    if (gtk_widget_is_visible (GTK_WIDGET (cmdline)))
                        gnome_cmd_cmdline_show_history (cmdline);
                }
                return TRUE;
            default:
                break;
        }
    }
    else if (state_is_ctrl_shift (state))
    {
        switch (keyval)
        {
            case GDK_KEY_H:
            case GDK_KEY_h:
                {
                    gboolean view;
                    g_object_get (G_OBJECT (mw), "view-hidden-files", &view, nullptr);
                    g_object_set (G_OBJECT (mw), "view-hidden-files", !view, nullptr);
                }
                return TRUE;
            default:
                break;
        }
    }
    else if (state_is_ctrl (state))
    {
        switch (keyval)
        {
            case GDK_KEY_X:
            case GDK_KEY_x:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "edit-cap-cut", nullptr);
                return TRUE;

            case GDK_KEY_C:
            case GDK_KEY_c:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "edit-cap-copy", nullptr);
                return TRUE;

            case GDK_KEY_V:
            case GDK_KEY_v:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "edit-cap-paste", nullptr);
                return TRUE;

            case GDK_KEY_e:
            case GDK_KEY_E:
            case GDK_KEY_Down:
                {
                    auto cmdline = gnome_cmd_main_win_get_cmdline (mw);
                    if (gtk_widget_is_visible (GTK_WIDGET (cmdline)))
                        gnome_cmd_cmdline_show_history (cmdline);
                }
                return TRUE;

            case GDK_KEY_s:
            case GDK_KEY_S:
                gnome_cmd_main_win_show_slide_popup (mw);
                return TRUE;

            case GDK_KEY_u:
            case GDK_KEY_U:
                gnome_cmd_main_win_swap_panels (mw);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_alt_shift (state))
    {
        switch (keyval)
        {
            case GDK_KEY_P:
            case GDK_KEY_p:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "plugins-configure", nullptr);
                break;

            case GDK_KEY_f:
            case GDK_KEY_F:
            {
                GListModel *connections = gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ());
                guint len = g_list_model_get_n_items (connections);
                GnomeCmdCon *remote_con = nullptr;
                for (guint i = 0; i < len; ++i)
                {
                    GnomeCmdCon *con = GNOME_CMD_CON (g_list_model_get_item (connections, i));
                    if (GNOME_CMD_IS_CON_REMOTE (con))
                    {
                        remote_con = con;
                        break;
                    }
                }
                mw->fs(ACTIVE)->set_connection(remote_con);
            }
            break;

            default:
                break;
        }
    }
    else if (state_is_blank (state))
    {
        switch (keyval)
        {
            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                gnome_cmd_main_win_switch_fs (mw, mw->fs(INACTIVE));
                return TRUE;

            case GDK_KEY_F1:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "help-help", nullptr);
                return TRUE;

            case GDK_KEY_F2:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "file-rename", nullptr);
                return TRUE;

            case GDK_KEY_F3:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "file-view", nullptr);
                return TRUE;

            case GDK_KEY_F4:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "file-edit", nullptr);
                return TRUE;

            case GDK_KEY_F5:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "file-copy", nullptr);
                return TRUE;

            case GDK_KEY_F6:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "file-move", nullptr);
                return TRUE;

            case GDK_KEY_F7:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "file-mkdir", nullptr);
                return TRUE;

            case GDK_KEY_F8:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "file-delete", nullptr);
                return TRUE;

            case GDK_KEY_F9:
                g_action_group_activate_action (G_ACTION_GROUP (mw), "file-search", nullptr);
                return TRUE;

            case GDK_KEY_Escape:
                {
                    auto cmdline = gnome_cmd_main_win_get_cmdline (mw);
                    if (gtk_widget_is_visible (GTK_WIDGET (cmdline)))
                        gnome_cmd_cmdline_set_text (cmdline, "");
                }
                return TRUE;

            default:
                break;
        }
    }

    return gnome_cmd_shortcuts_handle_key_event (priv->gcmd_shortcuts, mw, keyval, state);
}


void GnomeCmdMainWin::change_connection(FileSelectorID id)
{
    GnomeCmdFileSelector *fselector = this->fs(id);

    gnome_cmd_main_win_switch_fs (this, fselector);
    gnome_cmd_file_selector_activate_connection_list (fselector);
}


void GnomeCmdMainWin::set_fs_directory_to_opposite(FileSelectorID fsID)
{
    GnomeCmdFileSelector *fselector =  this->fs(fsID);
    GnomeCmdFileSelector *other = this->fs(!fsID);

    GnomeCmdDir *dir = other->get_directory();
    gboolean fs_is_active = fselector->is_active();

    if (!fs_is_active)
    {
        GnomeCmdFile *file = other->file_list()->get_selected_file();

        if (file && (file->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY))
            dir = GNOME_CMD_IS_DIR (file) ? GNOME_CMD_DIR (file) : gnome_cmd_dir_new_from_gfileinfo (file->get_file_info(), dir);
    }

    if (gnome_cmd_file_selector_is_current_tab_locked (fselector))
        fselector->new_tab(dir);
    else
        fselector->file_list()->set_connection(other->get_connection(), dir);

    other->set_active(!fs_is_active);
    fselector->set_active(fs_is_active);
}


void GnomeCmdMainWin::update_show_toolbar()
{
    gnome_cmd_main_win_update_drop_con_button (this, fs(ACTIVE)->file_list());
}


void GnomeCmdMainWin::update_cmdline()
{
    auto cmdline = gnome_cmd_main_win_get_cmdline(this);
    gchar *dpath = gnome_cmd_dir_get_display_path (fs(ACTIVE)->get_directory());
    gnome_cmd_cmdline_set_dir (cmdline, dpath);
    g_free (dpath);
}


GnomeCmdSearchDialog *GnomeCmdMainWin::get_or_create_search_dialog ()
{
    auto priv = gnome_cmd_main_win_priv (this);
    auto dlg = static_cast<GnomeCmdSearchDialog*>(g_weak_ref_get (&priv->file_search_dlg));
    if (!dlg)
    {
        dlg = gnome_cmd_search_dialog_new (&gnome_cmd_data.search_defaults,
            gnome_cmd_main_win_get_file_metadata_service (this),
            gnome_cmd_data.options.search_window_is_transient ? GTK_WINDOW (this) : nullptr);
        g_weak_ref_set (&priv->file_search_dlg, dlg);
    }
    return dlg;
}


GnomeCmdAdvrenameDialog *GnomeCmdMainWin::get_or_create_advrename_dialog ()
{
    auto priv = gnome_cmd_main_win_priv (this);
    auto dlg = static_cast<GnomeCmdAdvrenameDialog*>(g_weak_ref_get (&priv->advrename_dlg));
    if (!dlg)
    {
        auto file_metadata_service = gnome_cmd_main_win_get_file_metadata_service (this);
        dlg = gnome_cmd_advrename_dialog_new(&gnome_cmd_data.advrename_defaults, file_metadata_service, *this);
        g_weak_ref_set (&priv->advrename_dlg, dlg);
    }
    return dlg;
}

// FFI

void gnome_cmd_main_win_change_connection(GnomeCmdMainWin *main_win, FileSelectorID id)
{
    main_win->change_connection(id);
}

void gnome_cmd_main_win_focus_file_lists(GnomeCmdMainWin *main_win)
{
    return main_win->focus_file_lists();
}

void gnome_cmd_main_win_update_view(GnomeCmdMainWin *main_win)
{
    main_win->update_view();
}

GnomeCmdShortcuts *gnome_cmd_main_win_shortcuts(GnomeCmdMainWin *main_win)
{
    auto priv = gnome_cmd_main_win_priv (main_win);
    return priv->gcmd_shortcuts;
}
