/**
 * @file gnome-cmd-file-selector.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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
#ifdef HAVE_SAMBA
#include "gnome-cmd-con-smb.h"
#endif
#include "gnome-cmd-combo.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-list-popmenu.h"
#include "gnome-cmd-user-actions.h"
#include "history.h"
#include "cap.h"
#include "utils.h"
#include "dialogs/gnome-cmd-patternsel-dialog.h"

using namespace std;


struct GnomeCmdFileSelectorClass
{
    GtkVBoxClass parent_class;

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

    GnomeCmdFile *sym_file {nullptr};

    //////////////////////////////////////////////////////////////////  ->> GnomeCmdFileList

    gboolean sel_first_file {TRUE};
};

enum {DIR_CHANGED, LAST_SIGNAL};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GnomeCmdFileSelector, gnome_cmd_file_selector, GTK_TYPE_VBOX)


/*******************************
 * Utility functions
 *******************************/


inline void show_list_popup (GnomeCmdFileSelector *fs)
{
    // create the popup menu
    GtkWidget *menu = gnome_cmd_list_popmenu_new (fs);
    g_object_ref (menu);

    gtk_menu_popup (GTK_MENU (menu), nullptr, nullptr, nullptr, fs, 3, GDK_CURRENT_TIME);
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
    if (priv->realized)
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

    if (!con)
        return;

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gchar *s = gnome_cmd_con_get_free_space (con, get_directory(), _("%s free"));
    gtk_label_set_text (GTK_LABEL (vol_label), s ? s : "");
    g_free (s);
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

    GdkModifierType mask;

    gdk_window_get_pointer (nullptr, nullptr, nullptr, &mask);

    if (mask & GDK_CONTROL_MASK || fs->file_list()->locked)
        fs->new_tab(gnome_cmd_con_get_default_dir (con));
    else
        fs->set_connection(con, gnome_cmd_con_get_default_dir (con));
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

    auto con = static_cast<GnomeCmdCon*> (g_object_get_data (G_OBJECT (widget), "con"));

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    main_win->switch_fs(fs);

    if (event->button==2 || event->state&GDK_CONTROL_MASK || fs->file_list()->locked)
        fs->new_tab(gnome_cmd_con_get_default_dir (con));

    fs->set_connection(con, gnome_cmd_con_get_default_dir (con));
}


static void create_con_buttons (GnomeCmdFileSelector *fs)
{
    if (!gnome_cmd_data.show_devbuttons)
        return;

    for (GList *l = fs->priv->old_btns; l; l=l->next)
        gtk_widget_destroy (GTK_WIDGET (l->data));

    g_list_free (fs->priv->old_btns);
    fs->priv->old_btns = nullptr;

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l=l->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (l->data);

#ifdef HAVE_SAMBA
        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con) &&
            !GNOME_CMD_IS_CON_SMB (con))  continue;
#else
        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con))  continue;
#endif

        GdkPixbuf *pb = gnome_cmd_con_get_go_pixbuf (con);

        GtkWidget *btn = create_styled_button (nullptr);
        g_object_set_data (G_OBJECT (btn), "con", con);
        g_signal_connect (btn, "button-press-event", G_CALLBACK (on_con_btn_clicked), fs);
        gtk_box_pack_start (GTK_BOX (fs->con_btns_hbox), btn, FALSE, FALSE, 0);
        gtk_widget_set_can_focus (btn, FALSE);
        fs->priv->old_btns = g_list_append (fs->priv->old_btns, btn);
        gtk_widget_set_tooltip_text (btn, gnome_cmd_con_get_go_text (con));

        GtkWidget *hbox = gtk_hbox_new (FALSE, 1);
        g_object_ref (hbox);
        g_object_set_data_full (*fs, "con-hbox", hbox, g_object_unref);
        gtk_widget_show (hbox);

        if (pb)
        {
            GtkWidget *image = gtk_image_new_from_pixbuf (pb);
            if (image)
            {
                g_object_ref (image);
                g_object_set_data_full (*fs, "con-image", image, g_object_unref);
                gtk_widget_show (image);
                gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);
            }
        }

        if (!gnome_cmd_data.options.device_only_icon || !pb)
        {
            GtkWidget *label = gtk_label_new (gnome_cmd_con_get_alias (con));
            g_object_ref (label);
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


static void on_list_file_clicked (GnomeCmdFileList *fl, GnomeCmdFile *f, GtkTreeIter *iter, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1 && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
        fs->do_file_specific_action (fl, f);
}


static void on_list_file_released (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_BUTTON_RELEASE && event->button == 1 && !fl->modifier_click && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        fs->do_file_specific_action (fl, f);
}


static void on_list_list_clicked (GnomeCmdFileList *fl, GnomeCmdFile *f, GtkTreeIter *iter, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_BUTTON_PRESS)
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
                    if (f && f->is_dotdot)
                        fs->new_tab(gnome_cmd_dir_get_parent (fl->cwd));
                    else
                        fs->new_tab(f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
                                         ? GNOME_CMD_DIR (f)
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


// This function should only be called for input made when the file-selector was focused
static gboolean on_list_key_pressed (GnomeCmdFileList *list, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    if (!fs->file_list()->key_pressed(event) &&
        !fs->key_pressed(event) &&
        !main_win->key_pressed(event) &&
        !gcmd_user_actions.handle_key_event(main_win, fs->file_list(), event))
        return FALSE;

    g_signal_stop_emission_by_name (list, "key-press-event");

    return TRUE;
}


static gboolean on_list_key_pressed_private (GnomeCmdFileList *list, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    if (state_is_blank (event->state) || state_is_shift (event->state))
    {
        if ((event->keyval>=GDK_KEY_A && event->keyval<=GDK_KEY_Z) ||
            (event->keyval>=GDK_KEY_a && event->keyval<=GDK_KEY_z) ||
            event->keyval==GDK_KEY_period)
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

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (event->type)
    {
        case GDK_BUTTON_PRESS:
            switch (event->button)
            {
                // mid-click
                case 2:
                    tab_clicked = notebook->find_tab_num_at_pos(event->x_root, event->y_root);

                    if (tab_clicked>=0)
                    {
                        GnomeCmdFileList *fl = fs->file_list(tab_clicked);

                        if (!fl->locked || gnome_cmd_prompt_message (*main_win, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, _("The tab is locked, close anyway?"))==GTK_RESPONSE_OK)
                            fs->close_tab(tab_clicked);
                    }

                    return tab_clicked>=0;

                // right-click
                case 3:
                    tab_clicked = notebook->find_tab_num_at_pos(event->x_root, event->y_root);

                    if (tab_clicked>=0)
                    {
                        // notebook->set_current_page(tab_clicked);    // switch to the page the mouse is over

                        GtkWidget *menu = gtk_menu_new ();
                        GtkWidget *menuitem;

                        GnomeCmdFileList *fl = fs->file_list(tab_clicked);

                        menuitem = gtk_image_menu_item_new_with_mnemonic (_("Open in New _Tab"));
                        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_new_tab), fl);
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

                        menuitem = gtk_image_menu_item_new_with_mnemonic (fl->locked ? _("_Unlock Tab") : _("_Lock Tab"));
                        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), gtk_image_new_from_file (fl->locked ? PIXMAPS_DIR G_DIR_SEPARATOR_S "unpin.png" : PIXMAPS_DIR G_DIR_SEPARATOR_S "pin.png"));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_toggle_tab_lock), GINT_TO_POINTER (fs->is_active() ? tab_clicked+1 : -tab_clicked-1));
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Refresh Tab"));
                        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_refresh), fl);
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        menuitem = gtk_image_menu_item_new_with_mnemonic (_("Copy Tab to Other _Pane"));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_in_inactive_tab), fl);
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

                        menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Close Tab"));
                        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_close_tab), fl);
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        menuitem = gtk_image_menu_item_new_with_mnemonic (_("Close _All Tabs"));
                        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_close_all_tabs), fs);
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        menuitem = gtk_image_menu_item_new_with_mnemonic (_("Close _Duplicate Tabs"));
                        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
                        g_signal_connect (menuitem, "activate", G_CALLBACK (view_close_duplicate_tabs), fs);
                        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

                        gtk_widget_show_all (menu);
                        gtk_menu_popup (GTK_MENU (menu), nullptr, nullptr, nullptr, nullptr, event->button, event->time);
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
            {
                GnomeCmdFileList *fl = fs->file_list(tab_clicked);

                if (!fl->locked || gnome_cmd_prompt_message (*main_win, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, _("The tab is locked, close anyway?"))==GTK_RESPONSE_OK)
                    fs->close_tab(tab_clicked);


            }
            else
                fs->new_tab(fs->get_directory());

            return TRUE;

        default:
            return FALSE;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkWidget *object)
{
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (object);

    delete fs->priv;

    GTK_WIDGET_CLASS (gnome_cmd_file_selector_parent_class)->destroy (object);
}


static void map (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (gnome_cmd_file_selector_parent_class)->map (widget);
}


static void gnome_cmd_file_selector_class_init (GnomeCmdFileSelectorClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    signals[DIR_CHANGED] =
        g_signal_new ("dir-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileSelectorClass, dir_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    widget_class->destroy = destroy;
    widget_class->map = ::map;
}


static void gnome_cmd_file_selector_init (GnomeCmdFileSelector *fs)
{
    gint string_size = 0;
    gint max_string_size = 150;

    fs->list = nullptr;

    fs->priv = new GnomeCmdFileSelector::Private;

    GtkVBox *vbox = GTK_VBOX (fs);

    // create the box used for packing the dir_combo and buttons
    fs->update_show_devbuttons();

    // create the box used for packing the con_combo and information
    fs->con_hbox = create_hbox (*fs, FALSE, 2);

    // create the notebook and the first tab
    fs->notebook = new GnomeCmdNotebook;

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l = l->next)
    {
        auto con = static_cast<GnomeCmdCon*> (l->data);

#ifdef HAVE_SAMBA
        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
            && !GNOME_CMD_IS_CON_SMB (con))  continue;
#else
        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con))  continue;
#endif

        string textstring {gnome_cmd_con_get_alias (con)};
        string_size = get_string_pixel_size (textstring.c_str(), textstring.length());
        max_string_size = string_size > max_string_size ? string_size : max_string_size;
    }

    // create the connection combo
    GtkListStore *store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
    fs->con_combo = GNOME_CMD_COMBO (gnome_cmd_combo_new_with_store(store, 2, 1, 2));
    g_object_ref (fs->con_combo);
    g_object_set_data_full (*fs, "con_combo", fs->con_combo, g_object_unref);
    gtk_widget_set_size_request (*fs->con_combo, max_string_size, -1);
    gtk_editable_set_editable (GTK_EDITABLE (fs->con_combo->get_entry()), FALSE);

    // create the free space on volume label
    fs->vol_label = gtk_label_new ("");
    g_object_ref (fs->vol_label);
    g_object_set_data_full (*fs, "vol_label", fs->vol_label, g_object_unref);
    gtk_misc_set_alignment (GTK_MISC (fs->vol_label), 1, 0.5);

    // create the directory indicator
    fs->dir_indicator = gnome_cmd_dir_indicator_new (fs);
    g_object_ref (fs->dir_indicator);
    g_object_set_data_full (*fs, "dir_indicator", fs->dir_indicator, g_object_unref);

    // create the info label
    fs->info_label = gtk_label_new ("not initialized");
    g_object_ref (fs->info_label);
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
    // changing this value has only an effect when restarting gcmd
    if (gnome_cmd_data.show_devlist)
        gtk_box_pack_start (GTK_BOX (fs->con_hbox), fs->vol_label, TRUE, TRUE, 6);
    else
        gtk_box_pack_start (GTK_BOX (padding), fs->vol_label, TRUE, TRUE, 6);

    // connect signals
    g_signal_connect (fs, "realize", G_CALLBACK (on_realize), fs);
    g_signal_connect (fs->con_combo, "item-selected", G_CALLBACK (on_con_combo_item_selected), fs);
    g_signal_connect (fs->con_combo, "popwin-hidden", G_CALLBACK (on_combo_popwin_hidden), nullptr);
    g_signal_connect (gnome_cmd_con_list_get (), "list-changed", G_CALLBACK (on_con_list_list_changed), fs);
    g_signal_connect (fs->notebook, "switch-page", G_CALLBACK (on_notebook_switch_page), fs);
    g_signal_connect (fs->notebook, "button-press-event", G_CALLBACK (on_notebook_button_pressed), fs);

    // show the widgets
    gtk_widget_show (GTK_WIDGET (vbox));
    fs->update_show_devlist();
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

#ifdef HAVE_SAMBA
        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
            && !GNOME_CMD_IS_CON_SMB (con))  continue;
#else
        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con))  continue;
#endif

        if (con == get_connection())
            found_my_con = TRUE;

        GdkPixbuf *pixbuf = gnome_cmd_con_get_go_pixbuf (con);

        if (pixbuf)
            con_combo->append((gchar *) gnome_cmd_con_get_alias (con), con, 0, pixbuf, -1);
    }

    // If the connection is no longer available use the home connection
    if (!found_my_con)
        set_connection(get_home_con ());
    else
        con_combo->select_data(get_connection());

    create_con_buttons (this);
}


static void update_style_notebook_tab (GtkWidget *widget, GnomeCmdFileSelector *fs)
{
    auto fl = reinterpret_cast<GnomeCmdFileList*> (gtk_bin_get_child (GTK_BIN (widget)));

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (gnome_cmd_data.options.tab_lock_indicator!=GnomeCmdData::TAB_LOCK_ICON)
        gtk_widget_hide (fl->tab_label_pin);

    if (fl->locked)
        fs->update_tab_label(fl);
}


void GnomeCmdFileSelector::update_style()
{
    con_combo->update_style();

    if (list)
        list->update_style();

    if (priv->realized)
        update_files();

    notebook->show_tabs(gnome_cmd_data.options.always_show_tabs ? GnomeCmdNotebook::SHOW_TABS : GnomeCmdNotebook::HIDE_TABS_IF_ONE);

    gtk_container_foreach (*notebook, (GtkCallback) update_style_notebook_tab, this);

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
        auto con = gnome_cmd_dir_get_connection (dir);
        auto conPath = gnome_cmd_con_create_path (con, fname);
        gFile = gnome_cmd_con_create_gfile (con, conPath);
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


static gboolean on_create_symlink_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdFileSelector *fs)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), TRUE);
    g_return_val_if_fail (fs->priv->sym_file != nullptr, TRUE);
    GError *error = nullptr;

    const gchar *fname = values[0];

    // dont create any symlink if no name was passed or cancel was selected
    if (!fname || !*fname)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("No file name given")));
        return FALSE;
    }

    GnomeCmdDir *dir = fs->get_directory();
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), TRUE);

    GFile *gFile;
    if (fname[0] == '/')
    {
        auto con = gnome_cmd_dir_get_connection (dir);
        auto conPath = gnome_cmd_con_create_path (con, fname);
        gFile = gnome_cmd_con_create_gfile (con, conPath);
        delete conPath;
    }
    else
        gFile = gnome_cmd_dir_get_child_gfile (dir, fname);
    auto absolutePath = g_file_get_parse_name(fs->priv->sym_file->get_file());

    //if (g_file_make_symbolic_link (gFile, fs->priv->sym_file->get_uri_str(), nullptr, &error))
    if (g_file_make_symbolic_link (gFile, absolutePath, nullptr, &error))
    {
        auto parentGFile = g_file_get_parent (gFile);
        if (g_file_equal (parentGFile, gnome_cmd_dir_get_gfile (dir)))
        {
            gchar *uri_str = g_file_get_uri (gFile);
            gnome_cmd_dir_file_created (dir, uri_str);
            g_free (uri_str);
        }
        g_object_unref(parentGFile);

        g_free(absolutePath);
        g_object_unref (gFile);
        return TRUE;
    }

    g_object_unref (gFile);
    auto msg = g_strdup(error->message);
    gnome_cmd_string_dialog_set_error_desc (string_dialog, msg);
    g_free(msg);
    g_free(absolutePath);
    g_error_free(error);

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


gboolean GnomeCmdFileSelector::key_pressed(GdkEventKey *event)
{
    g_return_val_if_fail (event != nullptr, FALSE);

    GnomeCmdFile *f;

    if (state_is_ctrl_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                view_prev_tab ();
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
                g_signal_stop_emission_by_name (list, "key-press-event");
                return TRUE;

            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                forward();
                g_signal_stop_emission_by_name (list, "key-press-event");
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
                view_next_tab ();
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
                g_signal_stop_emission_by_name (list, "key-press-event");
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


void gnome_cmd_file_selector_create_symlink (GnomeCmdFileSelector *fs, GnomeCmdFile *f)
{
    const gchar *labels[] = {_("Symbolic link name:")};

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    gchar *text = g_strdup_printf (gnome_cmd_data_get_symlink_prefix (), f->get_name());
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

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
        GError *error = nullptr;
        auto f = static_cast<GnomeCmdFile*> (files->data);
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
        gchar *symlink_name = g_strdup_printf (gnome_cmd_data_get_symlink_prefix (), f->get_name());
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

        auto gFile = gnome_cmd_dir_get_child_gfile (fs->get_directory(), symlink_name);

        g_free (symlink_name);

        gboolean result;

        do
        {
            auto absolutePathName = g_file_get_parse_name(f->get_file());
            result = g_file_make_symbolic_link (gFile, absolutePathName, nullptr, &error);
            g_free(absolutePathName);
            if (!result) // 0 means it worked
            {
                gchar *uri_str = g_file_get_uri (gFile);
                gnome_cmd_dir_file_created (fs->get_directory(), uri_str);
                g_free (uri_str);
            }
            else
                if (choice != 1 && error)  // choice != SKIP_ALL
                {
                    gchar *msg = g_strdup (error->message);
                    choice = run_simple_dialog (*main_win, TRUE, GTK_MESSAGE_QUESTION, msg, _("Create Symbolic Link"), 3, _("Skip"), _("Skip all"), _("Cancel"), _("Retry"), nullptr);
                    g_free (msg);
                    g_error_free(error);
                }
        }
        while (result != true && choice == 3);  // choice != RETRY

        g_object_unref (gFile);
    }
}


void GnomeCmdFileSelector::update_show_devbuttons()
{
    if (!gnome_cmd_data.show_devbuttons)
    {
        if (con_btns_hbox)
        {
            gtk_widget_destroy (GTK_WIDGET (con_btns_hbox));
            con_btns_hbox = nullptr;
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


void GnomeCmdFileSelector::update_show_devlist()
{
    if (gnome_cmd_data.show_devlist)
        gtk_widget_show (con_hbox);
    else
        gtk_widget_hide (con_hbox);
}


static void on_filter_box_close (GtkButton *btn, GnomeCmdFileSelector *fs)
{
    if (!fs->priv->filter_box) return;

    gtk_widget_destroy (fs->priv->filter_box);
    fs->priv->filter_box = nullptr;
}


static gboolean on_filter_box_keypressed (GtkEntry *entry, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    if (state_is_blank (event->state))
        if (event->keyval == GDK_KEY_Escape)
        {
            on_filter_box_close (nullptr, fs);
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
    GtkWidget *close_btn = create_button_with_data (*main_win, "x", G_CALLBACK (on_filter_box_close), this);

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
    GtkWidget *scrolled_window = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled_window), *fl);

    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);

    fl->tab_label_pin = gtk_image_new_from_file (PIXMAPS_DIR G_DIR_SEPARATOR_S "pin.png");
    fl->tab_label_text = gtk_label_new (dir ? GNOME_CMD_FILE (dir)->get_name() : nullptr);

    gtk_box_pack_start (GTK_BOX (hbox), fl->tab_label_pin, FALSE, FALSE, 3);
    gtk_box_pack_start (GTK_BOX (hbox), fl->tab_label_text, FALSE, FALSE, 0);

    if (locked && gnome_cmd_data.options.tab_lock_indicator==GnomeCmdData::TAB_LOCK_ICON)
        gtk_widget_show (fl->tab_label_pin);
    gtk_widget_show (fl->tab_label_text);

    gint n = notebook->append_page(scrolled_window, hbox);
    gtk_notebook_set_tab_reorderable (*notebook, scrolled_window, TRUE);

    gtk_widget_show_all (scrolled_window);

    g_signal_connect (fl, "con-changed", G_CALLBACK (on_list_con_changed), this);
    g_signal_connect (fl, "dir-changed", G_CALLBACK (on_list_dir_changed), this);
    g_signal_connect (fl, "files-changed", G_CALLBACK (on_list_files_changed), this);

    if (dir)
        fl->set_connection(gnome_cmd_dir_get_connection (dir), dir);

    if (activate)
    {
        notebook->set_current_page(n);
        gtk_widget_grab_focus (*fl);
    }

    g_signal_connect (fl, "file-clicked", G_CALLBACK (on_list_file_clicked), this);
    g_signal_connect (fl, "file-released", G_CALLBACK (on_list_file_released), this);
    g_signal_connect (fl, "list-clicked", G_CALLBACK (on_list_list_clicked), this);
    g_signal_connect (fl, "empty-space-clicked", G_CALLBACK (on_list_empty_space_clicked), this);

    g_signal_connect (fl, "key-press-event", G_CALLBACK (on_list_key_pressed), this);
    g_signal_connect (fl, "key-press-event", G_CALLBACK (on_list_key_pressed_private), this);

    return scrolled_window;
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


GList* GnomeCmdFileSelector::GetTabs()
{
    GList *tabs = gtk_container_get_children (*this->notebook);

    return tabs;
}
