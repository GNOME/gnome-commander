/**
 * @file gnome-cmd-options-dialog.cc
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

#include "gnome-cmd-includes.h"
#include "utils.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"
#include "widget-factory.h"

using namespace std;

extern "C" GtkWidget *create_general_tab (GtkWidget *parent, GnomeCmdData::Options &cfg);
extern "C" GtkWidget *create_format_tab (GtkWidget *parent, GnomeCmdData::Options &cfg);
extern "C" void store_format_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);
extern "C" void store_general_options (GtkWidget *dialog, GnomeCmdData::Options &cfg);

static void on_save_tabs_toggled (GtkCheckButton *check_button, GtkWidget *dialog)
{
    GtkWidget *check = lookup_widget (dialog, "save_dirs");

    gtk_widget_set_sensitive (check, !gtk_check_button_get_active (check_button));
}


/***********************************************************************
 *
 *  The General tab
 *
 **********************************************************************/

GtkWidget *create_general_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *vbox, *cat, *cat_box;
    GtkWidget *radio, *check;

    vbox = create_tabvbox (parent);

    GtkWidget *scrolled_window;
    /* create a new scrolled window. */
    scrolled_window = gtk_scrolled_window_new ();
    /* the policy is one of GTK_POLICY AUTOMATIC, or GTK_POLICY_ALWAYS.
     * GTK_POLICY_AUTOMATIC will automatically decide whether you need
     * scrollbars, whereas GTK_POLICY_ALWAYS will always leave the scrollbars
     * there.  The first one is the horizontal scrollbar, the second,
     * the vertical. */
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_widget_set_hexpand (scrolled_window, TRUE);
    gtk_widget_set_vexpand (scrolled_window, TRUE);
    gtk_widget_set_margin_top (scrolled_window, 6);
    gtk_widget_set_margin_bottom (scrolled_window, 6);
    gtk_widget_set_margin_start (scrolled_window, 6);
    gtk_widget_set_margin_end (scrolled_window, 6);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), vbox);

    // Left mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Left mouse button"));
    gtk_box_append (GTK_BOX (vbox), cat);

    radio = create_radio (parent, NULL, _("Single click to open items"), "lmb_singleclick_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
    radio = create_radio (parent, radio, _("Double click to open items"), "lmb_doubleclick_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);

    check = create_check (parent, _("Single click unselects files"), "lmb_unselects_check");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.left_mouse_button_unselects);


    // Middle mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Middle mouse button"));
    gtk_box_append (GTK_BOX (vbox), cat);

    radio = create_radio (parent, NULL, _("Up one directory"), "mmb_cd_up_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.middle_mouse_button_mode == GnomeCmdData::MIDDLE_BUTTON_GOES_UP_DIR)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
    radio = create_radio (parent, radio, _("Opens new tab"), "mmb_new_tab_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.middle_mouse_button_mode == GnomeCmdData::MIDDLE_BUTTON_OPENS_NEW_TAB)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);


    // Right mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Right mouse button"));
    gtk_box_append (GTK_BOX (vbox), cat);

    radio = create_radio (parent, NULL, _("Shows popup menu"), "rmb_popup_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_POPUPS_MENU)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
    radio = create_radio (parent, radio, _("Selects files"), "rmb_sel_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);


    // Delete options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Deletion"));
    gtk_box_append (GTK_BOX (vbox), cat);

    check = create_check (parent, _("Move to trash"), "delete_to_trash");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.deleteToTrash);


    // Selection options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Selection"));
    gtk_box_append (GTK_BOX (vbox), cat);

    check = create_check (parent, _("Select directories"), "select_dirs");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.select_dirs);


    // Sort options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Sorting/Quick search"));
    gtk_box_append (GTK_BOX (vbox), cat);

    check = create_check (parent, _("Case sensitive"), "case_sens_check");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.case_sens_sort);

    check = create_check (
                parent,
                _("Treat symbolic links as regular files"),
                "symbolic_links_as_regular_files_check"
            );
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.symbolic_links_as_regular_files);

    // Quick search options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Quick search"));
    gtk_box_append (GTK_BOX (vbox), cat);

    radio = create_radio (parent, NULL, _("CTRL+ALT+letters"), "ctrl_alt_quick_search");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.quick_search == GNOME_CMD_QUICK_SEARCH_CTRL_ALT)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
    radio = create_radio (parent, radio, _("ALT+letters (menu access with F12)"), "alt_quick_search");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.quick_search == GNOME_CMD_QUICK_SEARCH_ALT)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
    radio = create_radio (parent, radio, _("Just letters (command line access with CTRL+ALT+C)"), "quick_search");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.quick_search == GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);

    check = create_check (parent, _("Match beginning of the file name"), "qsearch_exact_match_begin");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.quick_search_exact_match_begin);

    check = create_check (parent, _("Match end of the file name"), "qsearch_exact_match_end");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.quick_search_exact_match_end);

    // Search window options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Search Window"));
    gtk_box_append (GTK_BOX (vbox), cat);

    check = create_check (parent, _("Search window is minimizable\n(Needs program restart if altered)"), "search_window_transient");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), !cfg.search_window_is_transient);

    // Multiple instances
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Multiple instances"));
    gtk_box_append (GTK_BOX (vbox), cat);

    check = create_check (parent, _("Don’t start a new instance"), "multiple_instance_check");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), !cfg.allow_multiple_instances);

    // Save on exit
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Save on exit"));
    gtk_box_append (GTK_BOX (vbox), cat);

    check = create_check (parent, _("Directories"), "save_dirs");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.save_dirs_on_exit);

    check = create_check (parent, _("Tabs"), "save_tabs");
    gtk_box_append (GTK_BOX (cat_box), check);
    g_signal_connect (check, "toggled", G_CALLBACK (on_save_tabs_toggled), parent);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.save_tabs_on_exit);

    check = create_check (parent, _("Directory history"), "save_dir_history");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.save_dir_history_on_exit);

    check = create_check (parent, _("Commandline history"), "save_cmdline_history");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.save_cmdline_history_on_exit);

    check = create_check (parent, _("Search history"), "save_search_history");
    gtk_box_append (GTK_BOX (cat_box), check);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), cfg.save_search_history_on_exit);

    return scrolled_window;
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
    GtkWidget *symbolic_links_as_regular_files_check = lookup_widget (dialog, "symbolic_links_as_regular_files_check");
    GtkWidget *ctrl_alt_quick_search = lookup_widget (dialog, "ctrl_alt_quick_search");
    GtkWidget *alt_quick_search = lookup_widget (dialog, "alt_quick_search");
    GtkWidget *multiple_instance_check = lookup_widget (dialog, "multiple_instance_check");
    GtkWidget *qsearch_exact_match_begin = lookup_widget (dialog, "qsearch_exact_match_begin");
    GtkWidget *qsearch_exact_match_end = lookup_widget (dialog, "qsearch_exact_match_end");
    GtkWidget *search_window_transient = lookup_widget (dialog, "search_window_transient");
    GtkWidget *save_dirs = lookup_widget (dialog, "save_dirs");
    GtkWidget *save_tabs = lookup_widget (dialog, "save_tabs");
    GtkWidget *save_dir_history = lookup_widget (dialog, "save_dir_history");
    GtkWidget *save_cmdline_history = lookup_widget (dialog, "save_cmdline_history");
    GtkWidget *save_search_history = lookup_widget (dialog, "save_search_history");

    cfg.left_mouse_button_mode = gtk_check_button_get_active (GTK_CHECK_BUTTON (lmb_singleclick_radio)) ? GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK : GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK;

    cfg.left_mouse_button_unselects = gtk_check_button_get_active (GTK_CHECK_BUTTON (lmb_unselects_check));

    cfg.middle_mouse_button_mode = gtk_check_button_get_active (GTK_CHECK_BUTTON (mmb_cd_up_radio)) ? GnomeCmdData::MIDDLE_BUTTON_GOES_UP_DIR
                                                                                                      : GnomeCmdData::MIDDLE_BUTTON_OPENS_NEW_TAB;

    cfg.right_mouse_button_mode = gtk_check_button_get_active (GTK_CHECK_BUTTON (rmb_popup_radio)) ? GnomeCmdData::RIGHT_BUTTON_POPUPS_MENU
                                                                                                     : GnomeCmdData::RIGHT_BUTTON_SELECTS;

    cfg.select_dirs = gtk_check_button_get_active (GTK_CHECK_BUTTON (select_dirs));
    cfg.deleteToTrash = gtk_check_button_get_active (GTK_CHECK_BUTTON (delete_to_trash));
    cfg.case_sens_sort = gtk_check_button_get_active (GTK_CHECK_BUTTON (case_sens_check));
    cfg.symbolic_links_as_regular_files = gtk_check_button_get_active (GTK_CHECK_BUTTON (symbolic_links_as_regular_files_check));
    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (ctrl_alt_quick_search)))
        cfg.quick_search = GNOME_CMD_QUICK_SEARCH_CTRL_ALT;
    else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (alt_quick_search)))
        cfg.quick_search = GNOME_CMD_QUICK_SEARCH_ALT;
    else
        cfg.quick_search = GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER;
    cfg.allow_multiple_instances = !gtk_check_button_get_active (GTK_CHECK_BUTTON (multiple_instance_check));
    cfg.quick_search_exact_match_begin = gtk_check_button_get_active (GTK_CHECK_BUTTON (qsearch_exact_match_begin));
    cfg.quick_search_exact_match_end = gtk_check_button_get_active (GTK_CHECK_BUTTON (qsearch_exact_match_end));
    cfg.search_window_is_transient = !gtk_check_button_get_active (GTK_CHECK_BUTTON (search_window_transient));
    cfg.save_dirs_on_exit = gtk_check_button_get_active (GTK_CHECK_BUTTON (save_dirs));
    cfg.save_tabs_on_exit = gtk_check_button_get_active (GTK_CHECK_BUTTON (save_tabs));
    cfg.save_dir_history_on_exit = gtk_check_button_get_active (GTK_CHECK_BUTTON (save_dir_history));
    cfg.save_cmdline_history_on_exit = gtk_check_button_get_active (GTK_CHECK_BUTTON (save_cmdline_history));
    cfg.save_search_history_on_exit = gtk_check_button_get_active (GTK_CHECK_BUTTON (save_search_history));
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

    const char *format = gtk_editable_get_text (GTK_EDITABLE (format_entry));
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


GtkWidget *create_format_tab (GtkWidget *parent, GnomeCmdData::Options &cfg)
{
    GtkWidget *scrolled_window, *vbox, *cat, *cat_box, *grid;
    GtkWidget *radio, *label, *entry;

    vbox = create_tabvbox (parent);

    scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_widget_set_hexpand (scrolled_window, TRUE);
    gtk_widget_set_vexpand (scrolled_window, TRUE);
    gtk_widget_set_margin_top (scrolled_window, 6);
    gtk_widget_set_margin_bottom (scrolled_window, 6);
    gtk_widget_set_margin_start (scrolled_window, 6);
    gtk_widget_set_margin_end (scrolled_window, 6);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), vbox);

    // Size display mode
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Size display mode"));
    gtk_box_append (GTK_BOX (vbox), cat);

    // Translators: 'Powered' refers to the mode of file size display (here - display using units of data: kB, MB, GB, ...)
    radio = create_radio (parent, NULL, _("Powered"), "size_powered_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_POWERED)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);

    // Translators: '<locale>' refers to the mode of file size display (here - use current locale settings)
    radio = create_radio (parent, radio, _("<locale>"), "size_locale_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_LOCALE)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);

    radio = create_radio (parent, radio, _("Grouped"), "size_grouped_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_GROUPED)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);

    radio = create_radio (parent, radio, _("Plain"), "size_plain_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_PLAIN)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);


    // Permission display mode
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Permission display mode"));
    gtk_box_append (GTK_BOX (vbox), cat);

    radio = create_radio (parent, NULL, _("Text (rw-r--r--)"), "perm_text_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.perm_disp_mode == GNOME_CMD_PERM_DISP_MODE_TEXT)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);

    radio = create_radio (parent, radio, _("Number (644)"), "perm_num_radio");
    gtk_box_append (GTK_BOX (cat_box), radio);
    if (cfg.perm_disp_mode == GNOME_CMD_PERM_DISP_MODE_NUMBER)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);


    // Date options
    grid = create_grid (parent);
    cat = create_category (parent, grid, _("Date format"));
    gtk_box_append (GTK_BOX (vbox), cat);

    label = create_label (parent, _("Format:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    gchar *utf8_date_format = g_locale_to_utf8 (cfg.date_format, -1, NULL, NULL, NULL);
    entry = create_entry (parent, "date_format_entry", utf8_date_format);
    g_free (utf8_date_format);
    gtk_widget_grab_focus (entry);
    g_signal_connect (entry, "realize", G_CALLBACK (on_date_format_update), parent);
    g_signal_connect (entry, "changed", G_CALLBACK (on_date_format_update), parent);
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

    label = create_label (parent, _("Test result:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

    label = create_label (parent, "");
    g_object_set_data (G_OBJECT (parent), "date_format_test_label", label);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

    label = create_label (parent, _("See the help page in the documentation on how to set the format string."));
    gtk_label_set_wrap (GTK_LABEL (label), TRUE);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);

    return scrolled_window;
}


void store_format_options (GtkWidget *dialog, GnomeCmdData::Options &cfg)
{
    GtkWidget *size_powered_radio = lookup_widget (dialog, "size_powered_radio");
    GtkWidget *size_locale_radio = lookup_widget (dialog, "size_locale_radio");
    GtkWidget *size_grouped_radio = lookup_widget (dialog, "size_grouped_radio");
    GtkWidget *perm_text_radio = lookup_widget (dialog, "perm_text_radio");
    GtkWidget *entry = lookup_widget (dialog, "date_format_entry");
    const gchar *format = gtk_editable_get_text (GTK_EDITABLE (entry));

    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (size_powered_radio)))
        cfg.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_POWERED;
    else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (size_locale_radio)))
        cfg.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_LOCALE;
    else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (size_grouped_radio)))
        cfg.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_GROUPED;
    else
        cfg.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_PLAIN;

    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (perm_text_radio)))
        cfg.perm_disp_mode = GNOME_CMD_PERM_DISP_MODE_TEXT;
    else
        cfg.perm_disp_mode = GNOME_CMD_PERM_DISP_MODE_NUMBER;

    cfg.set_date_format(g_locale_from_utf8 (format, -1, NULL, NULL, NULL));
}
