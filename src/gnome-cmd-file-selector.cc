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
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-user-actions.h"
#include "history.h"
#include "utils.h"
#include "widget-factory.h"

using namespace std;


struct GnomeCmdFileSelectorClass
{
    GtkBoxClass parent_class;

    void (* dir_changed) (GnomeCmdFileSelector *fs, GnomeCmdDir *dir);
    void (* list_clicked) (GnomeCmdFileSelector *fs);
};


class GnomeCmdFileSelector::Private
{
  public:

    GtkWidget *connection_bar;
    GtkWidget *filter_box {nullptr};

    gboolean active {FALSE};
    gboolean realized {FALSE};
    gboolean select_connection_in_progress {FALSE};
};

enum {
    DIR_CHANGED,
    LIST_CLICKED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GnomeCmdFileSelector, gnome_cmd_file_selector, GTK_TYPE_BOX)


/*******************************
 * Utility functions
 *******************************/

extern "C" gboolean mime_exec_file (GtkWindow *parent_window, GnomeCmdFile *f);
extern "C" GType gnome_cmd_connection_bar_get_type();
extern "C" gchar *gnome_cmd_file_list_stats(GnomeCmdFileList *list);

inline void GnomeCmdFileSelector::update_selected_files_label()
{
    gchar *info_str = gnome_cmd_file_list_stats (list);
    gtk_label_set_text (GTK_LABEL (info_label), info_str);
    g_free (info_str);
}


inline void GnomeCmdFileSelector::update_files()
{
    GnomeCmdDir *dir = get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    list->show_files(dir);

    if (priv->realized)
        update_selected_files_label();
    if (priv->active)
        list->select_row(0);
}


inline void GnomeCmdFileSelector::update_direntry()
{
    GnomeCmdDir *dir = get_directory();

    if (!dir)
        return;

    gnome_cmd_dir_indicator_set_dir (GNOME_CMD_DIR_INDICATOR (dir_indicator), dir);
}


inline void GnomeCmdFileSelector::update_vol_label()
{
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
    gtk_label_set_text (GTK_LABEL (vol_label), text);
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


inline void add_file_to_cmdline (GnomeCmdFileList *fl, gboolean fullpath)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    auto cmdline = gnome_cmd_main_win_get_cmdline (main_win);
    if (f && gtk_widget_is_visible (GTK_WIDGET (cmdline)))
    {
        gchar *text = fullpath ? f->get_quoted_real_path() : f->get_quoted_name();

        gnome_cmd_cmdline_append_text (cmdline, text);
        gtk_widget_grab_focus (GTK_WIDGET (cmdline));
        g_free (text);
    }
}


inline void add_cwd_to_cmdline (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    auto cmdline = gnome_cmd_main_win_get_cmdline (main_win);
    if (gtk_widget_is_visible (GTK_WIDGET (cmdline)))
    {
        gchar *dpath = GNOME_CMD_FILE (gnome_cmd_file_list_get_directory (fl))->get_real_path();
        gnome_cmd_cmdline_append_text (cmdline, dpath);
        g_free (dpath);

        gtk_widget_grab_focus (GTK_WIDGET (cmdline));
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
    if (fs->priv->select_connection_in_progress)
        return;

    GnomeCmdCon *con = GNOME_CMD_CON (gtk_drop_down_get_selected_item (con_dropdown));
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gnome_cmd_main_win_switch_fs (main_win, fs);

    GdkModifierType mask = get_modifiers_state();

    if (mask & GDK_CONTROL_MASK || gnome_cmd_file_selector_is_current_tab_locked (fs))
        fs->new_tab(gnome_cmd_con_get_default_dir (con));
    else
        fs->set_connection(con, gnome_cmd_con_get_default_dir (con));

    main_win->refocus();
}


static void on_navigate (GnomeCmdDirIndicator *indicator, const gchar *path, gboolean new_tab, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    gnome_cmd_main_win_switch_fs (main_win, fs);

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
}


static void on_con_btn_clicked (GtkWidget *button, GnomeCmdCon *con, gboolean new_tab, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (user_data));
    auto fs = static_cast<GnomeCmdFileSelector*>(user_data);

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gnome_cmd_main_win_switch_fs (main_win, fs);

    if (new_tab || gnome_cmd_file_selector_is_current_tab_locked (fs))
        fs->new_tab(gnome_cmd_con_get_default_dir (con));

    fs->set_connection(con, gnome_cmd_con_get_default_dir (con));
}


static void on_realize (GnomeCmdFileSelector *fs, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    fs->priv->realized = TRUE;

    fs->update_connections();
}


static gboolean select_connection (GnomeCmdFileSelector *fs, GnomeCmdCon *con)
{
    GListStore *store = G_LIST_STORE (gtk_drop_down_get_model (GTK_DROP_DOWN (fs->con_dropdown)));
    guint position;
    if (g_list_store_find (store, con, &position))
    {
        fs->priv->select_connection_in_progress = TRUE;
        gtk_drop_down_set_selected (GTK_DROP_DOWN (fs->con_dropdown), position);
        fs->priv->select_connection_in_progress = FALSE;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


static void on_notebook_switch_page (GtkNotebook *notebook, gpointer page, guint n, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdDir *prev_dir = fs->get_directory();
    GnomeCmdCon *prev_con = fs->get_connection();

    fs->list = fs->file_list(n);
    fs->update_direntry();
    fs->update_selected_files_label();
    fs->update_vol_label();
    fs->update_style();

    if (prev_dir!=fs->get_directory())
        g_signal_emit (fs, signals[DIR_CHANGED], 0, fs->get_directory());

    if (prev_con!=fs->get_connection())
        select_connection (fs, fs->get_connection());
}


static void on_list_list_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, GnomeCmdFileSelector *fs)
{
    switch (event->button)
    {
        case 1:
        case 3:
            g_signal_emit (fs, signals[LIST_CLICKED], 0);
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

    fs->update_tab_label(fl);
    fs->update_files();
    fs->update_selected_files_label();

    g_signal_emit (fs, signals[DIR_CHANGED], 0, dir);
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


static gboolean on_list_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    auto fs = static_cast<GnomeCmdFileSelector*>(user_data);

    if (state_is_ctrl_shift (state))
    {
        switch (keyval)
        {
            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                fs->prev_tab();
                return TRUE;

            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                add_file_to_cmdline (fs->list, TRUE);
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
            case GDK_KEY_P:
            case GDK_KEY_p:
                add_cwd_to_cmdline (fs->list);
                return TRUE;

            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                fs->next_tab();
                return TRUE;

            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                add_file_to_cmdline (fs->list, FALSE);
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
                    fs->list->invalidate_tree_size();
                    fs->list->goto_directory("..");
                }
                else
                    fs->new_tab(gnome_cmd_dir_get_parent (gnome_cmd_file_list_get_directory (fs->list)));
                return TRUE;

            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                {
                    auto f = fs->list->get_selected_file();
                    if (f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
                        fs->do_file_specific_action (fs->list, f);
                }
                return TRUE;

            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                {
                    auto cmdline = gnome_cmd_main_win_get_cmdline (main_win);
                    if (gtk_widget_is_visible (GTK_WIDGET (cmdline)) && !gnome_cmd_cmdline_is_empty (cmdline))
                        gnome_cmd_cmdline_exec (cmdline);
                    else
                        fs->do_file_specific_action (fs->list, fs->list->get_focused_file());
                }
                return TRUE;

            default:
                break;
        }
    }

    return FALSE;
}


extern "C" void on_notebook_button_pressed_r (GtkGestureClick *gesture,
                                              GnomeCmdFileSelector *fs,
                                              GtkNotebook *notebook,
                                              int n_press,
                                              double x,
                                              double y);


static void on_notebook_button_pressed (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    auto fs = static_cast<GnomeCmdFileSelector*>(user_data);
    GtkNotebook *notebook = GTK_NOTEBOOK (fs->notebook);
    on_notebook_button_pressed_r(gesture, fs, notebook, n_press, x, y);
}


static void toggle_tab_lock (GtkWidget* widget, const char* action_name, GVariant* parameter)
{
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (widget);
    gint tab_index = g_variant_get_int32 (parameter);

    GnomeCmdFileList *fl = fs->file_list(tab_index);
    if (fl)
    {
        gboolean locked = gnome_cmd_file_selector_is_tab_locked (fs, fl);
        gnome_cmd_file_selector_set_tab_locked (fs, fl, !locked);
        fs->update_tab_label(fl);
    }
}


static void refresh_tab (GtkWidget* widget, const char* action_name, GVariant* parameter)
{
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (widget);
    gint tab_index = g_variant_get_int32 (parameter);

    GnomeCmdFileList *fl = fs->file_list(tab_index);
    if (fl)
        fl->reload();
}


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (object);

    g_signal_handlers_disconnect_by_data (fs->notebook, fs);

    if (fs->priv)
    {
        delete fs->priv;
        fs->priv = nullptr;
    }

    G_OBJECT_CLASS (gnome_cmd_file_selector_parent_class)->dispose (object);
}


static void gnome_cmd_file_selector_class_init (GnomeCmdFileSelectorClass *klass)
{
    gtk_widget_class_install_action (GTK_WIDGET_CLASS (klass), "fs.toggle-tab-lock", "i", toggle_tab_lock);
    gtk_widget_class_install_action (GTK_WIDGET_CLASS (klass), "fs.refresh-tab", "i", refresh_tab);

    signals[DIR_CHANGED] =
        g_signal_new ("dir-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileSelectorClass, dir_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[LIST_CLICKED] =
        g_signal_new ("list-clicked",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileSelectorClass, list_clicked),
            nullptr, nullptr,
            nullptr,
            G_TYPE_NONE,
            0);

    G_OBJECT_CLASS (klass)->dispose = dispose;
}


static void gnome_cmd_file_selector_init (GnomeCmdFileSelector *fs)
{
    fs->list = nullptr;

    fs->priv = new GnomeCmdFileSelector::Private;

    g_object_set (fs, "orientation", GTK_ORIENTATION_VERTICAL, NULL);

    fs->priv->connection_bar = GTK_WIDGET (g_object_new (gnome_cmd_connection_bar_get_type(),
        "connection-list", gnome_cmd_con_list_get (),
        nullptr));
    gtk_box_append (GTK_BOX (fs), fs->priv->connection_bar);

    // create the box used for packing the con_combo and information
    fs->con_hbox = create_hbox (*fs, FALSE, 2);

    // create the notebook and the first tab
    fs->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
    gtk_notebook_set_show_tabs (fs->notebook, FALSE);
    gtk_notebook_set_scrollable (fs->notebook, TRUE);
    gtk_notebook_set_show_border (fs->notebook, FALSE);
    gtk_widget_set_vexpand (GTK_WIDGET (fs->notebook), TRUE);

    GListModel *store = G_LIST_MODEL (create_connections_store ());

    // create the connection combo
    fs->con_dropdown = gtk_drop_down_new (store, nullptr);
    auto factory = con_dropdown_factory();
    gtk_drop_down_set_factory (GTK_DROP_DOWN (fs->con_dropdown), factory);
    g_object_unref (factory);

    // create the free space on volume label
    fs->vol_label = gtk_label_new ("");
    gtk_widget_set_halign (fs->vol_label, GTK_ALIGN_END);
    gtk_widget_set_valign (fs->vol_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (fs->vol_label, 6);
    gtk_widget_set_margin_end (fs->vol_label, 6);
    gtk_widget_set_hexpand (fs->vol_label, TRUE);

    // create the directory indicator
    fs->dir_indicator = gnome_cmd_dir_indicator_new (fs);

    // create the info label
    fs->info_label = gtk_label_new ("not initialized");
    gtk_label_set_ellipsize (GTK_LABEL (fs->info_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign (fs->info_label, GTK_ALIGN_START);
    gtk_widget_set_valign (fs->info_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (GTK_WIDGET (fs->info_label), 6);
    gtk_widget_set_margin_end (GTK_WIDGET (fs->info_label), 6);

    // pack the widgets
    GtkWidget *padding = create_hbox (*fs, FALSE, 6);
    gtk_box_append (GTK_BOX (fs), fs->con_hbox);
    gtk_box_append (GTK_BOX (fs), fs->dir_indicator);
    gtk_box_append (GTK_BOX (fs), GTK_WIDGET (fs->notebook));
    gtk_box_append (GTK_BOX (fs), padding);
    gtk_box_append (GTK_BOX (padding), fs->info_label);
    gtk_box_append (GTK_BOX (fs->con_hbox), fs->con_dropdown);
    // changing this value has only an effect when restarting gcmd
    if (gnome_cmd_data.show_devlist)
        gtk_box_append (GTK_BOX (fs->con_hbox), fs->vol_label);
    else
        gtk_box_append (GTK_BOX (padding), fs->vol_label);

    // connect signals
    g_signal_connect (fs, "realize", G_CALLBACK (on_realize), fs);
    g_signal_connect (fs->priv->connection_bar, "clicked", G_CALLBACK (on_con_btn_clicked), fs);
    g_signal_connect (fs->con_dropdown, "notify::selected-item", G_CALLBACK (on_con_combo_item_selected), fs);
    g_signal_connect (fs->dir_indicator, "navigate", G_CALLBACK (on_navigate), fs);
    g_signal_connect (gnome_cmd_con_list_get (), "list-changed", G_CALLBACK (on_con_list_list_changed), fs);
    g_signal_connect (fs->notebook, "switch-page", G_CALLBACK (on_notebook_switch_page), fs);

    GtkGesture *notebook_click = gtk_gesture_click_new ();
    gtk_widget_add_controller (GTK_WIDGET (fs->notebook), GTK_EVENT_CONTROLLER (notebook_click));
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (notebook_click), GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (notebook_click), 0);
    g_signal_connect (notebook_click, "pressed", G_CALLBACK (on_notebook_button_pressed), fs);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_event_controller_set_propagation_phase (key_controller, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller (GTK_WIDGET (fs), GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_list_key_pressed), fs);

    fs->update_show_devlist();
    fs->update_style();
}

/***********************************
 * Public functions
 ***********************************/

void GnomeCmdFileSelector::set_active(gboolean value)
{
    priv->active = value;

    if (value)
        gtk_widget_grab_focus (*list);

    gnome_cmd_dir_indicator_set_active (GNOME_CMD_DIR_INDICATOR (dir_indicator), value);
}


void GnomeCmdFileSelector::update_connections()
{
    if (!priv->realized)
        return;

    priv->select_connection_in_progress = TRUE;
    gtk_drop_down_set_model (GTK_DROP_DOWN (con_dropdown), G_LIST_MODEL (create_connections_store ()));
    priv->select_connection_in_progress = FALSE;

    // If the connection is no longer available use the home connection
    if (!select_connection (this, get_connection()))
        set_connection (get_home_con ());
}


void GnomeCmdFileSelector::update_style()
{
    if (list)
        list->update_style();

    if (priv->realized)
        update_files();

    update_show_tabs();

    int tabs = gtk_notebook_get_n_pages (notebook);
    for (int i = 0; i < tabs; ++i)
    {
        auto fl = file_list(i);

        fl->update_style();

        update_tab_label (fl);
    }

    update_connections();
}


void GnomeCmdFileSelector::update_show_devlist()
{
    if (gnome_cmd_data.show_devlist)
        gtk_widget_show (con_hbox);
    else
        gtk_widget_hide (con_hbox);
}


void GnomeCmdFileSelector::update_show_tabs()
{
    gboolean show = gnome_cmd_data.options.always_show_tabs || gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) > 1;
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), show);
}


static void on_filter_box_close (GtkButton *btn, GnomeCmdFileSelector *fs)
{
    if (!fs->priv->filter_box) return;

    gtk_box_remove (GTK_BOX (fs), fs->priv->filter_box);
    fs->priv->filter_box = nullptr;
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
    if (priv->filter_box) return;

    priv->filter_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start (priv->filter_box, 6);
    gtk_widget_set_margin_end (priv->filter_box, 6);

    GtkWidget *label = create_label (*this, _("Filter:"));
    GtkWidget *entry = create_entry (*this, "entry", "");
    gtk_widget_set_hexpand (entry, TRUE);
    GtkWidget *close_btn = create_button_with_data (*main_win, "x", G_CALLBACK (on_filter_box_close), this);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (entry, GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_filter_box_keypressed), this);

    gtk_box_append (GTK_BOX (priv->filter_box), label);
    gtk_box_append (GTK_BOX (priv->filter_box), entry);
    gtk_box_append (GTK_BOX (priv->filter_box), close_btn);

    gtk_box_append (*this, priv->filter_box);

    gtk_widget_grab_focus (entry);
}


gboolean GnomeCmdFileSelector::is_active()
{
    return priv->active;
}


GtkWidget *GnomeCmdFileSelector::new_tab(GnomeCmdDir *dir, GnomeCmdFileList::ColumnID sort_col, GtkSortType sort_order, gboolean locked, gboolean activate)
{
    // create the list
    GnomeCmdFileList *fl = new GnomeCmdFileList(sort_col,sort_order);

    if (activate)
        this->list = fl;               //  ... update GnomeCmdFileSelector::list to point at newly created tab

    gnome_cmd_file_selector_set_tab_locked (this, fl, locked);
    fl->update_style();

    // hide dir column
    fl->show_column(GnomeCmdFileList::COLUMN_DIR, FALSE);

    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    auto tab_label_pin = gtk_image_new_from_file (PIXMAPS_DIR G_DIR_SEPARATOR_S "pin.png");
    auto tab_label_text = gtk_label_new (nullptr);

    g_object_set_data (G_OBJECT (hbox), "tab_label_pin", tab_label_pin);
    g_object_set_data (G_OBJECT (hbox), "tab_label_text", tab_label_text);

    gtk_box_append (GTK_BOX (hbox), tab_label_pin);
    gtk_box_append (GTK_BOX (hbox), tab_label_text);

    gint n = gtk_notebook_append_page (notebook, GTK_WIDGET (fl), hbox);
    update_show_tabs();

    gtk_notebook_set_tab_reorderable (notebook, GTK_WIDGET (fl), TRUE);

    g_signal_connect (fl, "con-changed", G_CALLBACK (on_list_con_changed), this);
    g_signal_connect (fl, "dir-changed", G_CALLBACK (on_list_dir_changed), this);
    g_signal_connect (fl, "files-changed", G_CALLBACK (on_list_files_changed), this);
    g_signal_connect (fl, "file-activated", G_CALLBACK (on_list_file_activated), this);

    if (dir)
        fl->set_connection(gnome_cmd_file_get_connection (GNOME_CMD_FILE (dir)), dir);

    update_tab_label(fl);

    if (activate)
    {
        gtk_notebook_set_current_page (notebook, n);
        gtk_widget_grab_focus (*fl);
    }

    g_signal_connect (fl, "list-clicked", G_CALLBACK (on_list_list_clicked), this);

    return GTK_WIDGET (fl);
}


void GnomeCmdFileSelector::close_tab()
{
    gint n = gtk_notebook_get_current_page (notebook);
    close_tab(n);
}


void GnomeCmdFileSelector::close_tab(gint n)
{
    if (gtk_notebook_get_n_pages (notebook) > 1)
        gtk_notebook_remove_page (notebook, n);
    update_show_tabs ();
}


void GnomeCmdFileSelector::prev_tab()
{
    if (gtk_notebook_get_current_page (notebook) > 0)
        gtk_notebook_prev_page (notebook);
    else
        if (gtk_notebook_get_n_pages (notebook) > 1)
            gtk_notebook_set_current_page (notebook, -1);
}


void GnomeCmdFileSelector::next_tab()
{
    gint n = gtk_notebook_get_n_pages (notebook);

    if (gtk_notebook_get_current_page (notebook) + 1 < n)
        gtk_notebook_next_page (notebook);
    else
        if (n > 1)
            gtk_notebook_set_current_page (notebook, 0);
}


void GnomeCmdFileSelector::update_tab_label(GnomeCmdFileList *fl)
{
    GnomeCmdDir *dir = gnome_cmd_file_list_get_directory (fl);
    const gchar *name = dir != nullptr ? GNOME_CMD_FILE (dir)->get_name() : nullptr;

    gboolean locked = gnome_cmd_file_selector_is_tab_locked (this, fl);

    GtkWidget *hbox = gtk_notebook_get_tab_label (notebook, GTK_WIDGET (fl));
    GtkWidget *tab_label_pin = GTK_WIDGET (g_object_get_data (G_OBJECT (hbox), "tab_label_pin"));
    GtkWidget *tab_label_text = GTK_WIDGET (g_object_get_data (G_OBJECT (hbox), "tab_label_text"));

    switch (gnome_cmd_data.options.tab_lock_indicator)
    {
        case GnomeCmdData::TAB_LOCK_ICON:
            gtk_widget_set_visible (tab_label_pin, locked);
            gtk_label_set_text (GTK_LABEL (tab_label_text), name);
            break;

        case GnomeCmdData::TAB_LOCK_ASTERISK:
            gtk_widget_hide (tab_label_pin);
            if (locked)
            {
                gchar *s = g_strconcat ("* ", name, nullptr);
                gtk_label_set_text (GTK_LABEL (tab_label_text), s);
                g_free (s);
            }
            else
            {
                gtk_label_set_text (GTK_LABEL (tab_label_text), name);
            }
            break;

        case GnomeCmdData::TAB_LOCK_STYLED_TEXT:
            gtk_widget_hide (tab_label_pin);
            if (locked)
            {
                gchar *s = g_strconcat ("<span foreground='blue'>", name, "</span>", nullptr);
                gtk_label_set_markup (GTK_LABEL (tab_label_text), s);
                g_free (s);
            }
            else
            {
                gtk_label_set_text (GTK_LABEL (tab_label_text), name);
            }
            break;

        default:
            break;
    }
}


// FFI
GnomeCmdFileList *gnome_cmd_file_selector_file_list (GnomeCmdFileSelector *fs)
{
    return fs->file_list();
}

GnomeCmdFileList *gnome_cmd_file_selector_file_list_nth (GnomeCmdFileSelector *fs, gint n)
{
    return fs->file_list(n);
}

extern "C" GtkNotebook *gnome_cmd_file_selector_get_notebook (GnomeCmdFileSelector *fs)
{
    return GTK_NOTEBOOK (fs->notebook);
}

GtkWidget *gnome_cmd_file_selector_connection_bar(GnomeCmdFileSelector *fs)
{
    return fs->priv->connection_bar;
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

void gnome_cmd_file_selector_close_tab (GnomeCmdFileSelector *fs)
{
    fs->close_tab();
}

void gnome_cmd_file_selector_close_tab_nth (GnomeCmdFileSelector *fs, guint n)
{
    fs->close_tab(n);
}

guint gnome_cmd_file_selector_tab_count (GnomeCmdFileSelector *fs)
{
    return gtk_notebook_get_n_pages (GTK_NOTEBOOK (fs->notebook));
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
