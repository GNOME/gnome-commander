/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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

#include <config.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-user-actions.h"
#include "plugin_manager.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-main-menu.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-bookmark-dialog.h"
#include "owner.h"
#include "utils.h"

#include "../pixmaps/copy_file_names.xpm"

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


struct GnomeCmdMainWinPrivate
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
    GtkWidget *quit_btn;

    GtkWidget *menubar;
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

static GnomeAppClass *parent_class = NULL;

static guint main_win_signals[LAST_SIGNAL] = { 0 };
static GtkTooltips *toolbar_tooltips = NULL;
extern gchar *start_dir_left;   //main.cc
extern gchar *start_dir_right;  //main.cc

static void gnome_cmd_main_win_real_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs);


gint gnome_cmd_key_snooper(GtkWidget *grab_widget, GdkEventKey *event, GnomeCmdMainWin *mw)
{
    g_return_val_if_fail (mw!=NULL, FALSE);
    g_return_val_if_fail (mw->priv!=NULL,FALSE);

    if (event->type!=GDK_KEY_PRESS)
        return FALSE;

    if (!((event->keyval >= GDK_A && event->keyval <= GDK_Z) ||
            (event->keyval >= GDK_a && event->keyval <= GDK_z) ||
            (event->keyval == GDK_period)))
        return FALSE;

    if (!gnome_cmd_data.alt_quick_search)
        return FALSE;

    if (!state_is_alt (event->state) && !state_is_alt_shift (event->state))
        return FALSE;

    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_fs (mw, ACTIVE);
    if (fs==NULL || fs->file_list()==NULL)
        return FALSE;

    if (!GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (fs->file_list())))
        return FALSE;

    if (gnome_cmd_file_list_quicksearch_shown (fs->file_list()))
        return FALSE;

    gnome_cmd_file_list_show_quicksearch (fs->file_list(), event->keyval);

    return TRUE;
}


inline GtkWidget *add_buttonbar_button (char *label,
                                        GnomeCmdMainWin *mw,
                                        char *data_label,
                                        GtkAccelGroup *accel_group,
                                        gint accel_signal_id)
{
    GtkWidget *button = create_styled_button (label);
    gtk_object_set_data_full (GTK_OBJECT (main_win),
                              data_label,
                              button,
                              (GtkDestroyNotify) gtk_widget_unref);
    GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);

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

    gtk_widget_ref (sep);
    gtk_widget_show (sep);

    gtk_widget_ref (box);
    gtk_widget_show (box);

    gtk_box_pack_start (GTK_BOX (box), sep, TRUE, TRUE, 3);

    return box;
}


static void create_toolbar (GnomeCmdMainWin *mw, GnomeUIInfo *uiinfo)
{
    gint i;

    mw->priv->toolbar = gtk_hbox_new (FALSE, 0);
    gtk_widget_ref (mw->priv->toolbar);
    gtk_object_set_data_full (GTK_OBJECT (mw), "toolbar", mw->priv->toolbar, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (mw->priv->toolbar);

    if (!toolbar_tooltips)
        toolbar_tooltips = gtk_tooltips_new ();

    for (i=0; uiinfo[i].type != GNOME_APP_UI_ENDOFINFO; i++)
    {
        GtkWidget *w;

        if (uiinfo[i].type == GNOME_APP_UI_SEPARATOR)
            w = create_separator (TRUE);
        else
        {
            GtkWidget *pixmap;

            w = create_styled_button (NULL);
            gtk_signal_connect (GTK_OBJECT (w), "clicked", GTK_SIGNAL_FUNC (uiinfo[i].moreinfo), uiinfo[i].user_data);
            gtk_tooltips_set_tip (toolbar_tooltips, w, uiinfo[i].hint, NULL);
            GTK_WIDGET_UNSET_FLAGS (w, GTK_CAN_FOCUS);

            pixmap = create_ui_pixmap (
                GTK_WIDGET (mw),
                uiinfo[i].pixmap_type, uiinfo[i].pixmap_info,
                GTK_ICON_SIZE_LARGE_TOOLBAR);
            if (pixmap)
            {
                gtk_widget_ref (pixmap);
                gtk_widget_show (pixmap);
                gtk_container_add (GTK_CONTAINER (w), pixmap);
            }

            switch (i)
            {
                case  TOOLBAR_BTN_FIRST:      mw->priv->tb_first_btn = w;  break;
                case  TOOLBAR_BTN_BACK:       mw->priv->tb_back_btn = w;  break;
                case  TOOLBAR_BTN_FORWARD:    mw->priv->tb_fwd_btn = w;  break;
                case  TOOLBAR_BTN_LAST:       mw->priv->tb_last_btn = w;  break;
                case  TOOLBAR_BTN_CUT:        mw->priv->tb_cap_cut_btn = w;  break;
                case  TOOLBAR_BTN_COPY:       mw->priv->tb_cap_copy_btn = w;  break;
                case  TOOLBAR_BTN_PASTE:      mw->priv->tb_cap_paste_btn = w;  break;
                case  TOOLBAR_BTN_DISCONNECT: mw->priv->tb_con_drop_btn = w;  break;
            }
        }

        gtk_widget_show (w);
        gtk_box_pack_start (GTK_BOX (mw->priv->toolbar), w, FALSE, TRUE, 2);
    }

    mw->priv->toolbar_sep = create_separator (FALSE);
    gtk_widget_set_sensitive (mw->priv->tb_cap_paste_btn, FALSE);
}


static void slide_set_100_0 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.list_orientation ? GTK_WIDGET (main_win)->allocation.height :
                                                              GTK_WIDGET (main_win)->allocation.width);
}


static void slide_set_80_20 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.list_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.8f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.8f));
}


static void slide_set_60_40 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.list_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.6f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.6f));
}


static void slide_set_50_50 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.list_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.5f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.5f));
}


static void slide_set_40_60 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.list_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.4f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.4f));
}


static void slide_set_20_80 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned),
                            gnome_cmd_data.list_orientation ? (int)(GTK_WIDGET (main_win)->allocation.height*0.2f) :
                                                              (int)(GTK_WIDGET (main_win)->allocation.width*0.2f));
}


static void slide_set_0_100 (GtkMenu *menu, gpointer user_data)
{
    gtk_paned_set_position (GTK_PANED (main_win->priv->paned), 0);
}


static GtkWidget *create_slide_popup ()
{
    static GnomeUIInfo popmenu_uiinfo[] =
    {
        GNOMEUIINFO_ITEM_NONE("100 - 0", NULL, slide_set_100_0),
        GNOMEUIINFO_ITEM_NONE("80 - 20", NULL, slide_set_80_20),
        GNOMEUIINFO_ITEM_NONE("60 - 40", NULL, slide_set_60_40),
        GNOMEUIINFO_ITEM_NONE("50 - 50", NULL, slide_set_50_50),
        GNOMEUIINFO_ITEM_NONE("40 - 60", NULL, slide_set_40_60),
        GNOMEUIINFO_ITEM_NONE("20 - 80", NULL, slide_set_20_80),
        GNOMEUIINFO_ITEM_NONE("0 - 100", NULL, slide_set_0_100),
        GNOMEUIINFO_END
    };

    // Set default callback data

    for (guint i = 0; popmenu_uiinfo[i].type != GNOME_APP_UI_ENDOFINFO; ++i)
        if (popmenu_uiinfo[i].type == GNOME_APP_UI_ITEM)
            popmenu_uiinfo[i].user_data = main_win;

    GtkWidget *menu = gtk_menu_new ();
    gtk_widget_ref (menu);
    gtk_object_set_data_full (GTK_OBJECT (main_win), "slide-popup", menu, (GtkDestroyNotify) gtk_widget_unref);

    // Fill the menu

    gnome_app_fill_menu (GTK_MENU_SHELL (menu), popmenu_uiinfo, NULL, FALSE, 0);

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


static void on_quit_clicked (GtkButton *button, GnomeCmdMainWin *mw)
{
    file_exit (NULL);
}


static void create_buttonbar (GnomeCmdMainWin *mw)
{
    mw->priv->buttonbar_sep = create_separator (FALSE);

    mw->priv->buttonbar = gtk_hbox_new (FALSE, 0);
    gtk_widget_ref (mw->priv->buttonbar);
    gtk_object_set_data_full (GTK_OBJECT (main_win), "buttonbar", mw->priv->buttonbar,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (mw->priv->buttonbar);

    mw->priv->view_btn = add_buttonbar_button(_("F3 View"), main_win, "view_btn", mw->priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    mw->priv->edit_btn = add_buttonbar_button(_("F4 Edit"), main_win, "edit_btn", mw->priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    mw->priv->copy_btn = add_buttonbar_button(_("F5 Copy"), main_win, "copy_btn", mw->priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    mw->priv->move_btn = add_buttonbar_button(_("F6 Move"), main_win, "move_btn", mw->priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    mw->priv->mkdir_btn = add_buttonbar_button(_("F7 Mkdir"), main_win, "mkdir_btn", mw->priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    mw->priv->delete_btn = add_buttonbar_button(_("F8 Delete"), main_win, "delete_btn", mw->priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    mw->priv->find_btn = add_buttonbar_button(_("F9 Search"), main_win, "find_btn", mw->priv->accel_group, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->buttonbar), create_separator (TRUE), FALSE, TRUE, 0);
    mw->priv->quit_btn = add_buttonbar_button(_("F10 Quit"), main_win, "quit_btn", mw->priv->accel_group, 0);

    gtk_signal_connect (GTK_OBJECT (mw->priv->view_btn), "clicked", GTK_SIGNAL_FUNC (on_view_clicked), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->edit_btn), "clicked", GTK_SIGNAL_FUNC (on_edit_clicked), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->copy_btn), "clicked", GTK_SIGNAL_FUNC (on_copy_clicked), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->move_btn), "clicked", GTK_SIGNAL_FUNC (on_move_clicked), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->mkdir_btn), "clicked", GTK_SIGNAL_FUNC (on_mkdir_clicked), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->delete_btn), "clicked", GTK_SIGNAL_FUNC (on_delete_clicked), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->find_btn), "clicked", GTK_SIGNAL_FUNC (on_search_clicked), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->quit_btn), "clicked", GTK_SIGNAL_FUNC (on_quit_clicked), mw);
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
            gtk_menu_popup (GTK_MENU(create_slide_popup ()), NULL, NULL, NULL, NULL, event->button, event->time);
            return TRUE;
        }
    }

    return FALSE;
}

static void on_main_win_realize (GtkWidget *widget, GnomeCmdMainWin *mw)
{
    gnome_cmd_main_win_set_equal_panes (mw);

    gnome_cmd_main_win_get_fs (mw, LEFT)->set_active(TRUE);
    gnome_cmd_main_win_get_fs (mw, RIGHT)->set_active(FALSE);

    // if (gnome_cmd_data.cmdline_visibility)
    // {
        // gchar *dpath = gnome_cmd_file_get_path (GNOME_CMD_FILE (gnome_cmd_main_win_get_fs (mw, LEFT)->get_directory()));
        // gnome_cmd_cmdline_set_dir (GNOME_CMD_CMDLINE (mw->priv->cmdline), dpath);
        // g_free (dpath);
    // }

    gdk_window_set_icon (GTK_WIDGET (main_win)->window, NULL,
                         IMAGE_get_pixmap (PIXMAP_LOGO),
                         IMAGE_get_mask (PIXMAP_LOGO));
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
}


inline void update_browse_buttons (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs == gnome_cmd_main_win_get_fs (mw, ACTIVE))
    {
        if (gnome_cmd_data.toolbar_visibility)
        {
            gtk_widget_set_sensitive (mw->priv->tb_first_btn, fs->can_back());
            gtk_widget_set_sensitive (mw->priv->tb_back_btn, fs->can_back());
            gtk_widget_set_sensitive (mw->priv->tb_fwd_btn, fs->can_forward());
            gtk_widget_set_sensitive (mw->priv->tb_last_btn, fs->can_forward());
        }

        gnome_cmd_main_menu_update_sens (GNOME_CMD_MAIN_MENU (mw->priv->menubar));
    }
}


static void update_drop_con_button (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdPixmap *pm = NULL;
    static GtkWidget *prev_pixmap = NULL;

    GnomeCmdCon *con = fs->get_connection();
    if (!con)
        return;

    if (!gnome_cmd_data.toolbar_visibility
        || (gnome_cmd_data.skip_mounting && GNOME_CMD_IS_CON_DEVICE (con)))
        return;

    GtkWidget *btn = mw->priv->tb_con_drop_btn;
    g_return_if_fail (GTK_IS_BUTTON (btn));

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

    g_return_if_fail (toolbar_tooltips != NULL);
    gtk_tooltips_set_tip (toolbar_tooltips, btn, gnome_cmd_con_get_close_tooltip (con), NULL);
    gtk_widget_set_sensitive (btn, TRUE);

    if (pm)
    {
        GtkWidget *pixmap = gtk_pixmap_new (pm->pixmap, pm->mask);
        if (pixmap)
        {
            gtk_widget_ref (pixmap);
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
            gtk_widget_ref (label);
            gtk_widget_show (label);
            gtk_container_add (GTK_CONTAINER (btn), label);
            prev_pixmap = label;
        }
    }
}


static void on_fs_dir_change (GnomeCmdFileSelector *fs, const gchar dir, GnomeCmdMainWin *mw)
{
    update_browse_buttons (mw, fs);
    update_drop_con_button (mw, fs);
}


static void restore_size_and_pos (GnomeCmdMainWin *mw)
{
    gint x, y;

    gtk_widget_set_size_request (GTK_WIDGET (main_win),
                                 gnome_cmd_data.main_win_width,
                                 gnome_cmd_data.main_win_height);

    gnome_cmd_data_get_main_win_pos (&x, &y);
    if (x >= 0 && y >= 0)
        gtk_window_move (GTK_WINDOW (mw), x, y);

    switch (gnome_cmd_data.main_win_state)
    {
        case GDK_WINDOW_STATE_MAXIMIZED:
        case GDK_WINDOW_STATE_FULLSCREEN:
            gtk_window_maximize (GTK_WINDOW (mw));
            break;

        default:
            break;
    }
}


static void on_delete_event (GnomeCmdMainWin *mw, GdkEvent *event, gpointer user_data)
{
    file_exit (NULL, mw);
}


static gboolean on_window_state_event (GtkWidget *mw, GdkEventWindowState *event, gpointer user_data)
{
    gint x, y;

    switch (event->new_window_state)
    {
        case GDK_WINDOW_STATE_MAXIMIZED:    // not usable
        case GDK_WINDOW_STATE_FULLSCREEN:   // not usable
                break;

        case GDK_WINDOW_STATE_ICONIFIED:
            if (gnome_cmd_data.main_win_state == GDK_WINDOW_STATE_MAXIMIZED ||  // prev state
                gnome_cmd_data.main_win_state == GDK_WINDOW_STATE_FULLSCREEN)
                break;  // not usable

        default:            // other are usable
            gdk_window_get_root_origin (mw->window, &x, &y);
            gnome_cmd_data_set_main_win_pos (x, y);
    }

    gnome_cmd_data.main_win_state = event->new_window_state;

    return FALSE;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdCon *con_home = get_home_con ();

    if (main_win && main_win->priv && main_win->priv->key_snooper_id)
    {
        gtk_key_snooper_remove(main_win->priv->key_snooper_id);
        main_win->priv->key_snooper_id = 0;
    }

    GnomeCmdDir *dir = gnome_cmd_main_win_get_fs (main_win, LEFT)->get_directory();
    if (con_home == gnome_cmd_dir_get_connection (dir))
        gnome_cmd_data_set_start_dir (LEFT, gnome_cmd_file_get_path (GNOME_CMD_FILE (dir)));

    dir = gnome_cmd_main_win_get_fs (main_win, RIGHT)->get_directory();
    if (con_home == gnome_cmd_dir_get_connection (dir))
        gnome_cmd_data_set_start_dir (RIGHT, gnome_cmd_file_get_path (GNOME_CMD_FILE (dir)));

    if (main_win->advrename_dlg)
        gtk_widget_destroy (*main_win->advrename_dlg);

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

    parent_class = (GnomeAppClass *) gtk_type_class (gnome_app_get_type ());

    main_win_signals[SWITCH_FS] =
        gtk_signal_new ("switch-fs",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdMainWinClass, switch_fs),
            gtk_marshal_NONE__POINTER,
            GTK_TYPE_NONE,
            1, GTK_TYPE_POINTER);

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
    mw->priv = g_new0 (GnomeCmdMainWinPrivate, 1);
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

    gnome_app_construct (GNOME_APP (main_win), "gnome-commander", gcmd_owner.is_root() ? _("GNOME Commander - ROOT PRIVILEGES") :
                                                                                         _("GNOME Commander"));
    gtk_object_set_data (GTK_OBJECT (main_win), "main_win", main_win);
    restore_size_and_pos (mw);
    gtk_window_set_policy (GTK_WINDOW (main_win), TRUE, TRUE, FALSE);

    mw->priv->vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_ref (mw->priv->vbox);
    gtk_object_set_data_full (GTK_OBJECT (main_win), "vbox", mw->priv->vbox,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (mw->priv->vbox);


    mw->priv->menubar = gnome_cmd_main_menu_new ();
    gtk_widget_ref (mw->priv->menubar);
    gtk_object_set_data_full (GTK_OBJECT (main_win), "vbox", mw->priv->menubar,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (mw->priv->menubar);
    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->menubar, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), create_separator (FALSE), FALSE, TRUE, 0);

    gnome_app_set_contents (GNOME_APP (main_win), mw->priv->vbox);

    mw->priv->paned = gnome_cmd_data.list_orientation ? gtk_vpaned_new () : gtk_hpaned_new ();

    gtk_widget_ref (mw->priv->paned);
    gtk_object_set_data_full (GTK_OBJECT (main_win), "paned", mw->priv->paned,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (mw->priv->paned);
    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->paned, TRUE, TRUE, 0);
    create_buttonbar (mw);

    mw->priv->file_selector[LEFT] = gnome_cmd_file_selector_new ();
    gtk_widget_ref (mw->priv->file_selector[LEFT]);
    gtk_object_set_data_full (GTK_OBJECT (main_win), "left_file_selector",
                              mw->priv->file_selector[LEFT],
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (mw->priv->file_selector[LEFT]);
    gtk_paned_pack1 (GTK_PANED (mw->priv->paned), mw->priv->file_selector[LEFT], TRUE, TRUE);

    mw->priv->file_selector[RIGHT] = gnome_cmd_file_selector_new ();
    gtk_widget_ref (mw->priv->file_selector[RIGHT]);
    gtk_object_set_data_full (GTK_OBJECT (main_win), "left_file_selector",
                              mw->priv->file_selector[RIGHT],
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (mw->priv->file_selector[RIGHT]);
    gtk_paned_pack2 (GTK_PANED (mw->priv->paned), mw->priv->file_selector[RIGHT], TRUE, TRUE);

    gnome_cmd_main_win_update_toolbar_visibility (main_win);
    gnome_cmd_main_win_update_cmdline_visibility (main_win);
    gnome_cmd_main_win_update_buttonbar_visibility (main_win);

    gtk_signal_connect (GTK_OBJECT (main_win), "realize", GTK_SIGNAL_FUNC (on_main_win_realize), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->file_selector[LEFT]), "changed-dir", GTK_SIGNAL_FUNC (on_fs_dir_change), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->file_selector[RIGHT]), "changed-dir", GTK_SIGNAL_FUNC (on_fs_dir_change), mw);

    gtk_signal_connect (GTK_OBJECT (GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[LEFT])->file_list()),
                        "resize-column", GTK_SIGNAL_FUNC (on_fs_list_resize_column),
                        GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[RIGHT])->file_list());
    gtk_signal_connect (GTK_OBJECT (GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[RIGHT])->file_list()),
                        "resize-column", GTK_SIGNAL_FUNC (on_fs_list_resize_column),
                        GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[LEFT])->file_list());
    gtk_signal_connect (GTK_OBJECT (GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[LEFT])->file_list()),
                        "button-press-event", GTK_SIGNAL_FUNC (on_left_fs_select), mw);
    gtk_signal_connect (GTK_OBJECT (GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[RIGHT])->file_list()),
                        "button-press-event", GTK_SIGNAL_FUNC (on_right_fs_select), mw);
    gtk_signal_connect (GTK_OBJECT (mw), "size-allocate", GTK_SIGNAL_FUNC (on_size_allocate), mw);
    gtk_signal_connect (GTK_OBJECT (mw), "delete-event", GTK_SIGNAL_FUNC (on_delete_event), mw);
    gtk_signal_connect (GTK_OBJECT (mw->priv->paned), "button-press-event", GTK_SIGNAL_FUNC (on_slide_button_press), mw);
    g_signal_connect (mw, "window-state-event", GTK_SIGNAL_FUNC (on_window_state_event), NULL);

    gnome_cmd_main_win_get_fs (mw, LEFT)->update_connections();
    gnome_cmd_main_win_get_fs (mw, RIGHT)->update_connections();

    gnome_cmd_main_win_get_fs (mw, LEFT)->set_connection(get_home_con ());
    gnome_cmd_main_win_get_fs (mw, RIGHT)->set_connection(get_home_con ());

    gnome_cmd_main_win_get_fs (mw, LEFT)->goto_directory(start_dir_left ? start_dir_left : gnome_cmd_data_get_start_dir (LEFT));
    gnome_cmd_main_win_get_fs (mw, RIGHT)->goto_directory(start_dir_right ? start_dir_right : gnome_cmd_data_get_start_dir (RIGHT));

    gtk_window_add_accel_group (GTK_WINDOW (main_win), mw->priv->accel_group);
    gnome_cmd_main_win_focus_file_lists (main_win);

    mw->priv->key_snooper_id = gtk_key_snooper_install ((GtkKeySnoopFunc) gnome_cmd_key_snooper, (gpointer) mw);
}


GtkType gnome_cmd_main_win_get_type ()
{
    static GtkType mw_type = 0;

    if (mw_type == 0)
    {
        GtkTypeInfo mw_info =
        {
            "GnomeCmdMainWin",
            sizeof (GnomeCmdMainWin),
            sizeof (GnomeCmdMainWinClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        mw_type = gtk_type_unique (gnome_app_get_type (), &mw_info);
    }

    return mw_type;
}


GtkWidget *gnome_cmd_main_win_new ()
{
    GnomeCmdMainWin *mw = (GnomeCmdMainWin *) gtk_type_new (gnome_cmd_main_win_get_type ());

    return GTK_WIDGET (mw);
}


GnomeCmdFileSelector *gnome_cmd_main_win_get_fs (GnomeCmdMainWin *mw, FileSelectorID fs)
{
    g_return_val_if_fail (GNOME_CMD_IS_MAIN_WIN (mw), NULL);

    switch (fs)
    {
        case LEFT:
        case RIGHT:
            return mw->priv->file_selector[fs] ?
                   GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[fs]) : NULL;

        case ACTIVE:
            return mw->priv->file_selector[mw->priv->current_fs] ?
                   GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[mw->priv->current_fs]) : NULL;

        case INACTIVE:
            return mw->priv->file_selector[!mw->priv->current_fs] ?
                   GNOME_CMD_FILE_SELECTOR (mw->priv->file_selector[!mw->priv->current_fs]): NULL;

       default:
            return NULL;
    }
}


void gnome_cmd_main_win_update_style (GnomeCmdMainWin *mw)
{
    g_return_if_fail (mw != NULL);
    g_return_if_fail (mw->priv != NULL);

    IMAGE_clear_mime_cache ();

    gnome_cmd_main_win_get_fs (mw, LEFT)->update_style();
    gnome_cmd_main_win_get_fs (mw, RIGHT)->update_style();

    if (gnome_cmd_data.cmdline_visibility)
        gnome_cmd_cmdline_update_style (GNOME_CMD_CMDLINE (mw->priv->cmdline));
}


void gnome_cmd_main_win_focus_cmdline (GnomeCmdMainWin *mw)
{
    if (gnome_cmd_data.cmdline_visibility)
    {
        gnome_cmd_cmdline_focus (GNOME_CMD_CMDLINE (mw->priv->cmdline));
        mw->priv->focused_widget = mw->priv->cmdline;
    }
}


void gnome_cmd_main_win_focus_file_lists (GnomeCmdMainWin *mw)
{
    gnome_cmd_main_win_get_fs (mw, ACTIVE)->set_active(TRUE);
    gnome_cmd_main_win_get_fs (mw, INACTIVE)->set_active(FALSE);

    mw->priv->focused_widget = mw->priv->file_selector[mw->priv->current_fs];
}


void gnome_cmd_main_win_refocus (GnomeCmdMainWin *mw)
{
    if (mw->priv->focused_widget)
        gtk_widget_grab_focus (mw->priv->focused_widget);
}


gboolean gnome_cmd_main_win_keypressed (GnomeCmdMainWin *mw, GdkEventKey *event)
{
    if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_1:
                {
                    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_fs (mw, LEFT);
                    gnome_cmd_main_win_switch_fs (mw, fs);
                    if (gnome_cmd_data.concombo_visibility)
                        gnome_cmd_combo_popup_list (GNOME_CMD_COMBO (fs->con_combo));
                }
                return TRUE;

            case GDK_2:
                {
                    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_fs (mw, RIGHT);
                    gnome_cmd_main_win_switch_fs (mw, fs);
                    if (gnome_cmd_data.concombo_visibility)
                        gnome_cmd_combo_popup_list (GNOME_CMD_COMBO (fs->con_combo));
                }
                return TRUE;

            case GDK_F8:
                if (gnome_cmd_data.cmdline_visibility)
                    gnome_cmd_cmdline_show_history (GNOME_CMD_CMDLINE (mw->priv->cmdline));
                return TRUE;
        }
    }
    else if (state_is_ctrl_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_H:
            case GDK_h:
                gnome_cmd_data.filter_settings.hidden = !gnome_cmd_data.filter_settings.hidden;
                gnome_cmd_style_create ();
                gnome_cmd_main_win_update_style (main_win);
                gnome_cmd_data.save();
                return TRUE;
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
                    gnome_cmd_cmdline_show_history (GNOME_CMD_CMDLINE (mw->priv->cmdline));
                return TRUE;

            case GDK_u:
            case GDK_U:
                {
                    GnomeCmdFileSelector *fs1 = gnome_cmd_main_win_get_fs (mw, LEFT);
                    GnomeCmdFileSelector *fs2 = gnome_cmd_main_win_get_fs (mw, RIGHT);

                    // swap widgets
                    g_object_ref (fs1);
                    g_object_ref (fs2);
                    gtk_container_remove (GTK_CONTAINER (mw->priv->paned), *fs1);
                    gtk_container_remove (GTK_CONTAINER (mw->priv->paned), *fs2);
                    gtk_paned_pack1 (GTK_PANED (mw->priv->paned), *fs2, TRUE, TRUE);
                    gtk_paned_pack2 (GTK_PANED (mw->priv->paned), *fs1, TRUE, TRUE);
                    g_object_unref (fs1);
                    g_object_unref (fs2);

                    // update priv->file_selector[]
                    GtkWidget *swap = mw->priv->file_selector[LEFT];
                    mw->priv->file_selector[LEFT] = mw->priv->file_selector[RIGHT];
                    mw->priv->file_selector[RIGHT] = swap;

                    // refocus ACTIVE fs
                    gnome_cmd_main_win_focus_file_lists (mw);

                    // update cmdline only for different directories
                    if (fs1->get_directory()!=fs2->get_directory())
                        gnome_cmd_main_win_switch_fs (mw, gnome_cmd_main_win_get_fs (mw, ACTIVE));

                    clear_event_key (event);
                }
                return TRUE;
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
                GnomeCmdConFtp *con = GNOME_CMD_CON_FTP (gnome_cmd_con_list_get_all_ftp (gnome_cmd_con_list_get ())->data);

                gnome_cmd_main_win_get_fs (main_win, ACTIVE)->set_connection(GNOME_CMD_CON (con));
            }
            break;
        }
    }
    else
        if (state_is_blank (event->state))
            switch (event->keyval)
            {
                case GDK_Tab:
                    // hack to avoid the default handling of the tab-key
                    clear_event_key (event);
                    gnome_cmd_main_win_switch_fs (mw, gnome_cmd_main_win_get_fs (mw, INACTIVE));
                    return TRUE;

                case GDK_F1:
                    on_help_clicked (NULL, mw);
                    return TRUE;

                case GDK_F2:
                    on_rename_clicked (NULL, mw);
                    return TRUE;

                case GDK_F3:
                    on_view_clicked (NULL, mw);
                    return TRUE;

                case GDK_F4:
                    on_edit_clicked (NULL, mw);
                    return TRUE;

                case GDK_F5:
                    on_copy_clicked (NULL, mw);
                    return TRUE;

                case GDK_F6:
                    on_move_clicked (NULL, mw);
                    return TRUE;

                case GDK_F7:
                    on_mkdir_clicked (NULL, mw);
                    return TRUE;

                case GDK_F8:
                    on_delete_clicked (NULL, mw);
                    return TRUE;

                case GDK_F9:
                    on_search_clicked (NULL, mw);
                    return TRUE;

                case GDK_F10:
                    on_quit_clicked (NULL, mw);
                    return TRUE;
            }

    if (gnome_cmd_main_win_get_fs (mw, ACTIVE)->key_pressed(event))
        return TRUE;

    return FALSE;
}


void gnome_cmd_main_win_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    gtk_signal_emit (GTK_OBJECT (mw), main_win_signals[SWITCH_FS], fs);
}


static void gnome_cmd_main_win_real_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs == gnome_cmd_main_win_get_fs (mw, ACTIVE))
        return;

    mw->priv->current_fs = (FileSelectorID) !mw->priv->current_fs;
    gnome_cmd_main_win_get_fs (mw, ACTIVE)->set_active(TRUE);
    gnome_cmd_main_win_get_fs (mw, INACTIVE)->set_active(FALSE);

    update_browse_buttons (mw, fs);
    update_drop_con_button (mw, fs);
}


GnomeCmdCmdline *gnome_cmd_main_win_get_cmdline (GnomeCmdMainWin *mw)
{
    g_return_val_if_fail (GNOME_CMD_IS_MAIN_WIN (mw), NULL);

    return GNOME_CMD_CMDLINE (mw->priv->cmdline);
}


void gnome_cmd_main_win_update_bookmarks (GnomeCmdMainWin *mw)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));

    gnome_cmd_main_menu_update_bookmarks (GNOME_CMD_MAIN_MENU (mw->priv->menubar));
}


void gnome_cmd_main_win_update_toolbar_visibility (GnomeCmdMainWin *mw)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));

    static GnomeUIInfo toolbar_uiinfo[] =
    {
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Refresh"), view_refresh, GTK_STOCK_REFRESH),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Up one directory"), view_up, GTK_STOCK_GO_UP),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Go to the oldest"), view_first, GTK_STOCK_GOTO_FIRST),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Go back"), view_back, GTK_STOCK_GO_BACK),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Go forward"), view_forward, GTK_STOCK_GO_FORWARD),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Go to the latest"), view_last, GTK_STOCK_GOTO_LAST),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_ITEM(NULL, _("Copy file names (SHIFT for full paths, ALT for URIs)"), edit_copy_fnames, copy_file_names_xpm),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Cut"), edit_cap_cut, GTK_STOCK_CUT),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Copy"), edit_cap_copy, GTK_STOCK_COPY),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Paste"), edit_cap_paste, GTK_STOCK_PASTE),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Delete"), file_delete, GTK_STOCK_DELETE),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Edit (SHIFT for new document)"), file_edit, GTK_STOCK_EDIT),
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Send files"), file_sendto, GNOME_STOCK_MAIL_SND),
        GNOMEUIINFO_ITEM_FILENAME(NULL, _("Open terminal"), command_open_terminal, PACKAGE_NAME G_DIR_SEPARATOR_S "terminal.svg"),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Remote Server"), connections_open, GTK_STOCK_CONNECT),
        GNOMEUIINFO_ITEM_NONE(NULL, _("Drop connection"), connections_close_current),
        GNOMEUIINFO_END
    };

    if (gnome_cmd_data.toolbar_visibility)
    {
        create_toolbar (mw, toolbar_uiinfo);
        gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->toolbar, FALSE, TRUE, 0);
        gtk_box_reorder_child (GTK_BOX (mw->priv->vbox), mw->priv->toolbar, 2);
        gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->toolbar_sep, FALSE, TRUE, 0);
        gtk_box_reorder_child (GTK_BOX (mw->priv->vbox), mw->priv->toolbar_sep, 3);
    }
    else
    {
        if (mw->priv->toolbar)
            gtk_widget_destroy (mw->priv->toolbar);
        if (mw->priv->toolbar_sep)
            gtk_widget_destroy (mw->priv->toolbar_sep);
        mw->priv->toolbar = NULL;
        mw->priv->toolbar_sep = NULL;
    }

    update_drop_con_button (mw, gnome_cmd_main_win_get_fs (mw, ACTIVE));
}


void gnome_cmd_main_win_update_buttonbar_visibility (GnomeCmdMainWin *mw)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));

    if (gnome_cmd_data.buttonbar_visibility)
    {
        create_buttonbar (mw);
        gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->buttonbar_sep, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->buttonbar, FALSE, TRUE, 0);
    }
    else
    {
        if (mw->priv->buttonbar)
            gtk_widget_destroy (mw->priv->buttonbar);
        if (mw->priv->buttonbar_sep)
            gtk_widget_destroy (mw->priv->buttonbar_sep);
        mw->priv->buttonbar = NULL;
        mw->priv->buttonbar_sep = NULL;
    }
}


void gnome_cmd_main_win_update_cmdline_visibility (GnomeCmdMainWin *mw)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));

    if (gnome_cmd_data.cmdline_visibility)
    {
        gint pos = 3;
        mw->priv->cmdline_sep = create_separator (FALSE);
        mw->priv->cmdline = gnome_cmd_cmdline_new ();
        gtk_widget_ref (mw->priv->cmdline);
        gtk_object_set_data_full (GTK_OBJECT (main_win), "cmdline", mw->priv->cmdline,
                                  (GtkDestroyNotify) gtk_widget_unref);
        gtk_widget_show (mw->priv->cmdline);
        if (gnome_cmd_data.toolbar_visibility)
            pos += 2;
        gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->cmdline_sep, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->cmdline, FALSE, TRUE, 1);
        gtk_box_reorder_child (GTK_BOX (mw->priv->vbox), mw->priv->cmdline_sep, pos);
        gtk_box_reorder_child (GTK_BOX (mw->priv->vbox), mw->priv->cmdline, pos+1);
    }
    else
    {
        if (mw->priv->cmdline)
            gtk_widget_destroy (mw->priv->cmdline);
        if (mw->priv->cmdline_sep)
            gtk_widget_destroy (mw->priv->cmdline_sep);
        mw->priv->cmdline = NULL;
        mw->priv->cmdline_sep = NULL;
    }
}


void gnome_cmd_main_win_update_list_orientation (GnomeCmdMainWin *mw)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));

    gint pos = 2;

    gtk_widget_ref (mw->priv->file_selector[LEFT]);
    gtk_widget_ref (mw->priv->file_selector[RIGHT]);
    gtk_container_remove (GTK_CONTAINER (mw->priv->paned), mw->priv->file_selector[LEFT]);
    gtk_container_remove (GTK_CONTAINER (mw->priv->paned), mw->priv->file_selector[RIGHT]);

    gtk_object_destroy (GTK_OBJECT (mw->priv->paned));

    mw->priv->paned = gnome_cmd_data.list_orientation ? gtk_vpaned_new () : gtk_hpaned_new ();

    gtk_widget_ref (mw->priv->paned);
    gtk_object_set_data_full (GTK_OBJECT (main_win), "paned", mw->priv->paned, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (mw->priv->paned);

    gtk_paned_pack1 (GTK_PANED (mw->priv->paned), mw->priv->file_selector[LEFT], TRUE, TRUE);
    gtk_paned_pack2 (GTK_PANED (mw->priv->paned), mw->priv->file_selector[RIGHT], TRUE, TRUE);

    if (gnome_cmd_data.toolbar_visibility)
        pos += 2;

    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->paned, TRUE, TRUE, 0);
    gtk_box_reorder_child (GTK_BOX (mw->priv->vbox), mw->priv->paned, pos);

    gtk_widget_unref (mw->priv->file_selector[LEFT]);
    gtk_widget_unref (mw->priv->file_selector[RIGHT]);

    gtk_signal_connect (GTK_OBJECT (mw->priv->paned), "button-press-event", GTK_SIGNAL_FUNC (on_slide_button_press), mw);
    slide_set_50_50 (NULL, NULL);
}


void gnome_cmd_main_win_add_plugin_menu (GnomeCmdMainWin *mw, PluginData *data)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));

    gnome_cmd_main_menu_add_plugin_menu (GNOME_CMD_MAIN_MENU (mw->priv->menubar), data);
}


GnomeCmdState *gnome_cmd_main_win_get_state (GnomeCmdMainWin *mw)
{
    g_return_val_if_fail (GNOME_CMD_IS_MAIN_WIN (mw), NULL);

    GnomeCmdFileSelector *fs1 = gnome_cmd_main_win_get_fs (main_win, ACTIVE);
    GnomeCmdFileSelector *fs2 = gnome_cmd_main_win_get_fs (main_win, INACTIVE);
    GnomeCmdDir *dir1 = fs1->get_directory();
    GnomeCmdDir *dir2 = fs2->get_directory();

    GnomeCmdState *state = &mw->priv->state;
    state->active_dir_uri = gnome_cmd_file_get_uri (GNOME_CMD_FILE (dir1));
    state->inactive_dir_uri = gnome_cmd_file_get_uri (GNOME_CMD_FILE (dir2));
    state->active_dir_files = fs1->file_list()->get_visible_files();
    state->inactive_dir_files = fs2->file_list()->get_visible_files();
    state->active_dir_selected_files = fs1->file_list()->get_selected_files();
    state->inactive_dir_selected_files = fs2->file_list()->get_selected_files();

    return state;
}


void gnome_cmd_main_win_set_cap_state (GnomeCmdMainWin *mw, gboolean state)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));

    gtk_widget_set_sensitive (mw->priv->tb_cap_paste_btn, state);
}


void gnome_cmd_main_win_set_equal_panes (GnomeCmdMainWin *mw)
{
    slide_set_50_50 (NULL, NULL);
}
