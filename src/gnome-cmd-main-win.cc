/**
 * @file gnome-cmd-main-win.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2020 Uwe Scholz\n
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
#include "gnome-cmd-style.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-owner.h"
#include "utils.h"

using namespace std;


enum {SWITCH_FS, LAST_SIGNAL};


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
    GtkWindowClass parent_class;

    void (* switch_fs) (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs);
};


struct GnomeCmdMainWin::Private
{
    FileSelectorID current_fs;
    GnomeCmdState state;

    GtkWidget *main_win;
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
    GtkWidget *menubar_new;
    GtkWidget *toolbar;
    GtkWidget *toolbar_sep;
    GtkWidget *cmdline;
    GtkWidget *cmdline_sep;
    GtkWidget *buttonbar;
    GtkWidget *buttonbar_sep;

    GtkWidget *tb_first_btn;
    GtkWidget *tb_back_btn;
    GtkWidget *tb_fwd_btn;
    GtkWidget *tb_last_btn;
    GtkWidget *tb_con_drop_btn;
    GtkWidget *tb_cap_cut_btn;
    GtkWidget *tb_cap_copy_btn;
    GtkWidget *tb_cap_paste_btn;

    guint key_snooper_id;
};

static GtkWindowClass *parent_class = nullptr;

static guint signals[LAST_SIGNAL] = { 0 };

static void gnome_cmd_main_win_real_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs);


static gint gnome_cmd_key_snooper(GtkWidget *grab_widget, GdkEventKey *event, GnomeCmdMainWin *mw)
{
    g_return_val_if_fail (mw!=NULL, FALSE);

    if (event->type!=GDK_KEY_PRESS)
    {
        return FALSE;
    }

    if (!((event->keyval >= GDK_A && event->keyval <= GDK_Z) || (event->keyval >= GDK_a && event->keyval <= GDK_z) ||
          (event->keyval >= GDK_0 && event->keyval <= GDK_9) ||
          event->keyval == GDK_period || event->keyval == GDK_question|| event->keyval == GDK_asterisk || event->keyval == GDK_bracketleft))
    {
        return FALSE;
    }

    if (gnome_cmd_data.options.quick_search == GNOME_CMD_QUICK_SEARCH_CTRL_ALT)
    {
        return FALSE;
    }

    if ((gnome_cmd_data.options.quick_search == GNOME_CMD_QUICK_SEARCH_ALT)
        && (!state_is_alt (event->state) && !state_is_alt_shift (event->state)))
    {
        return FALSE;
    }
    
    if ((gnome_cmd_data.options.quick_search == GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER) &&
        (state_is_ctrl (event->state) || state_is_ctrl_shift (event->state) || state_is_ctrl_alt_shift (event->state)
        || (state_is_alt (event->state) || state_is_alt_shift (event->state))
        || (state_is_ctrl_alt(event->state))))
    {
        return FALSE;
    }

    GnomeCmdFileSelector *fs = mw->fs(ACTIVE);
    if (!fs || !fs->file_list())
    {
        return FALSE;
    }

    if (!GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (fs->file_list())))
    {
        return FALSE;
    }

    if (gnome_cmd_file_list_quicksearch_shown (fs->file_list()))
    {
        return FALSE;
    }

    gnome_cmd_file_list_show_quicksearch (fs->file_list(), event->keyval);

    return TRUE;
}


inline GtkWidget *add_buttonbar_button (char *label,
                                        GnomeCmdMainWin *mw,
                                        const char *data_label,
                                        GtkAccelGroup *accel_group,
                                        gint accel_signal_id)
{
    GtkWidget *button = create_styled_button (label);
    g_object_set_data_full (*mw, data_label, button, g_object_unref);
    gtk_widget_set_can_focus(button, FALSE);

    gtk_box_pack_start (GTK_BOX (mw->priv->buttonbar), button, TRUE, TRUE, 0);

    return button;
}


static GtkWidget *create_separator (gboolean vertical)
{
    GtkWidget *sep;
    GtkWidget *box;

    if (vertical)
    {
        sep = gtk_vseparator_new ();
        box = gtk_vbox_new (FALSE, 0);
    }
    else
    {
        sep = gtk_hseparator_new ();
        box = gtk_hbox_new (FALSE, 0);
    }

    g_object_ref (sep);
    gtk_widget_show (sep);

    g_object_ref (box);
    gtk_widget_show (box);

    gtk_box_pack_start (GTK_BOX (box), sep, TRUE, TRUE, 3);

    return box;
}

static void create_toolbar (GnomeCmdMainWin *mw)
{
    static const GtkActionEntry entries[] =
    {
        { "Refresh", GTK_STOCK_REFRESH, nullptr, nullptr, _("Refresh"), (GCallback) view_refresh },
        { "Up", GTK_STOCK_GO_UP, nullptr, nullptr, _("Up one directory"), (GCallback) view_up },
        { "First", GTK_STOCK_GOTO_FIRST, nullptr, nullptr, _("Go to the oldest"), (GCallback) view_first },
        { "Back", GTK_STOCK_GO_BACK, nullptr, nullptr, _("Go back"), (GCallback) view_back },
        { "Forward", GTK_STOCK_GO_FORWARD, nullptr, nullptr, _("Go forward"), (GCallback) view_forward },
        { "Latest", GTK_STOCK_GOTO_LAST, nullptr, nullptr, _("Go to the latest"), (GCallback) view_last },
        { "CopyFileNames", COPYFILENAMES_STOCKID, nullptr, nullptr, _("Copy file names (SHIFT for full paths, ALT for URIs)"), (GCallback) edit_copy_fnames },
        { "Cut", GTK_STOCK_CUT, nullptr, nullptr, _("Cut"), (GCallback) edit_cap_cut },
        { "Copy", GTK_STOCK_COPY, nullptr, nullptr, _("Copy"), (GCallback) edit_cap_copy },
        { "Paste", GTK_STOCK_PASTE, nullptr, nullptr, _("Paste"), (GCallback) edit_cap_paste },
        { "Delete", GTK_STOCK_DELETE, nullptr, nullptr, _("Delete"), (GCallback) file_delete },
        { "Edit", GTK_STOCK_EDIT,nullptr, nullptr, _("Edit (SHIFT for new document)"), (GCallback) file_edit },
        { "Send", MAILSEND_STOCKID, nullptr, nullptr, _("Send files"), (GCallback) file_sendto },
        { "Terminal", TERMINAL_STOCKID, nullptr, nullptr, _("Open terminal (SHIFT for root privileges)"), (GCallback) command_open_terminal__internal },
        { "Remote", GTK_STOCK_CONNECT, nullptr, nullptr, _("Remote Server"), (GCallback) connections_open },
        { "Drop", nullptr, nullptr, nullptr, _("Drop connection"), (GCallback) connections_close_current }
    };

    static const char *ui_description =
    "<ui>"
    "  <toolbar action='Toolbar'>"
    "    <toolitem action='Refresh'/>"
    "    <toolitem action='Up'/>"
    "    <toolitem action='First'/>"
    "    <toolitem action='Back'/>"
    "    <toolitem action='Forward'/>"
    "    <toolitem action='Latest'/>"
    "    <separator/>"
    "    <toolitem action='CopyFileNames'/>"
    "    <toolitem action='Cut'/>"
    "    <toolitem action='Copy'/>"
    "    <toolitem action='Paste'/>"
    "    <toolitem action='Delete'/>"
    "    <toolitem action='Edit'/>"
    "    <toolitem action='Send'/>"
    "    <toolitem action='Terminal'/>"
    "    <separator/>"
    "    <toolitem action='Remote'/>"
    "    <toolitem action='Drop'/>"
    "  </toolbar>"
    "</ui>";

    GtkActionGroup *action_group;
    GtkUIManager *ui_manager;
    GError *error;

    action_group = gtk_action_group_new ("MenuActions");
    gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), mw);

    ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

    error = NULL;
    if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &error))
      {
        g_message ("building menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
      }

    mw->priv->toolbar = gtk_ui_manager_get_widget (ui_manager, "/Toolbar");
    mw->priv->tb_first_btn = gtk_ui_manager_get_widget (ui_manager, "/Toolbar/First");
    mw->priv->tb_back_btn = gtk_ui_manager_get_widget (ui_manager, "/Toolbar/Back");
    mw->priv->tb_fwd_btn = gtk_ui_manager_get_widget (ui_manager, "/Toolbar/Forward");
    mw->priv->tb_last_btn = gtk_ui_manager_get_widget (ui_manager, "/Toolbar/Latest");
    mw->priv->tb_cap_cut_btn = gtk_ui_manager_get_widget (ui_manager, "/Toolbar/Cut");
    mw->priv->tb_cap_copy_btn = gtk_ui_manager_get_widget (ui_manager, "/Toolbar/Copy");
    mw->priv->tb_cap_paste_btn = gtk_ui_manager_get_widget (ui_manager, "/Toolbar/Paste");
    mw->priv->tb_con_drop_btn = gtk_ui_manager_get_widget (ui_manager, "/Toolbar/Drop");

    gtk_widget_set_can_focus(mw->priv->tb_first_btn, FALSE);
    gtk_widget_set_can_focus(mw->priv->tb_back_btn, FALSE);
    gtk_widget_set_can_focus(mw->priv->tb_fwd_btn, FALSE);
    gtk_widget_set_can_focus(mw->priv->tb_last_btn, FALSE);
    gtk_widget_set_can_focus(mw->priv->tb_cap_cut_btn, FALSE);
    gtk_widget_set_can_focus(mw->priv->tb_cap_copy_btn, FALSE);
    gtk_widget_set_can_focus(mw->priv->tb_cap_paste_btn, FALSE);
    gtk_widget_set_can_focus(mw->priv->tb_con_drop_btn, FALSE);

    g_object_ref (mw->priv->toolbar);
    g_object_set_data_full (*mw, "toolbar", mw->priv->toolbar, g_object_unref);
    gtk_widget_show (mw->priv->toolbar);

    mw->priv->toolbar_sep = create_separator (FALSE);
    gtk_widget_set_sensitive (mw->priv->tb_cap_paste_btn, FALSE);
}


static void slide_set_100_0 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.horizontal_orientation ? GTK_WIDGET (main_win)->allocation.height :
                                                              GTK_WIDGET (main_win)->allocation.width);
}


static void slide_set_80_20 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.horizontal_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.8f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.8f));
}


static void slide_set_60_40 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.horizontal_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.6f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.6f));
}


static void slide_set_50_50 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.horizontal_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.5f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.5f));
}


static void slide_set_40_60 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.horizontal_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.4f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.4f));
}


static void slide_set_20_80 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.horizontal_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.2f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.2f));
}


static void slide_set_0_100 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned), 0);
}


static GtkWidget *create_slide_popup ()
{
    static const GtkActionEntry entries[] =
    {
        { "align_100_0", NULL, "100 - 0", nullptr, nullptr, (GCallback) slide_set_100_0},
        { "align_80_20", NULL, "80 - 20", nullptr, nullptr, (GCallback) slide_set_80_20},
        { "align_60_40", NULL, "60 - 40", nullptr, nullptr, (GCallback) slide_set_60_40},
        { "align_50_50", NULL, "50 - 50", nullptr, nullptr, (GCallback) slide_set_50_50},
        { "align_40_60", NULL, "40 - 60", nullptr, nullptr, (GCallback) slide_set_40_60},
        { "align_20_80", NULL, "20 - 80", nullptr, nullptr, (GCallback) slide_set_20_80},
        { "align_0_100", NULL, "0 - 100", nullptr, nullptr, (GCallback) slide_set_0_100},
    };

    static const char *ui_description =
    "<ui>"
    "  <popup name='Popup'>"
    "    <menuitem action='align_100_0'/>"
    "    <menuitem action='align_80_20'/>"
    "    <menuitem action='align_60_40'/>"
    "    <menuitem action='align_50_50'/>"
    "    <menuitem action='align_40_60'/>"
    "    <menuitem action='align_20_80'/>"
    "    <menuitem action='align_0_100'/>"
    "  </popup>"
    "</ui>";

    GtkActionGroup *action_group;
    GtkUIManager *ui_manager;
    GError *error;

    action_group = gtk_action_group_new ("MenuActions");
    gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), main_win);

    ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

    error = NULL;
    if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &error))
      {
        g_message ("building menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
      }

    GtkWidget *menu = gtk_ui_manager_get_widget (ui_manager, "/Popup");

    g_object_ref (menu);
    g_object_set_data_full (*main_win, "slide-popup", menu, g_object_unref);

    return GTK_WIDGET (menu);
}


/*****************************************************************************
    Buttonbar callbacks
*****************************************************************************/

static void on_help_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    help_help (NULL);
}


static void on_rename_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    file_rename (NULL);
}


static void on_view_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    file_view (NULL);
}


static void on_edit_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    file_edit (NULL);
}


static void on_copy_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    file_copy (NULL);
}


static void on_move_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    file_move (NULL);
}


static void on_mkdir_clicked(GtkButton *button, GnomeCmdMainWin *mw)
{
    file_mkdir (NULL);
}


static void on_delete_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    file_delete (NULL);
}


static void on_search_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    edit_search (NULL);
}


void GnomeCmdMainWin::create_buttonbar()
{
    priv->buttonbar_sep = create_separator (FALSE);

    priv->buttonbar = gtk_hbox_new (FALSE, 0);
    g_object_ref (priv->buttonbar);
    g_object_set_data_full (*this, "buttonbar", priv->buttonbar, g_object_unref);
    gtk_widget_show (priv->buttonbar);

    priv->view_btn = add_buttonbar_button(_("F3 View"), this, "view_btn", priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    priv->edit_btn = add_buttonbar_button(_("F4 Edit"), this, "edit_btn", priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    priv->copy_btn = add_buttonbar_button(_("F5 Copy"), this, "copy_btn", priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    priv->move_btn = add_buttonbar_button(_("F6 Move"), this, "move_btn", priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    priv->mkdir_btn = add_buttonbar_button(_("F7 Mkdir"), this, "mkdir_btn", priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    priv->delete_btn = add_buttonbar_button(_("F8 Delete"), this, "delete_btn", priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    priv->find_btn = add_buttonbar_button(_("F9 Search"), this, "find_btn", priv->accel_group, 0);

    g_signal_connect (priv->view_btn, "clicked", G_CALLBACK (on_view_clicked), this);
    g_signal_connect (priv->edit_btn, "clicked", G_CALLBACK (on_edit_clicked), this);
    g_signal_connect (priv->copy_btn, "clicked", G_CALLBACK (on_copy_clicked), this);
    g_signal_connect (priv->move_btn, "clicked", G_CALLBACK (on_move_clicked), this);
    g_signal_connect (priv->mkdir_btn, "clicked", G_CALLBACK (on_mkdir_clicked), this);
    g_signal_connect (priv->delete_btn, "clicked", G_CALLBACK (on_delete_clicked), this);
    g_signal_connect (priv->find_btn, "clicked", G_CALLBACK (on_search_clicked), this);
}


/*****************************************************************************
    Misc widgets callbacks
*****************************************************************************/

static gboolean on_slide_button_press (GtkWidget *widget, GdkEventButton *event, GnomeCmdMainWin *mw)
{
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        GtkPaned *paned = GTK_PANED (mw->priv->paned);

        // Check that the handle was clicked and not one of the children
        if (paned->handle == event->window)
        {
            gtk_menu_popup (GTK_MENU (create_slide_popup ()), NULL, NULL, NULL, NULL, event->button, event->time);
            return TRUE;
        }
    }

    return FALSE;
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

    gdk_window_set_icon (GTK_WIDGET (mw)->window, NULL, IMAGE_get_pixmap (PIXMAP_LOGO), IMAGE_get_mask (PIXMAP_LOGO));
}


static gboolean on_left_fs_select (GtkCList *list, GdkEventButton *event, GnomeCmdMainWin *mw)
{
    mw->priv->current_fs = LEFT;

    GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[LEFT])->set_active(TRUE);
    GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[RIGHT])->set_active(FALSE);

    return FALSE;
}


static gboolean on_right_fs_select (GtkCList *list, GdkEventButton *event, GnomeCmdMainWin *mw)
{
    mw->priv->current_fs = RIGHT;

    GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[RIGHT])->set_active(TRUE);
    GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[LEFT])->set_active(FALSE);

    return FALSE;
}


static void on_fs_list_resize_column (GtkCList *clist, gint column, gint width, GtkCList *other_clist)
{
    static gboolean column_resize_lock = FALSE;

    /* the lock is used so that we dont get into the situation where
       the left list triggers the right witch triggers the left ... */
    if (!column_resize_lock)
    {
        column_resize_lock = TRUE;
        gnome_cmd_data.fs_col_width[column] = width;
        gtk_clist_set_column_width (other_clist, column, width);
        column_resize_lock = FALSE;
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
        if (gnome_cmd_data.show_toolbar)
        {
            gtk_widget_set_sensitive (mw->priv->tb_first_btn, fs->can_back());
            gtk_widget_set_sensitive (mw->priv->tb_back_btn, fs->can_back());
            gtk_widget_set_sensitive (mw->priv->tb_fwd_btn, fs->can_forward());
            gtk_widget_set_sensitive (mw->priv->tb_last_btn, fs->can_forward());
        }

        gnome_cmd_main_menu_update_sens (GNOME_CMD_MAIN_MENU (mw->priv->menubar));
    }
}


void GnomeCmdMainWin::update_drop_con_button(GnomeCmdFileList *fl)
{
    if (!fl)
        return;

    GnomeCmdPixmap *pm = NULL;
    static GtkWidget *prev_pixmap = NULL;

    GnomeCmdCon *con = fl->con;
    if (!con)
        return;

    if (!gnome_cmd_data.show_toolbar
        || (gnome_cmd_data.options.skip_mounting && GNOME_CMD_IS_CON_DEVICE (con)))
        return;

    GtkWidget *btn = priv->tb_con_drop_btn;
    g_return_if_fail (GTK_IS_TOOL_BUTTON (btn));

    if (prev_pixmap)
    {
        gtk_widget_destroy (prev_pixmap);
        prev_pixmap = NULL;
    }

    if (gnome_cmd_con_is_closeable (con))
        pm = gnome_cmd_con_get_close_pixmap (con);
    else
    {
        gtk_widget_set_sensitive (btn, FALSE);
        return;
    }

    gtk_widget_set_tooltip_text(btn, gnome_cmd_con_get_close_tooltip (con));
    gtk_widget_set_sensitive (btn, TRUE);

    if (pm)
    {
        GtkWidget *pixmap = gtk_pixmap_new (pm->pixmap, pm->mask);
        if (pixmap)
        {
            g_object_ref (pixmap);
            gtk_widget_show (pixmap);
            gtk_container_add (GTK_CONTAINER (btn), pixmap);
            prev_pixmap = pixmap;
        }
    }
    else
    {
        GtkWidget *label = gtk_label_new (gnome_cmd_con_get_close_text (con));
        if (label)
        {
            g_object_ref (label);
            gtk_widget_show (label);
            gtk_container_add (GTK_CONTAINER (btn), label);
            prev_pixmap = label;
        }
    }
}


static void on_fs_dir_change (GnomeCmdFileSelector *fs, const gchar dir, GnomeCmdMainWin *mw)
{
    update_browse_buttons (mw, fs);
    mw->update_drop_con_button(fs->file_list());
}


inline void restore_size_and_pos (GnomeCmdMainWin *mw)
{
    gint x, y;

    gtk_widget_set_size_request (*mw,
                                 gnome_cmd_data.main_win_width,
                                 gnome_cmd_data.main_win_height);

    gnome_cmd_data_get_main_win_pos (&x, &y);
    if (x >= 0 && y >= 0)
        gtk_window_move (GTK_WINDOW (mw), x, y);

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


static void on_delete_event (GnomeCmdMainWin *mw, GdkEvent *event, gpointer user_data)
{
    file_exit (NULL, mw);
}


static gboolean on_window_state_event (GtkWidget *mw, GdkEventWindowState *event, gpointer user_data)
{
    gint x, y;

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (event->new_window_state)
    {
        case GDK_WINDOW_STATE_MAXIMIZED:    // not usable
        case GDK_WINDOW_STATE_FULLSCREEN:   // not usable
                break;

        case GDK_WINDOW_STATE_ICONIFIED:
            if (gnome_cmd_data.main_win_state == GDK_WINDOW_STATE_MAXIMIZED ||  // prev state
                gnome_cmd_data.main_win_state == GDK_WINDOW_STATE_FULLSCREEN)
                break;  // not usable

#if defined (__GNUC__) && __GNUC__ >= 7
        __attribute__ ((fallthrough));
#endif
        default:            // other are usable
            gdk_window_get_root_origin (mw->window, &x, &y);
            gnome_cmd_data_set_main_win_pos (x, y);
    }

#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
    gnome_cmd_data.main_win_state = event->new_window_state;

    return FALSE;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    if (main_win && main_win->priv && main_win->priv->key_snooper_id)
    {
        gtk_key_snooper_remove (main_win->priv->key_snooper_id);
        main_win->priv->key_snooper_id = 0;
    }

    if (main_win && main_win->advrename_dlg)
        gtk_widget_destroy (*main_win->advrename_dlg);

    if (main_win && main_win->file_search_dlg)
        gtk_widget_destroy (*main_win->file_search_dlg);

    gtk_main_quit ();
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdMainWinClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkWindowClass *) gtk_type_class (gtk_window_get_type ());

    signals[SWITCH_FS] =
        g_signal_new ("switch-fs",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdMainWinClass, switch_fs),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    object_class->destroy = destroy;
    widget_class->map = ::map;
    klass->switch_fs = gnome_cmd_main_win_real_switch_fs;
}


static void init (GnomeCmdMainWin *mw)
{
    /* It is very important that this global variable gets assigned here so that
     * child widgets to this window can use that variable when initializing
     */
    main_win = GNOME_CMD_MAIN_WIN (mw);

    mw->advrename_dlg = NULL;
    mw->file_search_dlg = NULL;
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

    gtk_window_set_title (GTK_WINDOW (mw),
                          gcmd_owner.is_root()
                            ? _("GNOME Commander — ROOT PRIVILEGES")
                            : _("GNOME Commander"));

    g_object_set_data (*mw, "main_win", mw);
    restore_size_and_pos (mw);
    gtk_window_set_policy (*mw, TRUE, TRUE, FALSE);

    mw->priv->vbox = gtk_vbox_new (FALSE, 0);
    g_object_ref (mw->priv->vbox);
    g_object_set_data_full (*mw, "vbox", mw->priv->vbox, g_object_unref);
    gtk_widget_show (mw->priv->vbox);

    mw->priv->menubar = gnome_cmd_main_menu_new ();
    mw->priv->menubar_new = get_gnome_cmd_main_menu_bar (GNOME_CMD_MAIN_MENU (mw->priv->menubar));

    g_object_ref (mw->priv->menubar);
    g_object_ref (mw->priv->menubar_new);
    g_object_set_data_full (*mw, "vbox", mw->priv->menubar, g_object_unref);
    g_object_set_data_full (*mw, "vbox", mw->priv->menubar_new, g_object_unref);
    if(gnome_cmd_data.mainmenu_visibility)
    {
        gtk_widget_show (mw->priv->menubar_new);
    }
    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->menubar_new, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), create_separator (FALSE), FALSE, TRUE, 0);

    gtk_widget_show (mw->priv->vbox);
    gtk_container_add (GTK_CONTAINER (mw), mw->priv->vbox);

    mw->priv->paned = gnome_cmd_data.horizontal_orientation ? gtk_vpaned_new () : gtk_hpaned_new ();

    g_object_ref (mw->priv->paned);
    g_object_set_data_full (*mw, "paned", mw->priv->paned, g_object_unref);
    gtk_widget_show (mw->priv->paned);
    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->paned, TRUE, TRUE, 0);
    mw->create_buttonbar();

    mw->priv->file_selector[LEFT] = gnome_cmd_file_selector_new ();
    g_object_ref (mw->priv->file_selector[LEFT]);
    g_object_set_data_full (*mw, "left_file_selector", mw->priv->file_selector[LEFT], g_object_unref);
    gtk_widget_show (mw->priv->file_selector[LEFT]);
    gtk_paned_pack1 (GTK_PANED (mw->priv->paned), mw->priv->file_selector[LEFT], TRUE, TRUE);

    mw->priv->file_selector[RIGHT] = gnome_cmd_file_selector_new ();
    g_object_ref (mw->priv->file_selector[RIGHT]);
    g_object_set_data_full (*mw, "right_file_selector", mw->priv->file_selector[RIGHT], g_object_unref);
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
    g_signal_connect (mw, "delete-event", G_CALLBACK (on_delete_event), mw);
    g_signal_connect (mw->priv->paned, "button-press-event", G_CALLBACK (on_slide_button_press), mw);
    g_signal_connect (mw, "window-state-event", G_CALLBACK (on_window_state_event), NULL);

    g_signal_connect (mw->fs(LEFT)->file_list(), "resize-column", G_CALLBACK (on_fs_list_resize_column), mw->fs(RIGHT)->file_list());
    g_signal_connect (mw->fs(RIGHT)->file_list(), "resize-column", G_CALLBACK (on_fs_list_resize_column), mw->fs(LEFT)->file_list());
    g_signal_connect (mw->fs(LEFT)->file_list(), "button-press-event", G_CALLBACK (on_left_fs_select), mw);
    g_signal_connect (mw->fs(RIGHT)->file_list(), "button-press-event", G_CALLBACK (on_right_fs_select), mw);

    gtk_window_add_accel_group (*mw, mw->priv->accel_group);
    mw->focus_file_lists();

    mw->priv->key_snooper_id = gtk_key_snooper_install ((GtkKeySnoopFunc) gnome_cmd_key_snooper, mw);
}


GtkType gnome_cmd_main_win_get_type ()
{
    static GtkType mw_type = 0;

    if (mw_type == 0)
    {
        GtkTypeInfo mw_info =
        {
            (gchar*) "GnomeCmdMainWin",
            sizeof (GnomeCmdMainWin),
            sizeof (GnomeCmdMainWinClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        mw_type = gtk_type_unique (gtk_window_get_type (), &mw_info);
    }

    return mw_type;
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
    gnome_cmd_style_create (gnome_cmd_data.options);
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


gboolean GnomeCmdMainWin::key_pressed(GdkEventKey *event)
{
    if (state_is_ctrl_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_c:
            case GDK_C:
                if (gnome_cmd_data.cmdline_visibility && (gnome_cmd_data.options.quick_search == GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER))
                    gnome_cmd_cmdline_focus(main_win->get_cmdline());
                return TRUE;
                break;
        }
    }
    else if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_F8:
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
            case GDK_H:
            case GDK_h:
                gnome_cmd_data.options.filter.hidden = !gnome_cmd_data.options.filter.hidden;
                gnome_cmd_data.save();
                return TRUE;
            default:
                break;
        }
    }
    else if (state_is_ctrl (event->state))
    {
        switch (event->keyval)
        {
            case GDK_e:
            case GDK_E:
            case GDK_Down:
                if (gnome_cmd_data.cmdline_visibility)
                    gnome_cmd_cmdline_show_history (GNOME_CMD_CMDLINE (priv->cmdline));
                return TRUE;

            case GDK_u:
            case GDK_U:
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

                    clear_event_key (event);
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
            case GDK_P:
            case GDK_p:
                plugin_manager_show ();
                break;

            case GDK_f:
            case GDK_F:
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
                case GDK_Tab:
                case GDK_ISO_Left_Tab:
                    // hack to avoid the default handling of TAB
                    clear_event_key (event);
                    switch_fs(fs(INACTIVE));
                    return TRUE;

                case GDK_F1:
                    on_help_clicked (NULL, this);
                    return TRUE;

                case GDK_F2:
                    on_rename_clicked (NULL, this);
                    return TRUE;

                case GDK_F3:
                    on_view_clicked (NULL, this);
                    return TRUE;

                case GDK_F4:
                    on_edit_clicked (NULL, this);
                    return TRUE;

                case GDK_F5:
                    on_copy_clicked (NULL, this);
                    return TRUE;

                case GDK_F6:
                    on_move_clicked (NULL, this);
                    return TRUE;

                case GDK_F7:
                    on_mkdir_clicked (NULL, this);
                    return TRUE;

                case GDK_F8:
                    on_delete_clicked (NULL, this);
                    return TRUE;

                case GDK_F9:
                    on_search_clicked (NULL, this);
                    return TRUE;

                default:
                    break;
            }

    return fs(ACTIVE)->key_pressed(event);
}


void GnomeCmdMainWin::open_tabs(FileSelectorID id)
{
    GnomeCmdCon *home = get_home_con ();

    if (gnome_cmd_data.tabs[id].empty())
        gnome_cmd_data.tabs[id].push_back(make_pair(string(g_get_home_dir ()), make_triple(GnomeCmdFileList::COLUMN_NAME, GTK_SORT_ASCENDING, FALSE)));

    auto last_tab = unique(gnome_cmd_data.tabs[id].begin(), gnome_cmd_data.tabs[id].end());

    for (auto tab=gnome_cmd_data.tabs[id].begin(); tab!=last_tab; ++tab)
    {
        auto *gnomeCmdDir = gnome_cmd_dir_new (home, gnome_cmd_con_create_path (home, tab->first.c_str()));
        fs(id)->new_tab(gnomeCmdDir, tab->second.first, tab->second.second, tab->second.third, TRUE);
    }
}


void GnomeCmdMainWin::switch_fs(GnomeCmdFileSelector *fselector)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fselector));

    g_signal_emit (this, signals[SWITCH_FS], 0, fselector);
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

        if (file && file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
            dir = GNOME_CMD_IS_DIR (file) ? GNOME_CMD_DIR (file) : gnome_cmd_dir_new_from_info (file->info, dir);
    }

    if (fselector->file_list()->locked)
        fselector->new_tab(dir);
    else
        fselector->file_list()->set_connection(other->get_connection(), dir);

    other->set_active(!fs_is_active);
    fselector->set_active(fs_is_active);
}


static void gnome_cmd_main_win_real_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs == mw->fs(ACTIVE))
        return;

    mw->priv->current_fs = (FileSelectorID) !mw->priv->current_fs;
    mw->fs(ACTIVE)->set_active(TRUE);
    mw->fs(INACTIVE)->set_active(FALSE);

    update_browse_buttons (mw, fs);
    mw->update_drop_con_button(fs->file_list());
}


GnomeCmdCmdline *GnomeCmdMainWin::get_cmdline() const
{
    return GNOME_CMD_CMDLINE (priv->cmdline);
}


void GnomeCmdMainWin::update_bookmarks()
{
    gnome_cmd_main_menu_update_bookmarks (GNOME_CMD_MAIN_MENU (priv->menubar));
}


void GnomeCmdMainWin::update_show_toolbar()
{
    if (gnome_cmd_data.show_toolbar)
    {
        create_toolbar (this);
        gtk_box_pack_start (GTK_BOX (priv->vbox), priv->toolbar, FALSE, TRUE, 0);
        gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->toolbar, 2);
        gtk_box_pack_start (GTK_BOX (priv->vbox), priv->toolbar_sep, FALSE, TRUE, 0);
        gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->toolbar_sep, 3);
    }
    else
    {
        if (priv->toolbar)
            gtk_widget_destroy (priv->toolbar);
        if (priv->toolbar_sep)
            gtk_widget_destroy (priv->toolbar_sep);
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
        gtk_box_pack_start (GTK_BOX (priv->vbox), priv->buttonbar_sep, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (priv->vbox), priv->buttonbar, FALSE, TRUE, 0);
    }
    else
    {
        if (priv->buttonbar)
            gtk_widget_destroy (priv->buttonbar);
        if (priv->buttonbar_sep)
            gtk_widget_destroy (priv->buttonbar_sep);
        priv->buttonbar = NULL;
        priv->buttonbar_sep = NULL;
    }
}


void GnomeCmdMainWin::update_cmdline_visibility()
{
    if (gnome_cmd_data.cmdline_visibility)
    {
        gint pos = 3;
        priv->cmdline_sep = create_separator (FALSE);
        priv->cmdline = gnome_cmd_cmdline_new ();
        g_object_ref (priv->cmdline);
        g_object_set_data_full (*this, "cmdline", priv->cmdline, g_object_unref);
        gtk_widget_show (priv->cmdline);
        if (gnome_cmd_data.show_toolbar)
            pos += 2;
        gtk_box_pack_start (GTK_BOX (priv->vbox), priv->cmdline_sep, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (priv->vbox), priv->cmdline, FALSE, TRUE, 1);
        gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->cmdline_sep, pos);
        gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->cmdline, pos+1);
    }
    else
    {
        if (priv->cmdline)
            gtk_widget_destroy (priv->cmdline);
        if (priv->cmdline_sep)
            gtk_widget_destroy (priv->cmdline_sep);
        priv->cmdline = NULL;
        priv->cmdline_sep = NULL;
    }
}


void GnomeCmdMainWin::update_horizontal_orientation()
{
    gint pos = 2;

    g_object_ref (priv->file_selector[LEFT]);
    g_object_ref (priv->file_selector[RIGHT]);
    gtk_container_remove (GTK_CONTAINER (priv->paned), priv->file_selector[LEFT]);
    gtk_container_remove (GTK_CONTAINER (priv->paned), priv->file_selector[RIGHT]);

    gtk_object_destroy (GTK_OBJECT (priv->paned));

    priv->paned = gnome_cmd_data.horizontal_orientation ? gtk_vpaned_new () : gtk_hpaned_new ();

    g_object_ref (priv->paned);
    g_object_set_data_full (*this, "paned", priv->paned, g_object_unref);
    gtk_widget_show (priv->paned);

    gtk_paned_pack1 (GTK_PANED (priv->paned), priv->file_selector[LEFT], TRUE, TRUE);
    gtk_paned_pack2 (GTK_PANED (priv->paned), priv->file_selector[RIGHT], TRUE, TRUE);

    if (gnome_cmd_data.show_toolbar)
        pos += 2;

    gtk_box_pack_start (GTK_BOX (priv->vbox), priv->paned, TRUE, TRUE, 0);
    gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->paned, pos);

    g_object_unref (priv->file_selector[LEFT]);
    g_object_unref (priv->file_selector[RIGHT]);

    g_signal_connect (priv->paned, "button-press-event", G_CALLBACK (on_slide_button_press), this);
    slide_set_50_50 (NULL, NULL);
}


void GnomeCmdMainWin::update_mainmenu_visibility()
{
    if (gnome_cmd_data.mainmenu_visibility)
    {
        gtk_widget_show (priv->menubar_new);
    }
    else
    {
        gtk_widget_hide (priv->menubar_new);
    }
}


void GnomeCmdMainWin::add_plugin_menu(PluginData *pluginData)
{
    gnome_cmd_main_menu_add_plugin_menu (GNOME_CMD_MAIN_MENU (priv->menubar), pluginData);
}


GnomeCmdState *GnomeCmdMainWin::get_state() const
{
    GnomeCmdFileSelector *fs1 = fs(ACTIVE);
    GnomeCmdFileSelector *fs2 = fs(INACTIVE);
    GnomeCmdDir *dir1 = fs1->get_directory();
    GnomeCmdDir *dir2 = fs2->get_directory();

    GnomeCmdState *state = &priv->state;
    state->active_dir_uri = GNOME_CMD_FILE (dir1)->get_uri();
    state->inactive_dir_uri = GNOME_CMD_FILE (dir2)->get_uri();
    state->active_dir_files = fs1->file_list()->get_visible_files();
    state->inactive_dir_files = fs2->file_list()->get_visible_files();
    state->active_dir_selected_files = fs1->file_list()->get_selected_files();
    state->inactive_dir_selected_files = fs2->file_list()->get_selected_files();

    return state;
}


void GnomeCmdMainWin::set_cap_state(gboolean state)
{
    gtk_widget_set_sensitive (priv->tb_cap_paste_btn, state);
}


void GnomeCmdMainWin::set_equal_panes()
{
    slide_set_50_50 (NULL, NULL);
}


void GnomeCmdMainWin::maximize_pane()
{
    if (priv->current_fs==LEFT)
        slide_set_100_0 (NULL, NULL);
    else
        slide_set_0_100 (NULL, NULL);
}
