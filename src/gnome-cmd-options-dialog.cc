/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

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
#include "gnome-cmd-options-dialog.h"
#include "utils.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-con-list.h"

using namespace std;


inline GtkWidget *create_font_picker (GtkWidget *parent, gchar *name)
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


/***********************************************************************
 *
 *  The General tab
 *
 **********************************************************************/

static GtkWidget *create_general_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat, *cat_box;
    GtkWidget *radio, *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);


    // Left mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Left mouse button"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Single click to open items"), "lmb_singleclick_radio");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Double click to open items"), "lmb_doubleclick_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    check = create_check (parent, _("Single click unselects files"), "lmb_unselects_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.left_mouse_button_unselects);


    // Middle mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Middle mouse button"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Up one directory"), "mmb_cd_up_radio");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (gnome_cmd_data.middle_mouse_button_mode == GnomeCmdData::MIDDLE_BUTTON_GOES_UP_DIR)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Opens new tab"), "mmb_new_tab_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.middle_mouse_button_mode == GnomeCmdData::MIDDLE_BUTTON_OPENS_NEW_TAB)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // Right mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Right mouse button"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Shows popup menu"), "rmb_popup_radio");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (gnome_cmd_data.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_POPUPS_MENU)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Selects files"), "rmb_sel_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // Sort options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Sorting options"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Case sensitive"), "case_sens_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.case_sens_sort);


    // Quick search options
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Quick search"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("CTRL+ALT+letters"), "ctrl_alt_quick_search");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), !gnome_cmd_data.alt_quick_search);
    radio = create_radio (parent, get_radio_group (radio), _("ALT+letters (menu access with F10)"), "alt_quick_search");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), gnome_cmd_data.alt_quick_search);

    check = create_check (parent, _("Match beginning of the file name"), "qsearch_exact_match_begin");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.quick_search_exact_match_begin);

    check = create_check (parent, _("Match end of the file name"), "qsearch_exact_match_end");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.quick_search_exact_match_end);


    // Multiple instances
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Multiple instances"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Don't start a new instance"), "multiple_instance_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), !gnome_cmd_data.allow_multiple_instances);


    // Save on exit
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Save on exit"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Directories"), "save_dirs");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.save_dirs_on_exit);

    check = create_check (parent, _("Tabs"), "save_tabs");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    g_signal_connect (check, "toggled", G_CALLBACK (on_save_tabs_toggled), parent);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.save_tabs_on_exit);


    return frame;
}


inline void store_general_options (GtkWidget *dialog)
{
    GtkWidget *lmb_singleclick_radio = lookup_widget (dialog, "lmb_singleclick_radio");
    GtkWidget *lmb_unselects_check = lookup_widget (dialog, "lmb_unselects_check");
    GtkWidget *mmb_cd_up_radio = lookup_widget (dialog, "mmb_cd_up_radio");
    GtkWidget *rmb_popup_radio = lookup_widget (dialog, "rmb_popup_radio");
    GtkWidget *case_sens_check = lookup_widget (dialog, "case_sens_check");
    GtkWidget *alt_quick_search = lookup_widget (dialog, "alt_quick_search");
    GtkWidget *multiple_instance_check = lookup_widget (dialog, "multiple_instance_check");
    GtkWidget *qsearch_exact_match_begin = lookup_widget (dialog, "qsearch_exact_match_begin");
    GtkWidget *qsearch_exact_match_end = lookup_widget (dialog, "qsearch_exact_match_end");
    GtkWidget *save_dirs = lookup_widget (dialog, "save_dirs");
    GtkWidget *save_tabs = lookup_widget (dialog, "save_tabs");

    gnome_cmd_data.left_mouse_button_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lmb_singleclick_radio)) ? GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK : GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK;

    gnome_cmd_data.left_mouse_button_unselects = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lmb_unselects_check));

    gnome_cmd_data.middle_mouse_button_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mmb_cd_up_radio)) ? GnomeCmdData::MIDDLE_BUTTON_GOES_UP_DIR
                                                                                                                 : GnomeCmdData::MIDDLE_BUTTON_OPENS_NEW_TAB;

    gnome_cmd_data.right_mouse_button_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rmb_popup_radio)) ? GnomeCmdData::RIGHT_BUTTON_POPUPS_MENU
                                                                                                                : GnomeCmdData::RIGHT_BUTTON_SELECTS;

    gnome_cmd_data.case_sens_sort = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (case_sens_check));
    gnome_cmd_data.alt_quick_search = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (alt_quick_search));
    gnome_cmd_data.allow_multiple_instances = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (multiple_instance_check));
    gnome_cmd_data.quick_search_exact_match_begin = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (qsearch_exact_match_begin));
    gnome_cmd_data.quick_search_exact_match_end = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (qsearch_exact_match_end));
    gnome_cmd_data.save_dirs_on_exit = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (save_dirs));
    gnome_cmd_data.save_tabs_on_exit = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (save_tabs));
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
    strftime (s, sizeof(s), locale_format, localtime (&t));
    gchar *utf8_str = g_locale_to_utf8 (s, -1, NULL, NULL, NULL);

    gtk_label_set_text (GTK_LABEL (test_label), utf8_str);
    g_free (utf8_str);
    g_free (locale_format);
}


static GtkWidget *create_format_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat, *cat_box, *table;
    GtkWidget *radio, *label, *entry;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    // Size display mode
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Size display mode"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    // Translators: 'Powered' refers to the mode of file size display (here - display using units of data: kB, MB, GB, ...)
    radio = create_radio (parent, NULL, _("Powered"), "size_powered_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_POWERED)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    // Translators: '<locale>' refers to the mode of file size display (here - use current locale settings)
    radio = create_radio (parent, get_radio_group (radio), _("<locale>"), "size_locale_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_LOCALE)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("Grouped"), "size_grouped_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_GROUPED)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("Plain"), "size_plain_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_PLAIN)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // Permission display mode
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Permission display mode"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Text (rw-r--r--)"), "perm_text_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.perm_disp_mode == GNOME_CMD_PERM_DISP_MODE_TEXT)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("Number (644)"), "perm_num_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.perm_disp_mode == GNOME_CMD_PERM_DISP_MODE_NUMBER)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // Date options
    table = create_table (parent, 2, 3);
    cat = create_category (parent, table, _("Date format"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    label = create_label (parent, _("Format:"));
    table_add (table, label, 0, 0, GTK_FILL);

    gchar *utf8_date_format = g_locale_to_utf8 (gnome_cmd_data_get_date_format (), -1, NULL, NULL, NULL);
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

    label = create_label (parent, _("See the manual page for \"strftime\" for help on how to set the format string."));
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    table_add (table, label, 1, 2, GTK_FILL);

    return frame;
}


inline void store_format_options (GtkWidget *dialog)
{
    GtkWidget *size_powered_radio = lookup_widget (dialog, "size_powered_radio");
    GtkWidget *size_locale_radio = lookup_widget (dialog, "size_locale_radio");
    GtkWidget *size_grouped_radio = lookup_widget (dialog, "size_grouped_radio");
    GtkWidget *perm_text_radio = lookup_widget (dialog, "perm_text_radio");
    GtkWidget *entry = lookup_widget (dialog, "date_format_entry");
    const gchar *format = gtk_entry_get_text (GTK_ENTRY (entry));

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (size_powered_radio)))
        gnome_cmd_data.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_POWERED;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (size_locale_radio)))
        gnome_cmd_data.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_LOCALE;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (size_grouped_radio)))
        gnome_cmd_data.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_GROUPED;
    else
        gnome_cmd_data.size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_PLAIN;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (perm_text_radio)))
        gnome_cmd_data.perm_disp_mode = GNOME_CMD_PERM_DISP_MODE_TEXT;
    else
        gnome_cmd_data.perm_disp_mode = GNOME_CMD_PERM_DISP_MODE_NUMBER;

    gnome_cmd_data_set_date_format (g_locale_from_utf8 (format, -1, NULL, NULL, NULL));
}


/***********************************************************************
 *
 *  The Layout tab
 *
 **********************************************************************/

static void on_layout_mode_changed (GtkOptionMenu *optmenu, GtkWidget *dialog)
{
    g_return_if_fail (GTK_IS_OPTION_MENU (optmenu));

    GtkWidget *icon_frame = lookup_widget (GTK_WIDGET (dialog), "mime_icon_settings_frame");
    GnomeCmdLayout mode = (GnomeCmdLayout) gtk_option_menu_get_history (GTK_OPTION_MENU (optmenu));

    if (icon_frame)
        gtk_widget_set_sensitive (icon_frame, mode == GNOME_CMD_LAYOUT_MIME_ICONS);
}


static void on_color_mode_changed (GtkOptionMenu *optmenu, GtkWidget *dialog)
{
    g_return_if_fail (GTK_IS_OPTION_MENU (optmenu));

    GtkWidget *btn = lookup_widget (GTK_WIDGET (dialog), "color_btn");
    GnomeCmdColorMode mode = (GnomeCmdColorMode) gtk_option_menu_get_history (GTK_OPTION_MENU (optmenu));

    if (btn)
        gtk_widget_set_sensitive (btn, mode == GNOME_CMD_COLOR_CUSTOM);
}


static void on_edit_colors_close (GtkButton *btn, GtkWidget *dlg)
{
    GnomeCmdColorTheme *colors = gnome_cmd_data_get_custom_color_theme ();

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
    GtkWidget *dlg = gnome_cmd_dialog_new (_("Edit Colors..."));
    g_object_ref (dlg);

    gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
    gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (parent));

    GtkWidget *cat, *cat_box;
    GtkWidget *table, *label;
    GtkWidget *cbutton;
    GnomeCmdColorTheme *colors = gnome_cmd_data_get_custom_color_theme ();

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
    GnomeCmdLsColorsPalette *palette = gnome_cmd_data_get_ls_colors_palette ();

    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "black_fg")), palette->black_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "black_bg")), palette->black_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "red_fg")), palette->red_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "red_bg")), palette->red_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "green_fg")), palette->green_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "green_bg")), palette->green_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "yellow_fg")), palette->yellow_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "yellow_bg")), palette->yellow_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "blue_fg")), palette->blue_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "blue_bg")), palette->blue_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "magenta_fg")), palette->magenta_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "magenta_bg")), palette->magenta_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "cyan_fg")), palette->cyan_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "cyan_bg")), palette->cyan_bg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "white_fg")), palette->white_fg);
    gtk_color_button_get_color (GTK_COLOR_BUTTON (lookup_widget (dlg, "white_bg")), palette->white_bg);

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
    GnomeCmdLsColorsPalette *palette = gnome_cmd_data_get_ls_colors_palette ();

    cat_box = create_vbox (dlg, FALSE, 12);
    cat = create_category (dlg, cat_box, _("Palette"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dlg), cat);

    table = create_table (dlg, 3, 9);
    gtk_container_add (GTK_CONTAINER (cat_box), table);

    cbutton = create_color_button (dlg, "black_fg");
    table_add (table, cbutton, 1, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->black_fg);
    cbutton = create_color_button (dlg, "black_bg");
    table_add (table, cbutton, 1, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->black_bg);
    cbutton = create_color_button (dlg, "red_fg");
    table_add (table, cbutton, 2, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->red_fg);
    cbutton = create_color_button (dlg, "red_bg");
    table_add (table, cbutton, 2, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->red_bg);
    cbutton = create_color_button (dlg, "green_fg");
    table_add (table, cbutton, 3, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->green_fg);
    cbutton = create_color_button (dlg, "green_bg");
    table_add (table, cbutton, 3, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->green_bg);
    cbutton = create_color_button (dlg, "yellow_fg");
    table_add (table, cbutton, 4, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->yellow_fg);
    cbutton = create_color_button (dlg, "yellow_bg");
    table_add (table, cbutton, 4, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->yellow_bg);
    cbutton = create_color_button (dlg, "blue_fg");
    table_add (table, cbutton, 5, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->blue_fg);
    cbutton = create_color_button (dlg, "blue_bg");
    table_add (table, cbutton, 5, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->blue_bg);
    cbutton = create_color_button (dlg, "magenta_fg");
    table_add (table, cbutton, 6, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->magenta_fg);
    cbutton = create_color_button (dlg, "magenta_bg");
    table_add (table, cbutton, 6, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->magenta_bg);
    cbutton = create_color_button (dlg, "cyan_fg");
    table_add (table, cbutton, 7, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->cyan_fg);
    cbutton = create_color_button (dlg, "cyan_bg");
    table_add (table, cbutton, 7, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->cyan_bg);
    cbutton = create_color_button (dlg, "white_fg");
    table_add (table, cbutton, 8, 1, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->white_fg);
    cbutton = create_color_button (dlg, "white_bg");
    table_add (table, cbutton, 8, 2, (GtkAttachOptions) 0);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cbutton), palette->white_bg);

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


static GtkWidget *create_layout_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat;
    GtkWidget *entry, *spin, *scale, *table, *label, *fpicker, *btn;
    GtkWidget *lm_optmenu, *cm_optmenu, *fe_optmenu, *check;
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
    const gchar *color_modes[GNOME_CMD_NUM_COLOR_MODES+1] = {
        _("Respect theme colors"),
        _("Modern"),
        _("Fusion"),
        _("Classic"),
        _("Deep blue"),
        _("Cafezinho"),
        _("Green tiger"),
        _("Custom"),
        NULL
    };

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);


    // File panes
    table = create_table (parent, 5, 2);
    gtk_table_set_homogeneous (GTK_TABLE (table), FALSE);
    cat = create_category (parent, table, _("File panes"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    fpicker = create_font_picker (parent, "list_font_picker");
    table_add (table, fpicker, 1, 0, GTK_FILL);
    gtk_font_button_set_font_name (GTK_FONT_BUTTON (fpicker), gnome_cmd_data_get_list_font ());

    spin = create_spin (parent, "row_height_spin", 8, 64, gnome_cmd_data.list_row_height);
    table_add (table, spin, 1, 1, GTK_FILL);

    label = create_label (parent, _("Font:"));
    table_add (table, label, 0, 0, GTK_FILL);
    label = create_label (parent, _("Row height:"));
    table_add (table, label, 0, 1, GTK_FILL);

    // File extensions
    label = create_label (parent, _("Display file extensions:"));
    table_add (table, label, 0, 2, GTK_FILL);

    fe_optmenu = create_option_menu (parent, ext_modes);
    g_object_set_data (G_OBJECT (parent), "fe_optmenu", fe_optmenu);
    table_add (table, fe_optmenu, 1, 2, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    // Graphical mode
    label = create_label (parent, _("Graphical mode:"));
    table_add (table, label, 0, 3, GTK_FILL);

    lm_optmenu = create_option_menu (parent, gfx_modes);
    g_object_set_data (G_OBJECT (parent), "lm_optmenu", lm_optmenu);
    g_signal_connect (lm_optmenu, "changed", G_CALLBACK (on_layout_mode_changed), parent);
    table_add (table, lm_optmenu, 1, 3, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    // Color scheme
    label = create_label (parent, _("Color scheme:"));
    table_add (table, label, 0, 4, GTK_FILL);

    hbox = create_hbox (parent, FALSE, 6);
    table_add (table, hbox, 1, 4, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    cm_optmenu = create_option_menu (parent, color_modes);
    g_object_set_data (G_OBJECT (parent), "cm_optmenu", cm_optmenu);
    g_signal_connect (cm_optmenu, "changed", G_CALLBACK (on_color_mode_changed), parent);
    gtk_box_pack_start (GTK_BOX (hbox), cm_optmenu, TRUE, TRUE, 0);


    btn = create_button_with_data (parent, _("Edit..."), GTK_SIGNAL_FUNC (on_colors_edit), parent);
    g_object_set_data (G_OBJECT (parent), "color_btn", btn);
    gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
    gtk_widget_set_sensitive (btn, gnome_cmd_data.color_mode == GNOME_CMD_COLOR_CUSTOM);


    // LS_COLORS
    check = create_check (parent, _("Colorize files according to the LS_COLORS environment variable"), "use_ls_colors");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.use_ls_colors);
    hbox = create_hbox (parent, FALSE, 6);
    gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, 5, 6, GTK_FILL, GTK_FILL, 0, 0);

    g_signal_connect (check, "toggled", G_CALLBACK (on_ls_colors_toggled), parent);
    gtk_box_pack_start (GTK_BOX (hbox), check, TRUE, TRUE, 0);

    btn = create_button_with_data (parent, _("Edit colors..."), GTK_SIGNAL_FUNC (on_ls_colors_edit), parent);
    g_object_set_data (G_OBJECT (parent), "ls_colors_edit_btn", btn);
    gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
    gtk_widget_set_sensitive (btn, gnome_cmd_data.use_ls_colors);


     // MIME icon settings
    table = create_table (parent, 4, 2);
    cat = create_category (parent, table, _("MIME icon settings"));
    g_object_set_data (G_OBJECT (parent), "mime_icon_settings_frame", cat);
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    spin = create_spin (parent, "iconsize_spin", 8, 64, gnome_cmd_data.icon_size);
    table_add (table, spin, 1, 0, (GtkAttachOptions) GTK_FILL);
    scale = create_scale (parent, "iconquality_scale", gnome_cmd_data.icon_scale_quality, 0, 3);
    table_add (table, scale, 1, 1, (GtkAttachOptions) GTK_FILL);
    entry = create_file_entry (parent, "theme_icondir_entry", gnome_cmd_data_get_theme_icon_dir ());
    table_add (table, entry, 1, 2, (GtkAttachOptions)0);
    entry = create_file_entry (parent, "doc_icondir_entry", gnome_cmd_data_get_document_icon_dir ());
    table_add (table, entry, 1, 3, (GtkAttachOptions)0);

    label = create_label (parent, _("Icon size:"));
    table_add (table, label, 0, 0, (GtkAttachOptions) GTK_FILL);
    label = create_label (parent, _("Scaling quality:"));
    table_add (table, label, 0, 1, (GtkAttachOptions) GTK_FILL);
    label = create_label (parent, _("Theme icon directory:"));
    table_add (table, label, 0, 2, (GtkAttachOptions) GTK_FILL);
    label = create_label (parent, _("Document icon directory:"));
    table_add (table, label, 0, 3, (GtkAttachOptions) GTK_FILL);


    gtk_option_menu_set_history (GTK_OPTION_MENU (fe_optmenu), (gint) gnome_cmd_data.ext_disp_mode);
    gtk_option_menu_set_history (GTK_OPTION_MENU (lm_optmenu), (gint) gnome_cmd_data.layout);
    gtk_option_menu_set_history (GTK_OPTION_MENU (cm_optmenu), (gint) gnome_cmd_data.color_mode);

    return frame;
}


inline void store_layout_options (GtkWidget *dialog)
{
    GtkWidget *iconsize_spin       = lookup_widget (dialog, "iconsize_spin");
    GtkWidget *iconquality_scale   = lookup_widget (dialog, "iconquality_scale");
    GtkWidget *theme_icondir_entry = lookup_widget (dialog, "theme_icondir_entry");
    GtkWidget *doc_icondir_entry   = lookup_widget (dialog, "doc_icondir_entry");
    GtkWidget *row_height_spin     = lookup_widget (dialog, "row_height_spin");
    GtkWidget *use_ls              = lookup_widget (dialog, "use_ls_colors");

    GtkWidget *lm_optmenu = lookup_widget (dialog, "lm_optmenu");
    GtkWidget *fe_optmenu = lookup_widget (dialog, "fe_optmenu");
    GtkWidget *cm_optmenu = lookup_widget (dialog, "cm_optmenu");

    GtkWidget *list_font_picker = lookup_widget (dialog, "list_font_picker");

    gnome_cmd_data.ext_disp_mode = (GnomeCmdExtDispMode) gtk_option_menu_get_history (GTK_OPTION_MENU (fe_optmenu));
    gnome_cmd_data.layout = (GnomeCmdLayout) gtk_option_menu_get_history (GTK_OPTION_MENU (lm_optmenu));
    gnome_cmd_data.color_mode = (GnomeCmdColorMode) gtk_option_menu_get_history (GTK_OPTION_MENU (cm_optmenu));

    gnome_cmd_data.use_ls_colors = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (use_ls));

    const gchar *list_font = gtk_font_button_get_font_name (GTK_FONT_BUTTON (list_font_picker));
    gnome_cmd_data_set_list_font (list_font);

    gnome_cmd_data_set_theme_icon_dir (gtk_entry_get_text (GTK_ENTRY (theme_icondir_entry)));
    gnome_cmd_data_set_document_icon_dir (gtk_entry_get_text (GTK_ENTRY (doc_icondir_entry)));
    gnome_cmd_data.icon_size = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (iconsize_spin));

    GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (iconquality_scale));
    gnome_cmd_data.icon_scale_quality = (GdkInterpType) adj->value;

    gnome_cmd_data.list_row_height = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (row_height_spin));
}


/***********************************************************************
 *
 *  The Tabs tab
 *
 **********************************************************************/

static GtkWidget *create_tabs_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat, *cat_box;
    GtkWidget *radio, *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);


    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Tab bar"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Always show the tab bar"), "always_show_tabs");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.always_show_tabs);


    // Size display mode
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Tab lock indicator"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Lock icon"), "tab_lock_icon_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.tab_lock_indicator == GnomeCmdData::TAB_LOCK_ICON)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("* (asterisk)"), "tab_lock_asterisk_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.tab_lock_indicator == GnomeCmdData::TAB_LOCK_ASTERISK)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    radio = create_radio (parent, get_radio_group (radio), _("Styled text"), "tab_lock_style_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.tab_lock_indicator == GnomeCmdData::TAB_LOCK_STYLED_TEXT)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    return frame;
}


inline void store_tabs_options (GtkWidget *dialog)
{
    GtkWidget *always_show_tabs = lookup_widget (dialog, "always_show_tabs");
    GtkWidget *tab_lock_icon_radio = lookup_widget (dialog, "tab_lock_icon_radio");
    GtkWidget *tab_lock_asterisk_radio = lookup_widget (dialog, "tab_lock_asterisk_radio");
    GtkWidget *tab_lock_style_radio = lookup_widget (dialog, "tab_lock_style_radio");

    gnome_cmd_data.always_show_tabs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (always_show_tabs));

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tab_lock_icon_radio)))
        gnome_cmd_data.tab_lock_indicator = GnomeCmdData::TAB_LOCK_ICON;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tab_lock_asterisk_radio)))
        gnome_cmd_data.tab_lock_indicator = GnomeCmdData::TAB_LOCK_ASTERISK;
    else
        gnome_cmd_data.tab_lock_indicator = GnomeCmdData::TAB_LOCK_STYLED_TEXT;
}


/***********************************************************************
 *
 *  The Confirmation tab
 *
 **********************************************************************/

static GtkWidget *create_confirmation_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat, *cat_box;
    GtkWidget *radio, *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);


    /* Delete options
     */
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Delete"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Confirm before delete"), "confirm_delete_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.confirm_delete);


    /* Copy overwrite options
     */
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Copy overwrite"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Silently"), "copy_overwrite_silently");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (gnome_cmd_data.confirm_copy_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Query first"), "copy_overwrite_query");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.confirm_copy_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_QUERY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Skip all"), "copy_overwrite_skip_all");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.confirm_copy_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    /* Move overwrite options
     */
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Move overwrite"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Silently"), "move_overwrite_silently");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (gnome_cmd_data.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Query first"), "move_overwrite_query");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_QUERY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group (radio), _("Skip all"), "move_overwrite_skip_all");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    /* Drag and Drop options
     */
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Drag and Drop"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Confirm mouse operation"), "confirm_mouse_dnd_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.confirm_mouse_dnd);


    return frame;
}


inline void store_confirmation_options (GtkWidget *dialog)
{
    GtkWidget *confirm_delete_check = lookup_widget (dialog, "confirm_delete_check");
    GtkWidget *confirm_copy_silent = lookup_widget (dialog, "copy_overwrite_silently");
    GtkWidget *confirm_copy_query = lookup_widget (dialog, "copy_overwrite_query");
    GtkWidget *confirm_copy_skip_all = lookup_widget (dialog, "copy_overwrite_skip_all");
    GtkWidget *confirm_move_silent = lookup_widget (dialog, "move_overwrite_silently");
    GtkWidget *confirm_move_query = lookup_widget (dialog, "move_overwrite_query");
    GtkWidget *confirm_move_skip_all = lookup_widget (dialog, "move_overwrite_skip_all");
    GtkWidget *confirm_mouse_dnd_check = lookup_widget (dialog, "confirm_mouse_dnd_check");

    gnome_cmd_data.confirm_delete = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_delete_check));

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_copy_silent)))
        gnome_cmd_data.confirm_copy_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_copy_query)))
        gnome_cmd_data.confirm_copy_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_QUERY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_copy_skip_all)))
        gnome_cmd_data.confirm_copy_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_move_silent)))
        gnome_cmd_data.confirm_move_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_move_query)))
        gnome_cmd_data.confirm_move_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_QUERY;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_move_skip_all)))
        gnome_cmd_data.confirm_move_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL;

    gnome_cmd_data.confirm_mouse_dnd = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (confirm_mouse_dnd_check));
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


static GtkWidget *create_filter_tab (GtkWidget *parent)
{
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *cat;
    GtkWidget *cat_box;
    GtkWidget *check, *backup_check;
    GtkWidget *entry;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);


    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Filetypes to hide"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    check = create_check (parent, _("Unknown"), "hide_unknown_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_UNKNOWN));
    check = create_check (parent, _("Regular files"), "hide_regular_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_REGULAR));
    check = create_check (parent, _("Directories"), "hide_directory_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_DIRECTORY));
    check = create_check (parent, _("Fifo files"), "hide_fifo_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_FIFO));
    check = create_check (parent, _("Socket files"), "hide_socket_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_SOCKET));
    check = create_check (parent, _("Character devices"), "hide_char_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE));
    check = create_check (parent, _("Block devices"), "hide_block_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_BLOCK_DEVICE));


    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Also hide"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    check = create_check (parent, _("Hidden files"), "hide_hidden_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.filter_settings.hidden);
    backup_check = create_check (parent, _("Backup files"), "hide_backup_check");
    gtk_container_add (GTK_CONTAINER (cat_box), backup_check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (backup_check),
                                  gnome_cmd_data.filter_settings.backup);
    check = create_check (parent, _("Symlinks"), "hide_symlink_check");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK));


    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Backup files"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    entry = create_entry (parent, "backup_pattern_entry", gnome_cmd_data_get_backup_pattern ());
    gtk_box_pack_start (GTK_BOX (cat_box), entry, TRUE, FALSE, 0);
    gtk_widget_set_sensitive (entry, gnome_cmd_data.filter_settings.backup);


    g_signal_connect (backup_check, "toggled", G_CALLBACK (on_filter_backup_files_toggled), frame);

    return frame;
}


inline void store_filter_options (GtkWidget *dialog)
{
    GtkWidget *hide_unknown_check;
    GtkWidget *hide_directory_check;
    GtkWidget *hide_regular_check;
    GtkWidget *hide_socket_check;
    GtkWidget *hide_fifo_check;
    GtkWidget *hide_block_check;
    GtkWidget *hide_char_check;
    GtkWidget *hide_hidden_check;
    GtkWidget *hide_backup_check;
    GtkWidget *hide_symlink_check;
    GtkWidget *backup_pattern_entry;

    hide_unknown_check = lookup_widget (dialog, "hide_unknown_check");
    hide_regular_check = lookup_widget (dialog, "hide_regular_check");
    hide_directory_check = lookup_widget (dialog, "hide_directory_check");
    hide_fifo_check = lookup_widget (dialog, "hide_fifo_check");
    hide_socket_check = lookup_widget (dialog, "hide_socket_check");
    hide_char_check = lookup_widget (dialog, "hide_char_check");
    hide_block_check = lookup_widget (dialog, "hide_block_check");
    hide_symlink_check = lookup_widget (dialog, "hide_symlink_check");
    hide_hidden_check = lookup_widget (dialog, "hide_hidden_check");
    hide_backup_check = lookup_widget (dialog, "hide_backup_check");
    backup_pattern_entry = lookup_widget (dialog, "backup_pattern_entry");

    gnome_cmd_data.filter_settings.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_unknown_check));

    gnome_cmd_data.filter_settings.file_types[GNOME_VFS_FILE_TYPE_REGULAR] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_regular_check));

    gnome_cmd_data.filter_settings.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_directory_check));

    gnome_cmd_data.filter_settings.file_types[GNOME_VFS_FILE_TYPE_FIFO] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_fifo_check));

    gnome_cmd_data.filter_settings.file_types[GNOME_VFS_FILE_TYPE_SOCKET] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_socket_check));

    gnome_cmd_data.filter_settings.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_char_check));

    gnome_cmd_data.filter_settings.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_block_check));

    gnome_cmd_data.filter_settings.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK] =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_symlink_check));

    gnome_cmd_data.filter_settings.hidden =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_hidden_check));
    gnome_cmd_data.filter_settings.backup =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hide_backup_check));

    gnome_cmd_data_set_backup_pattern (gtk_entry_get_text (GTK_ENTRY (backup_pattern_entry)));
}


/***********************************************************************
 *
 *  The Programs tab
 *
 **********************************************************************/

inline void add_app_to_list (GtkCList *clist, GnomeCmdApp *app)
{
    gchar *text[2];

    text[0] = NULL;
    text[1] = (gchar *) gnome_cmd_app_get_name (app);

    gint row = gtk_clist_append (GTK_CLIST (clist), text);
    GnomeCmdPixmap *pm = gnome_cmd_app_get_pixmap (app);

    if (pm)
        gtk_clist_set_pixmap (GTK_CLIST (clist), row, 0, pm->pixmap, pm->mask);

    gtk_clist_set_row_data (GTK_CLIST (clist), row, app);
}


inline void update_app_in_list (GtkCList *clist, GnomeCmdApp *app)
{
    gint row = gtk_clist_find_row_from_data (clist, app);
    GnomeCmdPixmap *pm = gnome_cmd_app_get_pixmap (app);

    if (pm)
        gtk_clist_set_pixmap (GTK_CLIST (clist), row, 0, pm->pixmap, pm->mask);

    gtk_clist_set_text (clist, row, 1, gnome_cmd_app_get_name (app));
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
    GtkWidget *icon_entry = lookup_widget (dialog, "icon_entry");
    GtkWidget *pattern_entry = lookup_widget (dialog, "pattern_entry");
    GtkWidget *target_files = lookup_widget (dialog, "show_for_all_files");
    GtkWidget *target_dirs = lookup_widget (dialog, "show_for_all_dirs");
    GtkWidget *target_dirs_and_files = lookup_widget (dialog, "show_for_all_dirs_and_files");
    GtkWidget *uris_check = lookup_widget (dialog, "handle_uris");
    GtkWidget *multiple_check = lookup_widget (dialog, "handle_multiple");
    GtkWidget *terminal_check = lookup_widget (dialog, "requires_terminal");

    *name = (gchar *) gtk_entry_get_text (GTK_ENTRY (name_entry));
    *cmd = (gchar *) gtk_entry_get_text (GTK_ENTRY (cmd_entry));
    *icon_path = (gchar *) gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (icon_entry));
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
    GtkWidget *clist = lookup_widget (options_dialog, "app_clist");

    get_app_dialog_values (dialog, &name, &cmd, &icon_path,
                           &target, &pattern_string,
                           &handles_uris, &handles_multiple, &requires_terminal);
    if (!name || strlen (name) < 1) return;

    GnomeCmdApp *app = gnome_cmd_app_new_with_values (name, cmd, icon_path,
                                                      (AppTarget) target,
                                                      pattern_string,
                                                      handles_uris, handles_multiple, requires_terminal);
    gnome_cmd_data_add_fav_app (app);
    add_app_to_list (GTK_CLIST (clist), app);
    gtk_widget_destroy (dialog);

    g_free (icon_path);
}


static void on_edit_app_dialog_ok (GtkButton *button, GtkWidget *dialog)
{
    gint target;
    gboolean handles_uris, handles_multiple, requires_terminal;
    gchar *name, *cmd, *icon_path, *pattern_string;

    GtkWidget *options_dialog = lookup_widget (dialog, "options_dialog");
    GtkWidget *clist = lookup_widget (options_dialog, "app_clist");

    get_app_dialog_values (dialog, &name, &cmd, &icon_path,
                           &target, &pattern_string,
                           &handles_uris, &handles_multiple, &requires_terminal);
    if (!name || strlen (name) < 1) return;

    GnomeCmdApp *app = (GnomeCmdApp *) gtk_object_get_data (GTK_OBJECT (options_dialog), "selected_app");
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

    update_app_in_list (GTK_CLIST (clist), app);
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

    if (app) s = gnome_cmd_app_get_icon_path (app);
    entry = create_icon_entry (dialog, "icon_entry", s);
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
    GnomeCmdApp *app = (GnomeCmdApp *) gtk_object_get_data (GTK_OBJECT (parent), "selected_app");
    if (app)
    {
        GtkWidget *dialog = create_app_dialog (app, GTK_SIGNAL_FUNC (on_edit_app_dialog_ok), GTK_SIGNAL_FUNC (on_app_dialog_cancel), parent);
        gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Application"));
    }
}


static void on_app_selected (GtkCList *clist, gint row, gint column, GdkEventButton *event, GtkWidget *parent)
{
    GnomeCmdApp *app = (GnomeCmdApp *) gtk_clist_get_row_data (clist, row);
    g_object_set_data (G_OBJECT (parent), "selected_app", app);

    gtk_widget_set_sensitive (lookup_widget (parent, "remove_app_button"), TRUE);
    gtk_widget_set_sensitive (lookup_widget (parent, "edit_app_button"), TRUE);
}


static void on_app_moved (GtkCList *clist, gint arg1, gint arg2, GtkWidget *frame)
{
    GList *apps = gnome_cmd_data_get_fav_apps ();

    if (!apps
        || MAX (arg1, arg2) >= g_list_length (apps)
        || MIN (arg1, arg2) < 0
        || arg1 == arg2)
        return;

    gpointer data = g_list_nth_data (apps, arg1);
    apps = g_list_remove (apps, data);

    apps = g_list_insert (apps, data, arg2);

    gnome_cmd_data_set_fav_apps (apps);
}


static void on_app_remove (GtkWidget *button, GtkWidget *frame)
{
    GtkCList *clist = GTK_CLIST (lookup_widget (frame, "app_clist"));

    if (clist->focus_row >= 0)
    {
        GnomeCmdApp *app = (GnomeCmdApp *) gtk_clist_get_row_data (clist, clist->focus_row);
        gnome_cmd_data_remove_fav_app (app);
        gtk_clist_remove (clist, clist->focus_row);
    }
}


static void on_app_move_up (GtkWidget *button, GtkWidget *frame)
{
    GtkCList *clist = GTK_CLIST (lookup_widget (frame, "app_clist"));

    if (clist->focus_row >= 1)
        gtk_clist_row_move (clist, clist->focus_row, clist->focus_row-1);
}


static void on_app_move_down (GtkWidget *button, GtkWidget *frame)
{
    GtkCList *clist = GTK_CLIST (lookup_widget (frame, "app_clist"));

    if (clist->focus_row >= 0)
        gtk_clist_row_move (clist, clist->focus_row, clist->focus_row+1);
}


static GtkWidget *create_programs_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat, *table;
    GtkWidget *entry, *button, *label, *clist, *bbox, *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);


    check = create_check (parent, _("Always download remote files before opening in external programs"), "honor_expect_uris");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), !gnome_cmd_data.honor_expect_uris);
    cat = create_category (parent, check, _("MIME applications"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    table = create_table (parent, 5, 2);
    cat = create_category (parent, table, _("Standard programs"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    label = create_label (parent, _("Viewer:"));
    table_add (table, label, 0, 0, GTK_FILL);
    label = create_label (parent, _("Editor:"));
    table_add (table, label, 0, 2, GTK_FILL);
    label = create_label (parent, _("Differ:"));
    table_add (table, label, 0, 3, GTK_FILL);
    label = create_label (parent, _("Terminal:"));
    table_add (table, label, 0, 4, GTK_FILL);

    entry = create_entry (parent, "viewer", gnome_cmd_data.get_viewer());
    table_add (table, entry, 1, 0, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    check = create_check (parent, _("Use Internal Viewer"), "use_internal_viewer");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.use_internal_viewer);
    table_add (table, check, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    entry = create_entry (parent, "editor", gnome_cmd_data.get_editor());
    table_add (table, entry, 1, 2, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    entry = create_entry (parent, "differ", gnome_cmd_data.get_differ());
    table_add (table, entry, 1, 3, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    entry = create_entry (parent, "term", gnome_cmd_data.get_term());
    table_add (table, entry, 1, 4, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));


    //Other favorite apps frame

    hbox = create_hbox (parent, FALSE, 0);
    gtk_box_set_spacing (GTK_BOX (hbox), 12);
    cat = create_category (parent, hbox, _("Other favorite apps"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);


    clist = create_clist (parent, "app_clist", 2, 16,
                          GTK_SIGNAL_FUNC (on_app_selected), GTK_SIGNAL_FUNC (on_app_moved));
    create_clist_column (clist, 0, 20, "");
    create_clist_column (clist, 1, 150, _("Label"));
    gtk_box_pack_start (GTK_BOX (hbox), clist, TRUE, TRUE, 0);

    bbox = create_vbuttonbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, TRUE, 0);

    button = create_stock_button (parent, GTK_STOCK_ADD, GTK_SIGNAL_FUNC (on_app_add));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_EDIT, GTK_SIGNAL_FUNC (on_app_edit));
    g_object_set_data (G_OBJECT (parent), "edit_app_button", button);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_REMOVE, GTK_SIGNAL_FUNC (on_app_remove));
    g_object_set_data (G_OBJECT (parent), "remove_app_button", button);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_GO_UP, GTK_SIGNAL_FUNC (on_app_move_up));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_GO_DOWN, GTK_SIGNAL_FUNC (on_app_move_down));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    clist = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (parent), "app_clist");
    for (GList *apps = gnome_cmd_data_get_fav_apps (); apps; apps = apps->next)
        add_app_to_list (GTK_CLIST (clist), (GnomeCmdApp *) apps->data);

    return frame;
}


inline void store_programs_options (GtkWidget *dialog)
{
    GtkWidget *entry1 = lookup_widget (dialog, "viewer");
    GtkWidget *entry2 = lookup_widget (dialog, "editor");
    GtkWidget *entry3 = lookup_widget (dialog, "differ");
    GtkWidget *entry5 = lookup_widget (dialog, "term");
    GtkWidget *check_uris = lookup_widget (dialog, "honor_expect_uris");
    GtkWidget *check_iv = lookup_widget (dialog, "use_internal_viewer");

    gnome_cmd_data.set_viewer(gtk_entry_get_text (GTK_ENTRY (entry1)));
    gnome_cmd_data.set_editor(gtk_entry_get_text (GTK_ENTRY (entry2)));
    gnome_cmd_data.set_differ(gtk_entry_get_text (GTK_ENTRY (entry3)));
    gnome_cmd_data.set_term(gtk_entry_get_text (GTK_ENTRY (entry5)));

    gnome_cmd_data.honor_expect_uris = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_uris));
    gnome_cmd_data.use_internal_viewer = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_iv));
}


/***********************************************************************
 *
 *  The Devices tab
 *
 **********************************************************************/

inline void add_device_to_list (GtkCList *clist, GnomeCmdConDevice *dev)
{
    gchar *text[2];

    text[0] = NULL;
    text[1] = (gchar *) gnome_cmd_con_device_get_alias (dev);

    gint row = gtk_clist_append (GTK_CLIST (clist), text);
    GnomeCmdPixmap *pm = gnome_cmd_con_get_open_pixmap (GNOME_CMD_CON (dev));

    if (pm)
        gtk_clist_set_pixmap (GTK_CLIST (clist), row, 0, pm->pixmap, pm->mask);

    gtk_clist_set_row_data (GTK_CLIST (clist), row, dev);
}


inline void update_device_in_list (GtkCList *clist, GnomeCmdConDevice *dev, gchar *alias, gchar *device_fn, gchar *mountp, gchar *icon_path)
{
    gnome_cmd_con_device_set_alias (dev, alias);
    gnome_cmd_con_device_set_device_fn (dev, device_fn);
    gnome_cmd_con_device_set_mountp (dev, mountp);
    gnome_cmd_con_device_set_icon_path (dev, icon_path);

    GnomeCmdPixmap *pm = gnome_cmd_con_get_open_pixmap (GNOME_CMD_CON (dev));
    gint row = gtk_clist_find_row_from_data (clist, dev);

    gtk_clist_set_text (GTK_CLIST (clist), row, 1, alias);
    gtk_clist_set_text (GTK_CLIST (clist), row, 2, device_fn);
    gtk_clist_set_text (GTK_CLIST (clist), row, 3, mountp);

    if (pm)
        gtk_clist_set_pixmap (GTK_CLIST (clist), row, 0, pm->pixmap, pm->mask);
    else
        gtk_clist_set_pixmap (GTK_CLIST (clist), row, 0, NULL, NULL);
}


static void on_device_dialog_cancel (GtkButton *button, GtkWidget *dialog)
{
    gtk_widget_destroy (dialog);
}


inline void get_device_dialog_values (GtkWidget *dialog, gchar **alias, gchar **device, gchar **mountp, gchar **icon_path)
{
    GtkWidget *alias_entry = lookup_widget (dialog, "alias_entry");
    GtkWidget *device_entry = lookup_widget (dialog, "device_entry");
    GtkWidget *mountp_entry = lookup_widget (dialog, "mountp_entry");
    GtkWidget *icon_entry = lookup_widget (dialog, "device_iconentry");

    *alias = (gchar *) gtk_entry_get_text (GTK_ENTRY (alias_entry));
    *device = (gchar *) gtk_entry_get_text (GTK_ENTRY (device_entry));
    *mountp = (gchar *) gtk_entry_get_text (GTK_ENTRY (mountp_entry));
    *icon_path = gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (icon_entry));
}


static void on_add_device_dialog_ok (GtkButton *button, GtkWidget *dialog)
{
    gchar *alias, *device, *mountp, *icon_path;

    GtkWidget *options_dialog = lookup_widget (dialog, "options_dialog");
    GtkWidget *clist = lookup_widget (options_dialog, "device_clist");

    get_device_dialog_values (dialog, &alias, &device, &mountp, &icon_path);
    if (!alias || strlen (alias) < 1) return;

    GnomeCmdConDevice *dev = gnome_cmd_con_device_new (alias, device, mountp, icon_path);
    add_device_to_list (GTK_CLIST (clist), GNOME_CMD_CON_DEVICE (dev));
    gtk_widget_destroy (dialog);

    gnome_cmd_con_list_get()->add(dev);

    g_free (icon_path);
}


static void on_edit_device_dialog_ok (GtkButton *button, GtkWidget *dialog)
{
    gchar *alias, *device, *mountp, *icon_path;

    GtkWidget *options_dialog = lookup_widget (dialog, "options_dialog");
    GtkWidget *clist = lookup_widget (options_dialog, "device_clist");

    get_device_dialog_values (dialog, &alias, &device, &mountp, &icon_path);
    if (!alias || strlen (alias) < 1) return;

    GnomeCmdConDevice *dev = GNOME_CMD_CON_DEVICE (gtk_object_get_data (GTK_OBJECT (options_dialog), "selected_device"));
    if (!dev) return;

    update_device_in_list (GTK_CLIST (clist), dev, alias, device, mountp, icon_path);
    gtk_widget_destroy (dialog);

    g_free (icon_path);
}


static GtkWidget *create_device_dialog (GnomeCmdConDevice *dev, GtkSignalFunc on_ok, GtkSignalFunc on_cancel, GtkWidget *options_dialog)
{
    GtkWidget *table, *entry, *label;
    GtkWidget *dialog;
    const gchar *s = NULL;
    gchar *icon_dir;

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
    label = create_label (dialog, _("Device:"));
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
    entry = create_file_entry (dialog, "device_entry", s);
    table_add (table, entry, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    if (dev) s = gnome_cmd_con_device_get_mountp (dev);
    entry = create_file_entry (dialog, "mountp_entry", s);
    table_add (table, entry, 1, 2, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    if (dev) s = gnome_cmd_con_device_get_icon_path (dev);
    entry = create_icon_entry (dialog, "device_iconentry", s);
    icon_dir = g_build_filename (PIXMAPS_DIR, "device-icons", NULL);
    gnome_icon_entry_set_pixmap_subdir (GNOME_ICON_ENTRY (entry), icon_dir);
    g_free (icon_dir);
    table_add (table, entry, 1, 3, GTK_FILL);

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
    GnomeCmdConDevice *dev = GNOME_CMD_CON_DEVICE (gtk_object_get_data (
        GTK_OBJECT (parent), "selected_device"));
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
    GtkCList *clist = GTK_CLIST (lookup_widget (frame, "device_clist"));

    if (clist->focus_row >= 0)
    {
        GnomeCmdConDevice *dev = GNOME_CMD_CON_DEVICE (gtk_clist_get_row_data (clist, clist->focus_row));
        gnome_cmd_con_list_get()->remove(dev);
        gtk_clist_remove (clist, clist->focus_row);
        gnome_cmd_con_list_get()->remove(dev);
    }
}


static void on_device_selected (GtkCList *clist, gint row, gint column, GdkEventButton *event, GtkWidget *parent)
{
    GnomeCmdConDevice *dev = GNOME_CMD_CON_DEVICE (gtk_clist_get_row_data (clist, row));
    g_object_set_data (G_OBJECT (parent), "selected_device", dev);

    gtk_widget_set_sensitive (lookup_widget (parent, "remove_device_button"), TRUE);
    gtk_widget_set_sensitive (lookup_widget (parent, "edit_device_button"), TRUE);
}


static void on_device_moved (GtkCList *clist, gint arg1, gint arg2, GtkWidget *frame)
{
    GList *list = gnome_cmd_con_list_get_all_dev (gnome_cmd_con_list_get ());

    if (!list
        || MAX (arg1, arg2) >= g_list_length (list)
        || MIN (arg1, arg2) < 0
        || arg1 == arg2)
        return;

    gpointer data = g_list_nth_data (list, arg1);
    list = g_list_remove (list, data);

    list = g_list_insert (list, data, arg2);

    gnome_cmd_con_list_set_all_dev (gnome_cmd_con_list_get (), list);
}


static void on_device_move_up (GtkWidget *button, GtkWidget *frame)
{
    GtkCList *clist = GTK_CLIST (lookup_widget (frame, "device_clist"));

    gtk_clist_row_move (clist, clist->focus_row, clist->focus_row-1);
}


static void on_device_move_down (GtkWidget *button, GtkWidget *frame)
{
    GtkCList *clist = GTK_CLIST (lookup_widget (frame, "device_clist"));

    gtk_clist_row_move (clist, clist->focus_row, clist->focus_row+1);
}


static GtkWidget *create_devices_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat, *cat_box;
    GtkWidget *button, *clist, *bbox, *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);


    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Devices"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    hbox = create_hbox (parent, FALSE, 0);
    gtk_box_set_spacing (GTK_BOX (hbox), 12);
    gtk_container_add (GTK_CONTAINER (cat_box), hbox);

    clist = create_clist (parent, "device_clist", 2, 24,
                          GTK_SIGNAL_FUNC (on_device_selected), GTK_SIGNAL_FUNC (on_device_moved));
    create_clist_column (clist, 0, 26, "");
    create_clist_column (clist, 1, 40, _("Alias"));
    gtk_box_pack_start (GTK_BOX (hbox), clist, TRUE, TRUE, 0);

    bbox = create_vbuttonbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, TRUE, 0);

    button = create_stock_button (parent, GTK_STOCK_ADD, GTK_SIGNAL_FUNC (on_device_add));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_EDIT, GTK_SIGNAL_FUNC (on_device_edit));
    g_object_set_data (G_OBJECT (parent), "edit_device_button", button);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_REMOVE, GTK_SIGNAL_FUNC (on_device_remove));
    g_object_set_data (G_OBJECT (parent), "remove_device_button", button);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_GO_UP, GTK_SIGNAL_FUNC (on_device_move_up));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_GO_DOWN, GTK_SIGNAL_FUNC (on_device_move_down));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    check = create_check (parent, _("Show only the icons"), "device_only_icon");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.device_only_icon);

    check = create_check (parent, _("Skip mounting (useful when using super-mount)"), "skip_mounting");
    gtk_container_add (GTK_CONTAINER (cat_box), check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.skip_mounting);


    clist = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (parent), "device_clist");
    for (GList *devices = gnome_cmd_con_list_get_all_dev (gnome_cmd_con_list_get ()); devices; devices = devices->next)
        if (!gnome_cmd_con_device_get_autovol ((GnomeCmdConDevice *) devices->data))
            add_device_to_list (GTK_CLIST (clist), GNOME_CMD_CON_DEVICE (devices->data));

    return frame;
}


inline void store_devices_options (GtkWidget *dialog)
{
    GtkWidget *device_only_icon = lookup_widget (dialog, "device_only_icon");
    GtkWidget *skip_mounting = lookup_widget (dialog, "skip_mounting");

    gnome_cmd_data.device_only_icon = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (device_only_icon));
    gnome_cmd_data.skip_mounting = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (skip_mounting));
}


static void response_callback (GtkDialog *dialog, int response_id, GnomeCmdNotebook *notebook)
{
    static const char *help_id[] = {"gnome-commander-prefs-general",
                                    "gnome-commander-prefs-format",
                                    "gnome-commander-prefs-layout",
                                    "gnome-commander-prefs-tabs",
                                    "gnome-commander-prefs-confirmation",
                                    "gnome-commander-prefs-filters",
                                    "gnome-commander-prefs-network",
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


gboolean gnome_cmd_options_dialog (GtkWindow *parent, GnomeCmdData &cfg)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Options"), parent,
                                                     GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                     NULL);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

#if GTK_CHECK_VERSION (2, 14, 0)
    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
#endif

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

    // HIG defaults
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_box_set_spacing (GTK_BOX (content_area), 2);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    gtk_box_set_spacing (GTK_BOX (content_area),6);
#else
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area),6);
#endif

    GnomeCmdNotebook *notebook = new GnomeCmdNotebook(GnomeCmdNotebook::SHOW_TABS);

#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_container_add (GTK_CONTAINER (content_area), *notebook);
#else
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), *notebook);
#endif

    notebook->append_page(create_general_tab (dialog), _("General"));
    notebook->append_page(create_format_tab (dialog), _("Format"));
    notebook->append_page(create_layout_tab (dialog), _("Layout"));
    notebook->append_page(create_tabs_tab (dialog), _("Tabs"));
    notebook->append_page(create_confirmation_tab (dialog), _("Confirmation"));
    notebook->append_page(create_filter_tab (dialog), _("Filters"));
    notebook->append_page(create_programs_tab (dialog), _("Programs"));
    notebook->append_page(create_devices_tab (dialog), _("Devices"));

#if GTK_CHECK_VERSION (2, 14, 0)
    gtk_widget_show_all (content_area);
#else
    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);
#endif

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), notebook);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    if (result==GTK_RESPONSE_OK)
    {
        store_general_options (dialog);
        store_format_options (dialog);
        store_layout_options (dialog);
        store_tabs_options (dialog);
        store_confirmation_options (dialog);
        store_filter_options (dialog);
        store_programs_options (dialog);
        store_devices_options (dialog);

        gnome_cmd_style_create ();
        main_win->update_style();

        gnome_cmd_data.save();
    }

    gtk_widget_destroy (dialog);

    return result==GTK_RESPONSE_OK;
}
