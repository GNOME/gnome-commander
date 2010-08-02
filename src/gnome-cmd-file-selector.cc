/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak

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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-patternsel-dialog.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-list-popmenu.h"
#include "gnome-cmd-user-actions.h"
#include "history.h"
#include "cap.h"
#include "utils.h"

using namespace std;


struct GnomeCmdFileSelectorClass
{
    GtkVBoxClass parent_class;

    void (* dir_changed) (GnomeCmdFileSelector *fs, GnomeCmdDir *dir);
};


static GtkVBoxClass *parent_class = NULL;


class GnomeCmdFileSelector::Private
{
  public:

    GList *old_btns;
    GtkWidget *filter_box;

    History *dir_history;
    gboolean active;
    gboolean realized;

    GnomeCmdFile *sym_file;

    Private();
    ~Private();

    //////////////////////////////////////////////////////////////////  ->> GnomeCmdFileList

    gboolean sel_first_file;
};


inline GnomeCmdFileSelector::Private::Private()
{
    old_btns = NULL;
    filter_box = NULL;
    active = FALSE;
    realized = FALSE;
    sel_first_file = TRUE;
    dir_history = NULL;
    sym_file = NULL;
}


inline GnomeCmdFileSelector::Private::~Private()
{
}


enum {DIR_CHANGED, LAST_SIGNAL};

static guint signals[LAST_SIGNAL] = { 0 };

/*******************************
 * Utility functions
 *******************************/


inline void show_list_popup (GnomeCmdFileSelector *fs)
{
    // create the popup menu
    GtkWidget *menu = gnome_cmd_list_popmenu_new (fs);
    gtk_widget_ref (menu);

    gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, fs, 3, GDK_CURRENT_TIME);
}


inline void GnomeCmdFileSelector::update_selected_files_label()
{
    GList *all_files = list->get_visible_files();

    if (!all_files)
        return;

    GnomeVFSFileSize sel_bytes = 0;
    GnomeVFSFileSize total_bytes = 0;
    gint num_files = 0;
    gint num_dirs = 0;
    gint num_sel_files = 0;
    gint num_sel_dirs = 0;

    GnomeCmdSizeDispMode size_mode = gnome_cmd_data.size_disp_mode;
    if (size_mode==GNOME_CMD_SIZE_DISP_MODE_POWERED)
        size_mode = GNOME_CMD_SIZE_DISP_MODE_GROUPED;

    for (GList *i = all_files; i; i = i->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        switch (f->info->type)
        {
            case GNOME_VFS_FILE_TYPE_DIRECTORY:
                if (!f->is_dotdot)
                {
                    num_dirs++;
                    if (gnome_cmd_file_has_tree_size (f))
                        total_bytes += gnome_cmd_file_get_tree_size (f);
                }
                break;

            case GNOME_VFS_FILE_TYPE_REGULAR:
                num_files++;
                total_bytes += f->info->size;
                break;

            default:
                break;
        }
    }

    for (GList *i = list->get_marked_files(); i; i = i->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        switch (f->info->type)
        {
            case GNOME_VFS_FILE_TYPE_DIRECTORY:
                num_sel_dirs++;
                if (gnome_cmd_file_has_tree_size (f))
                    sel_bytes += gnome_cmd_file_get_tree_size (f);
                break;

            case GNOME_VFS_FILE_TYPE_REGULAR:
                num_sel_files++;
                sel_bytes += f->info->size;
                break;

            default:
                break;
        }
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
    gnome_cmd_clist_set_voffset (*list, get_directory()->voffset);

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

    gchar *tmp = gnome_cmd_dir_get_display_path (dir);

    g_return_if_fail (tmp != NULL);

    gnome_cmd_dir_indicator_set_dir (GNOME_CMD_DIR_INDICATOR (dir_indicator), tmp);

    g_free (tmp);
}


inline void GnomeCmdFileSelector::update_vol_label()
{
    GnomeCmdCon *con = get_connection();

    if (!con)
        return;

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gchar *s = gnome_cmd_con_get_free_space (con, get_directory(), _("%s free"));
    gtk_label_set_text (GTK_LABEL (vol_label), s ? s : "");
    g_free (s);
}


static void do_file_specific_action (GnomeCmdFileList *fl, GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (f!=NULL);
    g_return_if_fail (f->info!=NULL);

    if (f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
        fl->invalidate_tree_size();

        if (f->is_dotdot)
            fl->goto_directory("..");
        else
            fl->set_directory(GNOME_CMD_DIR (f));
    }
}


inline void add_file_to_cmdline (GnomeCmdFileList *fl, gboolean fullpath)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (f && gnome_cmd_data.cmdline_visibility)
    {
        gchar *text = fullpath ? gnome_cmd_file_get_quoted_real_path (f) :
                                 gnome_cmd_file_get_quoted_name (f);

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
        gchar *dpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (fl->cwd));
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
    fs->set_connection(con);
}


static void on_combo_popwin_hidden (GnomeCmdCombo *combo, gpointer)
{
    main_win->refocus();
}


static void on_con_btn_clicked (GtkWidget *widget, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (event->type!=GDK_BUTTON_PRESS)
        return;

    if (event->button!=1 && event->button!=2)
        return;

    GnomeCmdCon *con = (GnomeCmdCon *) gtk_object_get_data (GTK_OBJECT (widget), "con");

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    main_win->switch_fs(fs);

    if (event->button==2 || event->state&GDK_CONTROL_MASK)
        fs->new_tab(gnome_cmd_con_get_default_dir(con));

    fs->set_connection(con);
}


static void create_con_buttons (GnomeCmdFileSelector *fs)
{
    static GtkTooltips *tooltips = NULL;

    if (!gnome_cmd_data.conbuttons_visibility)
        return;

    for (GList *l = fs->priv->old_btns; l; l=l->next)
        gtk_object_destroy (GTK_OBJECT (l->data));

    g_list_free (fs->priv->old_btns);
    fs->priv->old_btns = NULL;

    if (!tooltips)
        tooltips = gtk_tooltips_new ();

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l=l->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (l->data);

        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con) &&
            !GNOME_CMD_IS_CON_SMB (con))  continue;

        GnomeCmdPixmap *pm = gnome_cmd_con_get_go_pixmap (con);

        GtkWidget *btn = create_styled_button (NULL);
        g_object_set_data (G_OBJECT (btn), "con", con);
        g_signal_connect (btn, "button-press-event", (GtkSignalFunc) on_con_btn_clicked, fs);
        gtk_box_pack_start (GTK_BOX (fs->con_btns_hbox), btn, FALSE, FALSE, 0);
        GTK_WIDGET_UNSET_FLAGS (btn, GTK_CAN_FOCUS);
        fs->priv->old_btns = g_list_append (fs->priv->old_btns, btn);
        gtk_tooltips_set_tip (tooltips, btn, gnome_cmd_con_get_go_text (con), NULL);

        GtkWidget *hbox = gtk_hbox_new (FALSE, 1);
        gtk_widget_ref (hbox);
        g_object_set_data_full (*fs, "con-hbox", hbox, g_object_unref);
        gtk_widget_show (hbox);

        if (pm)
        {
            GtkWidget *pixmap = gtk_pixmap_new (pm->pixmap, pm->mask);
            if (pixmap)
            {
                gtk_widget_ref (pixmap);
                g_object_set_data_full (*fs, "con-pixmap", pixmap, g_object_unref);
                gtk_widget_show (pixmap);
                gtk_box_pack_start (GTK_BOX (hbox), pixmap, TRUE, TRUE, 0);
            }
        }

        if (!gnome_cmd_data.device_only_icon || !pm)
        {
            GtkWidget *label = gtk_label_new (gnome_cmd_con_get_alias (con));
            gtk_widget_ref (label);
            g_object_set_data_full (*fs, "con-label", label, g_object_unref);
            gtk_widget_show (label);
            gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
        }

        gtk_container_add (GTK_CONTAINER (btn), hbox);
    }
}


static void on_realize (GnomeCmdFileSelector *fs, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    fs->priv->realized = TRUE;

    create_con_buttons (fs);
    fs->update_connections();
}


static void on_notebook_switch_page (GtkNotebook *notebook, GtkNotebookPage *page, guint n, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdDir *prev_dir = fs->get_directory();

    fs->list = fs->file_list(n);
    fs->update_direntry();
    fs->update_selected_files_label();
    fs->update_vol_label();

    if (prev_dir!=fs->get_directory())
        g_signal_emit (fs, signals[DIR_CHANGED], 0, fs->get_directory());
}


static void on_list_file_clicked (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *event, gpointer)
{
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1 && gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
        do_file_specific_action (fl, f);
}


static void on_list_file_released (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *event, gpointer)
{
    if (event->type == GDK_BUTTON_RELEASE && event->button == 1 && !fl->modifier_click && gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        do_file_specific_action (fl, f);
}


static void on_list_list_clicked (GnomeCmdFileList *fl, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_BUTTON_PRESS)
        switch (event->button)
        {
            case 1:
            case 3:
                main_win->switch_fs(fs);
                break;

            case 2:
                fs->goto_directory("..");
                break;
        }
}


static void on_list_empty_space_clicked (GnomeCmdFileList *fl, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_BUTTON_PRESS)
        if (event->button == 3)
            show_list_popup (fs);
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
        gchar *fpath = gnome_cmd_file_get_path (GNOME_CMD_FILE (dir));
        fs->priv->dir_history->add(fpath);
        g_free (fpath);
    }

    if (fs->file_list()!=fl)  return;

    fs->update_direntry();
    fs->update_vol_label();

    if (fl->cwd != dir)  return;

    fs->notebook->set_label(gnome_cmd_file_get_name (GNOME_CMD_FILE (fl->cwd)));

    fs->priv->sel_first_file = FALSE;
    fs->update_files();
    fs->priv->sel_first_file = TRUE;

    if (!fs->priv->active)
    {
        GTK_CLIST (fl)->focus_row = -1;
        gtk_clist_unselect_all (*fl);
    }

    if (fs->priv->sel_first_file && fs->priv->active)
        gtk_clist_select_row (*fl, 0, 0);

    fs->update_selected_files_label();

    g_signal_emit (fs, signals[DIR_CHANGED], 0, dir);
}


static void on_list_files_changed (GnomeCmdFileList *fl, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->file_list()==fl)
        fs->update_selected_files_label();
}


// This function should only be called for input made when the file-selector was focused
static gboolean on_list_key_pressed (GtkCList *clist, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    if (!fs->file_list()->key_pressed(event) &&
        !fs->key_pressed(event) &&
        !main_win->key_pressed(event) &&
        !gcmd_user_actions.handle_key_event(main_win, fs->file_list(), event))
        return FALSE;

    stop_kp (GTK_OBJECT (clist));

    return TRUE;
}


static gboolean on_list_key_pressed_private (GtkCList *clist, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    if (state_is_blank (event->state) || state_is_shift (event->state))
    {
        if (event->keyval>=GDK_A && event->keyval<=GDK_Z ||
            event->keyval>=GDK_a && event->keyval<=GDK_z ||
            event->keyval==GDK_period)
        {
            static gchar text[2];

            if (!gnome_cmd_data.cmdline_visibility)
                gnome_cmd_file_list_show_quicksearch (fs->file_list(), (gchar) event->keyval);
            else
            {
                text[0] = event->keyval;
                gnome_cmd_cmdline_append_text (main_win->get_cmdline(), text);
                gnome_cmd_cmdline_focus (main_win->get_cmdline());
            }
            return TRUE;
        }
    }

    return FALSE;
}


static gboolean on_notebook_button_pressed (GtkWidget *widget, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    GnomeCmdNotebook *notebook = GNOME_CMD_NOTEBOOK (widget);

    int tab_clicked;

    switch (event->type)
    {
        case GDK_BUTTON_PRESS:
            switch (event->button)
            {
                case 2:
                    tab_clicked = notebook->find_tab_num_at_pos(event->x_root, event->y_root);

                    if (tab_clicked>=0)
                        fs->close_tab(tab_clicked);

                    return tab_clicked>=0;

                case 3:
                    tab_clicked = notebook->find_tab_num_at_pos(event->x_root, event->y_root);

                    if (tab_clicked>=0)
                    {
                        // notebook->set_current_page(tab_clicked);    // switch to the page the mouse is over

                        GtkWidget *menu = gtk_menu_new ();
                        GtkWidget *menuitem;

                        menuitem = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_new_tab), fs->file_list(tab_clicked));
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

                        menuitem = gtk_menu_item_new_with_mnemonic (_("_Refresh Tab"));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_refresh), fs->file_list(tab_clicked));
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        menuitem = gtk_menu_item_new_with_mnemonic (_("Copy Tab to Other _Pane"));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_in_inactive_tab), fs->file_list(tab_clicked));
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

                        menuitem = gtk_menu_item_new_with_mnemonic (_("_Close Tab"));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_close_tab), GINT_TO_POINTER (tab_clicked));
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        menuitem = gtk_menu_item_new_with_mnemonic (_("Close _All Tabs"));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_close_all_tabs), NULL);
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        menuitem = gtk_menu_item_new_with_mnemonic (_("Close _Duplicate Tabs"));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_close_duplicate_tabs), NULL);
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        gtk_widget_show_all (menu);
                        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
                    }
                    return TRUE;

                default:
                    return FALSE;
            }

        case GDK_2BUTTON_PRESS:
            if (event->button!=1)
                return FALSE;

            tab_clicked = notebook->find_tab_num_at_pos(event->x_root, event->y_root);

            if (tab_clicked>=0)
                fs->close_tab(tab_clicked);
            else
                fs->new_tab(fs->get_directory());

            return TRUE;

        default:
            return FALSE;
    }
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (object);

    delete fs->priv;

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdFileSelectorClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);;
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkVBoxClass *) gtk_type_class (gtk_vbox_get_type ());

    signals[DIR_CHANGED] =
        gtk_signal_new ("dir-changed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdFileSelectorClass, dir_changed),
            gtk_marshal_NONE__POINTER,
            GTK_TYPE_NONE,
            1, GTK_TYPE_POINTER);

    object_class->destroy = destroy;
    widget_class->map = ::map;
    klass->dir_changed = NULL;
}


static void init (GnomeCmdFileSelector *fs)
{
    fs->list = NULL;

    fs->priv = new GnomeCmdFileSelector::Private;

    GtkVBox *vbox = GTK_VBOX (fs);

    // create the box used for packing the dir_combo and buttons
    fs->update_conbuttons_visibility();

    // create the box used for packing the con_combo and information
    fs->con_hbox = create_hbox (*fs, FALSE, 2);

    // create the notebook and the first tab
    FileSelectorID id = main_win->fs(fs);
    fs->notebook = new GnomeCmdNotebook;
    fs->new_tab(NULL, gnome_cmd_data.get_sort_col(id), gnome_cmd_data.get_sort_direction(id));

    // create the connection combo
    fs->con_combo = new GnomeCmdCombo(2, 1);
    gtk_widget_ref (*fs->con_combo);
    g_object_set_data_full (*fs, "con_combo", fs->con_combo, g_object_unref);
    gtk_widget_set_size_request (*fs->con_combo, 150, -1);
    gtk_clist_set_row_height (GTK_CLIST (fs->con_combo->list), 20);
    gtk_entry_set_editable (GTK_ENTRY (fs->con_combo->entry), FALSE);
    gtk_clist_set_column_width (GTK_CLIST (fs->con_combo->list), 0, 20);
    gtk_clist_set_column_width (GTK_CLIST (fs->con_combo->list), 1, 60);
    GTK_WIDGET_UNSET_FLAGS (fs->con_combo->button, GTK_CAN_FOCUS);

    // create the free space on volume label
    fs->vol_label = gtk_label_new ("");
    gtk_widget_ref (fs->vol_label);
    g_object_set_data_full (*fs, "vol_label", fs->vol_label, g_object_unref);
    gtk_misc_set_alignment (GTK_MISC (fs->vol_label), 1, 0.5);

    // create the directory indicator
    fs->dir_indicator = gnome_cmd_dir_indicator_new (fs);
    gtk_widget_ref (fs->dir_indicator);
    g_object_set_data_full (*fs, "dir_indicator", fs->dir_indicator, g_object_unref);

    // create the info label
    fs->info_label = gtk_label_new ("not initialized");
    gtk_widget_ref (fs->info_label);
    g_object_set_data_full (*fs, "infolabel", fs->info_label, g_object_unref);
    gtk_misc_set_alignment (GTK_MISC (fs->info_label), 0.0f, 0.5f);

    // pack the widgets
    GtkWidget *padding = create_hbox (*fs, FALSE, 6);
    gtk_box_pack_start (GTK_BOX (fs), fs->con_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), fs->dir_indicator, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (fs->notebook), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), padding, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (padding), fs->info_label, FALSE, TRUE, 6);
    gtk_box_pack_start (GTK_BOX (fs->con_hbox), *fs->con_combo, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (fs->con_hbox), fs->vol_label, TRUE, TRUE, 6);

    // connect signals
    g_signal_connect (fs, "realize", G_CALLBACK (on_realize), fs);
    g_signal_connect (fs->con_combo, "item-selected", G_CALLBACK (on_con_combo_item_selected), fs);
    g_signal_connect (fs->con_combo, "popwin-hidden", G_CALLBACK (on_combo_popwin_hidden), NULL);
    g_signal_connect (gnome_cmd_con_list_get (), "list-changed", G_CALLBACK (on_con_list_list_changed), fs);
    g_signal_connect (fs->notebook, "switch-page", G_CALLBACK (on_notebook_switch_page), fs);
    g_signal_connect (fs->notebook, "button-press-event", G_CALLBACK (on_notebook_button_pressed), fs);

    // show the widgets
    gtk_widget_show (GTK_WIDGET (vbox));
    fs->update_concombo_visibility();
    gtk_widget_show (*fs->con_combo);
    gtk_widget_show (fs->vol_label);
    gtk_widget_show (fs->dir_indicator);
    gtk_widget_show_all (*fs->notebook);
    gtk_widget_show (fs->info_label);

    fs->update_style();
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_file_selector_get_type ()
{
    static GtkType fs_type = 0;

    if (fs_type == 0)
    {
        GtkTypeInfo fs_info =
        {
            "GnomeCmdFileSelector",
            sizeof (GnomeCmdFileSelector),
            sizeof (GnomeCmdFileSelectorClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        fs_type = gtk_type_unique (gtk_vbox_get_type (), &fs_info);
    }
    return fs_type;
}


GtkWidget *gnome_cmd_file_selector_new ()
{
    GnomeCmdFileSelector *fs = (GnomeCmdFileSelector *) gtk_type_new (gnome_cmd_file_selector_get_type ());

    return *fs;
}


void GnomeCmdFileSelector::first()
{
    if (!priv->dir_history->can_back())
        return;

    priv->dir_history->lock();
    goto_directory(priv->dir_history->first());
    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::back()
{
    if (!priv->dir_history->can_back())
        return;

    priv->dir_history->lock();
    goto_directory(priv->dir_history->back());
    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::forward()
{
    if (!priv->dir_history->can_forward())
        return;

    priv->dir_history->lock();
    goto_directory(priv->dir_history->forward());
    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::last()
{
    if (!priv->dir_history->can_forward())
        return;

    priv->dir_history->lock();
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
    {
        gtk_widget_grab_focus (*list);
        list->select_row(GTK_CLIST (list)->focus_row);
    }
    else
        gtk_clist_unselect_all (*list);

    gnome_cmd_dir_indicator_set_active (GNOME_CMD_DIR_INDICATOR (dir_indicator), value);
}


void GnomeCmdFileSelector::update_connections()
{
    if (!priv->realized)
        return;

    gboolean found_my_con = FALSE;

    con_combo->clear();
    con_combo->highest_pixmap = 20;
    con_combo->widest_pixmap = 20;
    gtk_clist_set_row_height (GTK_CLIST (con_combo->list), 20);
    gtk_clist_set_column_width (GTK_CLIST (con_combo->list), 0, 20);

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l = l->next)
    {
        gchar *text[3];
        GnomeCmdCon *con = (GnomeCmdCon *) l->data;

        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
            && !GNOME_CMD_IS_CON_SMB (con))  continue;

        if (con == get_connection())
            found_my_con = TRUE;

        text[0] = NULL;
        text[1] = (gchar *) gnome_cmd_con_get_alias (con);
        text[2] = NULL;

        GnomeCmdPixmap *pixmap = gnome_cmd_con_get_go_pixmap (con);

        if (pixmap)
        {
            gint row = con_combo->append(text, con);

            con_combo->set_pixmap(row, 0, pixmap);
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

    create_con_buttons (this);
    update_connections();
}


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

    gchar *dpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (dir));
    gchar *filepath = g_build_filename (dpath, fname, NULL);
    g_free (dpath);
    g_return_val_if_fail (filepath, TRUE);

    gchar *escaped_filepath = g_strdup_printf ("\"%s\"", filepath);
    gchar *cmd = g_strdup_printf (gnome_cmd_data.get_editor(), escaped_filepath);
    g_free (filepath);
    g_free (escaped_filepath);

    if (cmd)
        run_command (cmd);

    return TRUE;
}


static gboolean on_create_symlink_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdFileSelector *fs)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), TRUE);
    g_return_val_if_fail (fs->priv->sym_file != NULL, TRUE);

    const gchar *fname = values[0];

    // dont create any symlink if no name was passed or cancel was selected
    if (!fname || !*fname)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("No file name given")));
        return FALSE;
    }

    GnomeVFSURI *uri = gnome_cmd_dir_get_child_uri (fs->get_directory(), fname);
    GnomeVFSResult result = gnome_vfs_create_symbolic_link (uri, gnome_cmd_file_get_uri_str (fs->priv->sym_file));

    if (result == GNOME_VFS_OK)
    {
        gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
        gnome_cmd_dir_file_created (fs->get_directory(), uri_str);
        g_free (uri_str);
        gnome_vfs_uri_unref (uri);
        return TRUE;
    }

    gnome_vfs_uri_unref (uri);

    gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (gnome_vfs_result_to_string (result)));

    return FALSE;
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
        gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, f->info->name);

    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}


void gnome_cmd_file_selector_cap_paste (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdDir *dir = fs->get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    cap_paste_files (dir);
}


gboolean GnomeCmdFileSelector::key_pressed(GdkEventKey *event)
{
    g_return_val_if_fail (event != NULL, FALSE);

    GnomeCmdFile *f;

    if (state_is_ctrl_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Tab:
                view_prev_tab ();
                return TRUE;

            case GDK_Return:
            case GDK_KP_Enter:
                add_file_to_cmdline (list, TRUE);
                return TRUE;
        }
    }
    else if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Down:
                gnome_cmd_dir_indicator_show_history (GNOME_CMD_DIR_INDICATOR (dir_indicator));
                return TRUE;

            case GDK_Left:
                back();
                stop_kp (*list);
                return TRUE;

            case GDK_Right:
                forward();
                stop_kp (*list);
                return TRUE;
        }
    }
    else if (state_is_ctrl (event->state))
    {
        switch (event->keyval)
        {
            case GDK_V:
            case GDK_v:
                gnome_cmd_file_selector_cap_paste (this);
                return TRUE;

            case GDK_P:
            case GDK_p:
                add_cwd_to_cmdline (list);
                return TRUE;

            case GDK_Page_Up:
                goto_directory("..");
                return TRUE;

            case GDK_Page_Down:
                f = list->get_selected_file();
                if (f && f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
                    do_file_specific_action (list, f);
                return TRUE;

            case GDK_Tab:
                view_next_tab ();
                return TRUE;

            case GDK_Return:
            case GDK_KP_Enter:
                add_file_to_cmdline (list, FALSE);
                return TRUE;
        }
    }
    else if (state_is_blank (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Left:
            case GDK_KP_Left:
            case GDK_BackSpace:
                goto_directory("..");
                return TRUE;

            case GDK_Right:
            case GDK_KP_Right:
                f = list->get_selected_file();
                if (f && f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
                    do_file_specific_action (list, f);
                stop_kp (*list);
                return TRUE;

            case GDK_Return:
            case GDK_KP_Enter:
                if (gnome_cmd_data.cmdline_visibility
                    && gnome_cmd_cmdline_is_empty (main_win->get_cmdline()))
                    gnome_cmd_cmdline_exec (main_win->get_cmdline());
                else
                    do_file_specific_action (list, list->get_focused_file());
                return TRUE;

            case GDK_Escape:
                if (gnome_cmd_data.cmdline_visibility)
                    gnome_cmd_cmdline_set_text (main_win->get_cmdline(), "");
                return TRUE;
        }
    }

    return FALSE;
}


void gnome_cmd_file_selector_create_symlink (GnomeCmdFileSelector *fs, GnomeCmdFile *f)
{
    const gchar *labels[] = {_("Symbolic link name:")};

    gchar *fname = get_utf8 (gnome_cmd_file_get_name (f));
    gchar *text = g_strdup_printf (gnome_cmd_data_get_symlink_prefix (), fname);
    g_free (fname);

    GtkWidget *dialog = gnome_cmd_string_dialog_new (_("Create Symbolic Link"),
                                                     labels,
                                                     1,
                                                     (GnomeCmdStringDialogCallback) on_create_symlink_ok,
                                                     fs);

    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, text);
    g_free (text);
    fs->priv->sym_file = f;
    gtk_widget_show (dialog);
}


void gnome_cmd_file_selector_create_symlinks (GnomeCmdFileSelector *fs, GList *files)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    gint choice = -1;

    for (; files; files=files->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) files->data;
        gchar *fname = get_utf8 (gnome_cmd_file_get_name (f));
        gchar *symlink_name = g_strdup_printf (gnome_cmd_data_get_symlink_prefix (), fname);

        GnomeVFSURI *uri = gnome_cmd_dir_get_child_uri (fs->get_directory(), symlink_name);

        g_free (fname);
        g_free (symlink_name);

        GnomeVFSResult result;

        do
        {
            result = gnome_vfs_create_symbolic_link (uri, gnome_cmd_file_get_uri_str (f));

            if (result == GNOME_VFS_OK)
            {
                gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
                gnome_cmd_dir_file_created (fs->get_directory(), uri_str);
                g_free (uri_str);
            }
            else
                if (choice != 1)  // choice != SKIP_ALL
                {
                    gchar *msg = g_strdup (gnome_vfs_result_to_string (result));
                    choice = run_simple_dialog (GTK_WIDGET (main_win), TRUE, GTK_MESSAGE_QUESTION, msg, _("Create Symbolic Link"), 3, _("Skip"), _("Skip all"), _("Cancel"), _("Retry"), NULL);
                    g_free (msg);
                }
        }
        while (result != GNOME_VFS_OK && choice == 3);  // choice != RETRY

        gnome_vfs_uri_unref (uri);
    }
}


void GnomeCmdFileSelector::update_conbuttons_visibility()
{
    if (!gnome_cmd_data.conbuttons_visibility)
    {
        if (con_btns_hbox)
        {
            gtk_object_destroy (GTK_OBJECT (con_btns_hbox));
            con_btns_hbox = NULL;
        }
    }
    else
    {
        if (!con_btns_hbox)
        {
            con_btns_hbox = create_hbox (*this, FALSE, 2);
            gtk_box_pack_start (GTK_BOX (this), con_btns_hbox, FALSE, FALSE, 0);
            gtk_box_reorder_child (GTK_BOX (this), con_btns_hbox, 0);
            gtk_widget_show (con_btns_hbox);
            create_con_buttons (this);
        }
    }
}


void GnomeCmdFileSelector::update_concombo_visibility()
{
    if (gnome_cmd_data.concombo_visibility)
        gtk_widget_show (con_hbox);
    else
        gtk_widget_hide (con_hbox);
}


static void on_filter_box_close (GtkButton *btn, GnomeCmdFileSelector *fs)
{
    if (!fs->priv->filter_box) return;

    gtk_widget_destroy (fs->priv->filter_box);
    fs->priv->filter_box = NULL;
}


gboolean on_filter_box_keypressed (GtkEntry *entry, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    if (state_is_blank (event->state))
        if (event->keyval == GDK_Escape)
        {
            on_filter_box_close (NULL, fs);
            return TRUE;
        }

    return FALSE;
}


void GnomeCmdFileSelector::show_filter()
{
    if (priv->filter_box) return;

    priv->filter_box = create_hbox (*this, FALSE, 0);
    GtkWidget *label = create_label (*this, _("Filter:"));
    GtkWidget *entry = create_entry (*this, "entry", "");
    GtkWidget *close_btn = create_button_with_data (GTK_WIDGET (main_win), "x", GTK_SIGNAL_FUNC (on_filter_box_close), this);

    g_signal_connect (entry, "key-press-event", G_CALLBACK (on_filter_box_keypressed), this);
    gtk_box_pack_start (GTK_BOX (priv->filter_box), label, FALSE, TRUE, 6);
    gtk_box_pack_start (GTK_BOX (priv->filter_box), entry, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (priv->filter_box), close_btn, FALSE, TRUE, 0);

    gtk_box_pack_start (*this, priv->filter_box, FALSE, TRUE, 0);

    gtk_widget_grab_focus (entry);
}


gboolean GnomeCmdFileSelector::is_active()
{
    return priv->active;
}


GtkWidget *GnomeCmdFileSelector::new_tab(GnomeCmdDir *dir, GnomeCmdFileList::ColumnID sort_col, GtkSortType sort_order, gboolean activate)
{
    // create the list
    GnomeCmdFileList *list = new GnomeCmdFileList(sort_col,sort_order);

    if (activate)
        this->list = list;               //  ... update GnomeCmdFileSelector::list to point at newly created tab

    list->update_style();

    // hide dir column
    list->show_column(GnomeCmdFileList::COLUMN_DIR, FALSE);

    // create the scrollwindow that we'll place the list in
    GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled_window), *list);

    GtkWidget *label = gtk_label_new (dir ? gnome_cmd_file_get_name (GNOME_CMD_FILE (dir)) : NULL);
    gint n = notebook->append_page(scrolled_window, label);
#if GTK_CHECK_VERSION (2, 10, 0)
    gtk_notebook_set_tab_reorderable (*notebook, scrolled_window, TRUE);
#endif

    gtk_widget_show_all (scrolled_window);

    g_signal_connect (list, "con-changed", G_CALLBACK (on_list_con_changed), this);
    g_signal_connect (list, "dir-changed", G_CALLBACK (on_list_dir_changed), this);
    g_signal_connect (list, "files-changed", G_CALLBACK (on_list_files_changed), this);

    if (activate)
    {
        notebook->set_current_page(n);
        gtk_widget_grab_focus (*list);
    }

    if (dir)
        list->set_connection(gnome_cmd_dir_get_connection (dir), dir);

    g_signal_connect (list, "file-clicked", G_CALLBACK (on_list_file_clicked), NULL);
    g_signal_connect (list, "file-released", G_CALLBACK (on_list_file_released), NULL);
    g_signal_connect (list, "list-clicked", G_CALLBACK (on_list_list_clicked), this);
    g_signal_connect (list, "empty-space-clicked", G_CALLBACK (on_list_empty_space_clicked), this);

    g_signal_connect (list, "key-press-event", G_CALLBACK (on_list_key_pressed), this);
    g_signal_connect (list, "key-press-event", G_CALLBACK (on_list_key_pressed_private), this);

    return scrolled_window;
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdFileSelector &fs)
{
    if (gnome_cmd_data.save_tabs_on_exit)
    {
        GList *tabs = gtk_container_get_children (*fs.notebook);

        for (GList *i=tabs; i; i=i->next)
        {
            GnomeCmdFileList *fl = (GnomeCmdFileList *) gtk_bin_get_child (GTK_BIN (i->data));

            if (GNOME_CMD_FILE_LIST (fl) && gnome_cmd_con_is_local (fl->con))
                xml << *fl;
        }

        g_list_free (tabs);
    }
    else
        if (gnome_cmd_data.save_dirs_on_exit)
            if (fs.is_local())
                xml << *fs.file_list();

    return xml;
}
