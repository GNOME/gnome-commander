/** 
 * @file gnome-cmd-main-win.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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


struct GnomeCmdMainWinClass
{
    GnomeAppClass parent_class;

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

static guint signals[LAST_SIGNAL] = { 0 };
static GtkTooltips *toolbar_tooltips = NULL;

static void gnome_cmd_main_win_real_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs);


gint gnome_cmd_key_snooper(GtkWidget *grab_widget, GdkEventKey *event, GnomeCmdMainWin *mw)
{
    g_return_val_if_fail (mw!=NULL, FALSE);

    if (event->type!=GDK_KEY_PRESS)
        return FALSE;

    if (!(event->keyval >= GDK_A && event->keyval <= GDK_Z || event->keyval >= GDK_a && event->keyval <= GDK_z ||
          event->keyval >= GDK_0 && event->keyval <= GDK_9 ||
          event->keyval == GDK_period || event->keyval == GDK_question|| event->keyval == GDK_asterisk || event->keyval == GDK_bracketleft))
        return FALSE;

    if (!gnome_cmd_data.options.alt_quick_search)
        return FALSE;

    if (!state_is_alt (event->state) && !state_is_alt_shift (event->state))
        return FALSE;

    GnomeCmdFileSelector *fs = mw->fs(ACTIVE);
    if (!fs || !fs->file_list())
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
                                        const char *data_label,
                                        GtkAccelGroup *accel_group,
                                        gint accel_signal_id)
{
    GtkWidget *button = create_styled_button (label);
    g_object_set_data_full (*mw, data_label, button, g_object_unref);
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

    g_object_ref (sep);
    gtk_widget_show (sep);

    g_object_ref (box);
    gtk_widget_show (box);

    gtk_box_pack_start (GTK_BOX (box), sep, TRUE, TRUE, 3);

    return box;
}


static void create_toolbar (GnomeCmdMainWin *mw, GnomeUIInfo *uiinfo)
{
    gint i;

    mw->priv->toolbar = gtk_hbox_new (FALSE, 0);
    g_object_ref (mw->priv->toolbar);
    g_object_set_data_full (*mw, "toolbar", mw->priv->toolbar, g_object_unref);
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
            g_signal_connect (w, "clicked", G_CALLBACK (uiinfo[i].moreinfo), uiinfo[i].user_data);
            gtk_tooltips_set_tip (toolbar_tooltips, w, uiinfo[i].hint, NULL);
            GTK_WIDGET_UNSET_FLAGS (w, GTK_CAN_FOCUS);

            pixmap = create_ui_pixmap (
                GTK_WIDGET (mw),
                uiinfo[i].pixmap_type, uiinfo[i].pixmap_info,
                GTK_ICON_SIZE_LARGE_TOOLBAR);
            if (pixmap)
            {
                g_object_ref (pixmap);
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
    g_object_ref (menu);
    g_object_set_data_full (*main_win, "slide-popup", menu, g_object_unref);

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

    if (fs == mw->fs(ACTIVE))
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


void GnomeCmdMainWin::update_drop_con_button(GnomeCmdFileList *fl)
{
    if (!fl)
        return;

    GnomeCmdPixmap *pm = NULL;
    static GtkWidget *prev_pixmap = NULL;

    GnomeCmdCon *con = fl->con;
    if (!con)
        return;

    if (!gnome_cmd_data.toolbar_visibility
        || (gnome_cmd_data.options.skip_mounting && GNOME_CMD_IS_CON_DEVICE (con)))
        return;

    GtkWidget *btn = priv->tb_con_drop_btn;
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
    if (main_win && main_win->priv && main_win->priv->key_snooper_id)
    {
        gtk_key_snooper_remove (main_win->priv->key_snooper_id);
        main_win->priv->key_snooper_id = 0;
    }

    if (main_win->advrename_dlg)
        gtk_widget_destroy (*main_win->advrename_dlg);

    if (main_win->file_search_dlg)
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

    parent_class = (GnomeAppClass *) gtk_type_class (gnome_app_get_type ());

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

    gnome_app_construct (GNOME_APP (mw), "gnome-commander", gcmd_owner.is_root() ? _("GNOME Commander - ROOT PRIVILEGES") :
                                                                                   _("GNOME Commander"));
    g_object_set_data (*mw, "main_win", mw);
    restore_size_and_pos (mw);
    gtk_window_set_policy (*mw, TRUE, TRUE, FALSE);

    mw->priv->vbox = gtk_vbox_new (FALSE, 0);
    g_object_ref (mw->priv->vbox);
    g_object_set_data_full (*mw, "vbox", mw->priv->vbox, g_object_unref);
    gtk_widget_show (mw->priv->vbox);

    mw->priv->menubar = gnome_cmd_main_menu_new ();
    g_object_ref (mw->priv->menubar);
    g_object_set_data_full (*mw, "vbox", mw->priv->menubar, g_object_unref);
    if(gnome_cmd_data.mainmenu_visibility)
		gtk_widget_show (mw->priv->menubar);
    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), mw->priv->menubar, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (mw->priv->vbox), create_separator (FALSE), FALSE, TRUE, 0);

    gnome_app_set_contents (GNOME_APP (mw), mw->priv->vbox);

    mw->priv->paned = gnome_cmd_data.list_orientation ? gtk_vpaned_new () : gtk_hpaned_new ();

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

    mw->update_toolbar_visibility();
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


FileSelectorID GnomeCmdMainWin::fs() const
{
    return priv->current_fs;
}


FileSelectorID GnomeCmdMainWin::fs(GnomeCmdFileSelector *fs) const
{
    if (!priv->file_selector[LEFT] || priv->file_selector[LEFT]==*fs)
        return LEFT;

    if (!priv->file_selector[RIGHT] || priv->file_selector[RIGHT]==*fs)
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
    if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_F8:
                if (gnome_cmd_data.cmdline_visibility)
                    gnome_cmd_cmdline_show_history (GNOME_CMD_CMDLINE (priv->cmdline));
                return TRUE;
        }
    }
    else if (state_is_ctrl_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_H:
            case GDK_h:
                gnome_cmd_data.options.filter.hidden = !gnome_cmd_data.options.filter.hidden;
                gnome_cmd_style_create (gnome_cmd_data.options);
                update_style();
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
            }

    return fs(ACTIVE)->key_pressed(event);
}


inline void GnomeCmdMainWin::open_tabs(FileSelectorID id)
{
    GnomeCmdCon *home = get_home_con ();

    if (gnome_cmd_data.tabs[id].empty())
        gnome_cmd_data.tabs[id].push_back(make_pair(string(g_get_home_dir ()), make_triple(GnomeCmdFileList::COLUMN_NAME, GTK_SORT_ASCENDING, FALSE)));

    vector<GnomeCmdData::Tab>::const_iterator last_tab = unique(gnome_cmd_data.tabs[id].begin(),gnome_cmd_data.tabs[id].end());

    for (vector<GnomeCmdData::Tab>::const_iterator i=gnome_cmd_data.tabs[id].begin(); i!=last_tab; ++i)
    {
        GnomeCmdDir *dir = gnome_cmd_dir_new (home, gnome_cmd_con_create_path (home, i->first.c_str()));
        fs(id)->new_tab(dir, i->second.first, i->second.second, i->second.third, TRUE);
    }
}


void GnomeCmdMainWin::switch_fs(GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    g_signal_emit (this, signals[SWITCH_FS], 0, fs);
}


void GnomeCmdMainWin::change_connection(FileSelectorID id)
{
    GnomeCmdFileSelector *fs = this->fs(id);

    switch_fs(fs);
    if (gnome_cmd_data.concombo_visibility)
        fs->con_combo->popup_list();
}


void GnomeCmdMainWin::set_fs_directory_to_opposite(FileSelectorID fsID)
{
    GnomeCmdFileSelector *fs =  this->fs(fsID);
    GnomeCmdFileSelector *other = this->fs(!fsID);

    GnomeCmdDir *dir = other->get_directory();
    gboolean fs_is_active = fs->is_active();

    if (!fs_is_active)
    {
        GnomeCmdFile *file = other->file_list()->get_selected_file();

        if (file && file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
            dir = GNOME_CMD_IS_DIR (file) ? GNOME_CMD_DIR (file) : gnome_cmd_dir_new_from_info (file->info, dir);
    }

    if (fs->file_list()->locked)
        fs->new_tab(dir);
    else
        fs->file_list()->set_connection(other->get_connection(), dir);

    other->set_active(!fs_is_active);
    fs->set_active(fs_is_active);
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


void GnomeCmdMainWin::update_toolbar_visibility()
{
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
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Send files"), file_sendto, GTK_STOCK_EXECUTE),
        GNOMEUIINFO_ITEM_FILENAME(NULL, _("Open terminal (SHIFT for root privileges)"), command_open_terminal__internal, PACKAGE_NAME G_DIR_SEPARATOR_S "terminal.svg"),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_ITEM_STOCK(NULL, _("Remote Server"), connections_open, GTK_STOCK_CONNECT),
        GNOMEUIINFO_ITEM_NONE(NULL, _("Drop connection"), connections_close_current),
        GNOMEUIINFO_END
    };

    if (gnome_cmd_data.toolbar_visibility)
    {
        create_toolbar (this, toolbar_uiinfo);
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
        if (gnome_cmd_data.toolbar_visibility)
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


void GnomeCmdMainWin::update_list_orientation()
{
    gint pos = 2;

    g_object_ref (priv->file_selector[LEFT]);
    g_object_ref (priv->file_selector[RIGHT]);
    gtk_container_remove (GTK_CONTAINER (priv->paned), priv->file_selector[LEFT]);
    gtk_container_remove (GTK_CONTAINER (priv->paned), priv->file_selector[RIGHT]);

    gtk_object_destroy (GTK_OBJECT (priv->paned));

    priv->paned = gnome_cmd_data.list_orientation ? gtk_vpaned_new () : gtk_hpaned_new ();

    g_object_ref (priv->paned);
    g_object_set_data_full (*this, "paned", priv->paned, g_object_unref);
    gtk_widget_show (priv->paned);

    gtk_paned_pack1 (GTK_PANED (priv->paned), priv->file_selector[LEFT], TRUE, TRUE);
    gtk_paned_pack2 (GTK_PANED (priv->paned), priv->file_selector[RIGHT], TRUE, TRUE);

    if (gnome_cmd_data.toolbar_visibility)
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
		gtk_widget_show (priv->menubar);
    }
    else
    {
		gtk_widget_hide (priv->menubar);
    }
}


void GnomeCmdMainWin::add_plugin_menu(PluginData *data)
{
    gnome_cmd_main_menu_add_plugin_menu (GNOME_CMD_MAIN_MENU (priv->menubar), data);
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


XML::xstream &operator << (XML::xstream &xml, GnomeCmdMainWin &mw)
{
    xml << XML::tag("Layout");

        xml << XML::tag("Panel") << XML::attr("name") << "left" << *mw.fs(LEFT) << XML::endtag();
        xml << XML::tag("Panel") << XML::attr("name") << "right" << *mw.fs(RIGHT) << XML::endtag();

    xml << XML::endtag();

    return xml;
}
