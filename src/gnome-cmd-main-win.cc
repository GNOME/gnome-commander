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
#include "gnome-cmd-main-menu.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-owner.h"
#include "utils.h"

using namespace std;


enum
{
  TOOLBAR_BTN_REFRESH,
  TOOLBAR_BTN_UP,
  TOOLBAR_BTN_FIRST,
  TOOLBAR_BTN_BACK,
  TOOLBAR_BTN_FORWARD,
  TOOLBAR_BTN_LAST,
  TOOLBAR_BTN_SEP_0,
  TOOLBAR_BTN_COPY_FILENAMES,
  TOOLBAR_BTN_CUT,
  TOOLBAR_BTN_COPY,
  TOOLBAR_BTN_PASTE,
  TOOLBAR_BTN_DELETE,
  TOOLBAR_BTN_SEP_1,
  TOOLBAR_BTN_EDIT,
  TOOLBAR_BTN_MAIL,
  TOOLBAR_BTN_TERMINAL,
  TOOLBAR_BTN_SEP_2,
  TOOLBAR_BTN_CONNECT,
  TOOLBAR_BTN_DISCONNECT
};


struct GnomeCmdMainWinClass
{
    GtkApplicationWindowClass parent_class;
};


struct GnomeCmdMainWin::Private
{
    FileSelectorID current_fs;
    GnomeCmdState state;

    GtkWidget *vbox;
    GtkWidget *paned;
    GtkWidget *file_selector[2];
    GtkWidget *focused_widget;
    GtkAccelGroup *accel_group;

    GtkWidget *view_btn;
    GtkWidget *edit_btn;
    GtkWidget *copy_btn;
    GtkWidget *move_btn;
    GtkWidget *mkdir_btn;
    GtkWidget *delete_btn;
    GtkWidget *find_btn;

    GtkWidget *menubar;
    GtkWidget *toolbar;
    GtkWidget *toolbar_sep;
    GnomeCmdCmdline *cmdline;
    GtkWidget *cmdline_sep;
    GtkWidget *buttonbar;
    GtkWidget *buttonbar_sep;

    GtkWidget *tb_con_drop_btn;

    GWeakRef advrename_dlg;
    GWeakRef file_search_dlg;
    GWeakRef bookmarks_dlg;

    bool state_saved;
};


G_DEFINE_TYPE (GnomeCmdMainWin, gnome_cmd_main_win, GTK_TYPE_APPLICATION_WINDOW)


inline GtkWidget *add_buttonbar_button (char *label,
                                        GnomeCmdMainWin *mw,
                                        const char *action_name)
{
    GtkWidget *button = create_styled_button (label);
    gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action_name);
    gtk_widget_set_can_focus(button, FALSE);
    gtk_widget_set_hexpand (button, TRUE);

    gtk_box_append (GTK_BOX (mw->priv->buttonbar), button);

    return button;
}


static GtkWidget *create_separator (gboolean vertical)
{
    GtkWidget *sep;
    if (vertical)
    {
        sep = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
        gtk_widget_set_margin_top (sep, 3);
        gtk_widget_set_margin_bottom (sep, 3);
    }
    else
    {
        sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_margin_start (sep, 3);
        gtk_widget_set_margin_end (sep, 3);
    }
    gtk_widget_show (sep);
    return sep;
}


static GtkWidget *append_toolbar_button (GtkWidget *toolbar, const gchar *label, const gchar *icon, const gchar *action)
{
    GtkWidget *button = gtk_button_new_from_icon_name (icon, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text (button, label);
    gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "flat");
    gtk_box_append (GTK_BOX (toolbar), button);
    g_object_set (button, "always-show-image", TRUE, NULL);
    return button;
}


static void append_toolbar_separator (GtkWidget *toolbar)
{
    GtkWidget *separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_append (GTK_BOX (toolbar), separator);
}


static void create_toolbar (GnomeCmdMainWin *mw)
{
    mw->priv->toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    append_toolbar_button (mw->priv->toolbar, _("Refresh"),                                                 "view-refresh",         "win.view-refresh");
    append_toolbar_button (mw->priv->toolbar, _("Up one directory"),                                        "go-up",                "win.view-up");
    append_toolbar_button (mw->priv->toolbar, _("Go to the oldest"),                                        "go-first",             "win.view-first");
    append_toolbar_button (mw->priv->toolbar, _("Go back"),                                                 "go-previous",          "win.view-back");
    append_toolbar_button (mw->priv->toolbar, _("Go forward"),                                              "go-next",              "win.view-forward");
    append_toolbar_button (mw->priv->toolbar, _("Go to the latest"),                                        "go-last",              "win.view-last");
    append_toolbar_separator (mw->priv->toolbar);
    append_toolbar_button (mw->priv->toolbar, _("Copy file names (SHIFT for full paths, ALT for URIs)"),    COPYFILENAMES_STOCKID,  "win.edit-copy-fnames");
    append_toolbar_button (mw->priv->toolbar, _("Cut"),                                                     "edit-cut",             "win.edit-cap-cut");
    append_toolbar_button (mw->priv->toolbar, _("Copy"),                                                    "edit-copy",            "win.edit-cap-copy");
    append_toolbar_button (mw->priv->toolbar, _("Paste"),                                                   "edit-paste",           "win.edit-cap-paste");
    append_toolbar_button (mw->priv->toolbar, _("Delete"),                                                  "edit-delete",          "win.file-delete");
    append_toolbar_button (mw->priv->toolbar, _("Edit (SHIFT for new document)"),                           "gnome-commander-edit", "win.file-edit");
    append_toolbar_button (mw->priv->toolbar, _("Send files"),                                              GTK_MAILSEND_STOCKID,   "win.file-sendto");
    append_toolbar_button (mw->priv->toolbar, _("Open terminal (SHIFT for root privileges)"),               GTK_TERMINAL_STOCKID,   "win.command-open-terminal-internal");
    append_toolbar_separator (mw->priv->toolbar);
    append_toolbar_button (mw->priv->toolbar, _("Remote Server"),                                           "gnome-commander-connect", "win.connections-open");
    mw->priv->tb_con_drop_btn = g_object_ref (
        append_toolbar_button (mw->priv->toolbar, _("Drop connection"),                                     nullptr,                "win.connections-close-current"));

    gtk_widget_show_all (mw->priv->toolbar);

    mw->priv->toolbar_sep = create_separator (FALSE);
}


static void view_slide (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);
    gint percentage = g_variant_get_int32 (parameter);

    main_win->set_slide(percentage);
}


static GMenuModel *create_slide_popup ()
{
    GMenu *menu = g_menu_new ();
    g_menu_append (menu, "100 - 0", "win.view-slide(100)");
    g_menu_append (menu, "80 - 20", "win.view-slide(80)");
    g_menu_append (menu, "60 - 40", "win.view-slide(60)");
    g_menu_append (menu, "50 - 50", "win.view-slide(50)");
    g_menu_append (menu, "40 - 60", "win.view-slide(40)");
    g_menu_append (menu, "20 - 80", "win.view-slide(20)");
    g_menu_append (menu, "0 - 100", "win.view-slide(0)");
    return G_MENU_MODEL (menu);
}


void GnomeCmdMainWin::create_buttonbar()
{
    priv->buttonbar_sep = create_separator (FALSE);

    priv->buttonbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_show (priv->buttonbar);

    priv->view_btn = add_buttonbar_button(_("F3 View"), this, "win.file-view");
    gtk_box_append (GTK_BOX (priv->buttonbar), create_separator (TRUE));
    priv->edit_btn = add_buttonbar_button(_("F4 Edit"), this, "win.file-edit");
    gtk_box_append (GTK_BOX (priv->buttonbar), create_separator (TRUE));
    priv->copy_btn = add_buttonbar_button(_("F5 Copy"), this, "win.file-copy");
    gtk_box_append (GTK_BOX (priv->buttonbar), create_separator (TRUE));
    priv->move_btn = add_buttonbar_button(_("F6 Move"), this, "win.file-move");
    gtk_box_append (GTK_BOX (priv->buttonbar), create_separator (TRUE));
    priv->mkdir_btn = add_buttonbar_button(_("F7 Mkdir"), this, "win.file-mkdir");
    gtk_box_append (GTK_BOX (priv->buttonbar), create_separator (TRUE));
    priv->delete_btn = add_buttonbar_button(_("F8 Delete"), this, "win.file-delete");
    gtk_box_append (GTK_BOX (priv->buttonbar), create_separator (TRUE));
    priv->find_btn = add_buttonbar_button(_("F9 Search"), this, "win.file-search");
}


/*****************************************************************************
    Misc widgets callbacks
*****************************************************************************/

static void on_slide_button_press (GtkGestureMultiPress *gesture, int n_press, double x, double y, gpointer user_data)
{
    auto mw = static_cast<GnomeCmdMainWin *>(user_data);

    GtkPaned *paned = GTK_PANED (mw->priv->paned);

    GtkAllocation child_allocation;

    if (n_press != 1)
        return;

    gtk_widget_get_allocation (gtk_paned_get_child1 (paned), &child_allocation);
    gtk_widget_translate_coordinates (gtk_paned_get_child1 (paned), GTK_WIDGET (paned), 0, 0, &child_allocation.x, &child_allocation.y);
    if (gdk_rectangle_contains_point (&child_allocation, x, y))
        return;
    gtk_widget_get_allocation (gtk_paned_get_child2 (paned), &child_allocation);
    gtk_widget_translate_coordinates (gtk_paned_get_child2 (paned), GTK_WIDGET (paned), 0, 0, &child_allocation.x, &child_allocation.y);
    if (gdk_rectangle_contains_point (&child_allocation, x, y))
        return;

    GtkWidget *popover = gtk_popover_new_from_model (GTK_WIDGET (paned), create_slide_popup ());
    GdkRectangle rect = { (gint) x, (gint) y, 0, 0 };
    gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
    gtk_popover_popup (GTK_POPOVER (popover));
}


static void on_main_win_realize (GtkWidget *widget, GnomeCmdMainWin *mw)
{
    mw->set_equal_panes();

    mw->fs(LEFT)->set_active(TRUE);
    mw->fs(RIGHT)->set_active(FALSE);

    // if (gnome_cmd_data.cmdline_visibility)
    // {
        // gchar *dpath = GNOME_CMD_FILE (mw->fs(LEFT)->get_directory())->get_path();
        // gnome_cmd_cmdline_set_dir (GNOME_CMD_CMDLINE (mw->priv->cmdline), dpath);
        // g_free (dpath);
    // }

    gtk_window_set_icon_name (GTK_WINDOW (mw), "gnome-commander");
}


static gboolean on_left_fs_select (GnomeCmdFileList *list, GnomeCmdFileListButtonEvent *event, GnomeCmdMainWin *mw)
{
    mw->priv->current_fs = LEFT;

    GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[LEFT])->set_active(TRUE);
    GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[RIGHT])->set_active(FALSE);

    return FALSE;
}


static gboolean on_right_fs_select (GnomeCmdFileList *list, GnomeCmdFileListButtonEvent *event, GnomeCmdMainWin *mw)
{
    mw->priv->current_fs = RIGHT;

    GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[RIGHT])->set_active(TRUE);
    GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[LEFT])->set_active(FALSE);

    return FALSE;
}


static void on_fs_list_resize_column (GnomeCmdFileList *list, guint column_index, GtkTreeViewColumn *column, GnomeCmdFileSelector *other_selector)
{
    static gint column_resize_lock = 0;

    /* the lock is used so that we dont get into the situation where
       the left list triggers the right witch triggers the left ... */
    if (column_resize_lock == 0)
    {
        column_resize_lock += 1;

        GnomeCmdFileList::ColumnID column_id = static_cast<GnomeCmdFileList::ColumnID> (column_index);
        gint width = gtk_tree_view_column_get_width (column);

        other_selector->file_list()->resize_column (column_id, width);

        column_resize_lock -= 1;
    }
}


static void on_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (gnome_cmd_data.main_win_state)
    {
        case GDK_WINDOW_STATE_FULLSCREEN:
        case GDK_WINDOW_STATE_ICONIFIED:
        case GDK_WINDOW_STATE_MAXIMIZED:
            break;

        default:
            gnome_cmd_data.main_win_width = allocation->width;
            gnome_cmd_data.main_win_height = allocation->height;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


inline void update_browse_buttons (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs == mw->fs(ACTIVE))
    {
        g_simple_action_set_enabled (
            G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (mw), "view-first")),
            fs->can_back());
        g_simple_action_set_enabled (
            G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (mw), "view-back")),
            fs->can_back());
        g_simple_action_set_enabled (
            G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (mw), "view-forward")),
            fs->can_forward());
        g_simple_action_set_enabled (
            G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (mw), "view-last")),
            fs->can_forward());
    }
}


void GnomeCmdMainWin::update_drop_con_button(GnomeCmdFileList *fl)
{
    if (!fl)
        return;

    GIcon *icon = NULL;

    GnomeCmdCon *con = fl->con;
    if (!con)
        return;

    if (!gnome_cmd_data.show_toolbar)
        return;

    GtkWidget *btn = priv->tb_con_drop_btn;
    g_return_if_fail (GTK_IS_BUTTON (btn));

    bool closeable = gnome_cmd_con_is_closeable (con);

    g_simple_action_set_enabled (
        G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (this), "connections-close-current")),
        closeable);

    if (!closeable)
        return;

    gchar *close_tooltip = gnome_cmd_con_get_close_tooltip (con);
    gtk_widget_set_tooltip_text(btn, close_tooltip);
    g_free (close_tooltip);

    icon = gnome_cmd_con_get_close_icon (con);
    if (icon)
    {
        GtkWidget *image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
        if (image)
        {
            g_object_ref (image);
            gtk_widget_show (image);
            gtk_button_set_image (GTK_BUTTON (btn), image);
        }
        g_object_unref (icon);
    }
    else
    {
        gchar *close_text = gnome_cmd_con_get_close_text (con);
        gtk_button_set_label (GTK_BUTTON (btn), close_text);
        g_free (close_text);
    }
}


static void on_fs_dir_change (GnomeCmdFileSelector *fs, const gchar dir, GnomeCmdMainWin *mw)
{
    update_browse_buttons (mw, fs);
    mw->update_drop_con_button(fs->file_list());
    mw->update_cmdline();
}


inline void restore_size_and_pos (GnomeCmdMainWin *mw)
{
    gtk_window_set_default_size (*mw,
                                 gnome_cmd_data.main_win_width,
                                 gnome_cmd_data.main_win_height);

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (gnome_cmd_data.main_win_state)
    {
        case GDK_WINDOW_STATE_MAXIMIZED:
        case GDK_WINDOW_STATE_FULLSCREEN:
            gtk_window_maximize (GTK_WINDOW (mw));
            break;

        default:
            break;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


static gboolean on_window_state_event (GtkWidget *mw, GdkEventWindowState *event, gpointer user_data)
{
    gnome_cmd_data.main_win_state = event->new_window_state;
    return FALSE;
}


static void on_con_list_list_changed (GnomeCmdConList *con_list, GnomeCmdMainWin *main_win)
{
    main_win->update_mainmenu();
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

static void dispose (GObject *object)
{
    auto main_win = GNOME_CMD_MAIN_WIN (object);

    if (!main_win->priv->state_saved)
    {
        gnome_cmd_data.save(main_win);
        main_win->priv->state_saved = true;
    }

    g_clear_pointer (&main_win->priv->state.active_dir_files, g_list_free);
    g_clear_pointer (&main_win->priv->state.inactive_dir_files, g_list_free);

    g_weak_ref_set (&main_win->priv->advrename_dlg, nullptr);
    g_weak_ref_set (&main_win->priv->file_search_dlg, nullptr);

    G_OBJECT_CLASS (gnome_cmd_main_win_parent_class)->dispose (object);
}


static void gnome_cmd_main_win_class_init (GnomeCmdMainWinClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = dispose;
}


static void gnome_cmd_main_win_init (GnomeCmdMainWin *mw)
{
    g_action_map_add_action_entries (G_ACTION_MAP (mw), FILE_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), MARK_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), EDIT_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), COMMAND_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), VIEW_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), BOOKMARK_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), OPTIONS_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), CONNECTIONS_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), PLUGINS_ACTION_ENTRIES, -1, mw);
    g_action_map_add_action_entries (G_ACTION_MAP (mw), HELP_ACTION_ENTRIES, -1, mw);

    static const GActionEntry MW_VIEW_ACTION_ENTRIES[] = {
        { "view-slide", view_slide, "i", nullptr, nullptr },
        { nullptr }
    };
    g_action_map_add_action_entries (G_ACTION_MAP (mw), MW_VIEW_ACTION_ENTRIES, -1, mw);

    toggle_action_change_state (mw, "view-toolbar", gnome_cmd_data.show_toolbar);
    toggle_action_change_state (mw, "view-conbuttons", gnome_cmd_data.show_devbuttons);
    toggle_action_change_state (mw, "view-devlist", gnome_cmd_data.show_devlist);
    toggle_action_change_state (mw, "view-cmdline", gnome_cmd_data.cmdline_visibility);
    toggle_action_change_state (mw, "view-buttonbar", gnome_cmd_data.buttonbar_visibility);
    toggle_action_change_state (mw, "view-hidden-files", !gnome_cmd_data.options.filter.file_types[GnomeCmdData::G_FILE_IS_HIDDEN]);
    toggle_action_change_state (mw, "view-backup-files", !gnome_cmd_data.options.filter.file_types[GnomeCmdData::G_FILE_IS_BACKUP]);
    toggle_action_change_state (mw, "view-horizontal-orientation", gnome_cmd_data.horizontal_orientation);

    /* It is very important that this global variable gets assigned here so that
     * child widgets to this window can use that variable when initializing
     */
    main_win = GNOME_CMD_MAIN_WIN (mw);

    mw->priv = g_new0 (GnomeCmdMainWin::Private, 1);
    mw->priv->current_fs = LEFT;
    mw->priv->accel_group = gtk_accel_group_new ();
    mw->priv->toolbar = NULL;
    mw->priv->toolbar_sep = NULL;
    mw->priv->focused_widget = NULL;
    mw->priv->cmdline = NULL;
    mw->priv->cmdline_sep = NULL;
    mw->priv->buttonbar = NULL;
    mw->priv->buttonbar_sep = NULL;
    mw->priv->file_selector[LEFT] = NULL;
    mw->priv->file_selector[RIGHT] = NULL;
    mw->priv->advrename_dlg = { { nullptr } };
    mw->priv->file_search_dlg = { { nullptr } };
    mw->priv->bookmarks_dlg = { { nullptr } };
    mw->priv->state_saved = false;

    gtk_window_set_title (GTK_WINDOW (mw),
                          gcmd_owner.is_root()
                            ? _("GNOME Commander — ROOT PRIVILEGES")
                            : _("GNOME Commander"));

    g_object_set_data (*mw, "main_win", mw);
    restore_size_and_pos (mw);
    gtk_window_set_resizable (*mw, TRUE);

    mw->priv->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_show (mw->priv->vbox);

    auto menu = gnome_cmd_main_menu_new (G_ACTION_GROUP (mw));
    mw->priv->menubar = gtk_menu_bar_new_from_model (G_MENU_MODEL (menu.menu));
    gtk_window_add_accel_group (GTK_WINDOW (mw), menu.accel_group);

    if (gnome_cmd_data.mainmenu_visibility)
    {
        gtk_widget_show (mw->priv->menubar);
    }
    gtk_box_append (GTK_BOX (mw->priv->vbox), mw->priv->menubar);
    gtk_box_append (GTK_BOX (mw->priv->vbox), create_separator (FALSE));

    gtk_widget_show (mw->priv->vbox);
    gtk_container_add (GTK_CONTAINER (mw), mw->priv->vbox);

    mw->priv->paned = gtk_paned_new (gnome_cmd_data.horizontal_orientation ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_hexpand (mw->priv->paned, TRUE);
    gtk_widget_set_vexpand (mw->priv->paned, TRUE);
    gtk_widget_show (mw->priv->paned);
    gtk_box_append (GTK_BOX (mw->priv->vbox), mw->priv->paned);
    mw->create_buttonbar();

    mw->priv->file_selector[LEFT] = gnome_cmd_file_selector_new (LEFT);
    gtk_widget_show (mw->priv->file_selector[LEFT]);
    gtk_paned_pack1 (GTK_PANED (mw->priv->paned), mw->priv->file_selector[LEFT], TRUE, TRUE);

    mw->priv->file_selector[RIGHT] = gnome_cmd_file_selector_new (RIGHT);
    gtk_widget_show (mw->priv->file_selector[RIGHT]);
    gtk_paned_pack2 (GTK_PANED (mw->priv->paned), mw->priv->file_selector[RIGHT], TRUE, TRUE);

    mw->update_show_toolbar();
    mw->update_cmdline_visibility();
    mw->update_buttonbar_visibility();

    g_signal_connect (mw, "realize", G_CALLBACK (on_main_win_realize), mw);
    g_signal_connect (mw->fs(LEFT), "dir-changed", G_CALLBACK (on_fs_dir_change), mw);
    g_signal_connect (mw->fs(RIGHT), "dir-changed", G_CALLBACK (on_fs_dir_change), mw);

    mw->fs(LEFT)->update_connections();
    mw->fs(RIGHT)->update_connections();

    mw->open_tabs(LEFT);
    mw->open_tabs(RIGHT);

    gnome_cmd_data.tabs.clear();        //  free unused memory

    g_signal_connect (mw, "size-allocate", G_CALLBACK (on_size_allocate), mw);
    g_signal_connect (mw, "window-state-event", G_CALLBACK (on_window_state_event), NULL);

    GtkGesture *paned_click_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (mw->priv->paned));
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (paned_click_gesture), 3);
    g_signal_connect (paned_click_gesture, "pressed", G_CALLBACK (on_slide_button_press), mw);

    g_signal_connect (mw->fs(LEFT)->file_list(), "resize-column", G_CALLBACK (on_fs_list_resize_column), mw->fs(RIGHT));
    g_signal_connect (mw->fs(RIGHT)->file_list(), "resize-column", G_CALLBACK (on_fs_list_resize_column), mw->fs(LEFT));
    g_signal_connect (mw->fs(LEFT)->file_list(), "list-clicked", G_CALLBACK (on_left_fs_select), mw);
    g_signal_connect (mw->fs(RIGHT)->file_list(), "list-clicked", G_CALLBACK (on_right_fs_select), mw);

    g_signal_connect (gnome_cmd_con_list_get (), "list-changed", G_CALLBACK (on_con_list_list_changed), mw);

    gtk_window_add_accel_group (*mw, mw->priv->accel_group);
    mw->focus_file_lists();
}


FileSelectorID GnomeCmdMainWin::fs() const
{
    return priv->current_fs;
}


FileSelectorID GnomeCmdMainWin::fs(GnomeCmdFileSelector *fselector) const
{
    if (!priv->file_selector[LEFT] || priv->file_selector[LEFT]==*fselector)
        return LEFT;

    if (!priv->file_selector[RIGHT] || priv->file_selector[RIGHT]==*fselector)
        return RIGHT;

    g_assert_not_reached();

    return LEFT;    //  never reached, to make compiler happy
}


GnomeCmdFileSelector *GnomeCmdMainWin::fs(FileSelectorID id) const
{
    switch (id)
    {
        case LEFT:
        case RIGHT:
            return priv->file_selector[id] ?
                   GNOME_CMD_FILE_SELECTOR (priv->file_selector[id]) : NULL;

        case ACTIVE:
            return priv->file_selector[priv->current_fs] ?
                   GNOME_CMD_FILE_SELECTOR (priv->file_selector[priv->current_fs]) : NULL;

        case INACTIVE:
            return priv->file_selector[!priv->current_fs] ?
                   GNOME_CMD_FILE_SELECTOR (priv->file_selector[!priv->current_fs]): NULL;

       default:
            return NULL;
    }
}


void GnomeCmdMainWin::update_view()
{
    update_style();
}


void GnomeCmdMainWin::update_style()
{
    g_return_if_fail (priv != NULL);

    IMAGE_clear_mime_cache ();

    fs(LEFT)->update_style();
    fs(RIGHT)->update_style();

    if (gnome_cmd_data.cmdline_visibility)
        gnome_cmd_cmdline_update_style (GNOME_CMD_CMDLINE (priv->cmdline));

    if (auto file_search_dlg = static_cast<GnomeCmdSearchDialog*>(g_weak_ref_get (&priv->file_search_dlg)))
    {
        file_search_dlg->update_style();
        g_object_unref (file_search_dlg);
    }
}


void GnomeCmdMainWin::focus_file_lists()
{
    fs(ACTIVE)->set_active(TRUE);
    fs(INACTIVE)->set_active(FALSE);

    priv->focused_widget = priv->file_selector[priv->current_fs];
}


void GnomeCmdMainWin::refocus()
{
    if (priv->focused_widget)
        gtk_widget_grab_focus (priv->focused_widget);
}


gboolean GnomeCmdMainWin::key_pressed(GnomeCmdKeyPress *event)
{
    if (state_is_ctrl_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_c:
            case GDK_KEY_C:
                if (gnome_cmd_data.cmdline_visibility && (gnome_cmd_data.options.quick_search == GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER))
                    gnome_cmd_cmdline_focus(get_cmdline());
                return TRUE;
                break;
        }
    }
    else if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_F8:
                if (gnome_cmd_data.cmdline_visibility)
                    gnome_cmd_cmdline_show_history (GNOME_CMD_CMDLINE (priv->cmdline));
                return TRUE;
            default:
                break;
        }
    }
    else if (state_is_ctrl_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_H:
            case GDK_KEY_h:
                gnome_cmd_data.options.filter.file_types[GnomeCmdData::G_FILE_IS_HIDDEN] =
                    !gnome_cmd_data.options.filter.file_types[GnomeCmdData::G_FILE_IS_HIDDEN];
                gnome_cmd_data.save(this);
                return TRUE;
            default:
                break;
        }
    }
    else if (state_is_ctrl (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_e:
            case GDK_KEY_E:
            case GDK_KEY_Down:
                if (gnome_cmd_data.cmdline_visibility)
                    gnome_cmd_cmdline_show_history (GNOME_CMD_CMDLINE (priv->cmdline));
                return TRUE;

            case GDK_KEY_s:
            case GDK_KEY_S:
                {
                    // Calculate the middle of the widget
                    GtkAllocation allocation;
                    gtk_widget_get_allocation (priv->paned, &allocation);

                    GdkRectangle rect;
                    rect.x = allocation.width / 2;
                    rect.y = allocation.height / 2;
                    rect.width = 0;
                    rect.height = 0;
                    GtkWidget *popover = gtk_popover_new_from_model (priv->paned, create_slide_popup ());
                    gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
                    gtk_popover_popup (GTK_POPOVER (popover));
                }
                return TRUE;

            case GDK_KEY_u:
            case GDK_KEY_U:
                {
                    GnomeCmdFileSelector *fs1 = fs(LEFT);
                    GnomeCmdFileSelector *fs2 = fs(RIGHT);

                    // swap widgets
                    g_object_ref (fs1);
                    g_object_ref (fs2);
                    gtk_container_remove (GTK_CONTAINER (priv->paned), *fs1);
                    gtk_container_remove (GTK_CONTAINER (priv->paned), *fs2);
                    gtk_paned_pack1 (GTK_PANED (priv->paned), *fs2, TRUE, TRUE);
                    gtk_paned_pack2 (GTK_PANED (priv->paned), *fs1, TRUE, TRUE);
                    g_object_unref (fs1);
                    g_object_unref (fs2);

                    // update priv->file_selector[]
                    GtkWidget *swap = priv->file_selector[LEFT];
                    priv->file_selector[LEFT] = priv->file_selector[RIGHT];
                    priv->file_selector[RIGHT] = swap;

                    // refocus ACTIVE fs
                    focus_file_lists();

                    // update cmdline only for different directories
                    if (fs1->get_directory()!=fs2->get_directory())
                        switch_fs(fs(ACTIVE));
                }
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_alt_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_P:
            case GDK_KEY_p:
                plugin_manager_show (*this);
                break;

            case GDK_KEY_f:
            case GDK_KEY_F:
            {
                GnomeCmdConRemote *con = GNOME_CMD_CON_REMOTE (gnome_cmd_con_list_get_all_remote (gnome_cmd_con_list_get ())->data);

                fs(ACTIVE)->set_connection(GNOME_CMD_CON (con));
            }
            break;

            default:
                break;
        }
    }
    else
        if (state_is_blank (event->state))
            switch (event->keyval)
            {
                case GDK_KEY_Tab:
                case GDK_KEY_ISO_Left_Tab:
                    switch_fs(fs(INACTIVE));
                    return TRUE;

                case GDK_KEY_F1:
                    g_action_group_activate_action (*this, "help-help", nullptr);
                    return TRUE;

                case GDK_KEY_F2:
                    g_action_group_activate_action (*this, "file-rename", nullptr);
                    return TRUE;

                case GDK_KEY_F3:
                    g_action_group_activate_action (*this, "file-view", nullptr);
                    return TRUE;

                case GDK_KEY_F4:
                    g_action_group_activate_action (*this, "file-edit", nullptr);
                    return TRUE;

                case GDK_KEY_F5:
                    g_action_group_activate_action (*this, "file-copy", nullptr);
                    return TRUE;

                case GDK_KEY_F6:
                    g_action_group_activate_action (*this, "file-move", nullptr);
                    return TRUE;

                case GDK_KEY_F7:
                    g_action_group_activate_action (*this, "file-mkdir", nullptr);
                    return TRUE;

                case GDK_KEY_F8:
                    g_action_group_activate_action (*this, "file-delete", nullptr);
                    return TRUE;

                case GDK_KEY_F9:
                    g_action_group_activate_action (*this, "file-search", nullptr);
                    return TRUE;

                default:
                    break;
            }

    return fs(ACTIVE)->key_pressed(event);
}


void GnomeCmdMainWin::open_tabs(FileSelectorID id)
{
    // Always add a temporary tab pointing to the home directory in the list of stored tabs.
    // This one will be used as a backup in case none of the stored tab URI's is valid.
    gnome_cmd_data.tabs[id].push_back(make_pair(string(g_get_home_dir ()), make_tuple(GnomeCmdFileList::COLUMN_NAME, GTK_SORT_ASCENDING, FALSE)));

    auto last_tab = unique(gnome_cmd_data.tabs[id].begin(), gnome_cmd_data.tabs[id].end());
    auto backup_tab = gnome_cmd_data.tabs[id].end() - 1;
    auto one_valid_tab_found = false;

    for (auto stored_tab = gnome_cmd_data.tabs[id].begin(); stored_tab != last_tab; ++stored_tab)
    {
        auto uriString = stored_tab->first;
        auto uriScheme = g_uri_peek_scheme (uriString.c_str());
        auto uriIsRelative = false;
        gchar *path = nullptr;
        GnomeCmdCon *con;

        if (stored_tab == backup_tab && one_valid_tab_found)
            continue;

        if (!uriScheme)
        {
            uriScheme = g_strdup("file");
            uriIsRelative = true;
        }

        if (strcmp(uriScheme, "file") == 0 || uriIsRelative)
        {
            con = get_home_con ();
            if (uriIsRelative)
            {
                path = g_strdup(stored_tab->first.c_str());
            }
            else
            {
                auto gUri = g_uri_parse(stored_tab->first.c_str(), G_URI_FLAGS_NONE, nullptr);
                path = g_strdup(g_uri_get_path(gUri));
            }
        }
        else
        {
            GError *error = nullptr;
            auto gUri = g_uri_parse(stored_tab->first.c_str(), G_URI_FLAGS_NONE, &error);
            if (error)
            {
                g_warning("Stored URI is invalid: %s", error->message);
                g_error_free(error);
                // open home directory instead
                path = g_strdup(g_get_home_dir());
            }
            path = path ? path : g_strdup(g_uri_get_path(gUri));

            con = (GnomeCmdCon*) gnome_cmd_con_remote_new(nullptr, uriString.c_str());
        }

        GnomeCmdDir *gnomeCmdDir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, path), true);
        if (gnomeCmdDir != nullptr)
        {
            const auto& tabTuple = stored_tab->second;
            fs(id)->new_tab(gnomeCmdDir, std::get<0>(tabTuple), std::get<1>(tabTuple), std::get<2>(tabTuple), TRUE);
            one_valid_tab_found = true;
        }
        else
        {
            g_warning("Stored path %s is invalid. Skipping", path);
        }

        g_free(path);
    }
}


void GnomeCmdMainWin::switch_fs(GnomeCmdFileSelector *fselector)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fselector));

    if (fselector == fs(ACTIVE))
        return;

    priv->current_fs = (FileSelectorID) !priv->current_fs;
    fs(ACTIVE)->set_active(TRUE);
    fs(INACTIVE)->set_active(FALSE);

    update_browse_buttons (this, fselector);
    update_drop_con_button(fselector->file_list());
    update_cmdline();
}


void GnomeCmdMainWin::change_connection(FileSelectorID id)
{
    GnomeCmdFileSelector *fselector = this->fs(id);

    switch_fs(fselector);
    if (gnome_cmd_data.show_devlist)
        fselector->con_combo->popup_list();
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

    if (fselector->file_list()->locked)
        fselector->new_tab(dir);
    else
        fselector->file_list()->set_connection(other->get_connection(), dir);

    other->set_active(!fs_is_active);
    fselector->set_active(fs_is_active);
}


GnomeCmdCmdline *GnomeCmdMainWin::get_cmdline() const
{
    return GNOME_CMD_CMDLINE (priv->cmdline);
}


void GnomeCmdMainWin::update_mainmenu()
{
    auto menu = gnome_cmd_main_menu_new (G_ACTION_GROUP (this));
    gtk_menu_shell_bind_model (GTK_MENU_SHELL (priv->menubar), G_MENU_MODEL (menu.menu), NULL, FALSE);
    g_object_unref (menu.accel_group);
}


void GnomeCmdMainWin::update_bookmarks()
{
    update_mainmenu();

    if (auto bookmarks_dlg = static_cast<GnomeCmdBookmarksDialog*>(g_weak_ref_get (&priv->bookmarks_dlg)))
    {
        gnome_cmd_bookmarks_dialog_update (bookmarks_dlg);
        g_object_unref (bookmarks_dlg);
    }
}


void GnomeCmdMainWin::update_show_toolbar()
{
    if (gnome_cmd_data.show_toolbar)
    {
        create_toolbar (this);
        gtk_box_append (GTK_BOX (priv->vbox), priv->toolbar);
        gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->toolbar, 2);
        gtk_box_append (GTK_BOX (priv->vbox), priv->toolbar_sep);
        gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->toolbar_sep, 3);
    }
    else
    {
        if (priv->toolbar)
            gtk_container_remove (GTK_CONTAINER (priv->vbox), priv->toolbar);
        if (priv->toolbar_sep)
            gtk_container_remove (GTK_CONTAINER (priv->vbox), priv->toolbar_sep);
        priv->toolbar = NULL;
        priv->toolbar_sep = NULL;
    }

    update_drop_con_button(fs(ACTIVE)->file_list());
}


void GnomeCmdMainWin::update_buttonbar_visibility()
{
    if (gnome_cmd_data.buttonbar_visibility)
    {
        create_buttonbar();
        gtk_box_append (GTK_BOX (priv->vbox), priv->buttonbar_sep);
        gtk_box_append (GTK_BOX (priv->vbox), priv->buttonbar);
    }
    else
    {
        if (priv->buttonbar)
            gtk_container_remove (GTK_CONTAINER (priv->vbox), priv->buttonbar);
        if (priv->buttonbar_sep)
            gtk_container_remove (GTK_CONTAINER (priv->vbox), priv->buttonbar_sep);
        priv->buttonbar = NULL;
        priv->buttonbar_sep = NULL;
    }
}


void GnomeCmdMainWin::update_cmdline()
{
    if (priv->cmdline == nullptr)
        return;

    gchar *dpath = gnome_cmd_dir_get_display_path (fs(ACTIVE)->get_directory());
    gnome_cmd_cmdline_set_dir (priv->cmdline, dpath);
    g_free (dpath);
}


void GnomeCmdMainWin::update_cmdline_visibility()
{
    if (gnome_cmd_data.cmdline_visibility)
    {
        gint pos = 3;
        priv->cmdline_sep = create_separator (FALSE);
        priv->cmdline = gnome_cmd_cmdline_new ();
        gtk_widget_set_margin_top (GTK_WIDGET (priv->cmdline), 1);
        gtk_widget_set_margin_bottom (GTK_WIDGET (priv->cmdline), 1);
        gtk_widget_show (GTK_WIDGET (priv->cmdline));
        if (gnome_cmd_data.show_toolbar)
            pos += 2;
        gtk_box_append (GTK_BOX (priv->vbox), priv->cmdline_sep);
        gtk_box_append (GTK_BOX (priv->vbox), GTK_WIDGET (priv->cmdline));
        gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->cmdline_sep, pos);
        gtk_box_reorder_child (GTK_BOX (priv->vbox), GTK_WIDGET (priv->cmdline), pos+1);
    }
    else
    {
        if (priv->cmdline)
            gtk_container_remove (GTK_CONTAINER (priv->vbox), GTK_WIDGET (priv->cmdline));
        if (priv->cmdline_sep)
            gtk_container_remove (GTK_CONTAINER (priv->vbox), priv->cmdline_sep);
        priv->cmdline = NULL;
        priv->cmdline_sep = NULL;
    }
}


static gboolean set_equal_panes_idle (gpointer *user_data)
{
    GNOME_CMD_MAIN_WIN (user_data)->set_equal_panes();
    return G_SOURCE_REMOVE;
}


void GnomeCmdMainWin::update_horizontal_orientation()
{
    gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->paned),
        gnome_cmd_data.horizontal_orientation
            ? GTK_ORIENTATION_VERTICAL
            : GTK_ORIENTATION_HORIZONTAL);
    g_timeout_add (300, (GSourceFunc) set_equal_panes_idle, this);
}


void GnomeCmdMainWin::update_mainmenu_visibility()
{
    if (gnome_cmd_data.mainmenu_visibility)
    {
        gtk_widget_show (priv->menubar);
    }
    else
    {
        gtk_widget_hide (priv->menubar);
    }
}


void GnomeCmdMainWin::plugins_updated()
{
    update_mainmenu();
}


GnomeCmdState *GnomeCmdMainWin::get_state() const
{
    GnomeCmdFileSelector *fs1 = fs(ACTIVE);
    GnomeCmdFileSelector *fs2 = fs(INACTIVE);
    GnomeCmdDir *dir1 = fs1->get_directory();
    GnomeCmdDir *dir2 = fs2->get_directory();

    GnomeCmdState *state = &priv->state;
    state->activeDirGfile = dir1 ? GNOME_CMD_FILE (dir1)->get_file() : nullptr;
    state->inactiveDirGfile = dir2 ? GNOME_CMD_FILE (dir2)->get_file() : nullptr;
    state->active_dir_files = fs1->file_list()->get_visible_files();
    state->inactive_dir_files = fs2->file_list()->get_visible_files();
    state->active_dir_selected_files = fs1->file_list()->get_selected_files();
    state->inactive_dir_selected_files = fs2->file_list()->get_selected_files();

    return state;
}


void GnomeCmdMainWin::set_cap_state(gboolean state)
{
    g_simple_action_set_enabled (
        G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (this), "edit-cap-paste")),
        state);
}


void GnomeCmdMainWin::set_slide(gint percentage)
{
    GtkAllocation allocation;
    gtk_widget_get_allocation (GTK_WIDGET (priv->paned), &allocation);

    gint dimension = gnome_cmd_data.horizontal_orientation ? allocation.height : allocation.width;
    gint new_dimension = dimension * percentage / 100;

    gtk_paned_set_position (GTK_PANED (priv->paned), new_dimension);
}


void GnomeCmdMainWin::set_equal_panes()
{
    set_slide(50);
}


void GnomeCmdMainWin::maximize_pane()
{
    if (priv->current_fs==LEFT)
        set_slide(100);
    else
        set_slide(0);
}


GnomeCmdSearchDialog *GnomeCmdMainWin::get_or_create_search_dialog ()
{
    auto dlg = static_cast<GnomeCmdSearchDialog*>(g_weak_ref_get (&priv->file_search_dlg));
    if (!dlg)
    {
        dlg = new GnomeCmdSearchDialog(gnome_cmd_data.search_defaults);
        g_weak_ref_set (&priv->file_search_dlg, dlg);
    }
    return dlg;
}


GnomeCmdAdvrenameDialog *GnomeCmdMainWin::get_or_create_advrename_dialog ()
{
    auto dlg = static_cast<GnomeCmdAdvrenameDialog*>(g_weak_ref_get (&priv->advrename_dlg));
    if (!dlg)
    {
        dlg = new GnomeCmdAdvrenameDialog(gnome_cmd_data.advrename_defaults, *this);
        g_weak_ref_set (&priv->advrename_dlg, dlg);
    }
    return dlg;
}


GnomeCmdBookmarksDialog *GnomeCmdMainWin::get_or_create_bookmarks_dialog ()
{
    auto dlg = static_cast<GnomeCmdBookmarksDialog*>(g_weak_ref_get (&priv->bookmarks_dlg));
    if (!dlg)
    {
        dlg = gnome_cmd_bookmarks_dialog_new (GTK_WINDOW (this));
        g_weak_ref_set (&priv->bookmarks_dlg, dlg);
    }
    return dlg;
}

// FFI

GnomeCmdFileSelector *gnome_cmd_main_win_get_fs(GnomeCmdMainWin *main_win, FileSelectorID id)
{
    return main_win->fs(id);
}

void gnome_cmd_main_win_focus_file_lists(GnomeCmdMainWin *main_win)
{
    return main_win->focus_file_lists();
}

void gnome_cmd_main_win_update_bookmarks(GnomeCmdMainWin *main_win)
{
    main_win->update_bookmarks();
}
