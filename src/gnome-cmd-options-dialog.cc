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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-options-dialog.h"
#include "gnome-cmd-data.h"
#include "utils.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-combo.h"

using namespace std;


static GnomeCmdDialogClass *parent_class = NULL;


struct GnomeCmdOptionsDialog::Private
{
};



inline GtkWidget *create_font_picker (GtkWidget *parent, gchar *name)
{
    GtkWidget *w = gtk_font_button_new ();
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, w, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (w);

    return w;
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
    radio = create_radio (parent, get_radio_group(radio), _("Double click to open items"), "lmb_doubleclick_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    check = create_check (parent, _("Single click unselects files"), "lmb_unselects_check");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.left_mouse_button_unselects);


    // Right mouse button settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Right mouse button"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Shows popup menu"), "rmb_popup_radio");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    if (gnome_cmd_data.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_POPUPS_MENU)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group(radio), _("Selects files"), "rmb_sel_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    // FilterType settings
    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Match file names using"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("Shell syntax"), "ft_shell_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.filter_type == Filter::TYPE_FNMATCH)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group(radio), _("Regex syntax"), "ft_regex_radio");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.filter_type == Filter::TYPE_REGEX)
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
    cat = create_category (parent, cat_box, _("Quick search using"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    radio = create_radio (parent, NULL, _("CTRL+ALT+letters"), "ctrl_alt_quick_search");
    gtk_box_pack_start (GTK_BOX (cat_box), radio, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), !gnome_cmd_data.alt_quick_search);
    radio = create_radio (parent, get_radio_group(radio), _("ALT+letters (menu access with F10)"), "alt_quick_search");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), gnome_cmd_data.alt_quick_search);


    return frame;
}


inline void store_general_options (GnomeCmdOptionsDialog *dialog)
{
    GtkWidget *lmb_singleclick_radio = lookup_widget (GTK_WIDGET (dialog), "lmb_singleclick_radio");
    GtkWidget *lmb_unselects_check = lookup_widget (GTK_WIDGET (dialog), "lmb_unselects_check");
    GtkWidget *rmb_popup_radio = lookup_widget (GTK_WIDGET (dialog), "rmb_popup_radio");
    GtkWidget *ft_regex_radio = lookup_widget (GTK_WIDGET (dialog), "ft_regex_radio");
    GtkWidget *case_sens_check = lookup_widget (GTK_WIDGET (dialog), "case_sens_check");
    GtkWidget *alt_quick_search = lookup_widget (GTK_WIDGET (dialog), "alt_quick_search");

    gnome_cmd_data.left_mouse_button_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lmb_singleclick_radio)) ? GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK : GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK;

    gnome_cmd_data.left_mouse_button_unselects = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lmb_unselects_check));

    gnome_cmd_data.right_mouse_button_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rmb_popup_radio)) ? GnomeCmdData::RIGHT_BUTTON_POPUPS_MENU
                                                                                                                : GnomeCmdData::RIGHT_BUTTON_SELECTS;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ft_regex_radio)))
        gnome_cmd_data.filter_type = Filter::TYPE_REGEX;
    else
        gnome_cmd_data.filter_type = Filter::TYPE_FNMATCH;

    gnome_cmd_data.case_sens_sort = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (case_sens_check));
    gnome_cmd_data.alt_quick_search = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (alt_quick_search));
}


/***********************************************************************
 *
 *  The Format tab
 *
 **********************************************************************/

static void on_date_format_update (GtkButton *button, GtkWidget *options_dialog)
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
    GtkWidget *radio, *label, *entry, *button;

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

    gchar *utf8_date_format = g_locale_to_utf8 (gnome_cmd_data_get_date_format(), -1, NULL, NULL, NULL);
    entry = create_entry (parent, "date_format_entry", utf8_date_format);
    g_free (utf8_date_format);
    gtk_widget_grab_focus (entry);
    table_add (table, entry, 1, 0, GTK_FILL);

    button = create_button (parent, _("_Test"), GTK_SIGNAL_FUNC (on_date_format_update));
    table_add (table, button, 2, 0, GTK_FILL);

    label = create_label (parent, _("Test result:"));
    table_add (table, label, 0, 1, GTK_FILL);

    label = create_label (parent, "");
    gtk_object_set_data_full (GTK_OBJECT (parent), "date_format_test_label",
                              label, (GtkDestroyNotify) gtk_widget_unref);
    gtk_signal_connect (GTK_OBJECT (label), "realize", GTK_SIGNAL_FUNC (on_date_format_update), parent);
    table_add (table, label, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    label = create_label (parent, _("See the manual page for \"strftime\" for help on how to set the format string."));
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    table_add (table, label, 1, 2, GTK_FILL);

    return frame;
}


inline void store_format_options (GnomeCmdOptionsDialog *dialog)
{
    GtkWidget *size_powered_radio = lookup_widget (GTK_WIDGET (dialog), "size_powered_radio");
    GtkWidget *size_locale_radio = lookup_widget (GTK_WIDGET (dialog), "size_locale_radio");
    GtkWidget *size_grouped_radio = lookup_widget (GTK_WIDGET (dialog), "size_grouped_radio");
    GtkWidget *perm_text_radio = lookup_widget (GTK_WIDGET (dialog), "perm_text_radio");
    GtkWidget *entry = lookup_widget (GTK_WIDGET (dialog), "date_format_entry");
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
    gtk_widget_ref (dlg);

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


static GtkWidget *create_layout_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat;
    GtkWidget *entry, *spin, *scale, *table, *label, *fpicker, *btn;
    GtkWidget *lm_optmenu, *cm_optmenu, *fe_optmenu, *check;
    gchar *ext_modes[] = {
        _("With file name"),
        _("In separate column"),
        _("In both columns"),
        NULL
    };
    gchar *gfx_modes[] = {
        _("No icons"),
        _("File type icons"),
        _("MIME icons"),
        NULL
    };
    gchar *color_modes[GNOME_CMD_NUM_COLOR_MODES+1] = {
        _("Respect theme colors"),
        _("Modern"),
        _("Fusion"),
        _("Classic"),
        _("Deep blue"),
        _("Cafezinho"),
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
    gtk_object_set_data (GTK_OBJECT (parent), "fe_optmenu", fe_optmenu);
    table_add (table, fe_optmenu, 1, 2, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    // Graphical mode
    label = create_label (parent, _("Graphical mode:"));
    table_add (table, label, 0, 3, GTK_FILL);

    lm_optmenu = create_option_menu (parent, gfx_modes);
    gtk_object_set_data (GTK_OBJECT (parent), "lm_optmenu", lm_optmenu);
    gtk_signal_connect (GTK_OBJECT (lm_optmenu), "changed", GTK_SIGNAL_FUNC (on_layout_mode_changed), parent);
    table_add (table, lm_optmenu, 1, 3, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    // Color scheme
    label = create_label (parent, _("Color scheme:"));
    table_add (table, label, 0, 4, GTK_FILL);

    hbox = create_hbox (parent, FALSE, 6);
    table_add (table, hbox, 1, 4, (GtkAttachOptions) (GTK_FILL|GTK_EXPAND));

    cm_optmenu = create_option_menu (parent, color_modes);
    gtk_object_set_data (GTK_OBJECT (parent), "cm_optmenu", cm_optmenu);
    gtk_signal_connect (GTK_OBJECT (cm_optmenu), "changed", GTK_SIGNAL_FUNC (on_color_mode_changed), parent);
    gtk_box_pack_start (GTK_BOX (hbox), cm_optmenu, TRUE, TRUE, 0);


    btn = create_button_with_data (parent, _("Edit..."), GTK_SIGNAL_FUNC (on_colors_edit), parent);
    gtk_object_set_data (GTK_OBJECT (parent), "color_btn", btn);
    gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
    gtk_widget_set_sensitive (btn, gnome_cmd_data.color_mode == GNOME_CMD_COLOR_CUSTOM);


    // LS_COLORS
    check = create_check (parent, _("Colorize files according to the LS_COLORS environment variable"), "use_ls_colors");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.use_ls_colors);
    gtk_table_attach (GTK_TABLE (table), check, 0, 2, 5, 6, GTK_FILL, GTK_FILL, 0, 0);


     // MIME icon settings
    table = create_table (parent, 4, 2);
    cat = create_category (parent, table, _("MIME icon settings"));
    gtk_object_set_data (GTK_OBJECT (parent), "mime_icon_settings_frame", cat);
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    spin = create_spin (parent, "iconsize_spin", 8, 64, gnome_cmd_data.icon_size);
    table_add (table, spin, 1, 0, (GtkAttachOptions) GTK_FILL);
    scale = create_scale (parent, "iconquality_scale", gnome_cmd_data.icon_scale_quality, 0, 3);
    table_add (table, scale, 1, 1, (GtkAttachOptions) GTK_FILL);
    entry = create_file_entry (parent, "theme_icondir_entry", gnome_cmd_data_get_theme_icon_dir());
    table_add (table, entry, 1, 2, (GtkAttachOptions)0);
    entry = create_file_entry (parent, "doc_icondir_entry", gnome_cmd_data_get_document_icon_dir());
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


inline void store_layout_options (GnomeCmdOptionsDialog *dialog)
{
    GtkWidget *iconsize_spin       = lookup_widget (GTK_WIDGET (dialog), "iconsize_spin");
    GtkWidget *iconquality_scale   = lookup_widget (GTK_WIDGET (dialog), "iconquality_scale");
    GtkWidget *theme_icondir_entry = lookup_widget (GTK_WIDGET (dialog), "theme_icondir_entry");
    GtkWidget *doc_icondir_entry   = lookup_widget (GTK_WIDGET (dialog), "doc_icondir_entry");
    GtkWidget *row_height_spin     = lookup_widget (GTK_WIDGET (dialog), "row_height_spin");
    GtkWidget *use_ls              = lookup_widget (GTK_WIDGET (dialog), "use_ls_colors");

    GtkWidget *lm_optmenu = lookup_widget (GTK_WIDGET (dialog), "lm_optmenu");
    GtkWidget *fe_optmenu = lookup_widget (GTK_WIDGET (dialog), "fe_optmenu");
    GtkWidget *cm_optmenu = lookup_widget (GTK_WIDGET (dialog), "cm_optmenu");

    GtkWidget *list_font_picker = lookup_widget (GTK_WIDGET (dialog), "list_font_picker");

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
    radio = create_radio (parent, get_radio_group(radio), _("Query first"), "copy_overwrite_query");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.confirm_copy_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_QUERY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group(radio), _("Skip all"), "copy_overwrite_skip_all");
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
    radio = create_radio (parent, get_radio_group(radio), _("Query first"), "move_overwrite_query");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_QUERY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio (parent, get_radio_group(radio), _("Skip all"), "move_overwrite_skip_all");
    gtk_container_add (GTK_CONTAINER (cat_box), radio);
    if (gnome_cmd_data.confirm_move_overwrite==GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);


    return frame;
}


inline void store_confirmation_options (GnomeCmdOptionsDialog *dialog)
{
    GtkWidget *confirm_delete_check = lookup_widget (GTK_WIDGET (dialog), "confirm_delete_check");
    GtkWidget *confirm_copy_silent = lookup_widget (GTK_WIDGET (dialog), "copy_overwrite_silently");
    GtkWidget *confirm_copy_query = lookup_widget (GTK_WIDGET (dialog), "copy_overwrite_query");
    GtkWidget *confirm_copy_skip_all = lookup_widget (GTK_WIDGET (dialog), "copy_overwrite_skip_all");
    GtkWidget *confirm_move_silent = lookup_widget (GTK_WIDGET (dialog), "move_overwrite_silently");
    GtkWidget *confirm_move_query = lookup_widget (GTK_WIDGET (dialog), "move_overwrite_query");
    GtkWidget *confirm_move_skip_all = lookup_widget (GTK_WIDGET (dialog), "move_overwrite_skip_all");

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


    gtk_signal_connect (GTK_OBJECT (backup_check), "toggled", GTK_SIGNAL_FUNC (on_filter_backup_files_toggled), frame);

    return frame;
}


inline void store_filter_options (GnomeCmdOptionsDialog *dialog)
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

    hide_unknown_check = lookup_widget (GTK_WIDGET (dialog), "hide_unknown_check");
    hide_regular_check = lookup_widget (GTK_WIDGET (dialog), "hide_regular_check");
    hide_directory_check = lookup_widget (GTK_WIDGET (dialog), "hide_directory_check");
    hide_fifo_check = lookup_widget (GTK_WIDGET (dialog), "hide_fifo_check");
    hide_socket_check = lookup_widget (GTK_WIDGET (dialog), "hide_socket_check");
    hide_char_check = lookup_widget (GTK_WIDGET (dialog), "hide_char_check");
    hide_block_check = lookup_widget (GTK_WIDGET (dialog), "hide_block_check");
    hide_symlink_check = lookup_widget (GTK_WIDGET (dialog), "hide_symlink_check");
    hide_hidden_check = lookup_widget (GTK_WIDGET (dialog), "hide_hidden_check");
    hide_backup_check = lookup_widget (GTK_WIDGET (dialog), "hide_backup_check");
    backup_pattern_entry = lookup_widget (GTK_WIDGET (dialog), "backup_pattern_entry");

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
 *  The Network tab
 *
 **********************************************************************/

static GtkWidget *create_network_tab (GtkWidget *parent)
{
    GtkWidget *frame, *hbox, *vbox, *cat, *cat_box;
    GtkWidget *table, *label, *entry;
    GtkWidget *check;

    frame = create_tabframe (parent);
    hbox = create_tabhbox (parent);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = create_tabvbox (parent);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);


    // GNOME Keyring Manager

    cat_box = create_vbox (parent, FALSE, 0);
    cat = create_category (parent, cat_box, _("Authentication"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    check = create_check (parent, _("Use GNOME Keyring Manager for authentication"), "use_auth_manager");
    gtk_box_pack_start (GTK_BOX (cat_box), check, FALSE, TRUE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gnome_cmd_data.use_gnome_auth_manager);


    // Anonymous FTP password options

    table = create_table (parent, 1, 2);
    cat = create_category (parent, table, _("Anonymous FTP access"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, FALSE, 0);

    label = create_label (parent, _("Password:"));
    table_add (table, label, 0, 0, GTK_FILL);

    entry = create_entry (parent, "anonymous_ftp_password", gnome_cmd_data_get_ftp_anonymous_password ());
    table_add (table, entry, 1, 0, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    return frame;
}


inline void store_network_options (GnomeCmdOptionsDialog *dialog)
{
    GtkWidget *use_auth_manager_check = lookup_widget (GTK_WIDGET (dialog), "use_auth_manager");
    GtkWidget *entry = lookup_widget (GTK_WIDGET (dialog), "anonymous_ftp_password");

    gnome_cmd_data.use_gnome_auth_manager = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (use_auth_manager_check));
    gnome_cmd_data_set_ftp_anonymous_password (gtk_entry_get_text (GTK_ENTRY (entry)));
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


static void
get_app_dialog_values (GtkWidget *dialog, gchar **name, gchar **cmd, gchar **icon_path,
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

    g_free (name);
    g_free (cmd);
    g_free (icon_path);
    g_free (pattern_string);
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

    g_free (name);
    g_free (cmd);
    g_free (icon_path);
    g_free (pattern_string);
}


static GtkWidget *create_app_dialog (GnomeCmdApp *app, GtkSignalFunc on_ok, GtkSignalFunc on_cancel, GtkWidget *options_dialog)
{
    GtkWidget *vbox, *hbox, *table, *entry, *label, *cat, *radio, *check;
    const gchar *s = NULL;

    GtkWidget *dialog = gnome_cmd_dialog_new (NULL);
    gtk_widget_ref (dialog);
    gtk_object_set_data (GTK_OBJECT (dialog), "options_dialog", options_dialog);

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
    radio = create_radio (dialog, get_radio_group(radio), _("All directories"),
                          "show_for_all_dirs");
    if (app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (radio),
        gnome_cmd_app_get_target (app) == APP_TARGET_ALL_DIRS);
    gtk_table_attach (GTK_TABLE (table), radio, 0, 2, 1, 2, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    radio = create_radio (dialog, get_radio_group(radio), _("All directories and files"),
                          "show_for_all_dirs_and_files");
    if (app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (radio),
        gnome_cmd_app_get_target (app) == APP_TARGET_ALL_DIRS_AND_FILES);
    gtk_table_attach (GTK_TABLE (table), radio, 0, 2, 2, 3, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    radio = create_radio (dialog, get_radio_group(radio), _("Some files"),
                          "show_for_some_files");
    if (app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (radio),
        gnome_cmd_app_get_target (app) == APP_TARGET_SOME_FILES);
    gtk_table_attach (GTK_TABLE (table), radio, 0, 2, 3, 4, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    if (!app) gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (radio), TRUE);
    gtk_signal_connect (GTK_OBJECT (radio), "toggled",
                        GTK_SIGNAL_FUNC (on_some_files_toggled), dialog);

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
    gtk_object_set_data (GTK_OBJECT (parent), "selected_app", app);

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
    gtk_object_set_data (GTK_OBJECT (parent), "edit_app_button", button);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_REMOVE, GTK_SIGNAL_FUNC (on_app_remove));
    gtk_object_set_data (GTK_OBJECT (parent), "remove_app_button", button);
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


inline void store_programs_options (GnomeCmdOptionsDialog *dialog)
{
    GtkWidget *entry1 = lookup_widget (GTK_WIDGET (dialog), "viewer");
    GtkWidget *entry2 = lookup_widget (GTK_WIDGET (dialog), "editor");
    GtkWidget *entry3 = lookup_widget (GTK_WIDGET (dialog), "differ");
    GtkWidget *entry5 = lookup_widget (GTK_WIDGET (dialog), "term");
    GtkWidget *check_uris = lookup_widget (GTK_WIDGET (dialog), "honor_expect_uris");
    GtkWidget *check_iv = lookup_widget (GTK_WIDGET (dialog), "use_internal_viewer");

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

    gnome_cmd_con_list_add_device (gnome_cmd_con_list_get (), dev);

    g_free (alias);
    g_free (device);
    g_free (mountp);
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

    g_free (alias);
    g_free (device);
    g_free (mountp);
    g_free (icon_path);
}


static GtkWidget *create_device_dialog (GnomeCmdConDevice *dev, GtkSignalFunc on_ok, GtkSignalFunc on_cancel, GtkWidget *options_dialog)
{
    GtkWidget *table, *entry, *label;
    GtkWidget *dialog;
    const gchar *s = NULL;
    gchar *icon_dir;

    dialog = gnome_cmd_dialog_new ("");
    gtk_widget_ref (dialog);
    gtk_window_set_title (GTK_WINDOW (dialog), "");
    gtk_object_set_data (GTK_OBJECT (dialog), "options_dialog", options_dialog);

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
    icon_dir = g_build_path (G_DIR_SEPARATOR_S, PIXMAPS_DIR, "device-icons", NULL);
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
        gnome_cmd_con_list_remove_device (gnome_cmd_con_list_get (), dev);
        gtk_clist_remove (clist, clist->focus_row);
        gnome_cmd_con_list_remove_device (gnome_cmd_con_list_get (), dev);
    }
}


static void on_device_selected (GtkCList *clist, gint row, gint column, GdkEventButton *event, GtkWidget *parent)
{
    GnomeCmdConDevice *dev = GNOME_CMD_CON_DEVICE (gtk_clist_get_row_data (clist, row));
    gtk_object_set_data (GTK_OBJECT (parent), "selected_device", dev);

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
    gtk_object_set_data (GTK_OBJECT (parent), "edit_device_button", button);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    button = create_stock_button (parent, GTK_STOCK_REMOVE, GTK_SIGNAL_FUNC (on_device_remove));
    gtk_object_set_data (GTK_OBJECT (parent), "remove_device_button", button);
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
        if (!gnome_cmd_con_device_get_autovol((GnomeCmdConDevice *) devices->data))
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


static void on_options_dialog_close (GtkButton *button, GtkWidget *dialog)
{
    GnomeCmdOptionsDialog *options_dialog = GNOME_CMD_OPTIONS_DIALOG (dialog);

    store_general_options (options_dialog);
    store_format_options (options_dialog);
    store_layout_options (options_dialog);
    store_confirmation_options (options_dialog);
    store_filter_options (options_dialog);
    store_network_options (options_dialog);
    store_programs_options (options_dialog);
    store_devices_options (dialog);

    gtk_widget_destroy (dialog);

    gnome_cmd_style_create ();
    gnome_cmd_main_win_update_style (main_win);

    gnome_cmd_data.save();
}


/*******************************
 * Gtk class implementation
 *******************************/


 static void destroy (GtkObject *object)
{
    GnomeCmdOptionsDialog *dialog = GNOME_CMD_OPTIONS_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdOptionsDialogClass *klass)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = GTK_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (gnome_cmd_dialog_get_type ());

    object_class->destroy = destroy;

    widget_class->map = ::map;
}


static void init (GnomeCmdOptionsDialog *dialog)
{
    GtkWidget *options_dialog = GTK_WIDGET (dialog);

    dialog->priv = g_new0 (GnomeCmdOptionsDialog::Private, 1);

    gtk_object_set_data (GTK_OBJECT (options_dialog), "options_dialog", options_dialog);
    gtk_window_set_position (GTK_WINDOW (options_dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (options_dialog), _("Options"));

    dialog->notebook = gtk_notebook_new ();
    gtk_widget_ref (dialog->notebook);
    gtk_object_set_data_full (GTK_OBJECT (options_dialog), "notebook",
                              dialog->notebook, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (dialog->notebook);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), dialog->notebook);

    gtk_container_add (GTK_CONTAINER (dialog->notebook), create_general_tab (options_dialog));
    gtk_container_add (GTK_CONTAINER (dialog->notebook), create_format_tab (options_dialog));
    gtk_container_add (GTK_CONTAINER (dialog->notebook), create_layout_tab (options_dialog));
    gtk_container_add (GTK_CONTAINER (dialog->notebook), create_confirmation_tab (options_dialog));
    gtk_container_add (GTK_CONTAINER (dialog->notebook), create_filter_tab (options_dialog));
    gtk_container_add (GTK_CONTAINER (dialog->notebook), create_network_tab (options_dialog));
    gtk_container_add (GTK_CONTAINER (dialog->notebook), create_programs_tab (options_dialog));
    gtk_container_add (GTK_CONTAINER (dialog->notebook), create_devices_tab (options_dialog));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (dialog->notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (dialog->notebook), GnomeCmdOptionsDialog::TAB_GENERAL),
        gtk_label_new (_("General")));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (dialog->notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (dialog->notebook), GnomeCmdOptionsDialog::TAB_FORMAT),
        gtk_label_new (_("Format")));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (dialog->notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (dialog->notebook), GnomeCmdOptionsDialog::TAB_LAYOUT),
        gtk_label_new (_("Layout")));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (dialog->notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (dialog->notebook), GnomeCmdOptionsDialog::TAB_CONFIRMATION),
        gtk_label_new (_("Confirmation")));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (dialog->notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (dialog->notebook), GnomeCmdOptionsDialog::TAB_FILTERS),
        gtk_label_new (_("Filters")));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (dialog->notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (dialog->notebook), GnomeCmdOptionsDialog::TAB_NETWORK),
        gtk_label_new (_("Network")));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (dialog->notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (dialog->notebook), GnomeCmdOptionsDialog::TAB_PROGRAMS),
        gtk_label_new (_("Programs")));

    gtk_notebook_set_tab_label (
        GTK_NOTEBOOK (dialog->notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (dialog->notebook), GnomeCmdOptionsDialog::TAB_DEVICES),
        gtk_label_new (_("Devices")));


    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CLOSE,
                                 GTK_SIGNAL_FUNC (on_options_dialog_close), dialog);
}


/***********************************
 * Public functions
 ***********************************/


GtkType gnome_cmd_options_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdOptionsDialog",
            sizeof (GnomeCmdOptionsDialog),
            sizeof (GnomeCmdOptionsDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gnome_cmd_dialog_get_type (), &dlg_info);
    }

    return dlg_type;
}


GtkWidget *gnome_cmd_options_dialog_new ()
{
    return (GtkWidget *) gtk_type_new (gnome_cmd_options_dialog_get_type ());
}
