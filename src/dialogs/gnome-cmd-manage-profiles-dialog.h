/** 
 * @file gnome-cmd-manage-profiles-dialog.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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

#pragma once

#include "gnome-cmd-data.h"
#include "gnome-cmd-menu-button.h"
#include "gnome-cmd-treeview.h"
#include "gnome-cmd-edit-profile-dialog.h"
#include "gnome-cmd-hintbox.h"

namespace GnomeCmd
{
    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    class ManageProfilesDialog
    {
        enum {COL_PROFILE_IDX, COL_NAME, COL_TEMPLATE, NUM_COLUMNS};

        static std::vector<PROFILE> profiles;
        static const char *help_id;

        gint result;

        GtkTreeModel *create_and_fill_model();
        GtkWidget *create_view_and_model();

        static void add_profile(GtkWidget *view, PROFILE &p, guint idx);

        static void cell_edited_callback(GtkCellRendererText *cell, gchar *path_string, gchar *new_text, GtkWidget *view);
        static void row_activated_callback(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer);
        static void duplicate_clicked_callback(GtkButton *button, GtkWidget *view);
        static void edit_clicked_callback(GtkButton *button, GtkWidget *view);
        static void remove_clicked_callback(GtkButton *button, GtkWidget *view);
#if 0
        static void import_clicked_callback(GtkWidget *dialog, guint local, GtkWidget *widget);
#endif
        static void response_callback(GtkDialog *dialog, int response_id, ManageProfilesDialog<CONFIG,PROFILE,COMPONENT> *dlg);

      public:

        ManageProfilesDialog(GtkWindow *parent, CONFIG &cfg, guint new_profile, const gchar *title, const gchar *_help_id);

        operator gboolean () const       {  return result==GTK_RESPONSE_OK;  }
    };

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    void ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::add_profile(GtkWidget *view, PROFILE &p, guint idx)
    {
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
        GtkTreeIter i;

        gtk_list_store_append (GTK_LIST_STORE (model), &i);
        gtk_list_store_set (GTK_LIST_STORE (model), &i,
                            COL_PROFILE_IDX, idx,
                            COL_NAME, p.name.c_str(),
                            COL_TEMPLATE, p.description().c_str(),
                            -1);

        GtkTreePath *path = gtk_tree_model_get_path (model, &i);
        gtk_widget_grab_focus (view);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, gtk_tree_view_get_column (GTK_TREE_VIEW (view),0), TRUE);
        gtk_tree_path_free (path);
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    inline GtkTreeModel *ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::create_and_fill_model()
    {
        GtkListStore *store = gtk_list_store_new (NUM_COLUMNS,
                                                  G_TYPE_UINT,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING);

        GtkTreeIter i;

        for (typename std::vector<PROFILE>::const_iterator p=profiles.begin(); p!=profiles.end(); ++p)
        {
            gtk_list_store_append (store, &i);
            gtk_list_store_set (store, &i,
                                COL_PROFILE_IDX, p-profiles.begin(),
                                COL_NAME, p->name.c_str(),
                                COL_TEMPLATE, p->description().c_str(),
                                -1);
        }

        return GTK_TREE_MODEL (store);
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    inline GtkWidget *ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::create_view_and_model()
    {
        GtkWidget *view = gtk_tree_view_new ();

        g_object_set (view,
                      "rules-hint", TRUE,
                      "reorderable", TRUE,
                      "enable-search", TRUE,
                      "search-column", COL_NAME,
                      NULL);

        GtkCellRenderer *renderer = NULL;
        GtkTreeViewColumn *col = NULL;

        col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_NAME, _("Profile"));
        gtk_widget_set_tooltip_text (col->button, _("Profile name"));
        g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited_callback), view);

        g_object_set (renderer,
                      "editable", TRUE,
                      NULL);

        col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_TEMPLATE, _("Template"));
        gtk_widget_set_tooltip_text (col->button, _("Template"));

        g_object_set (renderer,
                      "foreground-set", TRUE,
                      "foreground", "DarkGray",
                      "ellipsize-set", TRUE,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      NULL);

        GtkTreeModel *model = create_and_fill_model();

        gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

        g_object_unref (model);          // destroy model automatically with view

        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

        GtkTreeIter iter;

        if (gtk_tree_model_get_iter_first (gtk_tree_view_get_model (GTK_TREE_VIEW (view)), &iter))      // select the first row here...
            gtk_tree_selection_select_iter (selection, &iter);

        return view;
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    void ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::cell_edited_callback(GtkCellRendererText *cell, gchar *path_string, gchar *new_text, GtkWidget *view)
    {
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
        GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
        GtkTreeIter iter;

        gint col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

        if (gtk_tree_model_get_iter (model, &iter, path))
        {
            guint idx;

            gtk_tree_model_get (model, &iter, COL_PROFILE_IDX, &idx, -1);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, new_text, -1);
            profiles[idx].name = new_text;
        }

        gtk_tree_path_free (path);
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    void ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::row_activated_callback(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer)
    {
        edit_clicked_callback (NULL, GTK_WIDGET (view));
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    void ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::duplicate_clicked_callback(GtkButton *button, GtkWidget *view)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        GtkTreeIter i;

        if (gtk_tree_selection_get_selected (selection, NULL, &i))
        {
            guint idx;

            GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
            gtk_tree_model_get (model, &i, COL_PROFILE_IDX, &idx, -1);

            profiles.push_back(profiles[idx]);
            add_profile(view, profiles.back(), profiles.size()-1);
        }
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    void ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::edit_clicked_callback(GtkButton *button, GtkWidget *view)
    {
        GtkWidget *dialog = gtk_widget_get_ancestor (view, GTK_TYPE_DIALOG);

        g_return_if_fail (dialog!=NULL);

        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        GtkTreeIter i;

        if (gtk_tree_selection_get_selected (selection, NULL, &i))
        {
            guint idx;

            GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
            gtk_tree_model_get (model, &i, COL_PROFILE_IDX, &idx, -1);

            PROFILE p = profiles[idx];

            if (GnomeCmd::EditProfileDialog<PROFILE,COMPONENT> (GTK_WINDOW (dialog), p, help_id))
            {
                profiles[idx] = p;

                gtk_list_store_set (GTK_LIST_STORE (model), &i,
                                    COL_NAME, p.name.c_str(),
                                    COL_TEMPLATE, p.description().c_str(),
                                    -1);
            }
        }
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    void ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::remove_clicked_callback(GtkButton *button, GtkWidget *view)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        GtkTreeIter iter;

        if (gtk_tree_selection_get_selected (selection, NULL, &iter))
        {
            GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
            gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
    }

#if 0
    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    void ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::import_clicked_callback(GtkWidget *dialog, guint local, GtkWidget *widget)
    {
        if (local)
            ;       //  FIXME:
        else
            ;       //  FIXME:
    }
#endif

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    void ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::response_callback(GtkDialog *dialog, int response_id, ManageProfilesDialog<CONFIG,PROFILE,COMPONENT> *dlg)
    {
        switch (response_id)
        {
            case GTK_RESPONSE_HELP:
                gnome_cmd_help_display ("gnome-commander.xml", dlg->help_id);
                g_signal_stop_emission_by_name (dialog, "response");
                break;

            case GTK_RESPONSE_OK:
                break;

            case GTK_RESPONSE_NONE:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_CANCEL:
                break;

            default :
                g_assert_not_reached ();
        }
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::ManageProfilesDialog(GtkWindow *parent, CONFIG &cfg, guint new_profile, const gchar *title, const gchar *_help_id)
    {
        help_id = _help_id;

        PROFILE default_profile = cfg.default_profile;
        default_profile.name = _("New profile");
        profiles = cfg.profiles;

        GtkWidget *dialog = gtk_dialog_new_with_buttons (title, parent,
                                                         GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                         GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                         NULL);

        GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

        GtkWidget *vbox, *hbox, *scrolled_window, *view, *box, *button;

        gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
        gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
        gtk_box_set_spacing (GTK_BOX (content_area), 2);
        gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

        vbox = gtk_vbox_new (FALSE, 12);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
        gtk_container_add (GTK_CONTAINER (content_area), vbox);

        hbox = gtk_hbox_new (FALSE, 12);
        gtk_container_add (GTK_CONTAINER (vbox), hbox);

        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
        gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);

        view = create_view_and_model();
        gtk_widget_set_size_request (view, 400, 200);
        g_signal_connect (view, "row-activated", G_CALLBACK (row_activated_callback), NULL);
        gtk_container_add (GTK_CONTAINER (scrolled_window), view);

        box = gnome_cmd_hint_box_new (_("To rename a profile, click on the "
                                        "corresponding row and type a new "
                                        "name, or press escape to cancel."));
        gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

        vbox = gtk_vbox_new (FALSE, 12);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

        button = gtk_button_new_with_mnemonic (_("_Duplicate"));
        gtk_button_set_image (GTK_BUTTON (button),
                              gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
        g_signal_connect (button, "clicked", G_CALLBACK (duplicate_clicked_callback), view);
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

        button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
        g_signal_connect (button, "clicked", G_CALLBACK (edit_clicked_callback), view);
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

        button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
        g_signal_connect (button, "clicked", G_CALLBACK (remove_clicked_callback), view);
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

        gtk_widget_show_all (content_area);

        gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

        g_signal_connect (dialog, "response", G_CALLBACK (response_callback), this);

        if (new_profile)
        {
            profiles.push_back(default_profile);
            add_profile(view, profiles.back(), profiles.size()-1);
        }

        result = gtk_dialog_run (GTK_DIALOG (dialog));

        if (result==GTK_RESPONSE_OK)
        {
            cfg.profiles.clear();

            GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
            GtkTreeIter i;

            for (gboolean valid_iter=gtk_tree_model_get_iter_first (model, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (model, &i))
            {
                guint n;

                gtk_tree_model_get (model, &i, COL_PROFILE_IDX, &n, -1);

                cfg.profiles.push_back(profiles[n]);
            }
        }

        gtk_widget_destroy (dialog);
    }

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    std::vector<PROFILE> ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::profiles;

    template <typename CONFIG, typename PROFILE, typename COMPONENT>
    const char *ManageProfilesDialog<CONFIG,PROFILE,COMPONENT>::help_id;
}
