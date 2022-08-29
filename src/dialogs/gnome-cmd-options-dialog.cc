/**
 * @file gnome-cmd-options-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2022 Uwe Scholz\n
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
#include <gtk/gtk.h>

#include "gnome-cmd-includes.h"
#include "dialogs/gnome-cmd-options-dialog.h"
#include "utils.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-con-list.h"

using namespace std;

GtkWidget *create_filter_tab (GtkWidget *parent, GnomeCmdData::Options &cfg);
GtkWidget *create_font_picker (GtkWidget *parent, const gchar *name);
GtkWidget *create_tabs_tab (GtkWidget *parent, GnomeCmdData::Options &cfg);
void add_app_to_list (GtkTreeView *view, GnomeCmdApp *app);
void add_device_to_list (GtkTreeView *view, GnomeCmdConDevice *dev);
void get_device_dialog_values (GtkWidget *dialog, gchar **alias, gchar **device_utf8, gchar **mountp_utf8, gchar **icon_path);
void store_confirmation_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
void store_devices_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
void store_filter_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
void store_format_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
void store_general_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
void store_layout_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
void store_programs_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
void store_tabs_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
void update_app_in_list (GtkTreeView *view, GnomeCmdApp *app);
void update_device_in_list (GtkTreeView *view, GnomeCmdConDevice *dev, gchar *alias, gchar *device_fn, gchar *mountp, gchar *icon_path);

GtkWidget *create_font_picker (GtkWidget *parent, const gchar *name)
{
    GtkWidget *w = gtk_font_button_new ();
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), name, w, g_object_unref);
    gtk_widget_show (w);

    return w;
}


static void on_save_tabs_toggled (GtkToggleButton *togglebutton, GtkWidget *dialog)
{
    GtkWidget *check = lookup_widget (dialog, "save_dirs");

    gtk_widget_set_sensitive (check, !gtk_toggle_button_get_active (togglebutton));
}


static void on_confirm_delete_toggled (GtkToggleButton *togglebutton, GtkWidget *dialog)
{
    GtkWidget *check = lookup_widget (dialog, "delete_default_check");

    gtk_widget_set_sensitive (check, gtk_toggle_button_get_active (togglebutton));
}


/***********************************************************************
 *
 *  The General tab
 *
 **********************************************************************/

static GtkWidget *create_general_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *frame, *hbox, *vbox, *cat, *cat_box;
    GtkWidget *radio, *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);

    GtkWidget *scrolled_window;
    /* create a new scrolled window. */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 0);
    /* the policy is one of GTK_POLICY AUTOMATIC, or GTK_POLICY_ALWAYS.
     * GTK_POLICY_AUTOMATIC will automatically decide whether you need
     * scrollbars, whereas GTK_POLICY_ALWAYS will always leave the scrollbars
     * there.  The first one is the horizontal scrollbar, the second,
     * the vertical. */
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    /* pack the scrolled_window into the hbox */
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    /* pack the vbox into the scrolled window */
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), vbox);
    /* https://stackoverflow.com/questions/9498699/remove-gtkscrolledwindow-frame-border-in-c */
    gtk_viewport_set_shadow_type(GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled_window))), GTK_SHADOW_NONE);

    // Left mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Left mouse button"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Single click to open items"), "lmb_singleclick_radio");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (cfg.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Double click to open items"), "lmb_doubleclick_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    check = create_check (parent, _("Single click unselects files"), "lmb_unselects_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.left_mouse_button_unselects);


    // Middle mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Middle mouse button"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Up one directory"), "mmb_cd_up_radio");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (cfg.middle_mouse_button_mode == GnomeCmdData::MIDDLE_BUTTON_GOES_UP_DIR)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Opens new tab"), "mmb_new_tab_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.middle_mouse_button_mode == GnomeCmdData::MIDDLE_BUTTON_OPENS_NEW_TAB)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // Right mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Right mouse button"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Shows popup menu"), "rmb_popup_radio");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (cfg.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_POPUPS_MENU)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Selects files"), "rmb_sel_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // Delete options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Deletion"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Move to trash"), "delete_to_trash");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.deleteToTrash);


    // Selection options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Selection"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Select directories"), "select_dirs");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.select_dirs);


    // Sort options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Sorting/Quick search"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Case sensitive"), "case_sens_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.case_sens_sort);


    // Quick search options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Quick search"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("CTRL+ALT+letters"), "ctrl_alt_quick_search");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (cfg.quick_search == GNOME_CMD_QUICK_SEARCH_CTRL_ALT)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("ALT+letters (menu access with F12)"), "alt_quick_search");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.quick_search == GNOME_CMD_QUICK_SEARCH_ALT)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Just letters (command line access with CTRL+ALT+C)"), "quick_search");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.quick_search == GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    check = create_check (parent, _("Match beginning of the file name"), "qsearch_exact_match_begin");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.quick_search_exact_match_begin);

    check = create_check (parent, _("Match end of the file name"), "qsearch_exact_match_end");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.quick_search_exact_match_end);

#ifdef HAVE_UNIQUE
    // Multiple instances
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Multiple instances"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Don’t start a new instance"), "multiple_instance_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), !cfg.allow_multiple_instances);
#endif

    // Save on exit
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Save on exit"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Directories"), "save_dirs");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.save_dirs_on_exit);

    check = create_check (parent, _("Tabs"), "save_tabs");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    g_signal_connect (check, "toggled", G_CALLBACK (on_save_tabs_toggled), parent);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.save_tabs_on_exit);

    check = create_check (parent, _("Directory history"), "save_dir_history");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.save_dir_history_on_exit);

    check = create_check (parent, _("Commandline history"), "save_cmdline_history");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.save_cmdline_history_on_exit);

    return frame;
}


void store_general_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *lmb_singleclick_radio = lookup_widget (dialog, "lmb_singleclick_radio");
    GtkWidget *lmb_unselects_check = lookup_widget (dialog, "lmb_unselects_check");
    GtkWidget *mmb_cd_up_radio = lookup_widget (dialog, "mmb_cd_up_radio");
    GtkWidget *rmb_popup_radio = lookup_widget (dialog, "rmb_popup_radio");
    GtkWidget *select_dirs = lookup_widget (dialog, "select_dirs");
    GtkWidget *delete_to_trash = lookup_widget (dialog, "delete_to_trash");
    GtkWidget *case_sens_check = lookup_widget (dialog, "case_sens_check");
    GtkWidget *ctrl_alt_quick_search = lookup_widget (dialog, "ctrl_alt_quick_search");
    GtkWidget *alt_quick_search = lookup_widget (dialog, "alt_quick_search");
#ifdef HAVE_UNIQUE
    GtkWidget *multiple_instance_check = lookup_widget (dialog, "multiple_instance_check");
#endif
    GtkWidget *qsearch_exact_match_begin = lookup_widget (dialog, "qsearch_exact_match_begin");
    GtkWidget *qsearch_exact_match_end = lookup_widget (dialog, "qsearch_exact_match_end");
    GtkWidget *save_dirs = lookup_widget (dialog, "save_dirs");
    GtkWidget *save_tabs = lookup_widget (dialog, "save_tabs");
    GtkWidget *save_dir_history = lookup_widget (dialog, "save_dir_history");
    GtkWidget *save_cmdline_history = lookup_widget (dialog, "save_cmdline_history");

    cfg.left_mouse_button_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lmb_singleclick_radio)) ? GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK : GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK;

    cfg.left_mouse_button_unselects = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lmb_unselects_check));

    cfg.middle_mouse_button_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mmb_cd_up_radio)) ? GnomeCmdData::MIDDLE_BUTTON_GOES_UP_DIR
                                                                                                      : GnomeCmdData::MIDDLE_BUTTON_OPENS_NEW_TAB;

    cfg.right_mouse_button_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rmb_popup_radio)) ? GnomeCmdData::RIGHT_BUTTON_POPUPS_MENU
                                                                                                     : GnomeCmdData::RIGHT_BUTTON_SELECTS;

    cfg.select_dirs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (select_dirs));
    cfg.deleteToTrash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (delete_to_trash));
    cfg.case_sens_sort = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (case_sens_check));
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctrl_alt_quick_search)))
        cfg.quick_search = GNOME_CMD_QUICK_SEARCH_CTRL_ALT;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (alt_quick_search)))
        cfg.quick_search = GNOME_CMD_QUICK_SEARCH_ALT;
    else
        cfg.quick_search = GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER;
#ifdef HAVE_UNIQUE
    cfg.allow_multiple_instances = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (multiple_instance_check));
#endif
    cfg.quick_search_exact_match_begin = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (qsearch_exact_match_begin));
    cfg.quick_search_exact_match_end = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (qsearch_exact_match_end));
    cfg.save_dirs_on_exit = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (save_dirs));
    cfg.save_tabs_on_exit = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (save_tabs));
    cfg.save_dir_history_on_exit = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (save_dir_history));
    cfg.save_cmdline_history_on_exit = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (save_cmdline_history));
}


/***********************************************************************
 *
 *  The Format tab
 *
 **********************************************************************/

static void on_date_format_update (GtkEditable *editable, GtkWidget *options_dialog)
{
    GtkWidget *format_entry = lookup_widget (options_dialog, "date_format_entry");
    GtkWidget *test_label = lookup_widget (options_dialog, "date_format_test_label");

    const char *format = gtk_entry_get_text (GTK_ENTRY (format_entry));
    gchar *locale_format = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);

    char s[256];
    time_t t = time (NULL);
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    strftime (s, sizeof(s), locale_format, localtime (&t));
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
    gchar *utf8_str = g_locale_to_utf8 (s, -1, NULL, NULL, NULL);

    gtk_label_set_text (GTK_LABEL (test_label), utf8_str);
    g_free (utf8_str);
    g_free (locale_format);
}


static GtkWidget *create_format_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *frame, *hbox, *scrolled_window, *vbox, *cat, *cat_box, *table;
    GtkWidget *radio, *label, *entry;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), vbox);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled_window))), GTK_SHADOW_NONE);

    // Size display mode
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Size display mode"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    // Translators: 'Powered' refers to the mode of file size display (here - display using units of data: kB, MB, GB, ...)
    radio = create_radio (parent, NULL, _("Powered"), "size_powered_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_POWERED)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    // Translators: '<locale>' refers to the mode of file size display (here - use current locale settings)
    radio = create_radio (parent, get_radio_group (radio), _("<locale>"), "size_locale_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_LOCALE)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("Grouped"), "size_grouped_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_GROUPED)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("Plain"), "size_plain_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_PLAIN)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // Permission display mode
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Permission display mode"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Text (rw-r--r--)"), "perm_text_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.perm_disp_mode == GNOME_CMD_PERM_DISP_MODE_TEXT)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("Number (644)"), "perm_num_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.perm_disp_mode == GNOME_CMD_PERM_DISP_MODE_NUMBER)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // Date options
    table = create_table (parent, 2, 3);
    cat = create_category (parent, table, _("Date format"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    label = create_label (parent, _("Format:"));
    table_add (table, label, 0, 0, GTK_FILL);

    gchar *utf8_date_format = g_locale_to_utf8 (cfg.date_format, -1, NULL, NULL, NULL);
    entry = create_entry (parent, "date_format_entry", utf8_date_format);
    g_free (utf8_date_format);
    gtk_widget_grab_focus (entry);
    g_signal_connect (entry, "realize", G_CALLBACK (on_date_format_update), parent);
    g_signal_connect (entry, "changed", G_CALLBACK (on_date_format_update), parent);
    table_add (table, entry, 1, 0, GTK_FILL);

    label = create_label (parent, _("Test result:"));
    table_add (table, label, 0, 1, GTK_FILL);

    label = create_label (parent, "");
    g_object_set_data (G_OBJECT (parent), "date_format_test_label", label);
    table_add (table, label, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    label = create_label (parent, _("See the manual page for “strftime” for help on how to set the format string."));
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    table_add (table, label, 1, 2, GTK_FILL);

    return frame;
}


void store_format_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *size_powered_radio = lookup_widget (dialog, "size_powered_radio");
    GtkWidget *size_locale_radio = lookup_widget (dialog, "size_locale_radio");
    GtkWidget *size_grouped_radio = lookup_widget (dialog, "size_grouped_radio");
    GtkWidget *perm_text_radio = lookup_widget (dialog, "perm_text_radio");
    GtkWidget *entry = lookup_widget (dialog, "date_format_entry");
    const gchar *format = gtk_entry_get_text (GTK_ENTRY (entry));

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (size_powered_radio)))
        cfg.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_POWERED;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (size_locale_radio)))
        cfg.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_LOCALE;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (size_grouped_radio)))
        cfg.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_GROUPED;
    else
        cfg.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_PLAIN;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (perm_text_radio)))
        cfg.perm_disp_mode = GNOME_CMD_PERM_DISP_MODE_TEXT;
    else
        cfg.perm_disp_mode = GNOME_CMD_PERM_DISP_MODE_NUMBER;

    cfg.set_date_format(g_locale_from_utf8 (format, -1, NULL, NULL, NULL));
}


/***********************************************************************
 *
 *  The Layout tab
 *
 **********************************************************************/

static void on_layout_mode_changed (GtkComboBox *combo, GtkWidget *dialog)
{
    g_return_if_fail (GTK_IS_COMBO_BOX (combo));

    GtkWidget *icon_frame = lookup_widget (GTK_WIDGET (dialog), "mime_icon_settings_frame");
    GnomeCmdLayout mode = (GnomeCmdLayout) gtk_combo_box_get_active (combo);

    if (icon_frame)
        gtk_widget_set_sensitive (icon_frame, mode == GNOME_CMD_LAYOUT_MIME_ICONS);
}


static void on_color_mode_changed (GtkComboBox *combo, GtkWidget *dialog)
{
    g_return_if_fail (GTK_IS_COMBO_BOX (combo));

    GtkWidget *btn = lookup_widget (GTK_WIDGET (dialog), "color_btn");
    GnomeCmdColorMode mode = (GnomeCmdColorMode) gtk_combo_box_get_active (combo);

    if (btn)
        gtk_widget_set_sensitive (btn, mode == GNOME_CMD_COLOR_CUSTOM);
}


static void on_edit_colors_close (GtkButton *btn, GtkWidget *dlg)
{
    GnomeCmdColorTheme *colors = gnome_cmd_data.options.get_custom_color_theme();

    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "default_fg")), colors->norm_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "default_bg")), colors->norm_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "alternate_fg")), colors->alt_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "alternate_bg")), colors->alt_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "selected_fg")), colors->sel_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "selected_bg")), colors->sel_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "cursor_fg")), colors->curs_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "cursor_bg")), colors->curs_bg);

    gtk_widget_destroy (dlg);
}


static void on_colors_edit (GtkButton *btn, GtkWidget *parent)
{
    GtkWidget *dlg = gnome_cmd_dialog_new (_("Edit Colors…"));
    g_object_ref (dlg);

    gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (parent));

    GtkWidget *cat, *cat_box;
    GtkWidget *table, *label;
    GtkWidget *cbutton;
    GnomeCmdColorTheme *colors = gnome_cmd_data.options.get_custom_color_theme();

    // The color buttons
    cat_box = create_vbox (dlg, FALSE, 12);
    cat = create_category (dlg, cat_box, _("Colors"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dlg), cat);

    table = create_table (dlg, 5, 3);
    gtk_container_add (GTK_CONTAINER (cat_box), table);

    cbutton = create_color_button (dlg, "default_fg");
    table_add (table, cbutton, 1, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), colors->norm_fg);
    cbutton = create_color_button (dlg, "default_bg");
    table_add (table, cbutton, 2, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), colors->norm_bg);
    cbutton = create_color_button (dlg, "alternate_fg");
    table_add (table, cbutton, 1, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), colors->alt_fg);
    cbutton = create_color_button (dlg, "alternate_bg");
    table_add (table, cbutton, 2, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), colors->alt_bg);
    cbutton = create_color_button (dlg, "selected_fg");
    table_add (table, cbutton, 1, 3, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), colors->sel_fg);
    cbutton = create_color_button (dlg, "selected_bg");
    table_add (table, cbutton, 2, 3, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), colors->sel_bg);
    cbutton = create_color_button (dlg, "cursor_fg");
    table_add (table, cbutton, 1, 4, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), colors->curs_fg);
    cbutton = create_color_button (dlg, "cursor_bg");
    table_add (table, cbutton, 2, 4, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), colors->curs_bg);

    label = create_label (dlg, _("Foreground"));
    table_add (table, label, 1, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Background"));
    table_add (table, label, 2, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Default:"));
    table_add (table, label, 0, 1, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Alternate:"));
    table_add (table, label, 0, 2, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Selected file:"));
    table_add (table, label, 0, 3, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Cursor:"));
    table_add (table, label, 0, 4, (GtkAttachOptions) GTK_FILL);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dlg), GTK_STOCK_CLOSE,
                                 GTK_SIGNAL_FUNC (on_edit_colors_close), dlg);

    gtk_widget_show (dlg);
}


static void on_ls_colors_toggled (GtkToggleButton *btn, GtkWidget *dialog)
{
    GtkWidget *edit_btn = lookup_widget (GTK_WIDGET (dialog), "ls_colors_edit_btn");
    if (edit_btn)
        gtk_widget_set_sensitive (edit_btn, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)));
}


static void on_edit_ls_colors_cancel (GtkButton *btn, GtkWidget *dlg)
{
    gtk_widget_destroy (dlg);
}


static void on_edit_ls_colors_ok (GtkButton *btn, GtkWidget *dlg)
{
    GnomeCmdLsColorsPalette &palette = gnome_cmd_data.options.ls_colors_palette;

    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "black_fg")), palette.black_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "black_bg")), palette.black_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "red_fg")), palette.red_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "red_bg")), palette.red_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "green_fg")), palette.green_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "green_bg")), palette.green_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "yellow_fg")), palette.yellow_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "yellow_bg")), palette.yellow_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "blue_fg")), palette.blue_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "blue_bg")), palette.blue_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "magenta_fg")), palette.magenta_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "magenta_bg")), palette.magenta_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "cyan_fg")), palette.cyan_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "cyan_bg")), palette.cyan_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "white_fg")), palette.white_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "white_bg")), palette.white_bg);

    gtk_widget_destroy (dlg);
}


static void on_edit_ls_colors_reset (GtkButton *btn, GtkWidget *dlg)
{
    static GdkColor black   = {0,0,0,0};
    static GdkColor red     = {0,0xffff,0,0};
    static GdkColor green   = {0,0,0xffff,0};
    static GdkColor yellow  = {0,0xffff,0xffff,0};
    static GdkColor blue    = {0,0,0,0xffff};
    static GdkColor magenta = {0,0xffff,0,0xffff};
    static GdkColor cyan    = {0,0,0xffff,0xffff};
    static GdkColor white   = {0,0xffff,0xffff,0xffff};

    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "black_fg")), &black);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "black_bg")), &black);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "red_fg")), &red);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "red_bg")), &red);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "green_fg")), &green);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "green_bg")), &green);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "yellow_fg")), &yellow);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "yellow_bg")), &yellow);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "blue_fg")), &blue);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "blue_bg")), &blue);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "magenta_fg")), &magenta);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "magenta_bg")), &magenta);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "cyan_fg")), &cyan);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "cyan_bg")), &cyan);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "white_fg")), &white);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "white_bg")), &white);
}


static void on_ls_colors_edit (GtkButton *btn, GtkWidget *parent)
{
    GtkWidget *dlg = gnome_cmd_dialog_new (_("Edit LS_COLORS Palette"));
    g_object_ref (dlg);

    gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (parent));

    GtkWidget *cat, *cat_box;
    GtkWidget *table, *label;
    GtkWidget *cbutton;
    GnomeCmdLsColorsPalette &palette = gnome_cmd_data.options.ls_colors_palette;

    cat_box = create_vbox (dlg, FALSE, 12);
    cat = create_category (dlg, cat_box, _("Palette"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dlg), cat);

    table = create_table (dlg, 3, 9);
    gtk_container_add (GTK_CONTAINER (cat_box), table);

    cbutton = create_color_button (dlg, "black_fg");
    table_add (table, cbutton, 1, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.black_fg);
    cbutton = create_color_button (dlg, "black_bg");
    table_add (table, cbutton, 1, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.black_bg);
    cbutton = create_color_button (dlg, "red_fg");
    table_add (table, cbutton, 2, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.red_fg);
    cbutton = create_color_button (dlg, "red_bg");
    table_add (table, cbutton, 2, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.red_bg);
    cbutton = create_color_button (dlg, "green_fg");
    table_add (table, cbutton, 3, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.green_fg);
    cbutton = create_color_button (dlg, "green_bg");
    table_add (table, cbutton, 3, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.green_bg);
    cbutton = create_color_button (dlg, "yellow_fg");
    table_add (table, cbutton, 4, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.yellow_fg);
    cbutton = create_color_button (dlg, "yellow_bg");
    table_add (table, cbutton, 4, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.yellow_bg);
    cbutton = create_color_button (dlg, "blue_fg");
    table_add (table, cbutton, 5, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.blue_fg);
    cbutton = create_color_button (dlg, "blue_bg");
    table_add (table, cbutton, 5, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.blue_bg);
    cbutton = create_color_button (dlg, "magenta_fg");
    table_add (table, cbutton, 6, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.magenta_fg);
    cbutton = create_color_button (dlg, "magenta_bg");
    table_add (table, cbutton, 6, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.magenta_bg);
    cbutton = create_color_button (dlg, "cyan_fg");
    table_add (table, cbutton, 7, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.cyan_fg);
    cbutton = create_color_button (dlg, "cyan_bg");
    table_add (table, cbutton, 7, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.cyan_bg);
    cbutton = create_color_button (dlg, "white_fg");
    table_add (table, cbutton, 8, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.white_fg);
    cbutton = create_color_button (dlg, "white_bg");
    table_add (table, cbutton, 8, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette.white_bg);

    label = create_label (dlg, _("Foreground:"));
    table_add (table, label, 0, 1, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Background:"));
    table_add (table, label, 0, 2, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Black"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);
    table_add (table, label, 1, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Red"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);
    table_add (table, label, 2, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Green"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);
    table_add (table, label, 3, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Yellow"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);
    table_add (table, label, 4, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Blue"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);
    table_add (table, label, 5, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Magenta"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);
    table_add (table, label, 6, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("Cyan"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);
    table_add (table, label, 7, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (dlg, _("White"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);
    table_add (table, label, 8, 0, (GtkAttachOptions) GTK_FILL);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dlg), _("_Reset"), GTK_SIGNAL_FUNC (on_edit_ls_colors_reset), dlg);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dlg), GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_edit_ls_colors_cancel), dlg);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dlg), GTK_STOCK_OK, GTK_SIGNAL_FUNC (on_edit_ls_colors_ok), dlg);

    gtk_widget_show (dlg);
}


static GtkWidget *create_layout_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *frame, *hbox, *scrolled_window, *vbox, *cat;
    GtkWidget *entry, *spin, *scale, *table, *label, *fpicker, *btn;
    GtkWidget *lm_combo, *cm_combo, *fe_combo, *check;
    const gchar *ext_modes[] = {
        _("With file name"),
        _("In separate column"),
        _("In both columns"),
        NULL
    };
    const gchar *gfx_modes[] = {
        _("No icons"),
        _("File type icons"),
        _("MIME icons"),
        NULL
    };
    const gchar *color_modes[GNOME_CMD_COLOR_CUSTOM+2] = {
        _("Respect theme colors"),
        _("Modern"),
        _("Fusion"),
        _("Classic"),
        _("Deep blue"),
        _("Cafezinho"),
        _("Green tiger"),
        _("Winter"),
        _("Custom"),
        NULL
    };

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), vbox);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled_window))), GTK_SHADOW_NONE);

    // File panes
    table = create_table (parent, 5, 2);
    gtk_table_set_homogeneous (GTK_TABLE (table), FALSE);
    cat = create_category (parent, table, _("File panes"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    fpicker = create_font_picker (parent, "list_font_picker");
    table_add (table, fpicker, 1, 0, GTK_FILL);
    gtk_font_button_set_font_name (GTK_FONT_BUTTON (fpicker), cfg.list_font);

    spin = create_spin (parent, "row_height_spin", 8, 64, cfg.list_row_height);
    table_add (table, spin, 1, 1, GTK_FILL);

    label = create_label (parent, _("Font:"));
    table_add (table, label, 0, 0, GTK_FILL);
    label = create_label (parent, _("Row height:"));
    table_add (table, label, 0, 1, GTK_FILL);

    // File extensions
    label = create_label (parent, _("Display file extensions:"));
    table_add (table, label, 0, 2, GTK_FILL);

    fe_combo = create_combo_box_text (parent, ext_modes);
    g_object_set_data (G_OBJECT (parent), "fe_combo", fe_combo);
    table_add (table, fe_combo, 1, 2, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    // Graphical mode
    label = create_label (parent, _("Graphical mode:"));
    table_add (table, label, 0, 3, GTK_FILL);

    lm_combo = create_combo_box_text (parent, gfx_modes);
    g_object_set_data (G_OBJECT (parent), "lm_combo", lm_combo);
    g_signal_connect (lm_combo, "changed", G_CALLBACK (on_layout_mode_changed), parent);
    table_add (table, lm_combo, 1, 3, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    // Color scheme
    label = create_label (parent, _("Color scheme:"));
    table_add (table, label, 0, 4, GTK_FILL);

    hbox = create_hbox (parent, FALSE, 6);
    table_add (table, hbox, 1, 4, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    cm_combo = create_combo_box_text (parent, color_modes);
    g_object_set_data (G_OBJECT (parent), "cm_combo", cm_combo);
    g_signal_connect (cm_combo, "changed", G_CALLBACK (on_color_mode_changed), parent);
    gtk_box_pack_start (GTK_BOX (hbox), cm_combo, TRUE, TRUE, 0);


    btn = create_button_with_data (parent, _("Edit…"), GTK_SIGNAL_FUNC (on_colors_edit), parent);
    g_object_set_data (G_OBJECT (parent), "color_btn", btn);
    gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
    gtk_widget_set_sensitive (btn, cfg.color_mode == GNOME_CMD_COLOR_CUSTOM);


    // LS_COLORS
    check = create_check (parent, _("Colorize files according to the LS_COLORS environment variable"), "use_ls_colors");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.use_ls_colors);
    hbox = create_hbox (parent, FALSE, 6);
    gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, 5, 6, GTK_FILL, GTK_FILL, 0, 0);

    g_signal_connect (check, "toggled", G_CALLBACK (on_ls_colors_toggled), parent);
    gtk_box_pack_start (GTK_BOX (hbox), check, TRUE, TRUE, 0);

    btn = create_button_with_data (parent, _("Edit colors…"), GTK_SIGNAL_FUNC (on_ls_colors_edit), parent);
    g_object_set_data (G_OBJECT (parent), "ls_colors_edit_btn", btn);
    gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
    gtk_widget_set_sensitive (btn, cfg.use_ls_colors);


     // MIME icon settings
    table = create_table (parent, 4, 2);
    cat = create_category (parent, table, _("MIME icon settings"));
    g_object_set_data (G_OBJECT (parent), "mime_icon_settings_frame", cat);
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    spin = create_spin (parent, "iconsize_spin", 8, 64, cfg.icon_size);
    table_add (table, spin, 1, 0, (GtkAttachOptions) GTK_FILL);
    scale = create_scale (parent, "iconquality_scale", cfg.icon_scale_quality, 0, 3);
    table_add (table, scale, 1, 1, (GtkAttachOptions) GTK_FILL);
    entry = create_directory_chooser_button (parent, "theme_icondir_entry", cfg.theme_icon_dir);
    table_add (table, entry, 1, 2, (GtkAttachOptions)0);

    label = create_label (parent, _("Icon size:"));
    table_add (table, label, 0, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (parent, _("Scaling quality:"));
    table_add (table, label, 0, 1, (GtkAttachOptions) GTK_FILL);
    label = create_label (parent, _("Theme icon directory:"));
    table_add (table, label, 0, 2, (GtkAttachOptions) GTK_FILL);

    gtk_combo_box_set_active (GTK_COMBO_BOX (fe_combo), (gint) cfg.ext_disp_mode);
    gtk_combo_box_set_active (GTK_COMBO_BOX (lm_combo), (gint) cfg.layout);
    gtk_combo_box_set_active (GTK_COMBO_BOX (cm_combo), (gint) cfg.color_mode);

    return frame;
}


void store_layout_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *iconsize_spin       = lookup_widget (dialog, "iconsize_spin");
    GtkWidget *iconquality_scale   = lookup_widget (dialog, "iconquality_scale");
    GtkWidget *theme_icondir_chooser = lookup_widget (dialog, "theme_icondir_entry");
    GtkWidget *row_height_spin     = lookup_widget (dialog, "row_height_spin");
    GtkWidget *use_ls              = lookup_widget (dialog, "use_ls_colors");

    GtkWidget *lm_combo = lookup_widget (dialog, "lm_combo");
    GtkWidget *fe_combo = lookup_widget (dialog, "fe_combo");
    GtkWidget *cm_combo = lookup_widget (dialog, "cm_combo");

    GtkWidget *list_font_picker = lookup_widget (dialog, "list_font_picker");

    cfg.ext_disp_mode = (GnomeCmdExtDispMode) gtk_combo_box_get_active (GTK_COMBO_BOX (fe_combo));
    cfg.layout = (GnomeCmdLayout) gtk_combo_box_get_active (GTK_COMBO_BOX (lm_combo));
    cfg.color_mode = (GnomeCmdColorMode) gtk_combo_box_get_active (GTK_COMBO_BOX (cm_combo));

    cfg.use_ls_colors = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (use_ls));

    const gchar *list_font = gtk_font_button_get_font_name (GTK_FONT_BUTTON (list_font_picker));
    cfg.set_list_font (list_font);

    gchar* icondir = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (theme_icondir_chooser));
    cfg.set_theme_icon_dir (g_filename_to_utf8(icondir, -1, nullptr, nullptr, nullptr));
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    cfg.icon_size = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (iconsize_spin));
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (iconquality_scale));
    cfg.icon_scale_quality = (GdkInterpType) gtk_adjustment_get_value (adj);

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    cfg.list_row_height = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (row_height_spin));
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    g_free(icondir);
}


/***********************************************************************
 *
 *  The Tabs tab
 *
 **********************************************************************/

GtkWidget *create_tabs_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *frame, *hbox, *scrolled_window, *vbox, *cat, *cat_box;
    GtkWidget *radio, *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), vbox);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled_window))), GTK_SHADOW_NONE);

    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Tab bar"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Always show the tab bar"), "always_show_tabs");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.always_show_tabs);


    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Tab lock indicator"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Lock icon"), "tab_lock_icon_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.tab_lock_indicator == GnomeCmdData::TAB_LOCK_ICON)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("* (asterisk)"), "tab_lock_asterisk_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.tab_lock_indicator == GnomeCmdData::TAB_LOCK_ASTERISK)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("Styled text"), "tab_lock_style_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.tab_lock_indicator == GnomeCmdData::TAB_LOCK_STYLED_TEXT)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    return frame;
}


void store_tabs_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *always_show_tabs = lookup_widget (dialog, "always_show_tabs");
    GtkWidget *tab_lock_icon_radio = lookup_widget (dialog, "tab_lock_icon_radio");
    GtkWidget *tab_lock_asterisk_radio = lookup_widget (dialog, "tab_lock_asterisk_radio");

    cfg.always_show_tabs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (always_show_tabs));

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tab_lock_icon_radio)))
        cfg.tab_lock_indicator = GnomeCmdData::TAB_LOCK_ICON;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tab_lock_asterisk_radio)))
        cfg.tab_lock_indicator = GnomeCmdData::TAB_LOCK_ASTERISK;
    else
        cfg.tab_lock_indicator = GnomeCmdData::TAB_LOCK_STYLED_TEXT;
}


/***********************************************************************
 *
 *  The Confirmation tab
 *
 **********************************************************************/

static GtkWidget *create_confirmation_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *frame, *hbox, *scrolled_window, *vbox, *cat, *cat_box;
    GtkWidget *radio, *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), vbox);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled_window))), GTK_SHADOW_NONE);


    /* Delete options
     */
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Delete"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Confirm before delete"), "confirm_delete_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.confirm_delete);
    g_signal_connect (check, "toggled", G_CALLBACK (on_confirm_delete_toggled), parent);

    check = create_check (parent, _("Confirm defaults to OK"), "delete_default_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.confirm_delete_default!=GTK_BUTTONS_CANCEL);
    gtk_widget_set_sensitive (check, cfg.confirm_delete);


    /* Copy overwrite options
     */
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Preselected overwrite action in copy dialog"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Query first"), "copy_overwrite_query");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.confirm_copy_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_QUERY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Rename"), "copy_rename_all");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.confirm_copy_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Skip"), "copy_overwrite_skip_all");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.confirm_copy_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Overwrite silently"), "copy_overwrite_silently");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (cfg.confirm_copy_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    /* Move overwrite options
     */
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Preselected overwrite action in move dialog"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Query first"), "move_overwrite_query");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_QUERY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Rename"), "move_rename_all");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Skip"), "move_overwrite_skip_all");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Overwrite silently"), "move_overwrite_silently");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (cfg.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    /* Drag and Drop options
     */
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Default Drag and Drop Action"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Confirm mouse operation"), "mouse_dnd_default");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.mouse_dnd_default==GNOME_CMD_DEFAULT_DND_QUERY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Copy"), "mouse_dnd_copy");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.mouse_dnd_default==GNOME_CMD_DEFAULT_DND_COPY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Move"), "mouse_dnd_move");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (cfg.mouse_dnd_default==GNOME_CMD_DEFAULT_DND_MOVE)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    return frame;
}


void store_confirmation_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *confirm_delete_check = lookup_widget (dialog, "confirm_delete_check");
    GtkWidget *delete_default_check = lookup_widget (dialog, "delete_default_check");
    GtkWidget *confirm_copy_silent = lookup_widget (dialog, "copy_overwrite_silently");
    GtkWidget *confirm_copy_query = lookup_widget (dialog, "copy_overwrite_query");
    GtkWidget *confirm_copy_rename_all = lookup_widget (dialog, "copy_rename_all");
    GtkWidget *confirm_copy_skip_all = lookup_widget (dialog, "copy_overwrite_skip_all");
    GtkWidget *confirm_move_silent = lookup_widget (dialog, "move_overwrite_silently");
    GtkWidget *confirm_move_query = lookup_widget (dialog, "move_overwrite_query");
    GtkWidget *confirm_move_rename_all = lookup_widget (dialog, "move_rename_all");
    GtkWidget *confirm_move_skip_all = lookup_widget (dialog, "move_overwrite_skip_all");
    GtkWidget *mouse_dnd_query = lookup_widget (dialog, "mouse_dnd_default");
    GtkWidget *mouse_dnd_copy = lookup_widget (dialog, "mouse_dnd_copy");
    GtkWidget *mouse_dnd_move = lookup_widget (dialog, "mouse_dnd_move");

    cfg.confirm_delete = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_delete_check));

    cfg.confirm_delete_default = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (delete_default_check)) ? GTK_BUTTONS_OK : GTK_BUTTONS_CANCEL;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_copy_silent)))
        cfg.confirm_copy_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_copy_query)))
        cfg.confirm_copy_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_QUERY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_copy_rename_all)))
        cfg.confirm_copy_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_copy_skip_all)))
        cfg.confirm_copy_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_move_silent)))
        cfg.confirm_move_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_move_query)))
        cfg.confirm_move_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_QUERY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_move_rename_all)))
        cfg.confirm_move_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_move_skip_all)))
        cfg.confirm_move_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mouse_dnd_query)))
        cfg.mouse_dnd_default = GNOME_CMD_DEFAULT_DND_QUERY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mouse_dnd_copy)))
        cfg.mouse_dnd_default = GNOME_CMD_DEFAULT_DND_COPY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mouse_dnd_move)))
        cfg.mouse_dnd_default = GNOME_CMD_DEFAULT_DND_MOVE;
}


/***********************************************************************
 *
 *  The Filter tab
 *
 **********************************************************************/

static void on_filter_backup_files_toggled (GtkToggleButton *btn, GtkWidget *dialog)
{
    GtkWidget *backup_pattern_entry = lookup_widget (dialog, "backup_pattern_entry");

    if (gtk_toggle_button_get_active (btn))
    {
        gtk_widget_set_sensitive (backup_pattern_entry, TRUE);
        gtk_widget_grab_focus (backup_pattern_entry);
    }
    else
        gtk_widget_set_sensitive (backup_pattern_entry, FALSE);
}


GtkWidget *create_filter_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *frame, *hbox, *vbox, *scrolled_window, *cat, *cat_box;
    GtkWidget *check, *backup_check, *entry;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), vbox);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled_window))), GTK_SHADOW_NONE);

    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Filetypes to hide"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    check = create_check (parent, _("Unknown"), "hide_unknown_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_UNKNOWN]);
    check = create_check (parent, _("Regular files"), "hide_regular_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_REGULAR]);
    check = create_check (parent, _("Directories"), "hide_directory_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_DIR]);
    check = create_check (parent, _("Socket, fifo, block, or character devices"), "hide_special_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_SPECIAL]);
    check = create_check (parent, _("Shortcuts (Windows systems)"), "hide_shortcut_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_SHORTCUT]);
    check = create_check (parent, _("Mountable locations"), "hide_mountable_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_MOUNTABLE]);
    check = create_check (parent, _("Virtual files"), "hide_virtual_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_VIRTUAL]);
    check = create_check (parent, _("Volatile files"), "hide_volatile_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_VOLATILE]);

    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Also hide"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    check = create_check (parent, _("Hidden files"), "hide_hidden_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_HIDDEN]);
    backup_check = create_check (parent, _("Backup files"), "hide_backup_check");
    gtk_container_add (GTK_CONTAINER (cat_box), backup_check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (backup_check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_BACKUP]);
    check = create_check (parent, _("Symlinks"), "hide_symlink_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.filter.file_types[GnomeCmdData::G_FILE_IS_SYMLINK]);


    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Backup files"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    entry = create_entry (parent, "backup_pattern_entry", cfg.backup_pattern);
    gtk_box_pack_start (GTK_BOX (cat_box), entry, TRUE, FALSE, 0);
    gtk_widget_set_sensitive (entry, cfg.filter.file_types[GnomeCmdData::G_FILE_IS_BACKUP]);


    g_signal_connect (backup_check, "toggled", G_CALLBACK (on_filter_backup_files_toggled), frame);

    return frame;
}


void store_filter_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *hide_unknown_check = lookup_widget (dialog, "hide_unknown_check");
    GtkWidget *hide_regular_check = lookup_widget (dialog, "hide_regular_check");
    GtkWidget *hide_directory_check = lookup_widget (dialog, "hide_directory_check");
    GtkWidget *hide_special_check = lookup_widget (dialog, "hide_special_check");
    GtkWidget *hide_shortcut_check = lookup_widget (dialog, "hide_shortcut_check");
    GtkWidget *hide_mountable_check = lookup_widget (dialog, "hide_mountable_check");
    GtkWidget *hide_virtual_check = lookup_widget (dialog, "hide_virtual_check");
    GtkWidget *hide_volatile_check = lookup_widget (dialog, "hide_volatile_check");
    GtkWidget *hide_symlink_check = lookup_widget (dialog, "hide_symlink_check");
    GtkWidget *hide_hidden_check = lookup_widget (dialog, "hide_hidden_check");
    GtkWidget *hide_backup_check = lookup_widget (dialog, "hide_backup_check");
    GtkWidget *backup_pattern_entry = lookup_widget (dialog, "backup_pattern_entry");

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_UNKNOWN] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_unknown_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_REGULAR] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_regular_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_DIR] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_directory_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_SPECIAL] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_special_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_SHORTCUT] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_shortcut_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_MOUNTABLE] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_mountable_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_VIRTUAL] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_virtual_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_VOLATILE] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_volatile_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_SYMLINK] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_symlink_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_HIDDEN] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_hidden_check));

    cfg.filter.file_types[GnomeCmdData::G_FILE_IS_BACKUP] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_backup_check));

    cfg.set_backup_pattern(gtk_entry_get_text (GTK_ENTRY (backup_pattern_entry)));
}


/***********************************************************************
 *
 *  The Programs tab
 *
 **********************************************************************/

void add_app_to_list (GtkTreeView *view, GnomeCmdApp *app)
{
    GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
    GtkTreeIter iter;

    GnomeCmdPixmap *pm = gnome_cmd_app_get_pixmap (app);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        0, pm ? pm->pixbuf : nullptr,
                        1, (gchar *) gnome_cmd_app_get_name (app),
                        2, (gchar *) gnome_cmd_app_get_command (app),
                        3, app,
                        -1);
}


void update_app_in_list (GtkTreeView *view, GnomeCmdApp *app)
{
    GnomeCmdPixmap *pm = gnome_cmd_app_get_pixmap (app);

    GtkTreeModel *model = gtk_tree_view_get_model (view);
    GtkTreeIter iter;
    GnomeCmdApp *row_app;

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gtk_tree_model_get (model, &iter, 3, &row_app, -1);

            if (row_app == app)
            {
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                    0, pm ? pm->pixbuf : nullptr,
                                    1, (gchar *) gnome_cmd_app_get_name (app),
                                    -1);
                break;
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }
}


static void on_app_dialog_cancel (GtkButton *button, GtkWidget *dialog)
{
    gtk_widget_destroy (dialog);
}


static void on_some_files_toggled (GtkToggleButton *btn, GtkWidget *dialog)
{
    GtkWidget *pattern_entry = lookup_widget (dialog, "pattern_entry");

    if (gtk_toggle_button_get_active (btn))
    {
        gtk_widget_set_sensitive (pattern_entry, TRUE);
        gtk_widget_grab_focus (pattern_entry);
    }
    else
        gtk_widget_set_sensitive (pattern_entry, FALSE);
}


static void get_app_dialog_values (GtkWidget *dialog, gchar **name, gchar **cmd, gchar **icon_path,
                                   gint *target, gchar **pattern_string,
                                   gboolean *handles_uris, gboolean *handles_multiple,
                                   gboolean *requires_terminal)
{
    GtkWidget *name_entry = lookup_widget (dialog, "name_entry");
    GtkWidget *cmd_entry = lookup_widget (dialog, "cmd_entry");
    GtkWidget *iconWidget = lookup_widget (dialog, "icon_entry");
    GtkWidget *pattern_entry = lookup_widget (dialog, "pattern_entry");
    GtkWidget *target_files = lookup_widget (dialog, "show_for_all_files");
    GtkWidget *target_dirs = lookup_widget (dialog, "show_for_all_dirs");
    GtkWidget *target_dirs_and_files = lookup_widget (dialog, "show_for_all_dirs_and_files");
    GtkWidget *uris_check = lookup_widget (dialog, "handle_uris");
    GtkWidget *multiple_check = lookup_widget (dialog, "handle_multiple");
    GtkWidget *terminal_check = lookup_widget (dialog, "requires_terminal");

    *name = (gchar *) gtk_entry_get_text (GTK_ENTRY (name_entry));
    *cmd = (gchar *) gtk_entry_get_text (GTK_ENTRY (cmd_entry));
    // Get icon_path string
    g_object_get (G_OBJECT (gtk_button_get_image (GTK_BUTTON (iconWidget))), "file", icon_path, NULL);
    *pattern_string = NULL;
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (target_files)))
        *target = APP_TARGET_ALL_FILES;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (target_dirs)))
        *target = APP_TARGET_ALL_DIRS;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (target_dirs_and_files)))
        *target = APP_TARGET_ALL_DIRS_AND_FILES;
    else
    {
        *target = APP_TARGET_SOME_FILES;
        *pattern_string = (gchar *) gtk_entry_get_text (GTK_ENTRY (pattern_entry));
    }

    *handles_uris = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uris_check));
    *handles_multiple = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (multiple_check));
    *requires_terminal = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (terminal_check));
}


static void on_add_app_dialog_ok (GtkButton *button, GtkWidget *dialog)
{
    gint target;
    gboolean handles_uris, handles_multiple, requires_terminal;
    gchar *name, *cmd, *icon_path, *pattern_string;

    GtkWidget *options_dialog = lookup_widget (dialog, "options_dialog");
    GtkWidget *view = lookup_widget (options_dialog, "app_view");

    get_app_dialog_values (dialog, &name, &cmd, &icon_path,
                           &target, &pattern_string,
                           &handles_uris, &handles_multiple, &requires_terminal);
    if (!name || strlen (name) < 1) return;

    if (gnome_cmd_data.options.is_name_double(name))
    {
        gnome_cmd_show_message (GTK_WINDOW (dialog),
            _("An app with this label exists already.\nPlease choose another label."));
        return;
    }

    GnomeCmdApp *app = gnome_cmd_app_new_with_values (name, cmd, icon_path,
                                                      (AppTarget) target,
                                                      pattern_string,
                                                      handles_uris, handles_multiple, requires_terminal, nullptr);
    gnome_cmd_data.options.add_fav_app(app);
    add_app_to_list (GTK_TREE_VIEW (view), app);
    gtk_widget_destroy (dialog);

    g_free (icon_path);
}


static void on_edit_app_dialog_ok (GtkButton *button, GtkWidget *dialog)
{
    gint target;
    gboolean handles_uris, handles_multiple, requires_terminal;
    gchar *name, *cmd, *icon_path, *pattern_string;

    GtkWidget *options_dialog = lookup_widget (dialog, "options_dialog");
    GtkWidget *view = lookup_widget (options_dialog, "app_view");

    get_app_dialog_values (dialog, &name, &cmd, &icon_path,
                           &target, &pattern_string,
                           &handles_uris, &handles_multiple, &requires_terminal);
    if (!name || strlen (name) < 1) return;

    GnomeCmdApp *app = (GnomeCmdApp *) g_object_get_data (G_OBJECT (options_dialog), "selected_app");
    if (!app) return;

    gnome_cmd_app_set_name (app, name);
    gnome_cmd_app_set_command (app, cmd);
    gnome_cmd_app_set_icon_path (app, icon_path);
    gnome_cmd_app_set_target (app, (AppTarget) target);
    if (pattern_string)
        gnome_cmd_app_set_pattern_string (app, pattern_string);
    gnome_cmd_app_set_handles_uris (app, handles_uris);
    gnome_cmd_app_set_handles_multiple (app, handles_multiple);
    gnome_cmd_app_set_requires_terminal (app, requires_terminal);

    update_app_in_list (GTK_TREE_VIEW (view), app);
    gtk_widget_destroy (dialog);

    g_free (icon_path);
}


static GtkWidget *create_app_dialog (GnomeCmdApp *app, GtkSignalFunc on_ok, GtkSignalFunc on_cancel, GtkWidget *options_dialog)
{
    GtkWidget *vbox, *hbox, *table, *entry, *label, *cat, *radio, *check;
    const gchar *s = NULL;

    GtkWidget *dialog = gnome_cmd_dialog_new (NULL);
    g_object_ref (dialog);
    g_object_set_data (G_OBJECT (dialog), "options_dialog", options_dialog);

    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (options_dialog));

    hbox = create_hbox (dialog, FALSE, 6);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), hbox);
    vbox = create_vbox (dialog, FALSE, 6);
    gtk_container_add (GTK_CONTAINER (hbox), vbox);
    table = create_table (dialog, 3, 2);


    gtk_container_add (GTK_CONTAINER (vbox), table);

    label = create_label (dialog, _("Label:"));
    table_add (table, label, 0, 0, GTK_FILL);
    label = create_label (dialog, _("Command:"));
    table_add (table, label, 0, 1, GTK_FILL);
    label = create_label (dialog, _("Icon:"));
    table_add (table, label, 0, 2, GTK_FILL);

    if (app) s = gnome_cmd_app_get_name (app);
    entry = create_entry (dialog, "name_entry", s);
    table_add (table, entry, 1, 0, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    gtk_widget_grab_focus (entry);

    if (app) s = gnome_cmd_app_get_command (app);
    entry = create_entry (dialog, "cmd_entry", s);
    table_add (table, entry, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    s = gnome_cmd_app_get_icon_path (app);
    entry = create_icon_button_widget (dialog, "icon_entry", s);

    table_add (table, entry, 1, 2, GTK_FILL);


    table = create_table (dialog, 3, 1);
    cat = create_category (dialog, table, _("Options"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (dialog, _("Can handle multiple files"), "handle_multiple");
    if (app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (check), gnome_cmd_app_get_handles_multiple (app));
    table_add (table, check, 0, 0, GTK_FILL);
    check = create_check (dialog, _("Can handle URIs"), "handle_uris");
    if (app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (check), gnome_cmd_app_get_handles_uris (app));
    table_add (table, check, 0, 1, GTK_FILL);
    check = create_check (dialog, _("Requires terminal"), "requires_terminal");
    if (app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (check), gnome_cmd_app_get_requires_terminal (app));
    table_add (table, check, 0, 2, GTK_FILL);


    table = create_table (dialog, 4, 2);
    cat = create_category (dialog, table, _("Show for"));
    gtk_box_pack_start (GTK_BOX (hbox), cat, FALSE, TRUE, 0);

    radio = create_radio (dialog, NULL, _("All files"),
                          "show_for_all_files");
    if (app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (radio),
        gnome_cmd_app_get_target (app) == APP_TARGET_ALL_FILES);
    gtk_table_attach (GTK_TABLE (table), radio, 0, 2, 0, 1, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    radio = create_radio (dialog, get_radio_group (radio), _("All directories"), "show_for_all_dirs");
    if (app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (radio),
        gnome_cmd_app_get_target (app) == APP_TARGET_ALL_DIRS);
    gtk_table_attach (GTK_TABLE (table), radio, 0, 2, 1, 2, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    radio = create_radio (dialog, get_radio_group (radio), _("All directories and files"),
                          "show_for_all_dirs_and_files");
    if (app) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio),
                                           gnome_cmd_app_get_target (app) == APP_TARGET_ALL_DIRS_AND_FILES);
    gtk_table_attach (GTK_TABLE (table), radio, 0, 2, 2, 3, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    radio = create_radio (dialog, get_radio_group (radio), _("Some files"),
                          "show_for_some_files");
    if (app) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio),
                                           gnome_cmd_app_get_target (app) == APP_TARGET_SOME_FILES);
    gtk_table_attach (GTK_TABLE (table), radio, 0, 2, 3, 4, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    if (!app)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    g_signal_connect (radio, "toggled", G_CALLBACK (on_some_files_toggled), dialog);

    label = create_label (dialog, _("File patterns"));
    table_add (table, label, 0, 4, (GtkAttachOptions) 0);
    entry = create_entry (dialog, "pattern_entry", app ? gnome_cmd_app_get_pattern_string (app) : "*.ext1;*.ext2");
    table_add (table, entry, 1, 4, GTK_FILL);
    if (app && gnome_cmd_app_get_target (app) != APP_TARGET_SOME_FILES)
        gtk_widget_set_sensitive (entry, FALSE);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL,
                                 GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK,
                                 GTK_SIGNAL_FUNC (on_ok), dialog);

    gtk_widget_show (dialog);

    return dialog;
}


static void on_app_add (GtkWidget *button, GtkWidget *parent)
{
    GtkWidget *dialog = create_app_dialog (
        NULL, GTK_SIGNAL_FUNC (on_add_app_dialog_ok), GTK_SIGNAL_FUNC (on_app_dialog_cancel), parent);
    gtk_window_set_title (GTK_WINDOW (dialog), _("New Application"));
}


static void on_app_edit (GtkWidget *button, GtkWidget *parent)
{
    GnomeCmdApp *app = (GnomeCmdApp *) g_object_get_data (G_OBJECT (parent), "selected_app");
    if (app)
    {
        GtkWidget *dialog = create_app_dialog (app, GTK_SIGNAL_FUNC (on_edit_app_dialog_ok), GTK_SIGNAL_FUNC (on_app_dialog_cancel), parent);
        gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Application"));
    }
}


static void on_app_selection_changed (GtkTreeSelection *selection, GtkWidget *parent)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GnomeCmdApp *app = nullptr;

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
        gtk_tree_model_get (model, &iter, 3, &app, -1);

    g_object_set_data (G_OBJECT (parent), "selected_app", app);

    gtk_widget_set_sensitive (lookup_widget (parent, "remove_app_button"), app != nullptr);
    gtk_widget_set_sensitive (lookup_widget (parent, "edit_app_button"), app != nullptr);
}


static void on_app_reordered (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gint *new_order, GtkWidget *frame)
{
    gint row;

    for (row = 0; new_order[row] == row; row++); // find first difference in order

    GList *apps = gnome_cmd_data.options.fav_apps;

    if (!apps
        || MAX (row, new_order[row]) >= (gint) g_list_length (apps) // cast will only be problematic for incredibly large lists
        || MIN (row, new_order[row]) < 0)
        return;

    gpointer data = g_list_nth_data (apps, row);
    apps = g_list_remove (apps, data);

    apps = g_list_insert (apps, data, new_order[row]);

    gnome_cmd_data.options.set_fav_apps(apps);
}


static void on_app_remove (GtkWidget *button, GtkWidget *frame)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (frame, "app_view"));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), &model, &iter))
    {
        GnomeCmdApp *app;
        gtk_tree_model_get (model, &iter, 3, &app, -1);
        gnome_cmd_data.options.remove_fav_app(app);
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}


static void on_app_move_up (GtkWidget *button, GtkWidget *frame)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (frame, "app_view"));
    GtkTreeModel *model;
    GtkTreeIter iter, prev;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), &model, &iter))
    {
        prev = iter;
        if (gtk_tree_model_iter_previous (model, &prev))
            gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &prev);
    }
}


static void on_app_move_down (GtkWidget *button, GtkWidget *frame)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (frame, "app_view"));
    GtkTreeModel *model;
    GtkTreeIter iter, next;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), &model, &iter))
    {
        next = iter;
        if (gtk_tree_model_iter_next (model, &next))
            gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &next);
    }
}


static GtkWidget *create_programs_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *frame, *hbox, *scrolled_window, *vbox, *cat, *table1, *table2;
    GtkWidget *entry, *button, *label, *view, *bbox, *check;
    GtkWidget *separator;
    GtkListStore *store;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), vbox);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled_window))), GTK_SHADOW_NONE);

    check = create_check (parent, _("Always download remote files before opening in external programs"), "honor_expect_uris");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), !cfg.honor_expect_uris);
    cat = create_category (parent, check, _("MIME applications"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    table1 = create_table (parent, 6, 2);
    cat = create_category (parent, table1, _("Standard programs"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    label = create_label (parent, _("Viewer:"));
    table_add (table1, label, 0, 0, GTK_FILL);
    label = create_label (parent, _("Editor:"));
    table_add (table1, label, 0, 2, GTK_FILL);
    label = create_label (parent, _("Differ:"));
    table_add (table1, label, 0, 3, GTK_FILL);
    label = create_label (parent, _("Search:"));
    table_add (table1, label, 0, 4, GTK_FILL);
    label = create_label (parent, _("Send files:"));
    table_add (table1, label, 0, 5, GTK_FILL);
    label = create_label (parent, _("Terminal:"));
    table_add (table1, label, 0, 6, GTK_FILL);

    entry = create_entry (parent, "viewer", cfg.viewer);
    table_add (table1, entry, 1, 0, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    check = create_check (parent, _("Use Internal Viewer"), "use_internal_viewer");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.use_internal_viewer);
    table_add (table1, check, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    entry = create_entry (parent, "editor", cfg.editor);
    table_add (table1, entry, 1, 2, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    entry = create_entry (parent, "differ", cfg.differ);
    table_add (table1, entry, 1, 3, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    entry = create_entry (parent, "search", cfg.search);
    table_add (table1, entry, 1, 4, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    entry = create_entry (parent, "sendto", cfg.sendto);
    table_add (table1, entry, 1, 5, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    entry = create_entry (parent, "termopen", cfg.termopen);
    table_add (table1, entry, 1, 6, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    separator = gtk_separator_menu_item_new ();
    gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);

    //Other favorite apps frame

    hbox = create_hbox (parent, FALSE, 0);
    gtk_box_set_spacing (GTK_BOX (hbox), 12);
    cat = create_category (parent, hbox, _("Other favourite apps"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);


    store = gtk_list_store_new (4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    view = create_treeview (parent, "app_view", GTK_TREE_MODEL (store), 16,
                            GTK_SIGNAL_FUNC (on_app_selection_changed), GTK_SIGNAL_FUNC (on_app_reordered));
    create_treeview_column (view, 0, 20, "");
    create_treeview_column (view, 1, 100, _("Label"));
    create_treeview_column (view, 2, 150, _("Command"));
    gtk_box_pack_start (GTK_BOX (hbox), view, TRUE, TRUE, 0);

    bbox = create_vbuttonbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, TRUE, 0);

    button = create_stock_button (parent, GTK_STOCK_ADD, GTK_SIGNAL_FUNC (on_app_add));
    gtk_widget_set_can_default (button, TRUE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_EDIT, GTK_SIGNAL_FUNC (on_app_edit));
    g_object_set_data (G_OBJECT (parent), "edit_app_button", button);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_REMOVE, GTK_SIGNAL_FUNC (on_app_remove));
    g_object_set_data (G_OBJECT (parent), "remove_app_button", button);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_GO_UP, GTK_SIGNAL_FUNC (on_app_move_up));
    gtk_widget_set_can_default (button, TRUE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_GO_DOWN, GTK_SIGNAL_FUNC (on_app_move_down));
    gtk_widget_set_can_default (button, TRUE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    view = (GtkWidget *) g_object_get_data (G_OBJECT (parent), "app_view");
    for (GList *apps = gnome_cmd_data.options.fav_apps; apps; apps = apps->next)
        add_app_to_list (GTK_TREE_VIEW (view), (GnomeCmdApp *) apps->data);

    table2 = create_table (parent, 1, 3);
    cat = create_category (parent, table2, _("Global app options"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    label = create_label (parent, _("Terminal command for apps in the list above:"));
    table_add (table2, label, 0, 0, GTK_FILL);

    entry = create_entry (parent, "termexec", cfg.termexec);
    table_add (table2, entry, 0, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    check = create_check (parent, _("Leave terminal window open"), "is_use_gcmd_block");
    table_add (table2, check, 0, 2, GTK_FILL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.use_gcmd_block);

    return frame;
}


void store_programs_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *entry1 = lookup_widget (dialog, "viewer");
    GtkWidget *entry2 = lookup_widget (dialog, "editor");
    GtkWidget *entry3 = lookup_widget (dialog, "differ");
    GtkWidget *entry4 = lookup_widget (dialog, "search");
    GtkWidget *entry5 = lookup_widget (dialog, "sendto");
    GtkWidget *entry6 = lookup_widget (dialog, "termopen");
    GtkWidget *entry7 = lookup_widget (dialog, "termexec");
    GtkWidget *check_use_gcmd_block = lookup_widget (dialog, "is_use_gcmd_block");
    GtkWidget *check_uris = lookup_widget (dialog, "honor_expect_uris");
    GtkWidget *check_iv = lookup_widget (dialog, "use_internal_viewer");

    cfg.set_viewer(gtk_entry_get_text (GTK_ENTRY (entry1)));
    cfg.set_editor(gtk_entry_get_text (GTK_ENTRY (entry2)));
    cfg.set_differ(gtk_entry_get_text (GTK_ENTRY (entry3)));
    cfg.set_search(gtk_entry_get_text (GTK_ENTRY (entry4)));
    cfg.set_sendto(gtk_entry_get_text (GTK_ENTRY (entry5)));
    cfg.set_termopen(gtk_entry_get_text (GTK_ENTRY (entry6)));
    cfg.set_termexec(gtk_entry_get_text (GTK_ENTRY (entry7)));
    gnome_cmd_data.use_gcmd_block = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_use_gcmd_block));

    cfg.honor_expect_uris = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_uris));
    cfg.use_internal_viewer = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_iv));
}


/***********************************************************************
 *
 *  The Devices tab
 *
 **********************************************************************/

void add_device_to_list (GtkTreeView *view, GnomeCmdConDevice *dev)
{
    GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
    GtkTreeIter iter;

    GnomeCmdPixmap *pm = gnome_cmd_con_get_open_pixmap (GNOME_CMD_CON (dev));

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        0, pm ? pm->pixbuf : nullptr,
                        1, (gchar *) gnome_cmd_con_device_get_alias (dev),
                        2, dev,
                        -1);
}


void update_device_in_list (GtkTreeView *view, GnomeCmdConDevice *dev, gchar *alias, gchar *device_fn, gchar *mountp, gchar *icon_path)
{
    gnome_cmd_con_device_set_alias (dev, alias);
    gnome_cmd_con_device_set_device_fn (dev, device_fn);
    gnome_cmd_con_device_set_mountp (dev, mountp);
    gnome_cmd_con_device_set_icon_path (dev, icon_path);

    GnomeCmdPixmap *pm = gnome_cmd_con_get_open_pixmap (GNOME_CMD_CON (dev));

    GtkTreeModel *model = gtk_tree_view_get_model (view);
    GtkTreeIter iter;
    GnomeCmdConDevice *row_dev;

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gtk_tree_model_get (model, &iter, 2, &row_dev, -1);

            if (row_dev == dev)
            {
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                    0, pm ? pm->pixbuf : nullptr,
                                    1, alias,
                                    -1);
                break;
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }
}


static void on_device_dialog_cancel (GtkButton *button, GtkWidget *dialog)
{
    gtk_widget_destroy (dialog);
}


void get_device_dialog_values (GtkWidget *dialog, gchar **alias, gchar **device_utf8,
    gchar **mountp_utf8, gchar **icon_path)
{
    GtkWidget *alias_entry = lookup_widget (dialog, "alias_entry");
    GtkWidget *device_entry = lookup_widget (dialog, "device_entry");
    GtkWidget *mountp_entry = lookup_widget (dialog, "mountp_entry");
    GtkWidget *iconWidget = lookup_widget (dialog, "device_iconentry");

    gchar* device = (gchar *) gtk_entry_get_text (GTK_ENTRY (device_entry));
    gchar* mountp = (gchar *) gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (mountp_entry));

    *alias = (gchar *) gtk_entry_get_text (GTK_ENTRY (alias_entry));
    if (device && strlen(device) > 0)
        *device_utf8 = g_filename_to_utf8(device, -1, nullptr, nullptr, nullptr);
    if (mountp && strlen(mountp) > 0)
        *mountp_utf8 = g_filename_to_utf8(mountp, -1, nullptr, nullptr, nullptr);
    // Get device_iconentry path
    g_object_get (G_OBJECT (gtk_button_get_image (GTK_BUTTON (iconWidget))), "file", icon_path, NULL);

    g_free(mountp);
}


static void on_add_device_dialog_ok (GtkButton *button, GtkWidget *dialog)
{
    gchar *alias = nullptr;
    gchar *device = nullptr;
    gchar *mountp = nullptr;
    gchar *icon_path = nullptr;

    GtkWidget *options_dialog = lookup_widget (dialog, "options_dialog");
    GtkWidget *view = lookup_widget (options_dialog, "device_view");

    get_device_dialog_values (dialog, &alias, &device, &mountp, &icon_path);
    if ((!alias || strlen (alias) < 1) ||
        (!device || strlen(device) < 1) ||
        (!mountp || strlen(mountp) < 1)) return;

    GnomeCmdConDevice *dev = gnome_cmd_con_device_new (alias, device, mountp, icon_path);
    add_device_to_list (GTK_TREE_VIEW (view), GNOME_CMD_CON_DEVICE (dev));
    gtk_widget_destroy (dialog);

    gnome_cmd_con_list_get()->add(dev);

    g_free (device);
    g_free (mountp);
    g_free (icon_path);
}


static void on_edit_device_dialog_ok (GtkButton *button, GtkWidget *dialog)
{
    gchar *alias, *device, *mountp, *icon_path;

    GtkWidget *options_dialog = lookup_widget (dialog, "options_dialog");
    GtkWidget *view = lookup_widget (options_dialog, "device_view");

    get_device_dialog_values (dialog, &alias, &device, &mountp, &icon_path);
    if ((!alias || strlen (alias) < 1) ||
        (!device || strlen(device) < 1) ||
        (!mountp || strlen(mountp) < 1)) return;

    GnomeCmdConDevice *dev = GNOME_CMD_CON_DEVICE (g_object_get_data (G_OBJECT (options_dialog), "selected_device"));
    if (!dev) return;

    update_device_in_list (GTK_TREE_VIEW (view), dev, alias, device, mountp, icon_path);
    gtk_widget_destroy (dialog);

    g_free (device);
    g_free (mountp);
    g_free (icon_path);
}


static void on_dialog_help (GtkButton *button,  GtkWidget *unused)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-prefs-devices");
}

static GtkWidget *create_device_dialog (GnomeCmdConDevice *dev, GtkSignalFunc on_ok, GtkSignalFunc on_cancel, GtkWidget *options_dialog)
{
    GtkWidget *table, *entry, *label;
    GtkWidget *dialog;
    const gchar *s = NULL;

    dialog = gnome_cmd_dialog_new ("");
    g_object_ref (dialog);
    gtk_window_set_title (GTK_WINDOW (dialog), "");
    g_object_set_data (G_OBJECT (dialog), "options_dialog", options_dialog);

    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (options_dialog));

    table = create_table (dialog, 4, 2);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), table);

    label = create_label (dialog, _("Alias:"));
    table_add (table, label, 0, 0, GTK_FILL);
    label = create_label (dialog, _("Device/Label:"));
    table_add (table, label, 0, 1, GTK_FILL);
    label = create_label (dialog, _("Mount point:"));
    table_add (table, label, 0, 2, GTK_FILL);
    label = create_label (dialog, _("Icon:"));
    table_add (table, label, 0, 3, GTK_FILL);

    if (dev) s = gnome_cmd_con_device_get_alias (dev);
    entry = create_entry (dialog, "alias_entry", s);
    table_add (table, entry, 1, 0, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    gtk_widget_grab_focus (entry);

    if (dev) s = gnome_cmd_con_device_get_device_fn (dev);
    entry = create_entry (dialog, "device_entry", s);
    table_add (table, entry, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    if (dev) s = gnome_cmd_con_device_get_mountp_string (dev);
    entry = create_directory_chooser_button (dialog, "mountp_entry", s);
    table_add (table, entry, 1, 2, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    if (dev) s = gnome_cmd_con_device_get_icon_path (dev);
    entry = create_icon_button_widget (dialog, "device_iconentry", s);

    table_add (table, entry, 1, 3, GTK_FILL);

    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), GTK_STOCK_HELP,
        GTK_SIGNAL_FUNC (on_dialog_help), nullptr);
    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL,
        GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK,
        GTK_SIGNAL_FUNC (on_ok), dialog);

    gtk_widget_show (dialog);

    return dialog;
}


static void on_device_add (GtkWidget *button, GtkWidget *parent)
{
    GtkWidget *dialog = create_device_dialog (
        NULL, GTK_SIGNAL_FUNC (on_add_device_dialog_ok),
        GTK_SIGNAL_FUNC (on_device_dialog_cancel), parent);
    gtk_window_set_title (GTK_WINDOW (dialog), _("New Device"));
}


static void on_device_edit (GtkWidget *button, GtkWidget *parent)
{
    GnomeCmdConDevice *dev = GNOME_CMD_CON_DEVICE (g_object_get_data (G_OBJECT (parent), "selected_device"));

    if (dev)
    {
        GtkWidget *dialog = create_device_dialog (
            dev, GTK_SIGNAL_FUNC (on_edit_device_dialog_ok),
            GTK_SIGNAL_FUNC (on_device_dialog_cancel), parent);
        gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Device"));
    }
}


static void on_device_remove (GtkWidget *button, GtkWidget *frame)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (frame, "device_view"));
    GtkTreeModel *model;
    GtkTreeIter iter;
    GnomeCmdConDevice *dev;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 2, &dev, -1);
        gnome_cmd_con_list_get()->remove(dev);
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}


static void on_device_selection_changed (GtkTreeSelection *selection, GtkWidget *parent)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GnomeCmdConDevice *dev = nullptr;

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
        gtk_tree_model_get (model, &iter, 2, &dev, -1);

    g_object_set_data (G_OBJECT (parent), "selected_device", dev);

    gtk_widget_set_sensitive (lookup_widget (parent, "remove_device_button"), dev != nullptr);
    gtk_widget_set_sensitive (lookup_widget (parent, "edit_device_button"), dev != nullptr);
}


static void on_device_reordered (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gint *new_order, GtkWidget *frame)
{
    gint row;

    for (row = 0; new_order[row] == row; row++); // find first difference in order

    GList *list = gnome_cmd_con_list_get_all_dev (gnome_cmd_con_list_get ());

    if (!list
        || MAX (row, new_order[row]) >= (gint) g_list_length (list) // cast will only be problematic for incredibly large lists
        || MIN (row, new_order[row]) < 0)
        return;

    gpointer data = g_list_nth_data (list, row);
    list = g_list_remove (list, data);

    list = g_list_insert (list, data, new_order[row]);

    gnome_cmd_con_list_set_all_dev (gnome_cmd_con_list_get (), list);
}


static void on_device_move_up (GtkWidget *button, GtkWidget *frame)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (frame, "device_view"));
    GtkTreeModel *model;
    GtkTreeIter iter, prev;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), &model, &iter))
    {
        prev = iter;
        if (gtk_tree_model_iter_previous (model, &prev))
            gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &prev);
    }
}


static void on_device_move_down (GtkWidget *button, GtkWidget *frame)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (frame, "device_view"));
    GtkTreeModel *model;
    GtkTreeIter iter, next;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), &model, &iter))
    {
        next = iter;
        if (gtk_tree_model_iter_next (model, &next))
            gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &next);
    }
}


static GtkWidget *create_devices_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *frame, *hbox, *scrolled_window, *vbox, *cat, *cat_box;
    GtkWidget *button, *view, *bbox, *check;
    GtkListStore *store;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), vbox);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled_window))), GTK_SHADOW_NONE);

    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Devices"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    hbox = create_hbox (parent, FALSE, 0);
    gtk_box_set_spacing (GTK_BOX (hbox), 12);
    gtk_container_add (GTK_CONTAINER (cat_box), hbox);

    store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
    view = create_treeview (parent, "device_view", GTK_TREE_MODEL (store), 24,
                            GTK_SIGNAL_FUNC (on_device_selection_changed), GTK_SIGNAL_FUNC (on_device_reordered));
    create_treeview_column (view, 0, 26, "");
    create_treeview_column (view, 1, 40, _("Alias"));
    gtk_box_pack_start (GTK_BOX (hbox), view, TRUE, TRUE, 0);

    bbox = create_vbuttonbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, TRUE, 0);

    button = create_stock_button (parent, GTK_STOCK_ADD, GTK_SIGNAL_FUNC (on_device_add));
    gtk_widget_set_can_default (button, TRUE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_EDIT, GTK_SIGNAL_FUNC (on_device_edit));
    g_object_set_data (G_OBJECT (parent), "edit_device_button", button);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_REMOVE, GTK_SIGNAL_FUNC (on_device_remove));
    g_object_set_data (G_OBJECT (parent), "remove_device_button", button);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_GO_UP, GTK_SIGNAL_FUNC (on_device_move_up));
    gtk_widget_set_can_default (button, TRUE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_GO_DOWN, GTK_SIGNAL_FUNC (on_device_move_down));
    gtk_widget_set_can_default (button, TRUE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

#ifdef HAVE_SAMBA
    check = create_check (parent, _("Show Samba workgroups button\n(Needs program restart if altered)"), "samba_workgroups_button");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.show_samba_workgroups_button);
#endif

    check = create_check (parent, _("Show only the icons"), "device_only_icon");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cfg.device_only_icon);

    view = (GtkWidget *) g_object_get_data (G_OBJECT (parent), "device_view");
    for (GList *devices = gnome_cmd_con_list_get_all_dev (gnome_cmd_con_list_get ()); devices; devices = devices->next)
        if (!gnome_cmd_con_device_get_autovol ((GnomeCmdConDevice *) devices->data))
            add_device_to_list (GTK_TREE_VIEW (view), GNOME_CMD_CON_DEVICE (devices->data));

    return frame;
}


void store_devices_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *device_only_icon = lookup_widget (dialog, "device_only_icon");
    cfg.device_only_icon = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (device_only_icon));

#ifdef HAVE_SAMBA
    GtkWidget *samba_workgroups_button = lookup_widget (dialog, "samba_workgroups_button");
    cfg.show_samba_workgroups_button = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (samba_workgroups_button));
#endif
}


static void response_callback (GtkDialog *dialog, int response_id, GnomeCmdNotebook *notebook)
{
    static const char *help_id[] = {"gnome-commander-prefs-general",
                                    "gnome-commander-prefs-format",
                                    "gnome-commander-prefs-layout",
                                    "gnome-commander-prefs-tabs",
                                    "gnome-commander-prefs-confirmation",
                                    "gnome-commander-prefs-filters",
                                    "gnome-commander-prefs-programs",
                                    "gnome-commander-prefs-devices"};

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", help_id[notebook->get_current_page()]);
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        default:
            g_assert_not_reached ();
    }
}


gboolean gnome_cmd_options_dialog (GtkWindow *parent, GnomeCmdData::Options &cfg)
{
    // variable for storing the last active tab
    static gint activetab = 0;

    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Options"), parent,
                                                     GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                     NULL);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
    gtk_window_set_default_size(GTK_WINDOW (dialog), gnome_cmd_data.opts_dialog_width, gnome_cmd_data.opts_dialog_height);

    // HIG defaults
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (content_area), 2);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    gtk_box_set_spacing (GTK_BOX (content_area),6);

    GnomeCmdNotebook *notebook = new GnomeCmdNotebook(GnomeCmdNotebook::SHOW_TABS);

    gtk_container_add (GTK_CONTAINER (content_area), *notebook);

    notebook->append_page(create_general_tab (dialog, cfg), _("General"));
    notebook->append_page(create_format_tab (dialog, cfg), _("Format"));
    notebook->append_page(create_layout_tab (dialog, cfg), _("Layout"));
    notebook->append_page(create_tabs_tab (dialog, cfg), _("Tabs"));
    notebook->append_page(create_confirmation_tab (dialog, cfg), _("Confirmation"));
    notebook->append_page(create_filter_tab (dialog, cfg), _("Filters"));
    notebook->append_page(create_programs_tab (dialog, cfg), _("Programs"));
    notebook->append_page(create_devices_tab (dialog, cfg), _("Devices"));

    // open the tab which was actinve when closing the options notebook last time
    notebook->set_current_page (activetab);

    gtk_widget_show_all (content_area);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), notebook);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    if (result==GTK_RESPONSE_OK)
    {
        store_general_options (dialog, cfg);
        store_format_options (dialog, cfg);
        store_layout_options (dialog, cfg);
        store_tabs_options (dialog, cfg);
        store_confirmation_options (dialog, cfg);
        store_filter_options (dialog, cfg);
        store_programs_options (dialog, cfg);
        store_devices_options (dialog, cfg);
    }

    GtkAllocation dialog_allocation;
    gtk_widget_get_allocation (dialog, &dialog_allocation);

    gnome_cmd_data.opts_dialog_width = dialog_allocation.width;
    gnome_cmd_data.opts_dialog_height = dialog_allocation.height;

    // store the current active tab
    activetab = notebook->get_current_page();

    gtk_widget_destroy (dialog);

    return result==GTK_RESPONSE_OK;
}
