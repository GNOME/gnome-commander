/**
 * @file gnome-cmd-advrename-profile-component.cc
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

#include <vector>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-advrename-profile-component.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-convert.h"
#include "gnome-cmd-treeview.h"
#include "utils.h"

// This define is used to remove warnings for CLAMP macro when doing CLAMP((uint) 1, 0, 2)
#define MYCLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) <= (low)) ? (low) : (x)))

using namespace std;


struct GnomeCmdAdvrenameProfileComponentClass
{
    GtkBoxClass parent_class;

    void (* template_changed) (GnomeCmdAdvrenameProfileComponent *component);
    void (* counter_changed) (GnomeCmdAdvrenameProfileComponent *component);
    void (* regex_changed) (GnomeCmdAdvrenameProfileComponent *component);
};


enum {TEMPLATE_CHANGED, COUNTER_CHANGED, REGEX_CHANGED, LAST_SIGNAL};

static guint signals[LAST_SIGNAL] = { 0 };


struct GnomeCmdAdvrenameProfileComponent::Private
{
    GnomeCmdConvertFunc convert_case;
    GnomeCmdConvertFunc trim_blanks;

    GtkWidget *template_entry;
    GtkWidget *template_combo;

    GtkWidget *counter_start_spin;
    GtkWidget *counter_step_spin;
    GtkWidget *counter_digits_combo;

    GtkWidget *metadata_button;

    GtkTreeModel *regex_model;
    GtkWidget *regex_view;

    GtkWidget *regex_add_button;
    GtkWidget *regex_edit_button;
    GtkWidget *regex_remove_button;
    GtkWidget *regex_remove_all_button;

    GtkWidget *case_combo;
    GtkWidget *trim_combo;

    gchar *sample_fname;

    Private();
    ~Private();

    void fill_regex_model(const GnomeCmdData::AdvrenameConfig::Profile &profile);

    void insert_tag(const gchar *text);

    static void on_template_entry_changed(GtkEntry *entry, GnomeCmdAdvrenameProfileComponent *component);

    static void on_counter_start_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component);
    static void on_counter_step_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component);
    static void on_counter_digits_combo_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component);

    static void on_regex_model_row_deleted (GtkTreeModel *treemodel, GtkTreePath *path, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_add_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_edit_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_remove_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_remove_all_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameProfileComponent *component);

    static void on_regex_changed (GnomeCmdAdvrenameProfileComponent *component);

    static void on_case_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameProfileComponent *component);
    static void on_trim_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameProfileComponent *component);
};


static GMenuModel *create_directory_tag_menu ()
{
    GMenu *menu = g_menu_new ();
    g_menu_append (menu, _("Grandparent"),  "advrenametag.insert-text-tag('$g')");
    g_menu_append (menu, _("Parent"),       "advrenametag.insert-text-tag('$p')");
    return G_MENU_MODEL (menu);
}


static GMenuModel *create_file_tag_menu ()
{
    GMenu *menu = g_menu_new ();
    g_menu_append (menu, _("File name"),                            "advrenametag.insert-text-tag('$N')");
    g_menu_append (menu, _("File name (range)"),                    "advrenametag.insert-range('$N')");
    g_menu_append (menu, _("File name without extension"),          "advrenametag.insert-text-tag('$n')");
    g_menu_append (menu, _("File name without extension (range)"),  "advrenametag.insert-range('$n')");
    g_menu_append (menu, _("File extension"),                       "advrenametag.insert-text-tag('$e')");
    return G_MENU_MODEL (menu);
}


static GMenuModel *create_counter_tag_menu ()
{
    GMenu *menu = g_menu_new ();
    g_menu_append (menu, _("Counter"),                              "advrenametag.insert-text-tag('$c')");
    g_menu_append (menu, _("Counter (width)"),                      "advrenametag.insert-text-tag('$c(2)')");
    g_menu_append (menu, _("Counter (auto)"),                       "advrenametag.insert-text-tag('$c(a)')");
    g_menu_append (menu, _("Hexadecimal random number (width)"),    "advrenametag.insert-text-tag('$x(8)')");
    return G_MENU_MODEL (menu);
}


static GMenuModel *create_date_tag_menu ()
{
    GMenu *menu = g_menu_new ();

    GMenu *date_menu = g_menu_new ();
    g_menu_append (date_menu, _("<locale>"),    "advrenametag.insert-text-tag('%x')");
    g_menu_append (date_menu, _("yyyy-mm-dd"),  "advrenametag.insert-text-tag('%Y-%m-%d')");
    g_menu_append (date_menu, _("yy-mm-dd"),    "advrenametag.insert-text-tag('%y-%m-%d')");
    g_menu_append (date_menu, _("yy.mm.dd"),    "advrenametag.insert-text-tag('%y.%m.%d')");
    g_menu_append (date_menu, _("yymmdd"),      "advrenametag.insert-text-tag('%y%m%d')");
    g_menu_append (date_menu, _("dd.mm.yy"),    "advrenametag.insert-text-tag('%d.%m.%y')");
    g_menu_append (date_menu, _("mm-dd-yy"),    "advrenametag.insert-text-tag('%m-%d-%y')");
    g_menu_append (date_menu, _("yyyy"),        "advrenametag.insert-text-tag('%Y')");
    g_menu_append (date_menu, _("yy"),          "advrenametag.insert-text-tag('%y')");
    g_menu_append (date_menu, _("mm"),          "advrenametag.insert-text-tag('%m')");
    g_menu_append (date_menu, _("mmm"),         "advrenametag.insert-text-tag('%b')");
    g_menu_append (date_menu, _("dd"),          "advrenametag.insert-text-tag('%d')");
    g_menu_append_submenu (menu, _("Date"), G_MENU_MODEL (date_menu));

    GMenu *time_menu = g_menu_new ();
    g_menu_append (time_menu, _("<locale>"),    "advrenametag.insert-text-tag('%X')");
    g_menu_append (time_menu, _("HH.MM.SS"),    "advrenametag.insert-text-tag('%H.%M.%S')");
    g_menu_append (time_menu, _("HH-MM-SS"),    "advrenametag.insert-text-tag('%H-%M-%S')");
    g_menu_append (time_menu, _("HHMMSS"),      "advrenametag.insert-text-tag('%H%M%S')");
    g_menu_append (time_menu, _("HH"),          "advrenametag.insert-text-tag('%H')");
    g_menu_append (time_menu, _("MM"),          "advrenametag.insert-text-tag('%M')");
    g_menu_append (time_menu, _("SS"),          "advrenametag.insert-text-tag('%S')");
    g_menu_append_submenu (menu, _("Time"), G_MENU_MODEL (time_menu));

    return G_MENU_MODEL (menu);
}


inline GtkTreeModel *create_regex_model ();

inline GtkWidget *create_regex_view ();


inline GnomeCmdAdvrenameProfileComponent::Private::Private()
{
    convert_case = gcmd_convert_unchanged;
    trim_blanks = gcmd_convert_strip;
    regex_model = NULL;
    sample_fname = NULL;
    template_entry = NULL;
    template_combo = NULL;
    counter_start_spin = NULL;
    counter_step_spin = NULL;
    counter_digits_combo = NULL;
    regex_view = NULL;
    regex_add_button = NULL;
    regex_edit_button = NULL;
    regex_remove_button = NULL;
    regex_remove_all_button = NULL;
    case_combo = NULL;
    trim_combo = NULL;
}


inline GnomeCmdAdvrenameProfileComponent::Private::~Private()
{
    g_clear_object (&template_entry);
    g_clear_object (&regex_model);
    g_clear_pointer (&sample_fname, g_free);
}


inline gboolean model_is_empty(GtkTreeModel *tree_model)
{
    GtkTreeIter iter;

    return !gtk_tree_model_get_iter_first (tree_model, &iter);
}


void GnomeCmdAdvrenameProfileComponent::Private::fill_regex_model(const GnomeCmdData::AdvrenameConfig::Profile &profile)
{
    if (!regex_model)
        return;

    GtkTreeIter iter;

    for (vector<GnomeCmd::ReplacePattern>::const_iterator r=profile.regexes.begin(); r!=profile.regexes.end(); ++r)
    {
        GnomeCmd::RegexReplace *rx = new GnomeCmd::RegexReplace(r->pattern, r->replacement, r->match_case);

        gtk_list_store_append (GTK_LIST_STORE (regex_model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (regex_model), &iter,
                            COL_MALFORMED_REGEX, !*rx,
                            COL_PATTERN, rx->pattern.c_str(),
                            COL_REPLACE, rx->replacement.c_str(),
                            COL_MATCH_CASE, rx->match_case,
                            COL_MATCH_CASE_LABEL, rx->match_case ? _("Yes") : _("No"),
                            -1);

        delete rx;
    }
}


static GtkWidget *create_button_with_menu(gchar *label_text, GMenuModel *model)
{
    GtkWidget *button = gtk_menu_button_new ();
    gtk_menu_button_set_label (GTK_MENU_BUTTON (button), label_text);
    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), model);
    return button;
}


inline void GnomeCmdAdvrenameProfileComponent::Private::insert_tag(const gchar *text)
{
    GtkEditable *editable = (GtkEditable *) template_entry;
    gint pos0 = gtk_editable_get_position (editable);
    gint pos;

    gtk_editable_insert_text (editable, text, -1, &pos);
    gtk_editable_set_position (editable, pos);
    gtk_widget_grab_focus ((GtkWidget *) editable);
    gtk_editable_select_region (editable, pos0, pos);
}


static void insert_text_tag(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto component = static_cast<GnomeCmdAdvrenameProfileComponent*>(user_data);
    const gchar *tag = g_variant_get_string (parameter, nullptr);

    component->priv->insert_tag(tag);
}


extern "C" void get_selected_range_r(GtkWindow *parent_window, const gchar *placeholder, const gchar *filename, gpointer user_data);
extern "C" void get_selected_range_done(const gchar *rtag, gpointer user_data);

void get_selected_range_done(const gchar *rtag, gpointer user_data)
{
    auto component = static_cast<GnomeCmdAdvrenameProfileComponent*>(user_data);
    component->priv->insert_tag(rtag);
}


static void insert_range(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto component = static_cast<GnomeCmdAdvrenameProfileComponent*>(user_data);
    const gchar *tag = g_variant_get_string (parameter, nullptr);

    gchar *fname = NULL;

    if (!strcmp(tag, "$n") && component->priv->sample_fname)
    {
        fname = g_strdup (component->priv->sample_fname);
        gchar *ext = g_utf8_strrchr (fname, -1, '.');
        if (ext)
            *ext = 0;
    }

    GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_root (*component));
    get_selected_range_r(parent_window, tag, fname ? fname : g_strdup(component->priv->sample_fname), user_data);
}


G_DEFINE_TYPE (GnomeCmdAdvrenameProfileComponent, gnome_cmd_advrename_profile_component, GTK_TYPE_BOX)


void GnomeCmdAdvrenameProfileComponent::Private::on_template_entry_changed(GtkEntry *entry, GnomeCmdAdvrenameProfileComponent *component)
{
    g_signal_emit (component, signals[TEMPLATE_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_counter_start_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component)
{
    component->profile.counter_start = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
    g_signal_emit (component, signals[COUNTER_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_counter_step_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component)
{
    component->profile.counter_step = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
    g_signal_emit (component, signals[COUNTER_CHANGED], 0);
}

void GnomeCmdAdvrenameProfileComponent::Private::on_counter_digits_combo_value_changed (GtkWidget *combo, GnomeCmdAdvrenameProfileComponent *component)
{
    component->profile.counter_width = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
    component->profile.counter_width = MYCLAMP(component->profile.counter_width, 0, 16);
    g_signal_emit (component, signals[COUNTER_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_model_row_deleted (GtkTreeModel *treemodel, GtkTreePath *path, GnomeCmdAdvrenameProfileComponent *component)
{
    g_signal_emit (component, signals[REGEX_CHANGED], 0);
}


extern "C" void gnome_cmd_advrename_profile_component_regex_add (GtkWidget *component, GtkTreeView *tree_view);

void GnomeCmdAdvrenameProfileComponent::Private::on_regex_add_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component)
{
    gnome_cmd_advrename_profile_component_regex_add (GTK_WIDGET (component), GTK_TREE_VIEW (component->priv->regex_view));
}


extern "C" void gnome_cmd_advrename_profile_component_regex_edit (GtkWidget *component, GtkTreeView *tree_view);

void GnomeCmdAdvrenameProfileComponent::Private::on_regex_edit_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component)
{
    gnome_cmd_advrename_profile_component_regex_edit (GTK_WIDGET (component), GTK_TREE_VIEW (component->priv->regex_view));
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_changed (GnomeCmdAdvrenameProfileComponent *component)
{
    bool empty = model_is_empty (component->priv->regex_model);

    gtk_widget_set_sensitive (component->priv->regex_edit_button, !empty);
    gtk_widget_set_sensitive (component->priv->regex_remove_button, !empty);
    gtk_widget_set_sensitive (component->priv->regex_remove_all_button, !empty);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_remove_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (component->priv->regex_view);
    GtkTreeIter i;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), NULL, &i))
    {
        gtk_list_store_remove (GTK_LIST_STORE (component->priv->regex_model), &i);
        on_regex_changed (component);
    }
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_remove_all_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component)
{
    g_signal_handlers_block_by_func (component->priv->regex_model, gpointer (on_regex_model_row_deleted), component);
    gtk_list_store_clear (GTK_LIST_STORE (component->priv->regex_model));
    g_signal_handlers_unblock_by_func (component->priv->regex_model, gpointer (on_regex_model_row_deleted), component);

    g_signal_emit (component, signals[REGEX_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameProfileComponent *component)
{
    on_regex_edit_btn_clicked (NULL, component);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_case_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameProfileComponent *component)
{
    gint item = gtk_combo_box_get_active (combo);

    switch (item)
    {
        case 0: component->priv->convert_case = gcmd_convert_unchanged; break;
        case 1: component->priv->convert_case = gcmd_convert_lowercase; break;
        case 2: component->priv->convert_case = gcmd_convert_uppercase; break;
        case 3: component->priv->convert_case = gcmd_convert_sentence_case; break;
        case 4: component->priv->convert_case = gcmd_convert_initial_caps; break;
        case 5: component->priv->convert_case = gcmd_convert_toggle_case; break;

        default:
            return;
    }

    component->profile.case_conversion = item;
    g_signal_emit (component, signals[REGEX_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_trim_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameProfileComponent *component)
{
    gint item = gtk_combo_box_get_active (combo);

    switch (item)
    {
        case 0: component->priv->trim_blanks = gcmd_convert_unchanged; break;
        case 1: component->priv->trim_blanks = gcmd_convert_ltrim; break;
        case 2: component->priv->trim_blanks = gcmd_convert_rtrim; break;
        case 3: component->priv->trim_blanks = gcmd_convert_strip; break;

        default:
            return;
    }

    component->profile.trim_blanks = item;
    g_signal_emit (component, signals[REGEX_CHANGED], 0);
}


static void gnome_cmd_advrename_profile_component_init (GnomeCmdAdvrenameProfileComponent *component)
{
    g_object_set (component, "orientation", GTK_ORIENTATION_VERTICAL, NULL);

    static GActionEntry action_entries[] = {
        { "insert-text-tag",    insert_text_tag,    "s" },
        { "insert-range",       insert_range,       "s" }
    };
    GSimpleActionGroup* action_group = g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (action_group), action_entries, G_N_ELEMENTS(action_entries), component);
    gtk_widget_insert_action_group (GTK_WIDGET (component), "advrenametag", G_ACTION_GROUP (action_group));

    component->priv = new GnomeCmdAdvrenameProfileComponent::Private;

    GtkWidget *label;
    GtkWidget *grid;
    GtkWidget *combo;
    GtkWidget *hbox;
    GtkWidget *spin;
    GtkWidget *button;

    gchar *str;

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_box_append (GTK_BOX (component), hbox);


    // Template
    {
        GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_hexpand (vbox, TRUE);
        gtk_box_append (GTK_BOX (hbox), vbox);

        str = g_strdup_printf ("<b>%s</b>", _("_Template"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (vbox), label);

        {
            GtkWidget *local_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
            gtk_widget_set_margin_start (local_vbox, 12);
            gtk_box_append (GTK_BOX (vbox), local_vbox);

            component->priv->template_combo = combo = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING)));
            component->priv->template_entry = gtk_combo_box_get_child (GTK_COMBO_BOX (component->priv->template_combo));
            gtk_entry_set_activates_default (GTK_ENTRY (component->priv->template_entry), TRUE);
            gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
            gtk_box_append (GTK_BOX (local_vbox), combo);
            g_object_ref (component->priv->template_entry);

            GtkWidget *local_bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
            GtkSizeGroup* local_bbox_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
            gtk_box_append (GTK_BOX (local_vbox), local_bbox);

            button = create_button_with_menu (_("Directory"), create_directory_tag_menu ());
            gtk_size_group_add_widget (local_bbox_size_group, button);
            gtk_box_append (GTK_BOX (local_bbox), button);

            button = create_button_with_menu (_("File"), create_file_tag_menu ());
            gtk_size_group_add_widget (local_bbox_size_group, button);
            gtk_box_append (GTK_BOX (local_bbox), button);

            button = create_button_with_menu (_("Counter"), create_counter_tag_menu ());
            gtk_size_group_add_widget (local_bbox_size_group, button);
            gtk_box_append (GTK_BOX (local_bbox), button);

            button = create_button_with_menu (_("Date"), create_date_tag_menu ());
            gtk_size_group_add_widget (local_bbox_size_group, button);
            gtk_box_append (GTK_BOX (local_bbox), button);

            component->priv->metadata_button = create_button_with_menu (_("Metatag"), nullptr);
            gtk_size_group_add_widget (local_bbox_size_group, component->priv->metadata_button);
            gtk_box_append (GTK_BOX (local_bbox), component->priv->metadata_button);
        }
    }


    // Counter
    {
        GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_box_append (GTK_BOX (hbox), vbox);

        str = g_strdup_printf ("<b>%s</b>", _("Counter"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (vbox), label);

        grid = gtk_grid_new ();
        gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
        gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
        gtk_widget_set_margin_start (grid, 12);
        gtk_box_append (GTK_BOX (vbox), grid);

        label = gtk_label_new_with_mnemonic (_("_Start:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        component->priv->counter_start_spin = spin = gtk_spin_button_new_with_range (0, 1000000, 1);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
        gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
        gtk_grid_attach (GTK_GRID (grid), spin, 1, 0, 1, 1);

        label = gtk_label_new_with_mnemonic (_("Ste_p:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        component->priv->counter_step_spin = spin = gtk_spin_button_new_with_range (-1000, 1000, 1);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
        gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
        gtk_grid_attach (GTK_GRID (grid), spin, 1, 1, 1, 1);

        label = gtk_label_new_with_mnemonic (_("Di_gits:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        component->priv->counter_digits_combo = combo = gtk_combo_box_text_new ();

        static const char *digit_widths[] = {N_("auto"),"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16",NULL};

        for (const char **i=digit_widths; *i; ++i)
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), *i);

        gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
        gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
        gtk_grid_attach (GTK_GRID (grid), combo, 1, 2, 1, 1);
    }


    // Regex
    {
        GtkWidget *bbox;
        str = g_strdup_printf ("<b>%s</b>", _("Regex replacing"));
        label = gtk_label_new (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (component), label);

        grid = gtk_grid_new ();
        gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
        gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
        gtk_widget_set_margin_top (grid, 6);
        gtk_widget_set_margin_bottom (grid, 12);
        gtk_widget_set_margin_start (grid, 12);
        gtk_box_append (GTK_BOX (component), grid);

        GtkWidget *scrolled_window = gtk_scrolled_window_new ();
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled_window), TRUE);
        gtk_widget_set_hexpand (scrolled_window, TRUE);
        gtk_widget_set_vexpand (scrolled_window, TRUE);
        gtk_grid_attach (GTK_GRID (grid), scrolled_window, 0, 0, 1, 1);

        component->priv->regex_view = create_regex_view ();
        gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), component->priv->regex_view);

        bbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        gtk_grid_attach (GTK_GRID (grid), bbox, 1, 0, 1, 1);

        component->priv->regex_add_button = button = gtk_button_new_with_mnemonic (_("_Add"));
        gtk_box_append (GTK_BOX (bbox), button);

        component->priv->regex_edit_button = button = gtk_button_new_with_mnemonic (_("_Edit"));
        gtk_box_append (GTK_BOX (bbox), button);

        component->priv->regex_remove_button = button = gtk_button_new_with_mnemonic (_("_Remove"));
        gtk_box_append (GTK_BOX (bbox), button);

        component->priv->regex_remove_all_button = button = gtk_button_new_with_mnemonic (_("Remove A_ll"));
        gtk_box_append (GTK_BOX (bbox), button);
    }


    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom (hbox, 18);
    gtk_box_append (GTK_BOX (component), hbox);

    // Case conversion & blank trimming
    {
        str = g_strdup_printf ("<b>%s</b>", _("Case"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (hbox), label);

        component->priv->case_combo = combo = gtk_combo_box_text_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

        gchar *case_modes[] = {
                                _("<unchanged>"),
                                _("lowercase"),
                                _("UPPERCASE"),
                                NULL,
                                _("Sentence case"),       //  FIXME
                                _("Initial Caps"),        //  FIXME
                                _("tOGGLE cASE"),         //  FIXME
                                NULL
                              };

        for (gchar **items=case_modes; *items; ++items)
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(*items));

        gtk_box_append (GTK_BOX (hbox), combo);


        str = g_strdup_printf ("<b>%s</b>", _("Trim blanks"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (hbox), label);

        component->priv->trim_combo = combo = gtk_combo_box_text_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

        gchar *trim_modes[] = {
                                  _("<none>"),
                                  _("leading"),
                                  _("trailing"),
                                  _("leading and trailing"),
                                  NULL
                              };

        for (gchar **items=trim_modes; *items; ++items)
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(*items));

        gtk_box_append (GTK_BOX (hbox), combo);
    }

    g_signal_connect (component, "regex-changed", G_CALLBACK (GnomeCmdAdvrenameProfileComponent::Private::on_regex_changed), nullptr);
}


static void gnome_cmd_advrename_profile_component_finalize (GObject *object)
{
    GnomeCmdAdvrenameProfileComponent *component = GNOME_CMD_ADVRENAME_PROFILE_COMPONENT (object);

    delete component->priv;

    G_OBJECT_CLASS (gnome_cmd_advrename_profile_component_parent_class)->finalize (object);
}


static void gnome_cmd_advrename_profile_component_class_init (GnomeCmdAdvrenameProfileComponentClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_advrename_profile_component_finalize;

    signals[TEMPLATE_CHANGED] =
        g_signal_new ("template-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdAdvrenameProfileComponentClass, template_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[COUNTER_CHANGED] =
        g_signal_new ("counter-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdAdvrenameProfileComponentClass, counter_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[REGEX_CHANGED] =
        g_signal_new ("regex-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdAdvrenameProfileComponentClass, regex_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);
}


GnomeCmdAdvrenameProfileComponent::GnomeCmdAdvrenameProfileComponent(GnomeCmdData::AdvrenameConfig::Profile &p,
                                                                     GnomeCmdFileMetadataService *file_metadata_service)
    : profile(p)
{
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
    gtk_menu_button_set_menu_model (
        GTK_MENU_BUTTON (priv->metadata_button),
        G_MENU_MODEL (gnome_cmd_file_metadata_service_create_menu (file_metadata_service, "advrenametag.insert-text-tag")));

    // Template
    gtk_widget_grab_focus (priv->template_entry);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
    g_signal_connect (priv->template_combo, "changed", G_CALLBACK (Private::on_template_entry_changed), this);

    // Counter
    g_signal_connect (priv->counter_start_spin, "value-changed", G_CALLBACK (Private::on_counter_start_spin_value_changed), this);
    g_signal_connect (priv->counter_step_spin, "value-changed", G_CALLBACK (Private::on_counter_step_spin_value_changed), this);
    g_signal_connect (priv->counter_digits_combo, "changed", G_CALLBACK (Private::on_counter_digits_combo_value_changed), this);

    // Regex
    priv->regex_model = create_regex_model ();
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->regex_view), priv->regex_model);

    g_signal_connect (priv->regex_model, "row-deleted", G_CALLBACK (Private::on_regex_model_row_deleted), this);
    g_signal_connect (priv->regex_view, "row-activated", G_CALLBACK (Private::on_regex_view_row_activated), this);
    g_signal_connect (priv->regex_add_button, "clicked", G_CALLBACK (Private::on_regex_add_btn_clicked), this);
    g_signal_connect (priv->regex_edit_button, "clicked", G_CALLBACK (Private::on_regex_edit_btn_clicked), this);
    g_signal_connect (priv->regex_remove_button, "clicked", G_CALLBACK (Private::on_regex_remove_btn_clicked), this);
    g_signal_connect (priv->regex_remove_all_button, "clicked", G_CALLBACK (Private::on_regex_remove_all_btn_clicked), this);

    // Case conversion & blank trimming
    g_signal_connect (priv->case_combo, "changed", G_CALLBACK (Private::on_case_combo_changed), this);
    g_signal_connect (priv->trim_combo, "changed", G_CALLBACK (Private::on_trim_combo_changed), this);
}


inline GtkTreeModel *create_regex_model ()
{
    GtkTreeModel *model = GTK_TREE_MODEL (gtk_list_store_new (GnomeCmdAdvrenameProfileComponent::NUM_REGEX_COLS,
                                                              G_TYPE_BOOLEAN,
                                                              G_TYPE_STRING,
                                                              G_TYPE_STRING,
                                                              G_TYPE_BOOLEAN,
                                                              G_TYPE_STRING));
    return model;
}


static void copy_regex_model(GtkTreeModel *model, vector<GnomeCmd::ReplacePattern> &v)
{
    v.clear();

    GtkTreeIter i;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (model, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (model, &i))
    {
        gchar *pattern = nullptr;
        gchar *replacement = nullptr;
        gboolean match_case;

        gtk_tree_model_get (model, &i,
            GnomeCmdAdvrenameProfileComponent::COL_PATTERN, &pattern,
            GnomeCmdAdvrenameProfileComponent::COL_REPLACE, &replacement,
            GnomeCmdAdvrenameProfileComponent::COL_MATCH_CASE, &match_case,
            -1);

        v.push_back(GnomeCmd::ReplacePattern(pattern, replacement, match_case));

        g_free (pattern);
        g_free (replacement);
    }
}


inline GtkWidget *create_regex_view ()
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "reorderable", TRUE,
                  "enable-search", FALSE,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameProfileComponent::COL_PATTERN, _("Search for"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameProfileComponent::COL_MALFORMED_REGEX);
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Regex pattern"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameProfileComponent::COL_REPLACE, _("Replace with"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameProfileComponent::COL_MALFORMED_REGEX);
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Replacement"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameProfileComponent::COL_MATCH_CASE_LABEL, _("Match case"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameProfileComponent::COL_MALFORMED_REGEX);
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Case sensitive matching"));

    g_object_set (renderer,
                  "xalign", 0.0,
                  NULL);

    return view;
}


void GnomeCmdAdvrenameProfileComponent::update()
{
    set_template_history(gnome_cmd_data.advrename_defaults.templates.ents);

    gtk_editable_set_text (GTK_EDITABLE (priv->template_entry), profile.template_string.empty() ? "$N" : profile.template_string.c_str());
    gtk_editable_set_position (GTK_EDITABLE (priv->template_entry), -1);
    gtk_editable_select_region (GTK_EDITABLE (priv->template_entry), -1, -1);

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->counter_start_spin), profile.counter_start);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->counter_step_spin), profile.counter_step);

    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->counter_digits_combo), profile.counter_width);

    if (!model_is_empty(priv->regex_model))
    {
        g_signal_handlers_block_by_func (priv->regex_model, gpointer (Private::on_regex_model_row_deleted), this);
        gtk_list_store_clear (GTK_LIST_STORE (priv->regex_model));
        g_signal_handlers_unblock_by_func (priv->regex_model, gpointer (Private::on_regex_model_row_deleted), this);
    }

    priv->fill_regex_model(profile);

    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->case_combo), profile.case_conversion);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->trim_combo), profile.trim_blanks);

    g_signal_emit (this, signals[REGEX_CHANGED], 0);
}


const gchar *GnomeCmdAdvrenameProfileComponent::get_template_entry() const
{
    return gtk_editable_get_text (GTK_EDITABLE (priv->template_entry));
}


void GnomeCmdAdvrenameProfileComponent::set_template_history(GList *history)
{
    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->template_combo));
    gtk_list_store_clear (GTK_LIST_STORE (store));

    for (GList *i=history; i; i=i->next)
    {
        GtkTreeIter iter;
        gtk_list_store_append (GTK_LIST_STORE (store), &iter);
        gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, (const gchar *) i->data, -1);
    }
}


void GnomeCmdAdvrenameProfileComponent::set_sample_fname(const gchar *fname)
{
    g_free (priv->sample_fname);
    priv->sample_fname = g_strdup (fname);
}


void GnomeCmdAdvrenameProfileComponent::copy()
{
    const char *template_entry = get_template_entry();

    profile.template_string =  template_entry && *template_entry ? template_entry : "$N";
    copy_regex_model(priv->regex_model, profile.regexes);
}


void GnomeCmdAdvrenameProfileComponent::copy(GnomeCmdData::AdvrenameConfig::Profile &p)
{
    const char *template_entry = get_template_entry();

    profile.template_string =  template_entry && *template_entry ? template_entry : "$N";
    copy_regex_model(priv->regex_model, p.regexes);

    if (&p==&profile)
        return;

    p.name.clear();
    p.template_string = profile.template_string;
    p.counter_start = profile.counter_start;
    p.counter_width = profile.counter_width;
    p.counter_step = profile.counter_step;
    p.case_conversion = profile.case_conversion;
    p.trim_blanks = profile.trim_blanks;
}


gchar *GnomeCmdAdvrenameProfileComponent::convert_case(gchar *string)
{
    return priv->convert_case(string);
}


gchar *GnomeCmdAdvrenameProfileComponent::trim_blanks(gchar *string)
{
    return priv->trim_blanks(string);
}

std::vector<GnomeCmd::RegexReplace> GnomeCmdAdvrenameProfileComponent::get_valid_regexes()
{
    GtkTreeModel *model = priv->regex_model;
    GtkTreeIter i;
    vector<GnomeCmd::RegexReplace> v;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (model, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (model, &i))
    {
        gboolean malformed = false;
        gchar *pattern = nullptr;
        gchar *replacement = nullptr;
        gboolean match_case = false;

        gtk_tree_model_get (model, &i,
            GnomeCmdAdvrenameProfileComponent::COL_MALFORMED_REGEX, &malformed,
            GnomeCmdAdvrenameProfileComponent::COL_PATTERN, &pattern,
            GnomeCmdAdvrenameProfileComponent::COL_REPLACE, &replacement,
            GnomeCmdAdvrenameProfileComponent::COL_MATCH_CASE, &match_case,
            -1);

        if (!malformed)
            v.push_back(GnomeCmd::RegexReplace(pattern, replacement, match_case));

        g_free (pattern);
        g_free (replacement);
    }

    return v;
}


GnomeCmdAdvrenameProfileComponent *gnome_cmd_advrename_profile_component_new (
    GnomeCmdData::AdvrenameConfig::Profile *profile,
    GnomeCmdFileMetadataService *file_metadata_service,
    GtkSizeGroup *labels_size_group)
{
    g_return_val_if_fail (profile != nullptr, nullptr);
    auto component = new GnomeCmdAdvrenameProfileComponent(*profile, file_metadata_service);
    component->update();
    return component;
}


void gnome_cmd_advrename_profile_component_update (GnomeCmdAdvrenameProfileComponent *component)
{
    component->update();
}


void gnome_cmd_advrename_profile_component_copy (GnomeCmdAdvrenameProfileComponent *component)
{
    component->copy();
}
