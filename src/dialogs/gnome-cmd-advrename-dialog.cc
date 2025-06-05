/**
 * @file gnome-cmd-advrename-dialog.cc
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
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtk.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-convert.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-advrename-profile-component.h"
#include "gnome-cmd-treeview.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"

using namespace std;


enum {
    GCMD_RESPONSE_PROFILES=123,
    GCMD_RESPONSE_RESET
};


enum {
    COL_FILE,
    COL_NAME,
    COL_NEW_NAME,
    COL_SIZE,
    COL_DATE,
    COL_RENAME_FAILED,
    COL_METADATA,
    NUM_FILE_COLS
};


struct GnomeCmdAdvrenameDialogPrivate
{
    GnomeCmdData::AdvrenameConfig *config;

    GtkTreeModel *files;

    GtkWidget *vbox {NULL};
    GnomeCmdAdvrenameProfileComponent *profile_component {NULL};
    GtkWidget *files_view {NULL};
    GtkWidget *profile_menu_button {NULL};

    GnomeCmdFileMetadataService *file_metadata_service {NULL};
};


extern "C" void gnome_cmd_advrename_dialog_update_template (GnomeCmdAdvrenameDialog *dialog);
extern "C" void gnome_cmd_advrename_dialog_update_new_filenames (GnomeCmdAdvrenameDialog *dialog);


static void files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog, GdkRectangle *point_to);

static void on_profile_template_changed (GnomeCmdAdvrenameProfileComponent *component, GnomeCmdAdvrenameDialog *dialog);
static void on_profile_counter_changed (GnomeCmdAdvrenameProfileComponent *component, GnomeCmdAdvrenameDialog *dialog);
static void on_profile_regex_changed (GnomeCmdAdvrenameProfileComponent *component, GnomeCmdAdvrenameDialog *dialog);
static void on_files_model_row_deleted (GtkTreeModel *files, GtkTreePath *path, GnomeCmdAdvrenameDialog *dialog);
static void on_files_view_popup_menu__remove (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_files_view_popup_menu__view_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_files_view_popup_menu__show_properties (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_files_view_popup_menu__update_files (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_files_view_button_pressed (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
static void on_files_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameDialog *dialog);
static void on_files_view_cursor_changed (GtkTreeView *view, GnomeCmdAdvrenameDialog *dialog);

static void on_dialog_show (GtkWidget *widget, GnomeCmdAdvrenameDialog *dialog);
static void on_dialog_response (GnomeCmdAdvrenameDialog *dialog, int response_id, gpointer data);


GnomeCmdAdvrenameDialogPrivate *advrename_dialog_priv (GnomeCmdAdvrenameDialog *dialog)
{
    return (GnomeCmdAdvrenameDialogPrivate *) g_object_get_data (G_OBJECT (dialog), "priv");
}


inline gboolean model_is_empty(GtkTreeModel *tree_model)
{
    GtkTreeIter iter;

    return !gtk_tree_model_get_iter_first (tree_model, &iter);
}


inline GMenu *create_placeholder_menu(GnomeCmdData::AdvrenameConfig *cfg)
{
    GMenu *menu = g_menu_new ();

    g_menu_append (menu, _("_Save Profile As…"), "advrename.save-profile");

    guint count = g_list_model_get_n_items (G_LIST_MODEL (cfg->profiles));
    if (count > 0)
    {
        g_menu_append (menu, _("_Manage Profiles…"), "advrename.manage-profiles");

        GMenu *profiles = g_menu_new ();
        for (guint i = 0; i < count; ++i)
        {
            auto p = (AdvancedRenameProfile *) g_list_model_get_item (G_LIST_MODEL (cfg->profiles), i);

            gchar *name;
            g_object_get (p, "name", &name, nullptr);

            GMenuItem *item = g_menu_item_new (name, nullptr);
            g_menu_item_set_action_and_target (item, "advrename.load-profile", "i", i);
            g_menu_append_item (profiles, item);

            g_free (name);
        }
        g_menu_append_section (menu, nullptr, G_MENU_MODEL (profiles));
    }

    return menu;
}


extern "C" void gnome_cmd_advrename_dialog_update_profile_menu (GnomeCmdAdvrenameDialog *dialog)
{
    auto priv = advrename_dialog_priv (dialog);
    GnomeCmdData::AdvrenameConfig *cfg = priv->config;

    gtk_menu_button_set_menu_model (
        GTK_MENU_BUTTON (priv->profile_menu_button),
        G_MENU_MODEL (create_placeholder_menu(cfg)));
}


extern "C" void gnome_cmd_advrename_dialog_do_manage_profiles(GnomeCmdAdvrenameDialog *dialog, GnomeCmdData::AdvrenameConfig *cfg, GnomeCmdFileMetadataService *file_metadata_service, gboolean new_profile);

static void do_manage_profiles(GnomeCmdAdvrenameDialog *dialog, bool new_profile)
{
    auto priv = advrename_dialog_priv (dialog);

    if (new_profile)
        gnome_cmd_advrename_profile_component_copy (priv->profile_component);

    GnomeCmdData::AdvrenameConfig *cfg = priv->config;
    gnome_cmd_advrename_dialog_do_manage_profiles (dialog, cfg, priv->file_metadata_service, new_profile);
}


static void save_profile(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdAdvrenameDialog*>(user_data);
    do_manage_profiles(dialog, true);
}


static void manage_profiles(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdAdvrenameDialog*>(user_data);
    do_manage_profiles(dialog, false);
}


static void load_profile(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdAdvrenameDialog*>(user_data);
    auto priv = advrename_dialog_priv (dialog);
    gsize profile_idx = g_variant_get_int32(parameter);

    GnomeCmdData::AdvrenameConfig *cfg = priv->config;

    auto profile = (AdvancedRenameProfile *) g_list_model_get_item (G_LIST_MODEL (cfg->profiles), profile_idx);
    g_return_if_fail (profile != nullptr);

    gnome_cmd_advanced_rename_profile_copy_from (cfg->default_profile, profile);
    gnome_cmd_advrename_profile_component_set_template_history (priv->profile_component, gnome_cmd_data.advrename_defaults.templates.ents);
    gnome_cmd_advrename_profile_component_update (priv->profile_component);

    gnome_cmd_advrename_dialog_update_new_filenames (dialog);     //  FIXME:  ??
}


inline GtkTreeModel *create_files_model ();
inline GtkWidget *create_files_view ();


void on_profile_template_changed (GnomeCmdAdvrenameProfileComponent *component, GnomeCmdAdvrenameDialog *dialog)
{
    gnome_cmd_advrename_dialog_update_template (dialog);
    gnome_cmd_advrename_dialog_update_new_filenames (dialog);
}


void on_profile_counter_changed (GnomeCmdAdvrenameProfileComponent *component, GnomeCmdAdvrenameDialog *dialog)
{
    gnome_cmd_advrename_dialog_update_new_filenames (dialog);
}


void on_profile_regex_changed (GnomeCmdAdvrenameProfileComponent *component, GnomeCmdAdvrenameDialog *dialog)
{
    gnome_cmd_advrename_dialog_update_new_filenames (dialog);
}


void on_files_model_row_deleted (GtkTreeModel *files, GtkTreePath *path, GnomeCmdAdvrenameDialog *dialog)
{
    gnome_cmd_advrename_dialog_update_new_filenames (dialog);
}


void on_files_view_popup_menu__remove (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdAdvrenameDialog*>(user_data);
    auto priv = advrename_dialog_priv (dialog);
    GtkTreeView *treeview = GTK_TREE_VIEW (priv->files_view);

    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (treeview);

        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

        f->unref();
    }
}


extern "C" void gnome_cmd_file_view (GtkWindow *parent_window, GnomeCmdFile *f, GnomeCmdFileMetadataService *fms);

void on_files_view_popup_menu__view_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdAdvrenameDialog*>(user_data);
    auto priv = advrename_dialog_priv (dialog);
    GtkTreeView *treeview = GTK_TREE_VIEW (priv->files_view);

    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (treeview);
        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);

        if (f)
            gnome_cmd_file_view (GTK_WINDOW (dialog), f, priv->file_metadata_service);
    }
}


extern "C" GType gnome_cmd_file_properties_dialog_get_type ();


static void file_properties_dialog_closed (GtkWindow *props_dialog, gboolean file_changed, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_window_close (GTK_WINDOW (props_dialog));
    if (file_changed)
        on_files_view_popup_menu__update_files (nullptr, nullptr, dialog);
}


void on_files_view_popup_menu__show_properties (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdAdvrenameDialog*>(user_data);
    auto priv = advrename_dialog_priv (dialog);
    GtkTreeView *treeview = GTK_TREE_VIEW (priv->files_view);

    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (treeview);
        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);

        if (f)
        {
            auto file_props_dialog = g_object_new (gnome_cmd_file_properties_dialog_get_type (),
                "transient-for", dialog,
                "file-metadata-service", priv->file_metadata_service,
                "file", f,
                nullptr);
            g_signal_connect (file_props_dialog, "dialog-response", G_CALLBACK (file_properties_dialog_closed), dialog);
            gtk_window_present (GTK_WINDOW (file_props_dialog));
        }
    }
}


void on_files_view_popup_menu__update_files (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdAdvrenameDialog*>(user_data);
    auto priv = advrename_dialog_priv (dialog);

    GtkTreeIter i;
    GnomeCmdFile *f;
    GnomeCmdFileMetadata *metadata;

    //  re-read file attributes, as they could be changed...
    for (gboolean valid_iter=gtk_tree_model_get_iter_first (priv->files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (priv->files, &i))
    {
        gtk_tree_model_get (priv->files, &i,
                            COL_FILE, &f,
                            -1);

        metadata = gnome_cmd_file_metadata_service_extract_metadata (priv->file_metadata_service, f);

        gtk_list_store_set (GTK_LIST_STORE (priv->files), &i,
                            COL_NAME, f->get_name(),
                            COL_SIZE, f->get_size(),
                            COL_DATE, f->get_mdate(FALSE),
                            COL_RENAME_FAILED, FALSE,
                            COL_METADATA, metadata,
                            -1);
    }

    gnome_cmd_advrename_dialog_update_template (dialog);
    gnome_cmd_advrename_dialog_update_new_filenames (dialog);
}


inline void files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog, GdkRectangle *point_to)
{
    GMenu *menu = g_menu_new ();

    GMenu *file_section = g_menu_new ();
    g_menu_append (file_section, _("Remove from file list"), "advrename.file-list-remove");
    g_menu_append (file_section, _("View file"), "advrename.file-list-view");
    g_menu_append (file_section, _("File properties"), "advrename.file-list-properties");
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (file_section));

    g_menu_append (menu, _("Update file list"), "advrename.update-file-list");

    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (popover, GTK_WIDGET (treeview));
    if (point_to)
    {
        gtk_popover_set_pointing_to (GTK_POPOVER (popover), point_to);
    }
    gtk_popover_popup (GTK_POPOVER (popover));
}


void on_files_view_button_pressed (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdAdvrenameDialog*>(user_data);
    auto priv = advrename_dialog_priv (dialog);

    if (n_press == 1)
    {
        auto treeview = GTK_TREE_VIEW (priv->files_view);

        // optional: select row if no row is selected or only one other row is selected
        // (will only do something if you set a tree selection mode
        // as described later in the tutorial)
        if (1)
        {
            GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
            if (gtk_tree_selection_count_selected_rows (selection) <= 1)
            {
                GtkTreePath *path;

                if (gtk_tree_view_get_path_at_pos (treeview,
                                                   (gint) x, (gint) y,
                                                   &path,
                                                   NULL, NULL, NULL))
                {
                    gtk_tree_selection_unselect_all (selection);
                    gtk_tree_selection_select_path (selection, path);
                    gtk_tree_path_free (path);
                }
            }
        }
        GdkRectangle point_to = { (gint) x, (gint) y, 0, 0 };
        files_view_popup_menu (GTK_WIDGET (treeview), dialog, &point_to);
    }
}


void on_files_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameDialog *dialog)
{
    on_files_view_popup_menu__show_properties (nullptr, nullptr, dialog);
}


void on_files_view_cursor_changed (GtkTreeView *view, GnomeCmdAdvrenameDialog *dialog)
{
    auto priv = advrename_dialog_priv (dialog);

    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (view);
        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);

        if (f)
            gnome_cmd_advrename_profile_component_set_sample_fname (priv->profile_component, f->get_name());
    }
}


void on_dialog_show(GtkWidget *widget, GnomeCmdAdvrenameDialog *dialog)
{
    auto priv = advrename_dialog_priv (dialog);
    gnome_cmd_advrename_profile_component_set_template_history (priv->profile_component, gnome_cmd_data.advrename_defaults.templates.ents);
    gnome_cmd_advrename_profile_component_update (priv->profile_component);
}


void on_dialog_response (GnomeCmdAdvrenameDialog *dialog, int response_id, gpointer unused)
{
    auto priv = advrename_dialog_priv (dialog);

    GtkTreeIter i;
    const gchar *old_focused_file_name = NULL;
    gchar *new_focused_file_name = NULL;
    gchar *template_entry;

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
        case GTK_RESPONSE_APPLY:

            old_focused_file_name = main_win->fs(ACTIVE)->file_list()->get_focused_file()->get_name();

            for (gboolean valid_iter=gtk_tree_model_get_iter_first (priv->files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (priv->files, &i))
            {
                GnomeCmdFile *f;
                gchar *new_name;

                gtk_tree_model_get (priv->files, &i,
                                    COL_FILE, &f,
                                    COL_NEW_NAME, &new_name,
                                    -1);

                gboolean result = FALSE;

                if (strcmp (g_file_info_get_display_name(f->get_file_info()), new_name) != 0)
                    result = f->rename(new_name, nullptr);

                gtk_list_store_set (GTK_LIST_STORE (priv->files), &i,
                                    COL_NAME, f->get_name(),
                                    COL_RENAME_FAILED, !result,
                                    -1);

                if (!new_focused_file_name && result
                    && !strcmp(old_focused_file_name, g_file_info_get_display_name(f->get_file_info())))
                    new_focused_file_name = g_strdup(new_name);

                g_free (new_name);
            }
            if (new_focused_file_name)
            {
                main_win->fs(ACTIVE)->file_list()->focus_file(new_focused_file_name, TRUE);
                g_free (new_focused_file_name);
                new_focused_file_name = NULL;
            }
            gnome_cmd_advrename_dialog_update_new_filenames (dialog);
            template_entry = gnome_cmd_advrename_profile_component_get_template_entry (priv->profile_component);
            priv->config->templates.add(template_entry);
            g_free (template_entry);
            gnome_cmd_advrename_profile_component_set_template_history (priv->profile_component, priv->config->templates.ents);
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_CLOSE:
            gnome_cmd_advrename_profile_component_copy (priv->profile_component);
            gtk_widget_hide (GTK_WIDGET (dialog));
            gnome_cmd_advrename_dialog_unset (dialog);
            g_signal_stop_emission_by_name (dialog, "response");        //  FIXME:  ???
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-advanced-rename");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GCMD_RESPONSE_PROFILES:
            break;

        case GCMD_RESPONSE_RESET:
            gnome_cmd_advanced_rename_profile_reset (priv->config->default_profile);
            gnome_cmd_advrename_profile_component_set_template_history (priv->profile_component, gnome_cmd_data.advrename_defaults.templates.ents);
            gnome_cmd_advrename_profile_component_update (priv->profile_component);
            break;

        default :
            g_assert_not_reached ();
    }
}


extern "C" void gnome_cmd_advrename_dialog_init (GnomeCmdAdvrenameDialog *dialog)
{
    static GActionEntry action_entries[] = {
        { "save-profile",           save_profile },
        { "manage-profiles",        manage_profiles },
        { "load-profile",           load_profile, "i" },
        { "file-list-remove",       on_files_view_popup_menu__remove },
        { "file-list-view",         on_files_view_popup_menu__view_file },
        { "file-list-properties",   on_files_view_popup_menu__show_properties },
        { "update-file-list",       on_files_view_popup_menu__update_files }
    };
    GSimpleActionGroup* action_group = g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (action_group), action_entries, G_N_ELEMENTS(action_entries), dialog);
    gtk_widget_insert_action_group (GTK_WIDGET (dialog), "advrename", G_ACTION_GROUP (action_group));

    auto priv = g_new0(GnomeCmdAdvrenameDialogPrivate, 1);
    g_object_set_data_full (G_OBJECT (dialog), "priv", priv, g_free);

    gtk_window_set_title (GTK_WINDOW (dialog), _("Advanced Rename Tool"));
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG (dialog));

    gtk_widget_set_margin_top (content_area, 10);
    gtk_widget_set_margin_bottom (content_area, 10);
    gtk_widget_set_margin_start (content_area, 10);
    gtk_widget_set_margin_end (content_area, 10);
    gtk_box_set_spacing (GTK_BOX (content_area), 6);

    GtkWidget *vbox = priv->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append (GTK_BOX (content_area), vbox);

    // Results
    gchar *str = g_strdup_printf ("<b>%s</b>", _("Results"));
    GtkWidget *label = gtk_label_new_with_mnemonic (str);
    g_free (str);

    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (vbox), label);

    GtkWidget *scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled_window), TRUE);
    gtk_widget_set_margin_bottom (scrolled_window, 6);
    gtk_widget_set_margin_start (scrolled_window, 12);
    gtk_widget_set_vexpand (scrolled_window, TRUE);
    gtk_box_append (GTK_BOX (vbox), scrolled_window);

    priv->files_view = create_files_view ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), priv->files_view);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->files_view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
}


inline GtkTreeModel *create_files_model ()
{
    GtkListStore *store = gtk_list_store_new (NUM_FILE_COLS,
                                              G_TYPE_POINTER,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_BOOLEAN,
                                              G_TYPE_POINTER);

    return GTK_TREE_MODEL (store);
}


inline GtkWidget *create_files_view ()
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "reorderable", TRUE,
                  "enable-search", TRUE,
                  "search-column", COL_NAME,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_NAME, _("Old name"));
    g_object_set (renderer, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", COL_RENAME_FAILED);
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Current file name"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_NEW_NAME, _("New name"));
    g_object_set (renderer, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", COL_RENAME_FAILED);
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("New file name"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_SIZE, _("Size"));
    g_object_set (renderer, "xalign", 1.0, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", COL_RENAME_FAILED);
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("File size"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_DATE, _("Date"));
    g_object_set (renderer, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", COL_RENAME_FAILED);
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("File modification date"));

    return view;
}


extern "C" GnomeCmdAdvrenameDialog *gnome_cmd_advrename_dialog_new (GnomeCmdData::AdvrenameConfig *cfg,
                                                 GnomeCmdFileMetadataService *file_metadata_service,
                                                 GtkWindow *parent_window)
{
    auto dialog = (GnomeCmdAdvrenameDialog *) g_object_new (gnome_cmd_advrename_dialog_get_type (), nullptr);
    auto priv = advrename_dialog_priv (dialog);

    priv->config = cfg;

    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent_window);

    priv->file_metadata_service = file_metadata_service;

    priv->profile_menu_button = gtk_menu_button_new ();
    gtk_menu_button_set_label (GTK_MENU_BUTTON (priv->profile_menu_button), _("Profiles…"));
    gtk_menu_button_set_menu_model (
        GTK_MENU_BUTTON (priv->profile_menu_button),
        G_MENU_MODEL (create_placeholder_menu(cfg)));

    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), priv->profile_menu_button, GCMD_RESPONSE_PROFILES);

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            _("_Help"), GTK_RESPONSE_HELP,
                            _("Reset"), GCMD_RESPONSE_RESET,
                            _("_Close"), GTK_RESPONSE_CLOSE,
                            _("_Apply"), GTK_RESPONSE_APPLY,
                            NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_APPLY);

    gtk_window_set_hide_on_close (GTK_WINDOW (dialog), TRUE);

    priv->profile_component = (GnomeCmdAdvrenameProfileComponent *) g_object_new (gnome_cmd_advrename_profile_component_get_type (),
        "file-metadata-service", file_metadata_service,
        "profile", cfg->default_profile,
        nullptr);

    gtk_widget_set_vexpand (GTK_WIDGET (priv->profile_component), TRUE);
    gtk_box_append (GTK_BOX (priv->vbox), GTK_WIDGET (priv->profile_component));
    gtk_box_reorder_child_after (GTK_BOX (priv->vbox), GTK_WIDGET (priv->profile_component), nullptr);

    // Results
    priv->files = create_files_model ();

    g_signal_connect (priv->profile_component, "template-changed", G_CALLBACK (on_profile_template_changed), dialog);
    g_signal_connect (priv->profile_component, "counter-changed", G_CALLBACK (on_profile_counter_changed), dialog);
    g_signal_connect (priv->profile_component, "regex-changed", G_CALLBACK (on_profile_regex_changed), dialog);
    g_signal_connect (priv->files, "row-deleted", G_CALLBACK (on_files_model_row_deleted), dialog);

    GtkGesture *button_gesture = gtk_gesture_click_new ();
    gtk_widget_add_controller (GTK_WIDGET (priv->files_view), GTK_EVENT_CONTROLLER (button_gesture));
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (button_gesture), 3);
    g_signal_connect (button_gesture, "pressed", G_CALLBACK (on_files_view_button_pressed), dialog);

    g_signal_connect (priv->files_view, "row-activated", G_CALLBACK (on_files_view_row_activated), dialog);
    g_signal_connect (priv->files_view, "cursor-changed", G_CALLBACK (on_files_view_cursor_changed), dialog);

    g_signal_connect (dialog, "show", G_CALLBACK (on_dialog_show), dialog);
    g_signal_connect (dialog, "response", G_CALLBACK (on_dialog_response), dialog);

    gnome_cmd_advrename_dialog_update_template (dialog);

    gtk_widget_grab_focus (GTK_WIDGET (priv->profile_component));

    return dialog;
}


void gnome_cmd_advrename_dialog_set(GnomeCmdAdvrenameDialog *dialog, GList *file_list)
{
    auto priv = advrename_dialog_priv (dialog);

    gnome_cmd_advrename_profile_component_set_sample_fname (priv->profile_component,
        file_list ? ((GnomeCmdFile *) file_list->data)->get_name() : NULL);

    for (GtkTreeIter iter; file_list; file_list=file_list->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) file_list->data;

        GnomeCmdFileMetadata *metadata = gnome_cmd_file_metadata_service_extract_metadata (priv->file_metadata_service, f);

        gtk_list_store_append (GTK_LIST_STORE (priv->files), &iter);
        gtk_list_store_set (GTK_LIST_STORE (priv->files), &iter,
                            COL_FILE, f->ref(),
                            COL_NAME, f->get_name(),
                            COL_SIZE, f->get_size(),
                            COL_DATE, f->get_mdate(FALSE),
                            COL_METADATA, metadata,
                            -1);
    }

    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->files_view), priv->files);
    // g_object_unref (files);          // destroy model automatically with view

    gnome_cmd_advrename_dialog_update_new_filenames(dialog);
}


void gnome_cmd_advrename_dialog_unset(GnomeCmdAdvrenameDialog *dialog)
{
    auto priv = advrename_dialog_priv (dialog);

    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->files_view), NULL);       // unset the model

    GnomeCmdFile *f;
    GtkTreeIter i;
    GnomeCmdFileMetadata *metadata;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (priv->files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (priv->files, &i))
    {
        gtk_tree_model_get (priv->files, &i,
                            COL_FILE, &f,
                            COL_METADATA, &metadata,
                            -1);

        f->unref();
        if (metadata)
            gnome_cmd_file_metadata_free (metadata);
    }

    g_signal_handlers_block_by_func (priv->files, gpointer (on_files_model_row_deleted), dialog);
    gtk_list_store_clear (GTK_LIST_STORE (priv->files));
    g_signal_handlers_unblock_by_func (priv->files, gpointer (on_files_model_row_deleted), dialog);
}


extern "C" GtkListStore *gnome_cmd_advrename_dialog_get_files (GnomeCmdAdvrenameDialog *dialog)
{
    auto priv = advrename_dialog_priv (dialog);
    return GTK_LIST_STORE (priv->files);
}


extern "C" GnomeCmdAdvrenameProfileComponent *gnome_cmd_advrename_dialog_get_profile_component (GnomeCmdAdvrenameDialog *dialog)
{
    auto priv = advrename_dialog_priv (dialog);
    return priv->profile_component;
}


extern "C" GnomeCmdData::AdvrenameConfig *gnome_cmd_advrename_dialog_get_config (GnomeCmdAdvrenameDialog *dialog)
{
    auto priv = advrename_dialog_priv (dialog);
    return priv->config;
}
