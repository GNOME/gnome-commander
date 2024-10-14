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
#include "gnome-cmd-combo.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-user-actions.h"
#include "history.h"
#include "cap.h"
#include "utils.h"
#include "dialogs/gnome-cmd-patternsel-dialog.h"

using namespace std;


struct GnomeCmdFileSelectorClass
{
    GtkBoxClass parent_class;

    void (* dir_changed) (GnomeCmdFileSelector *fs, GnomeCmdDir *dir);
};


class GnomeCmdFileSelector::Private
{
  public:

    GList *old_btns {nullptr};
    GtkWidget *filter_box {nullptr};

    History *dir_history {nullptr};
    gboolean active {FALSE};
    gboolean realized {FALSE};

    //////////////////////////////////////////////////////////////////  ->> GnomeCmdFileList

    gboolean sel_first_file {TRUE};

    GSimpleActionGroup *action_group;
};

enum {DIR_CHANGED, LAST_SIGNAL};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GnomeCmdFileSelector, gnome_cmd_file_selector, GTK_TYPE_BOX)


/*******************************
 * Utility functions
 *******************************/


inline void show_list_popup (GnomeCmdFileSelector *fs, gint x, gint y)
{
    auto menu = MenuBuilder()
        .submenu(_("_New"))
            .item(_("_Directory"),          "win.file_mkdir",                       nullptr, FILETYPEDIR_STOCKID)
            .item(_("_Text File"),          "fs.new-text-file",                     nullptr, FILETYPEREGULARFILE_STOCKID)
        .endsubmenu()
        .section()
            .item(_("_Paste"),              "fs.paste")
        .endsection()
        .section()
            .item(_("Open _terminal here"), "win.command-open-terminal",            nullptr, GTK_TERMINAL_STOCKID)
        .endsection()
        .section()
            .item(_("_Refresh"),            "fs.refresh")
        .endsection()
        .build();

    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu.menu));
    gtk_widget_set_parent (popover, GTK_WIDGET (fs));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
    GdkRectangle rect = { x, y, 0, 0 };
    gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
    gtk_popover_popup (GTK_POPOVER (popover));
}


inline void GnomeCmdFileSelector::update_selected_files_label()
{
    auto all_files = list->get_all_files();

    if (all_files.size() == 0)
        return;

    guint64 sel_bytes = 0;
    guint64 total_bytes = 0;
    gint num_files = 0;
    gint num_dirs = 0;
    gint num_sel_files = 0;
    gint num_sel_dirs = 0;

    GnomeCmdSizeDispMode size_mode = gnome_cmd_data.options.size_disp_mode;
    if (size_mode==GNOME_CMD_SIZE_DISP_MODE_POWERED)
        size_mode = GNOME_CMD_SIZE_DISP_MODE_GROUPED;

    for (auto i = all_files.begin(); i != all_files.end(); ++i)
    {
        GnomeCmdFile *f = *i;

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
        switch (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE))
        {
            case G_FILE_TYPE_DIRECTORY:
                if (!f->is_dotdot)
                {
                    num_dirs++;
                    if (f->has_tree_size())
                        total_bytes += f->get_tree_size();
                }
                break;

            case G_FILE_TYPE_REGULAR:
                num_files++;
                total_bytes += g_file_info_get_size(f->get_file_info());
                break;

            default:
                break;
        }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
    }

    GnomeCmd::Collection<GnomeCmdFile *> marked_files = list->get_marked_files();

    for (GnomeCmd::Collection<GnomeCmdFile *>::const_iterator i=marked_files.begin(); i!=marked_files.end(); ++i)
    {
        GnomeCmdFile *f = *i;

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
        switch (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE))
        {
            case G_FILE_TYPE_DIRECTORY:
                num_sel_dirs++;
                if (f->has_tree_size())
                    sel_bytes += f->get_tree_size();
                break;

            case G_FILE_TYPE_REGULAR:
                num_sel_files++;
                sel_bytes += f->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_STANDARD_SIZE);
                break;

            default:
                break;
        }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
    }

    gchar *sel_str = g_strdup (size2string (sel_bytes/1024, size_mode));
    gchar *total_str = g_strdup (size2string (total_bytes/1024, size_mode));

    gchar *file_str = g_strdup_printf (ngettext("%s of %s kB in %d of %d file",
                                                "%s of %s kB in %d of %d files",
                                                num_files),
                                       sel_str, total_str, num_sel_files, num_files);
    gchar *info_str = g_strdup_printf (ngettext("%s, %d of %d dir selected",
                                                "%s, %d of %d dirs selected",
                                                num_dirs),
                                       file_str, num_sel_dirs, num_dirs);

    gtk_label_set_text (GTK_LABEL (info_label), info_str);

    g_free (sel_str);
    g_free (total_str);
    g_free (file_str);
    g_free (info_str);
}


inline void GnomeCmdFileSelector::update_files()
{
    GnomeCmdDir *dir = get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    list->show_files(dir);
    if (gtk_widget_get_realized (*list))
        gtk_tree_view_scroll_to_point (*list, 0, 0);

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
        if (!fl->locked)
        {
            fl->invalidate_tree_size();

            if (f->is_dotdot)
                fl->goto_directory("..");
            else
                fl->set_directory(GNOME_CMD_DIR (f));
        }
        else
            new_tab(f->is_dotdot ? gnome_cmd_dir_get_parent (fl->cwd) : GNOME_CMD_DIR (f));
    }
}


inline void add_file_to_cmdline (GnomeCmdFileList *fl, gboolean fullpath)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (f && gnome_cmd_data.cmdline_visibility)
    {
        gchar *text = fullpath ? f->get_quoted_real_path() : f->get_quoted_name();

        gnome_cmd_cmdline_append_text (main_win->get_cmdline(), text);
        gnome_cmd_cmdline_focus (main_win->get_cmdline());
        g_free (text);
    }
}


inline void add_cwd_to_cmdline (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (gnome_cmd_data.cmdline_visibility)
    {
        gchar *dpath = GNOME_CMD_FILE (fl->cwd)->get_real_path();
        gnome_cmd_cmdline_append_text (main_win->get_cmdline(), dpath);
        g_free (dpath);

        gnome_cmd_cmdline_focus (main_win->get_cmdline());
    }
}



/*******************************
 * Callbacks
 *******************************/

static void on_con_list_list_changed (GnomeCmdConList *con_list, GnomeCmdFileSelector *fs)
{
    fs->update_connections();
}


static void on_con_combo_item_selected (GnomeCmdCombo *con_combo, GnomeCmdCon *con, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    main_win->switch_fs(fs);

    GdkModifierType mask = get_modifiers_state();

    if (mask & GDK_CONTROL_MASK || fs->file_list()->locked)
        fs->new_tab(gnome_cmd_con_get_default_dir (con));
    else
        fs->set_connection(con, gnome_cmd_con_get_default_dir (con));
}


static void on_navigate (GnomeCmdDirIndicator *indicator, const gchar *path, gboolean new_tab, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    main_win->switch_fs(fs);

    if (new_tab || fs->file_list()->locked)
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


static void on_combo_popwin_hidden (GnomeCmdCombo *combo, gpointer)
{
    main_win->refocus();
}


static void on_con_btn_clicked (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (user_data));
    auto fs = static_cast<GnomeCmdFileSelector*>(user_data);

    if (n_press != 1)
        return;

    gint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    if (button !=1 && button != 2)
        return;

    auto widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
    auto con = static_cast<GnomeCmdCon*> (g_object_get_data (G_OBJECT (widget), "con"));

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    main_win->switch_fs(fs);

    if (button == 2 || get_modifiers_state() & GDK_CONTROL_MASK || fs->file_list()->locked)
        fs->new_tab(gnome_cmd_con_get_default_dir (con));

    fs->set_connection(con, gnome_cmd_con_get_default_dir (con));
}


static void create_con_buttons (GnomeCmdFileSelector *fs)
{
    if (!gnome_cmd_data.show_devbuttons)
        return;

    for (GList *l = fs->priv->old_btns; l; l=l->next)
        gtk_box_remove (GTK_BOX (fs->con_btns_hbox), GTK_WIDGET (l->data));

    g_list_free (fs->priv->old_btns);
    fs->priv->old_btns = nullptr;

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l=l->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (l->data);

        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con) &&
            !GNOME_CMD_IS_CON_SMB (con))  continue;

        GIcon *icon = gnome_cmd_con_get_go_icon (con);

        GtkWidget *btn = create_styled_button (nullptr);
        g_object_set_data (G_OBJECT (btn), "con", con);
        GtkGesture *con_btn_click = gtk_gesture_click_new ();
        gtk_widget_add_controller (GTK_WIDGET (btn), GTK_EVENT_CONTROLLER (con_btn_click));
        gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (con_btn_click), 0);
        g_signal_connect (con_btn_click, "pressed", G_CALLBACK (on_con_btn_clicked), fs);
        gtk_box_append (GTK_BOX (fs->con_btns_hbox), btn);
        gtk_widget_set_can_focus (btn, FALSE);
        fs->priv->old_btns = g_list_append (fs->priv->old_btns, btn);
        gchar *go_text = gnome_cmd_con_get_go_text (con);
        gtk_widget_set_tooltip_text (btn, go_text);
        g_free (go_text);

        GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);

        if (icon)
        {
            GtkWidget *image = gtk_image_new_from_gicon (icon);
            if (image)
            {
                gtk_box_append (GTK_BOX (hbox), image);
            }
        }

        if (!gnome_cmd_data.options.device_only_icon || !icon)
        {
            GtkWidget *label = gtk_label_new (gnome_cmd_con_get_alias (con));
            gtk_box_append (GTK_BOX (hbox), label);
        }

        g_clear_object (&icon);

        gtk_button_set_child (GTK_BUTTON (btn), hbox);
    }
}


static void on_realize (GnomeCmdFileSelector *fs, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    fs->priv->realized = TRUE;

    create_con_buttons (fs);
    fs->update_connections();
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
        fs->con_combo->select_data(fs->get_connection());
}


static void on_list_file_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, GnomeCmdFileSelector *fs)
{
    if (event->n_press == 2 && event->button == 1 && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
        fs->do_file_specific_action (fl, event->file);
}


static void on_list_file_released (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, GnomeCmdFileSelector *fs)
{
    if (event->button == 1 && !fl->modifier_click && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        fs->do_file_specific_action (fl, event->file);
}


static void on_list_list_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, GnomeCmdFileSelector *fs)
{
    switch (event->button)
    {
        case 1:
        case 3:
            main_win->switch_fs(fs);
            break;

        case 2:
            if (gnome_cmd_data.options.middle_mouse_button_mode==GnomeCmdData::MIDDLE_BUTTON_GOES_UP_DIR)
            {
                if (fl->locked)
                    fs->new_tab(gnome_cmd_dir_get_parent (fl->cwd));
                else
                    fs->goto_directory("..");
            }
            else
            {
                if (event->file && event->file->is_dotdot)
                    fs->new_tab(gnome_cmd_dir_get_parent (fl->cwd));
                else
                    fs->new_tab(event->file && event->file->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
                                     ? GNOME_CMD_DIR (event->file)
                                     : fl->cwd);
            }
            break;

        case 6:
        case 8:
            fs->back();
            break;

        case 7:
        case 9:
            fs->forward();
            break;

        default:
            break;
    }
}


static void on_list_empty_space_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, GnomeCmdFileSelector *fs)
{
    if (event->n_press == 1 && event->button == 3)
    {
        double x, y;
        gtk_widget_translate_coordinates (GTK_WIDGET (fl), GTK_WIDGET (fs), event->x, event->y, &x, &y);
        show_list_popup (fs, x, y);
    }
}


static void on_list_con_changed (GnomeCmdFileList *fl, GnomeCmdCon *con, GnomeCmdFileSelector *fs)
{
    fs->priv->dir_history = gnome_cmd_con_get_dir_history (con);
    fs->con_combo->select_data(con);
}


static void on_list_dir_changed (GnomeCmdFileList *fl, GnomeCmdDir *dir, GnomeCmdFileSelector *fs)
{
    if (fs->priv->dir_history && !fs->priv->dir_history->locked())
    {
        gchar *fpath = GNOME_CMD_FILE (dir)->GetPathStringThroughParent();
        fs->priv->dir_history->add(fpath);
        g_free (fpath);
    }

    if (fs->file_list()!=fl)  return;

    fs->update_direntry();
    fs->update_vol_label();

    if (fl->cwd != dir)  return;

    fs->update_tab_label(fl);

    fs->priv->sel_first_file = FALSE;
    fs->update_files();
    fs->priv->sel_first_file = TRUE;

    fs->update_selected_files_label();

    g_signal_emit (fs, signals[DIR_CHANGED], 0, dir);
}


static void on_list_files_changed (GnomeCmdFileList *fl, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->file_list()==fl)
        fs->update_selected_files_label();
}


static gboolean on_list_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    auto fs = static_cast<GnomeCmdFileSelector*>(user_data);

    GnomeCmdKeyPress key_press_event = { .keyval = keyval, .state = state };

    if (fs->file_list()->key_pressed(&key_press_event) ||
        fs->key_pressed(&key_press_event) ||
        main_win->key_pressed(&key_press_event) ||
        gnome_cmd_shortcuts_handle_key_event (gcmd_shortcuts, main_win, keyval, state))
    {
        return TRUE;
    }

    if (state_is_blank (state) || state_is_shift (state))
    {
        if ((keyval>=GDK_KEY_A && keyval<=GDK_KEY_Z) ||
            (keyval>=GDK_KEY_a && keyval<=GDK_KEY_z) ||
            keyval==GDK_KEY_period)
        {
            if (!gnome_cmd_data.cmdline_visibility)
                gnome_cmd_file_list_show_quicksearch (fs->file_list(), (gchar) keyval);
            else
            {
                gchar text[2];
                text[0] = keyval;
                text[1] = '\0';
                gnome_cmd_cmdline_append_text (main_win->get_cmdline(), text);
                gnome_cmd_cmdline_focus (main_win->get_cmdline());
            }
            return TRUE;
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


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (object);

    if (fs->priv)
    {
        delete fs->priv;
        fs->priv = nullptr;
    }

    G_OBJECT_CLASS (gnome_cmd_file_selector_parent_class)->dispose (object);
}


static void gnome_cmd_file_selector_class_init (GnomeCmdFileSelectorClass *klass)
{
    signals[DIR_CHANGED] =
        g_signal_new ("dir-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileSelectorClass, dir_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    G_OBJECT_CLASS (klass)->dispose = dispose;
}


static void on_new_textfile (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GnomeCmdFileSelector *fs = static_cast<GnomeCmdFileSelector *>(user_data);
    gnome_cmd_file_selector_show_new_textfile_dialog (fs);
}


static void on_refresh (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GnomeCmdFileSelector *fs = static_cast<GnomeCmdFileSelector *>(user_data);
    fs->file_list()->reload();
}


static void on_paste (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GnomeCmdFileSelector *fs = static_cast<GnomeCmdFileSelector *>(user_data);
    gnome_cmd_file_selector_cap_paste (fs);
}


static void gnome_cmd_file_selector_init (GnomeCmdFileSelector *fs)
{
    gint string_size = 0;
    gint max_string_size = 150;

    fs->list = nullptr;

    fs->priv = new GnomeCmdFileSelector::Private;

    fs->priv->action_group = g_simple_action_group_new ();
    static const GActionEntry action_entries[] = {
        { "paste",              on_paste,           nullptr, nullptr, nullptr },
        { "refresh",            on_refresh,         nullptr, nullptr, nullptr },
        { "new-text-file",      on_new_textfile,    nullptr, nullptr, nullptr }
    };
    g_action_map_add_action_entries (G_ACTION_MAP (fs->priv->action_group), action_entries, G_N_ELEMENTS (action_entries), fs);
    gtk_widget_insert_action_group (GTK_WIDGET (fs), "fs", G_ACTION_GROUP (fs->priv->action_group));

    g_object_set (fs, "orientation", GTK_ORIENTATION_VERTICAL, NULL);

    // create the box used for packing the dir_combo and buttons
    fs->update_show_devbuttons();

    // create the box used for packing the con_combo and information
    fs->con_hbox = create_hbox (*fs, FALSE, 2);

    // create the notebook and the first tab
    fs->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
    gtk_notebook_set_show_tabs (fs->notebook, FALSE);
    gtk_notebook_set_scrollable (fs->notebook, TRUE);
    gtk_notebook_set_show_border (fs->notebook, FALSE);
    gtk_widget_set_vexpand (GTK_WIDGET (fs->notebook), TRUE);

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l = l->next)
    {
        auto con = static_cast<GnomeCmdCon*> (l->data);

        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
            && !GNOME_CMD_IS_CON_SMB (con))  continue;

        string textstring {gnome_cmd_con_get_alias (con)};
        string_size = get_string_pixel_size (textstring.c_str(), textstring.length());
        max_string_size = string_size > max_string_size ? string_size : max_string_size;
    }

    // create the connection combo
    GtkListStore *store = gtk_list_store_new (3, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_POINTER);
    fs->con_combo = GNOME_CMD_COMBO (gnome_cmd_combo_new_with_store(store, 2, 1, 2));
    gtk_widget_set_size_request (*fs->con_combo, max_string_size, -1);
    gtk_editable_set_editable (GTK_EDITABLE (fs->con_combo->get_entry()), FALSE);

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
    gtk_box_append (GTK_BOX (fs->con_hbox), *fs->con_combo);
    // changing this value has only an effect when restarting gcmd
    if (gnome_cmd_data.show_devlist)
        gtk_box_append (GTK_BOX (fs->con_hbox), fs->vol_label);
    else
        gtk_box_append (GTK_BOX (padding), fs->vol_label);

    // connect signals
    g_signal_connect (fs, "realize", G_CALLBACK (on_realize), fs);
    g_signal_connect (fs->con_combo, "item-selected", G_CALLBACK (on_con_combo_item_selected), fs);
    g_signal_connect (fs->con_combo, "popwin-hidden", G_CALLBACK (on_combo_popwin_hidden), nullptr);
    g_signal_connect (fs->dir_indicator, "navigate", G_CALLBACK (on_navigate), fs);
    g_signal_connect (gnome_cmd_con_list_get (), "list-changed", G_CALLBACK (on_con_list_list_changed), fs);
    g_signal_connect (fs->notebook, "switch-page", G_CALLBACK (on_notebook_switch_page), fs);

    GtkGesture *notebook_click = gtk_gesture_click_new ();
    gtk_widget_add_controller (GTK_WIDGET (fs->notebook), GTK_EVENT_CONTROLLER (notebook_click));
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (notebook_click), GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (notebook_click), 0);
    g_signal_connect (notebook_click, "pressed", G_CALLBACK (on_notebook_button_pressed), fs);

    fs->update_show_devlist();
    fs->update_style();
}

/***********************************
 * Public functions
 ***********************************/

void GnomeCmdFileSelector::first()
{
    if (!priv->dir_history->can_back())
        return;

    priv->dir_history->lock();

    if (list->locked)
    {
        GnomeCmdCon *con = get_connection();

        new_tab(gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, priv->dir_history->first())));
    }
    else
        goto_directory(priv->dir_history->first());

    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::back()
{
    if (!priv->dir_history->can_back())
        return;

    priv->dir_history->lock();

    if (list->locked)
    {
        GnomeCmdCon *con = get_connection();

        new_tab(gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, priv->dir_history->back())));
    }
    else
        goto_directory(priv->dir_history->back());

    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::forward()
{
    if (!priv->dir_history->can_forward())
        return;

    priv->dir_history->lock();

    if (list->locked)
    {
        GnomeCmdCon *con = get_connection();

        new_tab(gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, priv->dir_history->forward())));
    }
    else
        goto_directory(priv->dir_history->forward());

    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::last()
{
    if (!priv->dir_history->can_forward())
        return;

    priv->dir_history->lock();

    if (list->locked)
    {
        GnomeCmdCon *con = get_connection();

        new_tab(gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, priv->dir_history->last())));
    }
    else
        goto_directory(priv->dir_history->last());

    priv->dir_history->unlock();
}


gboolean GnomeCmdFileSelector::can_back()
{
    return priv->dir_history && priv->dir_history->can_back();
}


gboolean GnomeCmdFileSelector::can_forward()
{
    return priv->dir_history && priv->dir_history->can_forward();
}


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

    gboolean found_my_con = FALSE;

    con_combo->clear();

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l = l->next)
    {
        auto con = static_cast<GnomeCmdCon*> (l->data);

        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
            && !GNOME_CMD_IS_CON_SMB (con))  continue;

        if (con == get_connection())
            found_my_con = TRUE;

        GIcon *icon = gnome_cmd_con_get_go_icon (con);

        if (icon)
        {
            con_combo->append((gchar *) gnome_cmd_con_get_alias (con), con, 0, icon, -1);
            g_object_unref (icon);
        }
    }

    // If the connection is no longer available use the home connection
    if (!found_my_con)
        set_connection(get_home_con ());
    else
        con_combo->select_data(get_connection());

    create_con_buttons (this);
}


void GnomeCmdFileSelector::update_style()
{
    con_combo->update_style();

    if (list)
        list->update_style();

    if (priv->realized)
        update_files();

    update_show_tabs();

    int tabs = gtk_notebook_get_n_pages (notebook);
    for (int i = 0; i < tabs; ++i)
    {
        auto fl = file_list(i);

        if (gnome_cmd_data.options.tab_lock_indicator != GnomeCmdData::TAB_LOCK_ICON)
            gtk_widget_hide (fl->tab_label_pin);

        if (fl->locked)
            update_tab_label(fl);
    }

    create_con_buttons (this);
    update_connections();
}

/**
 * @brief This function tries to create a new file.
 *
 * @param string_dialog a GnomeCmdStringDialog object reference
 * @param values a pointer to char pointers
 * @param fs a GnomeCmdFileSelector object reference
 * @return TRUE if all went well, FALSE if not
 */
static gboolean on_new_textfile_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdFileSelector *fs)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), TRUE);

    const gchar *fname = values[0];

    if (!fname || !*fname)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("No file name entered")));
        return FALSE;
    }

    GnomeCmdDir *dir = fs->get_directory();
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), TRUE);

    GError *error = nullptr;
    GFile *gFile;

    if (fname[0] == '/')
    {
        auto con = gnome_cmd_file_get_connection (GNOME_CMD_FILE (dir));
        auto conPath = gnome_cmd_con_create_path (con, fname);
        gFile = gnome_cmd_con_create_gfile (con, conPath->get_path());
        delete conPath;
    }
    else
    {
        auto uriBaseString = static_cast<gchar*>(GNOME_CMD_FILE (dir)->get_uri_str());
        // Let the URI to the current directory end with '/'
        auto conLastCharacter = uriBaseString[strlen(uriBaseString)-1];
        auto uriBaseStringSep = conLastCharacter == G_DIR_SEPARATOR
            ? g_strdup(uriBaseString)
            : g_strdup_printf("%s%s", uriBaseString, G_DIR_SEPARATOR_S);
        g_free (uriBaseString);

        auto relativeFileNamePath = g_build_filename(".", fname, nullptr);
        auto uriString = g_uri_resolve_relative (uriBaseStringSep, relativeFileNamePath,
                                                 G_URI_FLAGS_NONE, &error);
        g_free(relativeFileNamePath);
        g_free (uriBaseStringSep);
        if (error)
        {
            gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup_printf("%s", error->message));
            g_error_free(error);
            return FALSE;
        }

        gFile = g_file_new_for_uri(uriString);
        g_free (uriString);
    }

    auto gFileOutputStream = g_file_create(gFile, G_FILE_CREATE_NONE, nullptr, &error);
    if (error)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup_printf("%s", error->message));
        g_error_free(error);
        g_object_unref (gFile);
        return FALSE;
    }
    g_object_unref(gFileOutputStream);

    // focus the created file (if possible)
    auto parentGFile = g_file_get_parent (gFile);
    if (g_file_equal (parentGFile, gnome_cmd_dir_get_gfile (dir)))
    {
        gchar *uri_str = g_file_get_uri (gFile);
        gnome_cmd_dir_file_created (dir, uri_str);
        g_free (uri_str);
        gchar *focus_filename = g_file_get_basename (gFile);
        fs->file_list()->focus_file(focus_filename, TRUE);
        g_free (focus_filename);
    }
    g_object_unref(parentGFile);

    g_object_unref (gFile);

    return TRUE;
}


void gnome_cmd_file_selector_show_new_textfile_dialog (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    const gchar *labels[] = {_("File name:")};
    GtkWidget *dialog;

    dialog = gnome_cmd_string_dialog_new (_("New Text File"), labels, 1,
                                          (GnomeCmdStringDialogCallback) on_new_textfile_ok, fs);
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));

    GnomeCmdFile *f = fs->file_list()->get_selected_file();

    if (f)
    {
        gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog),
            0,
            g_file_info_get_display_name(f->get_file_info()));
    }
    g_object_ref (dialog);
    gtk_widget_show (dialog);
}


void gnome_cmd_file_selector_cap_paste (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdDir *dir = fs->get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    cap_paste_files (dir);
}


gboolean GnomeCmdFileSelector::key_pressed(GnomeCmdKeyPress *event)
{
    g_return_val_if_fail (event != nullptr, FALSE);

    GnomeCmdFile *f;

    if (state_is_ctrl_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                prev_tab();
                return TRUE;

            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                add_file_to_cmdline (list, TRUE);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_Left:
            case GDK_KEY_KP_Left:
                back();
                return TRUE;

            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                forward();
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_ctrl (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_V:
            case GDK_KEY_v:
                gnome_cmd_file_selector_cap_paste (this);
                return TRUE;

            case GDK_KEY_P:
            case GDK_KEY_p:
                add_cwd_to_cmdline (list);
                return TRUE;

            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                next_tab();
                return TRUE;

            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                add_file_to_cmdline (list, FALSE);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_blank (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_Left:
            case GDK_KEY_KP_Left:
            case GDK_KEY_BackSpace:
                if (!list->locked)
                {
                    list->invalidate_tree_size();
                    list->goto_directory("..");
                }
                else
                    new_tab(gnome_cmd_dir_get_parent (list->cwd));
                return TRUE;

            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                f = list->get_selected_file();
                if (f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
                    do_file_specific_action (list, f);
                return TRUE;

            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                if (gnome_cmd_data.cmdline_visibility
                    && gnome_cmd_cmdline_is_empty (main_win->get_cmdline()))
                    gnome_cmd_cmdline_exec (main_win->get_cmdline());
                else
                    do_file_specific_action (list, list->get_focused_file());
                return TRUE;

            case GDK_KEY_Escape:
                if (gnome_cmd_data.cmdline_visibility)
                    gnome_cmd_cmdline_set_text (main_win->get_cmdline(), "");
                return TRUE;

            default:
                break;
        }
    }

    return FALSE;
}


void GnomeCmdFileSelector::update_show_devbuttons()
{
    if (!gnome_cmd_data.show_devbuttons)
    {
        if (con_btns_hbox)
        {
            gtk_box_remove (GTK_BOX (this), con_btns_sw);
            con_btns_sw = nullptr;
            con_btns_hbox = nullptr;
        }
    }
    else
    {
        if (!con_btns_hbox)
        {
            con_btns_sw = gtk_scrolled_window_new ();
            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (con_btns_sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
            con_btns_hbox = create_hbox (*this, FALSE, 2);
            gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (con_btns_sw), con_btns_hbox);
            gtk_box_append (GTK_BOX (this), con_btns_sw);
            gtk_box_reorder_child_after (GTK_BOX (this), con_btns_sw, nullptr);
            create_con_buttons (this);
        }
    }
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

    fl->locked = locked;
    fl->update_style();

    // hide dir column
    fl->show_column(GnomeCmdFileList::COLUMN_DIR, FALSE);

    // create the scrollwindow that we'll place the list in
    GtkWidget *scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand (scrolled_window, TRUE);
    gtk_widget_set_vexpand (scrolled_window, TRUE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), *fl);

    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    fl->tab_label_pin = gtk_image_new_from_file (PIXMAPS_DIR G_DIR_SEPARATOR_S "pin.png");
    fl->tab_label_text = gtk_label_new (dir ? GNOME_CMD_FILE (dir)->get_name() : nullptr);

    gtk_box_append (GTK_BOX (hbox), fl->tab_label_pin);
    gtk_box_append (GTK_BOX (hbox), fl->tab_label_text);

    if (locked && gnome_cmd_data.options.tab_lock_indicator==GnomeCmdData::TAB_LOCK_ICON)
        gtk_widget_show (fl->tab_label_pin);
    else
        gtk_widget_hide (fl->tab_label_pin);

    gint n = gtk_notebook_append_page (notebook, scrolled_window, hbox);
    update_show_tabs();

    gtk_notebook_set_tab_reorderable (notebook, scrolled_window, TRUE);

    g_signal_connect (fl, "con-changed", G_CALLBACK (on_list_con_changed), this);
    g_signal_connect (fl, "dir-changed", G_CALLBACK (on_list_dir_changed), this);
    g_signal_connect (fl, "files-changed", G_CALLBACK (on_list_files_changed), this);

    if (dir)
        fl->set_connection(gnome_cmd_file_get_connection (GNOME_CMD_FILE (dir)), dir);

    if (activate)
    {
        gtk_notebook_set_current_page (notebook, n);
        gtk_widget_grab_focus (*fl);
    }

    g_signal_connect (fl, "file-clicked", G_CALLBACK (on_list_file_clicked), this);
    g_signal_connect (fl, "file-released", G_CALLBACK (on_list_file_released), this);
    g_signal_connect (fl, "list-clicked", G_CALLBACK (on_list_list_clicked), this);
    g_signal_connect (fl, "empty-space-clicked", G_CALLBACK (on_list_empty_space_clicked), this);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (fl), GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_list_key_pressed), this);

    return scrolled_window;
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
    g_return_if_fail(fl->cwd != nullptr);

    const gchar *name = GNOME_CMD_FILE (fl->cwd)->get_name();

    switch (gnome_cmd_data.options.tab_lock_indicator)
    {
        case GnomeCmdData::TAB_LOCK_ICON:
            if (fl->locked)
                gtk_widget_show (fl->tab_label_pin);
            else
                gtk_widget_hide (fl->tab_label_pin);
            break;

        case GnomeCmdData::TAB_LOCK_ASTERISK:
            if (fl->locked)
            {
                gchar *s = g_strconcat ("* ", name, nullptr);
                gtk_label_set_text (GTK_LABEL (fl->tab_label_text), s);
                g_free (s);
                return;
            }
            break;

        case GnomeCmdData::TAB_LOCK_STYLED_TEXT:
            if (fl->locked)
            {
                gchar *s = g_strconcat ("<span foreground='blue'>", name, "</span>", nullptr);
                gtk_label_set_markup (GTK_LABEL (fl->tab_label_text), s);
                g_free (s);
                return;
            }
            break;

        default:
            break;
    }

    gtk_label_set_text (GTK_LABEL (fl->tab_label_text), name);
}


GListModel* GnomeCmdFileSelector::GetTabs()
{
    return gtk_notebook_get_pages (GTK_NOTEBOOK (this->notebook));
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

GnomeCmdDir *gnome_cmd_file_selector_get_directory(GnomeCmdFileSelector *fs)
{
    return fs->list->cwd;
}

GnomeCmdCon *gnome_cmd_file_selector_get_connection (GnomeCmdFileSelector *fs)
{
    return fs->get_connection();
}

void gnome_cmd_file_selector_set_connection(GnomeCmdFileSelector *fs, GnomeCmdCon *con, GnomeCmdDir *start_dir)
{
    fs->set_connection(con, start_dir);
}

GtkWidget *gnome_cmd_file_selector_new_tab (GnomeCmdFileSelector *fs)
{
    return fs->new_tab();
}

GtkWidget *gnome_cmd_file_selector_new_tab_with_dir (GnomeCmdFileSelector *fs, GnomeCmdDir *dir, gboolean activate)
{
    return fs->new_tab(dir, activate);
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

gint gnome_cmd_file_selector_get_fs_id (GnomeCmdFileSelector *fs)
{
    return (gint) fs->fs_id;
}

gboolean gnome_cmd_file_selector_is_active (GnomeCmdFileSelector *fs)
{
    return fs->is_active();
}

