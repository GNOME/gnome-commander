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
#include "gnome-cmd-data.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-user-actions.h"
#include "history.h"
#include "utils.h"
#include "widget-factory.h"

using namespace std;


struct GnomeCmdFileSelectorPrivate
{
    GtkWidget *dir_indicator;
    GtkWidget *dir_label;
    GtkWidget *info_label;
    GtkWidget *con_dropdown;
    GtkWidget *vol_label;

    GnomeCmdFileList *list;

    GtkWidget *connection_bar;
    GtkWidget *filter_box {nullptr};

    gboolean active {FALSE};
    gboolean realized {FALSE};
    gboolean select_connection_in_progress {FALSE};
};


#define DIR_CHANGED_SIGNAL          "dir-changed"
#define LIST_CLICKED_SIGNAL         "list-clicked"
#define ACTIVATE_REQUEST_SIGNAL     "activate-request"

/*******************************
 * Utility functions
 *******************************/

extern "C" gboolean mime_exec_file (GtkWindow *parent_window, GnomeCmdFile *f);
extern "C" GType gnome_cmd_connection_bar_get_type();
extern "C" gchar *gnome_cmd_file_list_stats(GnomeCmdFileList *list);
extern "C" GtkNotebook *gnome_cmd_file_selector_get_notebook (GnomeCmdFileSelector *fs);
extern "C" void gnome_cmd_file_selector_update_show_tabs (GnomeCmdFileSelector *fs);


static GnomeCmdFileSelectorPrivate *file_selector_priv (GnomeCmdFileSelector *fs)
{
    return (GnomeCmdFileSelectorPrivate *) g_object_get_data (G_OBJECT (fs), "priv");
}


inline void GnomeCmdFileSelector::update_selected_files_label()
{
    auto priv = file_selector_priv (this);

    gchar *info_str = gnome_cmd_file_list_stats (priv->list);
    gtk_label_set_text (GTK_LABEL (priv->info_label), info_str);
    g_free (info_str);
}


inline void GnomeCmdFileSelector::update_files()
{
    auto priv = file_selector_priv (this);

    GnomeCmdDir *dir = get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    priv->list->show_files(dir);

    if (priv->realized)
        update_selected_files_label();
    if (priv->active)
        priv->list->select_row(0);
}


inline void GnomeCmdFileSelector::update_direntry()
{
    auto priv = file_selector_priv (this);

    GnomeCmdDir *dir = get_directory();

    if (!dir)
        return;

    gnome_cmd_dir_indicator_set_dir (GNOME_CMD_DIR_INDICATOR (priv->dir_indicator), dir);
}


inline void GnomeCmdFileSelector::update_vol_label()
{
    auto priv = file_selector_priv (this);

    GnomeCmdCon *con = get_connection();
    if (!con || !gnome_cmd_con_can_show_free_space (con))
        return;

    GnomeCmdDir *dir = get_directory();
    if (!dir)
        return;

    gchar *s = gnome_cmd_file_get_free_space (GNOME_CMD_FILE (dir));
    gchar *text;
    if (s)
    {
        text = g_strdup_printf(_("%s free"), s);
        g_free (s);
    }
    else
    {
        text = g_strdup(_("Unknown disk usage"));
    }
    gtk_label_set_text (GTK_LABEL (priv->vol_label), text);
    g_free (text);
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


GListStore *create_connections_store ()
{
    GListStore *store = g_list_store_new (GNOME_CMD_TYPE_CON);

    GListModel *all_cons = gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ());

    guint n = g_list_model_get_n_items (all_cons);
    for (guint i = 0; i < n; ++i)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (g_list_model_get_item (all_cons, i));

        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
            && !GNOME_CMD_IS_CON_SMB (con))  continue;

        g_list_store_append (store, con);
    }

    return store;
}


void con_dropdown_item_setup (GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
    auto list_item = GTK_LIST_ITEM (object);

    auto hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append (GTK_BOX (hbox), gtk_image_new ());
    gtk_box_append (GTK_BOX (hbox), gtk_label_new (nullptr));

    gtk_list_item_set_child (list_item, hbox);
}


void con_dropdown_item_bind (GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
    auto list_item = GTK_LIST_ITEM (object);
    auto con = GNOME_CMD_CON (gtk_list_item_get_item (list_item));
    auto hbox = gtk_list_item_get_child (list_item);

    auto image = GTK_IMAGE (gtk_widget_get_first_child (hbox));
    auto label = GTK_LABEL (gtk_widget_get_next_sibling (GTK_WIDGET (image)));

    GIcon *icon = gnome_cmd_con_get_go_icon (con);
    gtk_image_set_from_gicon (image, icon);
    g_object_unref (icon);
    gtk_label_set_text (label, gnome_cmd_con_get_alias (con));
}


GtkListItemFactory *con_dropdown_factory ()
{
    auto factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (con_dropdown_item_setup), nullptr);
    g_signal_connect (factory, "bind", G_CALLBACK (con_dropdown_item_bind), nullptr);
    return factory;
}


/*******************************
 * Callbacks
 *******************************/

static void on_con_list_list_changed (GnomeCmdConList *con_list, GnomeCmdFileSelector *fs)
{
    fs->update_connections();
}


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


static void on_navigate (GnomeCmdDirIndicator *indicator, const gchar *path, gboolean new_tab, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (new_tab || gnome_cmd_file_selector_is_current_tab_locked (fs))
    {
        GnomeCmdCon *con = fs->get_connection();
        GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, path));
        fs->new_tab(dir);
    }
    else
    {
        fs->goto_directory (path);
    }

    g_signal_emit_by_name (fs, ACTIVATE_REQUEST_SIGNAL);
}


static void on_con_btn_clicked (GtkWidget *button, GnomeCmdCon *con, gboolean new_tab, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (user_data));
    auto fs = static_cast<GnomeCmdFileSelector*>(user_data);

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (new_tab || gnome_cmd_file_selector_is_current_tab_locked (fs))
        fs->new_tab(gnome_cmd_con_get_default_dir (con));

    fs->set_connection(con, gnome_cmd_con_get_default_dir (con));

    g_signal_emit_by_name (fs, ACTIVATE_REQUEST_SIGNAL);
}


static void on_realize (GnomeCmdFileSelector *fs, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    auto priv = file_selector_priv (fs);

    priv->realized = TRUE;

    fs->update_connections();
}


static gboolean select_connection (GnomeCmdFileSelector *fs, GnomeCmdCon *con)
{
    auto priv = file_selector_priv (fs);

    GListStore *store = G_LIST_STORE (gtk_drop_down_get_model (GTK_DROP_DOWN (priv->con_dropdown)));
    guint position;
    if (g_list_store_find (store, con, &position))
    {
        priv->select_connection_in_progress = TRUE;
        gtk_drop_down_set_selected (GTK_DROP_DOWN (priv->con_dropdown), position);
        priv->select_connection_in_progress = FALSE;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


extern "C" void on_notebook_switch_page (GnomeCmdFileSelector *fs, guint n)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    auto priv = file_selector_priv (fs);

    GnomeCmdDir *prev_dir = fs->get_directory();
    GnomeCmdCon *prev_con = fs->get_connection();

    priv->list = fs->file_list(n);
    fs->update_direntry();
    fs->update_selected_files_label();
    fs->update_vol_label();
    fs->update_style();

    if (prev_dir!=fs->get_directory())
        g_signal_emit_by_name (fs, DIR_CHANGED_SIGNAL, fs->get_directory());

    if (prev_con!=fs->get_connection())
        select_connection (fs, fs->get_connection());
}


static void on_list_list_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, GnomeCmdFileSelector *fs)
{
    switch (event->button)
    {
        case 1:
        case 3:
            g_signal_emit_by_name (fs, LIST_CLICKED_SIGNAL);
            break;

        case 2:
            if (gnome_cmd_data.options.middle_mouse_button_mode==GnomeCmdData::MIDDLE_BUTTON_GOES_UP_DIR)
            {
                if (gnome_cmd_file_selector_is_tab_locked (fs, fl))
                    fs->new_tab(gnome_cmd_dir_get_parent (gnome_cmd_file_list_get_directory (fl)));
                else
                    fs->goto_directory("..");
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
    fs->update_vol_label();

    if (gnome_cmd_file_list_get_directory (fl) != dir)  return;

    gnome_cmd_file_selector_update_tab_label (fs, fl);
    fs->update_files();
    fs->update_selected_files_label();

    g_signal_emit_by_name (fs, DIR_CHANGED_SIGNAL, dir);
}


static void on_list_files_changed (GnomeCmdFileList *fl, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->file_list()==fl)
        fs->update_selected_files_label();
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


static gboolean on_list_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    auto fs = static_cast<GnomeCmdFileSelector*>(user_data);
    auto priv = file_selector_priv (fs);

    if (state_is_ctrl_shift (state))
    {
        switch (keyval)
        {
            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                fs->prev_tab();
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_alt (state))
    {
        switch (keyval)
        {
            case GDK_KEY_Left:
            case GDK_KEY_KP_Left:
                gnome_cmd_file_selector_back(fs);
                return TRUE;

            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                gnome_cmd_file_selector_forward(fs);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_ctrl (state))
    {
        switch (keyval)
        {
            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                fs->next_tab();
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_blank (state))
    {
        switch (keyval)
        {
            case GDK_KEY_Left:
            case GDK_KEY_KP_Left:
            case GDK_KEY_BackSpace:
                if (!gnome_cmd_file_selector_is_current_tab_locked (fs))
                {
                    priv->list->invalidate_tree_size();
                    priv->list->goto_directory("..");
                }
                else
                    fs->new_tab(gnome_cmd_dir_get_parent (gnome_cmd_file_list_get_directory (priv->list)));
                return TRUE;

            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                {
                    auto f = priv->list->get_selected_file();
                    if (f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
                        fs->do_file_specific_action (priv->list, f);
                }
                return TRUE;

            default:
                break;
        }
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

    gtk_grid_set_column_spacing (GTK_GRID (fs), 6);

    priv->connection_bar = GTK_WIDGET (g_object_new (gnome_cmd_connection_bar_get_type(),
        "connection-list", gnome_cmd_con_list_get (),
        "hexpand", TRUE,
        nullptr));
    gtk_grid_attach (GTK_GRID (fs), priv->connection_bar, 0, 0, 2, 1);

    GListModel *store = G_LIST_MODEL (create_connections_store ());

    // create the connection combo
    priv->con_dropdown = gtk_drop_down_new (store, nullptr);
    gtk_widget_set_halign (priv->con_dropdown, GTK_ALIGN_START);
    auto factory = con_dropdown_factory();
    gtk_drop_down_set_factory (GTK_DROP_DOWN (priv->con_dropdown), factory);
    g_object_unref (factory);

    // create the free space on volume label
    priv->vol_label = gtk_label_new ("");
    gtk_widget_set_halign (priv->vol_label, GTK_ALIGN_END);
    gtk_widget_set_valign (priv->vol_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (priv->vol_label, 6);
    gtk_widget_set_margin_end (priv->vol_label, 6);
    gtk_widget_set_hexpand (priv->vol_label, TRUE);

    // create the directory indicator
    priv->dir_indicator = gnome_cmd_dir_indicator_new (fs);

    // create the info label
    priv->info_label = gtk_label_new ("not initialized");
    gtk_label_set_ellipsize (GTK_LABEL (priv->info_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign (priv->info_label, GTK_ALIGN_START);
    gtk_widget_set_valign (priv->info_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (GTK_WIDGET (priv->info_label), 6);
    gtk_widget_set_margin_end (GTK_WIDGET (priv->info_label), 6);

    // pack the widgets
    gtk_grid_attach (GTK_GRID (fs), priv->con_dropdown, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (fs), priv->vol_label, 1, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (fs), priv->dir_indicator, 0, 2, 2, 1);
    gtk_grid_attach (GTK_GRID (fs), GTK_WIDGET (gnome_cmd_file_selector_get_notebook (fs)), 0, 3, 2, 1);
    gtk_grid_attach (GTK_GRID (fs), priv->info_label, 0, 4, 1, 1);

    // connect signals
    g_signal_connect (fs, "realize", G_CALLBACK (on_realize), fs);
    g_signal_connect (priv->connection_bar, "clicked", G_CALLBACK (on_con_btn_clicked), fs);
    g_signal_connect (priv->con_dropdown, "notify::selected-item", G_CALLBACK (on_con_combo_item_selected), fs);
    g_signal_connect (priv->dir_indicator, "navigate", G_CALLBACK (on_navigate), fs);
    g_signal_connect (gnome_cmd_con_list_get (), "list-changed", G_CALLBACK (on_con_list_list_changed), fs);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_event_controller_set_propagation_phase (key_controller, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller (GTK_WIDGET (fs), GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_list_key_pressed), fs);

    fs->update_style();
}

/***********************************
 * Public functions
 ***********************************/

void GnomeCmdFileSelector::set_active(gboolean value)
{
    auto priv = file_selector_priv (this);

    priv->active = value;

    if (value)
        gtk_widget_grab_focus (GTK_WIDGET (priv->list));

    gnome_cmd_dir_indicator_set_active (GNOME_CMD_DIR_INDICATOR (priv->dir_indicator), value);
}


void GnomeCmdFileSelector::update_connections()
{
    auto priv = file_selector_priv (this);

    if (!priv->realized)
        return;

    priv->select_connection_in_progress = TRUE;
    gtk_drop_down_set_model (GTK_DROP_DOWN (priv->con_dropdown), G_LIST_MODEL (create_connections_store ()));
    priv->select_connection_in_progress = FALSE;

    // If the connection is no longer available use the home connection
    if (!select_connection (this, get_connection()))
        set_connection (get_home_con ());
}


void GnomeCmdFileSelector::update_style()
{
    auto priv = file_selector_priv (this);

    if (priv->list)
        priv->list->update_style();

    if (priv->realized)
        update_files();

    gnome_cmd_file_selector_update_show_tabs (this);

    GtkNotebook *notebook = gnome_cmd_file_selector_get_notebook (this);
    int tabs = gtk_notebook_get_n_pages (notebook);
    for (int i = 0; i < tabs; ++i)
    {
        auto fl = file_list(i);

        fl->update_style();

        gnome_cmd_file_selector_update_tab_label (this, fl);
    }

    update_connections();
}


void gnome_cmd_file_selector_update_show_devlist(GnomeCmdFileSelector *fs, gboolean visible)
{
    auto priv = file_selector_priv (fs);

    gtk_widget_set_visible (priv->con_dropdown, visible);
    auto lm = gtk_widget_get_layout_manager (GTK_WIDGET (fs));
    auto lc = gtk_layout_manager_get_layout_child (lm, priv->vol_label);
    gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), visible ? 1 : 4);
}


static void on_filter_box_close (GtkButton *btn, GnomeCmdFileSelector *fs)
{
    auto priv = file_selector_priv (fs);
    if (!priv->filter_box) return;

    gtk_widget_set_visible (priv->filter_box, FALSE);
}


static gboolean on_filter_box_keypressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    auto fs = static_cast<GnomeCmdFileSelector*>(user_data);

    if (state_is_blank (state) && keyval == GDK_KEY_Escape)
    {
        on_filter_box_close (nullptr, fs);
        return TRUE;
    }

    return FALSE;
}


void GnomeCmdFileSelector::show_filter()
{
    auto priv = file_selector_priv (this);

    if (!priv->filter_box)
    {
        priv->filter_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_margin_start (priv->filter_box, 6);
        gtk_widget_set_margin_end (priv->filter_box, 6);

        GtkWidget *label = create_label (*this, _("Filter:"));
        GtkWidget *entry = create_entry (*this, "entry", "");
        gtk_widget_set_hexpand (entry, TRUE);
        GtkWidget *close_btn = gtk_button_new_with_mnemonic ("x");
        g_signal_connect (close_btn, "clicked", G_CALLBACK (on_filter_box_close), this);

        GtkEventController *key_controller = gtk_event_controller_key_new ();
        gtk_widget_add_controller (entry, GTK_EVENT_CONTROLLER (key_controller));
        g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_filter_box_keypressed), this);

        gtk_box_append (GTK_BOX (priv->filter_box), label);
        gtk_box_append (GTK_BOX (priv->filter_box), entry);
        gtk_box_append (GTK_BOX (priv->filter_box), close_btn);

        gtk_grid_attach (GTK_GRID (this), priv->filter_box, 0, 5, 2, 1);

        gtk_widget_grab_focus (entry);
    }

    gtk_widget_set_visible (priv->filter_box, TRUE);
}


gboolean GnomeCmdFileSelector::is_active()
{
    auto priv = file_selector_priv (this);
    return priv->active;
}


extern "C" GType gnome_cmd_tab_label_get_type();


GtkWidget *GnomeCmdFileSelector::new_tab(GnomeCmdDir *dir, GnomeCmdFileList::ColumnID sort_col, GtkSortType sort_order, gboolean locked, gboolean activate)
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
        gtk_widget_grab_focus (*fl);
    }

    g_signal_connect (fl, "list-clicked", G_CALLBACK (on_list_list_clicked), this);

    return GTK_WIDGET (fl);
}


void GnomeCmdFileSelector::prev_tab()
{
    GtkNotebook *notebook = gnome_cmd_file_selector_get_notebook (this);
    if (gtk_notebook_get_current_page (notebook) > 0)
        gtk_notebook_prev_page (notebook);
    else
        if (gtk_notebook_get_n_pages (notebook) > 1)
            gtk_notebook_set_current_page (notebook, -1);
}


void GnomeCmdFileSelector::next_tab()
{
    GtkNotebook *notebook = gnome_cmd_file_selector_get_notebook (this);
    gint n = gtk_notebook_get_n_pages (notebook);

    if (gtk_notebook_get_current_page (notebook) + 1 < n)
        gtk_notebook_next_page (notebook);
    else
        if (n > 1)
            gtk_notebook_set_current_page (notebook, 0);
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
    auto priv = file_selector_priv (fs);
    GtkNotebook *notebook = gnome_cmd_file_selector_get_notebook (fs);
    auto page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), n);
    return GNOME_CMD_FILE_LIST (page);
}

GtkWidget *gnome_cmd_file_selector_connection_bar(GnomeCmdFileSelector *fs)
{
    auto priv = file_selector_priv (fs);
    return priv->connection_bar;
}

GtkWidget *gnome_cmd_file_selector_new_tab (GnomeCmdFileSelector *fs)
{
    return fs->new_tab();
}

GtkWidget *gnome_cmd_file_selector_new_tab_with_dir (GnomeCmdFileSelector *fs, GnomeCmdDir *dir, gboolean activate)
{
    return fs->new_tab(dir, activate);
}

GtkWidget *gnome_cmd_file_selector_new_tab_full (GnomeCmdFileSelector *fs, GnomeCmdDir *dir, gint sort_col, gint sort_order, gboolean locked, gboolean activate)
{
    return fs->new_tab(dir, (GnomeCmdFileList::ColumnID) sort_col, (GtkSortType) sort_order, locked, activate);
}

gboolean gnome_cmd_file_selector_is_active (GnomeCmdFileSelector *fs)
{
    return fs->is_active();
}

void gnome_cmd_file_selector_set_active (GnomeCmdFileSelector *fs, gboolean active)
{
    fs->set_active(active);
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

void gnome_cmd_file_selector_show_bookmarks (GnomeCmdFileSelector *fs)
{
    auto priv = file_selector_priv (fs);
    gnome_cmd_dir_indicator_show_bookmarks (GNOME_CMD_DIR_INDICATOR (priv->dir_indicator));
}

void gnome_cmd_file_selector_show_history (GnomeCmdFileSelector *fs)
{
    auto priv = file_selector_priv (fs);
    gnome_cmd_dir_indicator_show_history (GNOME_CMD_DIR_INDICATOR (priv->dir_indicator));
}

void gnome_cmd_file_selector_activate_connection_list (GnomeCmdFileSelector *fs)
{
    auto priv = file_selector_priv (fs);
    if (gtk_widget_is_visible (GTK_WIDGET (priv->con_dropdown)))
    {
        g_signal_emit_by_name (priv->con_dropdown, "activate");
        gtk_widget_grab_focus (priv->con_dropdown);
    }
}

void gnome_cmd_file_selector_update_style (GnomeCmdFileSelector *fs)
{
    fs->update_style();
}
