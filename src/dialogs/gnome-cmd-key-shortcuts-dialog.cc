/**
 * @file gnome-cmd-key-shortcuts-dialog.cc
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-key-shortcuts-dialog.h"
#include "eggcellrendererkeys.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-treeview.h"
#include "gnome-cmd-hintbox.h"
#include "dict.h"
#include "utils.h"

using namespace std;


#define GNOME_CMD_TYPE_KEY_SHORTCUTS_DIALOG          (gnome_cmd_key_shortcuts_dialog_get_type())
#define GNOME_CMD_KEY_SHORTCUTS_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_KEY_SHORTCUTS_DIALOG, GnomeCmdKeyShortcutsDialog))
#define GNOME_CMD_KEY_SHORTCUTS_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_CMD_TYPE_KEY_SHORTCUTS_DIALOG, GnomeCmdKeyShortcutsDialogClass))
#define GNOME_CMD_IS_KEY_SHORTCUTS_DIALOG(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_CMD_TYPE_KEY_SHORTCUTS_DIALOG)


GType gnome_cmd_key_shortcuts_dialog_get_type ();


GtkTreeViewColumn *create_new_accel_column (GtkTreeView *view, GtkCellRenderer *&renderer, gint COL_KEYS_ID, gint COL_MODS_ID, const gchar *title);
GtkTreeViewColumn *create_new_combo_column (GtkTreeView *view, GtkTreeModel *model, GtkCellRenderer *&renderer, gint COL_ID, const gchar *title);
gboolean equal_accel (GtkTreeModel *model, GtkTreeIter *i, guint key, GdkModifierType mask);
gboolean find_accel (GtkTreeModel *model, GtkTreeIter *i, guint key, GdkModifierType mask);
void set_accel (GtkTreeModel *model, GtkTreePath *path, guint accel_key, GdkModifierType accel_mask);


struct GnomeCmdKeyShortcutsDialogPrivate
{
    static GnomeCmdUserActions *user_actions;

    GtkTreeView *view;

    // These fields are used to store and pass information between "change" and "edited" handlers.
    gchar *last_action_path;
    gchar *last_action_name;
};


GnomeCmdUserActions *GnomeCmdKeyShortcutsDialogPrivate::user_actions = NULL;


struct GnomeCmdKeyShortcutsDialog
{
    GtkDialog parent;
};


struct GnomeCmdKeyShortcutsDialogClass
{
    GtkDialogClass parent_class;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdKeyShortcutsDialog, gnome_cmd_key_shortcuts_dialog, GTK_TYPE_DIALOG)


static void gnome_cmd_key_shortcuts_dialog_finalize (GObject *object)
{
     GnomeCmdKeyShortcutsDialog *dialog = GNOME_CMD_KEY_SHORTCUTS_DIALOG (object);
     auto priv = static_cast<GnomeCmdKeyShortcutsDialogPrivate *> (gnome_cmd_key_shortcuts_dialog_get_instance_private (dialog));

    g_clear_pointer (&priv->last_action_path, g_free);
    g_clear_pointer (&priv->last_action_name, g_free);

    G_OBJECT_CLASS (gnome_cmd_key_shortcuts_dialog_parent_class)->finalize (object);
}


enum
{
    COL_ACCEL_KEY,
    COL_ACCEL_MASK,
    COL_ACTION,
    COL_NAME,
    COL_OPTION,
    NUM_COLUMNS
} ;


static void response_callback (GnomeCmdKeyShortcutsDialog *dialog, int response_id, GtkWidget *view)
{
    auto priv = static_cast<GnomeCmdKeyShortcutsDialogPrivate *> (gnome_cmd_key_shortcuts_dialog_get_instance_private (dialog));

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            if (priv->user_actions)
            {
                priv->user_actions->clear();

                GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
                GtkTreeIter i;

                // copy model -> dialog->user_actions

                for (gboolean valid_iter=gtk_tree_model_get_iter_first (model, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (model, &i))
                {
                    guint accel_key  = 0;
                    guint accel_mask = 0;
                    gchar *name = NULL;
                    gchar *options = NULL;

                    gtk_tree_model_get (model, &i,
                                        COL_ACCEL_KEY, &accel_key,
                                        COL_ACCEL_MASK, &accel_mask,
                                        COL_NAME, &name,
                                        COL_OPTION, &options,
                                        -1);

                    if (accel_key)
                        priv->user_actions->register_action(accel_mask, accel_key, name, g_strstrip (options));

                    g_free (name);
                    g_free (options);
                }

                priv->user_actions->unregister(GDK_KEY_F3);
                priv->user_actions->unregister(GDK_KEY_F4);
                priv->user_actions->unregister(GDK_KEY_F5);
                priv->user_actions->unregister(GDK_KEY_F6);
                priv->user_actions->unregister(GDK_KEY_F7);
                priv->user_actions->unregister(GDK_KEY_F8);
                priv->user_actions->unregister(GDK_KEY_F9);

                priv->user_actions->register_action(GDK_KEY_F3, "file.view");
                priv->user_actions->register_action(GDK_KEY_F4, "file.edit");
                priv->user_actions->register_action(GDK_KEY_F5, "file.copy");
                priv->user_actions->register_action(GDK_KEY_F6, "file.rename");
                priv->user_actions->register_action(GDK_KEY_F7, "file.mkdir");
                priv->user_actions->register_action(GDK_KEY_F8, "file.delete");
                priv->user_actions->register_action(GDK_KEY_F9, "edit.search");
            }

            gtk_window_close (GTK_WINDOW (dialog));
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            gtk_window_close (GTK_WINDOW (dialog));
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-keyboard");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        default :
            g_assert_not_reached ();
    }
}


static void gnome_cmd_key_shortcuts_dialog_class_init (GnomeCmdKeyShortcutsDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_key_shortcuts_dialog_finalize;
}


GtkWidget *create_view_and_model (GnomeCmdKeyShortcutsDialog *dialog);
GtkTreeModel *create_and_fill_model (GnomeCmdUserActions &user_actions);

extern "C" void accel_edited_callback_r (GtkCellRendererAccel *accel, const char *path_string, guint accel_key, GdkModifierType accel_mask, guint hardware_keycode, GtkTreeView *view);
static void cell_changed_action_callback (GtkCellRendererCombo *cell, gchar *path_string, GtkTreeIter *new_iter, GnomeCmdKeyShortcutsDialog *dialog);
static void cell_edited_action_callback (GtkCellRendererText *cell, gchar *path_string, gchar *new_text, GnomeCmdKeyShortcutsDialog *dialog);
static void cell_edited_option_callback (GtkCellRendererText *cell, gchar *path_string, gchar *new_text, GtkWidget *view);
static void add_clicked_callback (GtkButton *button, GtkWidget *view);
static void remove_clicked_callback (GtkButton *button, GtkWidget *view);


static void gnome_cmd_key_shortcuts_dialog_init (GnomeCmdKeyShortcutsDialog *dialog)
{
    auto priv = static_cast<GnomeCmdKeyShortcutsDialogPrivate *> (gnome_cmd_key_shortcuts_dialog_get_instance_private (dialog));

    gtk_window_set_title (GTK_WINDOW (dialog), _("Keyboard Shortcuts"));
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
    gtk_widget_set_size_request (GTK_WIDGET (dialog), 800, 600);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_widget_set_margin_top (content_area, 10);
    gtk_widget_set_margin_bottom (content_area, 10);
    gtk_widget_set_margin_start (content_area, 10);
    gtk_widget_set_margin_end (content_area, 10);
    gtk_box_set_spacing (GTK_BOX (content_area), 6);

    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_vexpand (vbox, TRUE);
    gtk_box_append (GTK_BOX (content_area), vbox);
    gtk_widget_show (vbox);

    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_vexpand (hbox, TRUE);
    gtk_box_append (GTK_BOX (vbox), hbox);
    gtk_widget_show (hbox);

    GtkWidget *scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled_window), TRUE);
    gtk_widget_set_hexpand (scrolled_window, TRUE);
    gtk_box_append (GTK_BOX (hbox), scrolled_window);
    gtk_widget_show (scrolled_window);

    GtkWidget *view = create_view_and_model (dialog);
    priv->view = GTK_TREE_VIEW (view);
    gtk_widget_set_size_request (view, 600, 400);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), view);
    gtk_widget_show (view);

    GtkWidget *box = gnome_cmd_hint_box_new (_("To edit a shortcut key, click on the "
                                               "corresponding row and type a new "
                                               "accelerator, or press escape to "
                                               "cancel."));
    gtk_box_append (GTK_BOX (vbox), box);
    gtk_widget_show (box);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append (GTK_BOX (hbox), vbox);
    gtk_widget_show (vbox);

    GtkWidget *button = gtk_button_new_with_mnemonic (_("_Add"));
    g_signal_connect (button, "clicked", G_CALLBACK (add_clicked_callback), view);
    gtk_box_append (GTK_BOX (vbox), button);
    gtk_widget_show (button);

    button = gtk_button_new_with_mnemonic (_("_Remove"));
    g_signal_connect (button, "clicked", G_CALLBACK (remove_clicked_callback), view);
    gtk_box_append (GTK_BOX (vbox), button);
    gtk_widget_show (button);

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            _("_Help"), GTK_RESPONSE_HELP,
                            _("_Cancel"), GTK_RESPONSE_CANCEL,
                            _("_OK"), GTK_RESPONSE_OK,
                            NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), view);

    gtk_widget_grab_focus (view);
}


void gnome_cmd_key_shortcuts_dialog_new (GtkWindow *parent_window, GnomeCmdUserActions &user_actions)
{
    GnomeCmdKeyShortcutsDialogPrivate::user_actions = &user_actions;        // ugly hack, but can't come to any better method of passing data to gnome_cmd_key_shortcuts_dialog_init ()

    GtkWidget *dialog = GTK_WIDGET (g_object_new (GNOME_CMD_TYPE_KEY_SHORTCUTS_DIALOG, NULL));

    g_return_if_fail (dialog != NULL);

    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent_window);

    gtk_window_present (GTK_WINDOW (dialog));
}


GtkTreeViewColumn *create_new_accel_column (GtkTreeView *view, GtkCellRenderer *&renderer, gint COL_KEYS_ID, gint COL_MODS_ID, const gchar *title=NULL)
{
    renderer = egg_cell_renderer_keys_new ();

    g_object_set (renderer,
                  "editable", TRUE,
                  "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_OTHER,
                  NULL);

    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes (title,
                                                                       renderer,
                                                                       "accel-key", COL_KEYS_ID,
                                                                       "accel-mods", COL_MODS_ID,
                                                                       NULL);
    g_object_set (col,
                  "clickable", TRUE,
                  "resizable", TRUE,
                  NULL);

    // pack tree view column into tree view
    gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

    return col;
}


GtkTreeViewColumn *create_new_combo_column (GtkTreeView *view, GtkTreeModel *model, GtkCellRenderer *&renderer, gint COL_ID, const gchar *title=NULL)
{
    renderer = gtk_cell_renderer_combo_new ();

    g_object_set (renderer,
                  "model", model,
                  "text-column", 2,
                  "has-entry", FALSE,
                  "editable", TRUE,
                  NULL);

    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes (title,
                                                                       renderer,
                                                                       "text", COL_ID,
                                                                       NULL);
    g_object_set (col,
                  "clickable", TRUE,
                  "resizable", TRUE,
                  NULL);

    g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COL_ID));

    // pack tree view column into tree view
    gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

    return col;
}


enum
{
    SORTID_ACCEL,
    SORTID_ACTION,
    SORTID_OPTION
};


GtkWidget *create_view_and_model (GnomeCmdKeyShortcutsDialog *dialog)
{
    auto priv = static_cast<GnomeCmdKeyShortcutsDialogPrivate *> (gnome_cmd_key_shortcuts_dialog_get_instance_private (dialog));

    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "enable-search", TRUE,
                  "search-column", COL_ACTION,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    col = create_new_accel_column (GTK_TREE_VIEW (view), renderer, COL_ACCEL_KEY, COL_ACCEL_MASK, _("Shortcut Key"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Keyboard shortcut for selected action"));
    gtk_tree_view_column_set_sort_column_id (col, SORTID_ACCEL);

    g_signal_connect (renderer, "accel-edited", G_CALLBACK (accel_edited_callback_r), view);

    GtkTreeModel *combo_model = gnome_cmd_user_actions_create_model ();

    col = create_new_combo_column (GTK_TREE_VIEW (view), combo_model, renderer, COL_ACTION, _("Action"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("User action"));
    gtk_tree_view_column_set_sort_column_id (col, SORTID_ACTION);
    g_signal_connect (renderer, "changed", (GCallback) cell_changed_action_callback, dialog);
    g_signal_connect (renderer, "edited", (GCallback) cell_edited_action_callback, dialog);

    g_object_unref (combo_model);          // destroy model automatically with view

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_OPTION, _("Options"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Optional data"));
    gtk_tree_view_column_set_sort_column_id (col, SORTID_OPTION);
    g_signal_connect (renderer, "edited", (GCallback) cell_edited_option_callback, view);

    g_object_set (renderer,
                  "editable", TRUE,
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  NULL);

    GtkTreeModel *model = create_and_fill_model (*priv->user_actions);

    gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

    g_object_unref (model);          // destroy model automatically with view

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_first (gtk_tree_view_get_model (GTK_TREE_VIEW (view)), &iter))      // select the first row here...
        gtk_tree_selection_select_iter (selection, &iter);

    return view;
}


static gint sort_by_col (GtkTreeModel *model, GtkTreeIter *i1, GtkTreeIter *i2, gpointer COL)
{
    gchar *s1;
    gchar *s2;

    gtk_tree_model_get (model, i1, GPOINTER_TO_UINT (COL), &s1, -1);
    gtk_tree_model_get (model, i2, GPOINTER_TO_UINT (COL), &s2, -1);

    gint retval = 0;

    if (!s1 && !s2)
        return retval;

    if (!s1)
        retval = 1;
    else
        if (!s2)
            retval = -1;
        else
        {
            // compare s1 and s2 in UTF8 aware way, case insensitive
            gchar *is1 = g_utf8_casefold (s1, -1);
            gchar *is2 = g_utf8_casefold (s2, -1);

            retval = g_utf8_collate (is1, is2);

            g_free (is1);
            g_free (is2);
        }

    g_free (s1);
    g_free (s2);

    return retval;
}


static gint sort_by_accel (GtkTreeModel *model, GtkTreeIter *i1, GtkTreeIter *i2, gpointer user_data)
{
    guint key1, key2;
    GdkModifierType mask1, mask2;

    gtk_tree_model_get (model, i1, COL_ACCEL_KEY, &key1, COL_ACCEL_MASK, &mask1, -1);
    gtk_tree_model_get (model, i2, COL_ACCEL_KEY, &key2, COL_ACCEL_MASK, &mask2, -1);

    if (mask1<mask2)
        return -1;

    if (mask1>mask2)
        return 1;

    return key1 - key2;
}


GtkTreeModel *create_and_fill_model (GnomeCmdUserActions &user_actions)
{
    GtkListStore *store = gtk_list_store_new (NUM_COLUMNS,
                                              G_TYPE_UINT,              //  COL_ACCEL_KEY
                                              GDK_TYPE_MODIFIER_TYPE,   //  COL_ACCEL_MASK
                                              G_TYPE_STRING,            //  COL_ACTION
                                              G_TYPE_STRING,            //  COL_NAME
                                              G_TYPE_STRING);           //  COL_OPTION

    GtkTreeIter iter;

    for (GnomeCmdUserActions::const_iterator i=user_actions.begin(); i!=user_actions.end(); ++i)
         if (!ascii_isupper (*i))                                       // ignore lowercase keys as they duplicate uppercase ones
         {
             gtk_list_store_append (store, &iter);
             gtk_list_store_set (store, &iter,
                                 COL_ACCEL_KEY, i->first.keyval,
                                 COL_ACCEL_MASK, i->first.state,
                                 COL_ACTION, user_actions.description(i),
                                 COL_NAME, user_actions.name(i),
                                 COL_OPTION, user_actions.options(i),
                                 -1);
         }

    GtkTreeSortable *sortable = GTK_TREE_SORTABLE (store);

    gtk_tree_sortable_set_sort_func (sortable, SORTID_ACCEL, sort_by_accel, NULL, NULL);
    gtk_tree_sortable_set_sort_func (sortable, SORTID_ACTION, sort_by_col, GUINT_TO_POINTER (COL_ACTION), NULL);
    gtk_tree_sortable_set_sort_func (sortable, SORTID_OPTION, sort_by_col, GUINT_TO_POINTER (COL_OPTION), NULL);

    gtk_tree_sortable_set_sort_column_id (sortable, SORTID_ACTION, GTK_SORT_ASCENDING);   // set initial sort order

    return GTK_TREE_MODEL (store);
}


gboolean equal_accel (GtkTreeModel *model, GtkTreeIter *i, guint key, GdkModifierType mask)
{
    guint accel_key  = 0;
    GdkModifierType accel_mask = (GdkModifierType) 0;

    gtk_tree_model_get (model, i,
                        COL_ACCEL_KEY, &accel_key,
                        COL_ACCEL_MASK, &accel_mask,
                        -1);

    return accel_key==key && accel_mask==mask;
}


gboolean find_accel (GtkTreeModel *model, GtkTreeIter *i, guint key, GdkModifierType mask)
{
    gboolean valid_iter;

    for (valid_iter=gtk_tree_model_get_iter_first (model, i); valid_iter; valid_iter=gtk_tree_model_iter_next (model, i))
        if (equal_accel (model, i, key, mask))
            return TRUE;

    return FALSE;
}


void set_accel (GtkTreeModel *model, GtkTreePath *path, guint accel_key, GdkModifierType accel_mask)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter (model, &iter, path))
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COL_ACCEL_KEY, accel_key,
                            COL_ACCEL_MASK, accel_mask,
                            -1);
}


static void cell_changed_action_callback (GtkCellRendererCombo *cell, gchar *path_string, GtkTreeIter *new_iter, GnomeCmdKeyShortcutsDialog *dialog)
{
    auto priv = static_cast<GnomeCmdKeyShortcutsDialogPrivate *> (gnome_cmd_key_shortcuts_dialog_get_instance_private (dialog));

    GtkTreeModel *combo_model;
    g_object_get (cell, "model", &combo_model, NULL);
    g_return_if_fail (combo_model != NULL);

    gchar *action_name = NULL;
    gtk_tree_model_get (combo_model, new_iter, 1, &action_name, -1);

    g_clear_pointer (&priv->last_action_path, g_free);
    priv->last_action_path = g_strdup (path_string);

    g_clear_pointer (&priv->last_action_name, g_free);
    priv->last_action_name = action_name;

    g_object_unref (combo_model);
}


static void cell_edited_action_callback (GtkCellRendererText *cell, gchar *path_string, gchar *new_text, GnomeCmdKeyShortcutsDialog *dialog)
{
    auto priv = static_cast<GnomeCmdKeyShortcutsDialogPrivate *> (gnome_cmd_key_shortcuts_dialog_get_instance_private (dialog));

    GtkTreeModel *model = gtk_tree_view_get_model (priv->view);
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter iter;

    const gchar *name = g_strcmp0 (priv->last_action_path, path_string) == 0
        ? priv->last_action_name
        : nullptr;

    if (name != nullptr && gtk_tree_model_get_iter (model, &iter, path))
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COL_ACTION, new_text,
                            COL_NAME, name,
                            -1);

    gtk_tree_path_free (path);
}


static void cell_edited_option_callback (GtkCellRendererText *cell, gchar *path_string, gchar *new_text, GtkWidget *view)
{
    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter (model, &iter, path))
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COL_OPTION, new_text,
                            -1);

    gtk_tree_path_free (path);
}


static void remove_clicked_callback (GtkButton *button, GtkWidget *view)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        gtk_tree_selection_select_iter (selection, &iter);
    }
}


static void add_clicked_callback (GtkButton *button, GtkWidget *view)
{
    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
    GtkTreeIter iter;

    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        COL_ACTION, _("Do nothing"),
                        COL_NAME, "no.action",
                        -1);

    GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
    gtk_widget_grab_focus (view);
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, gtk_tree_view_get_column (GTK_TREE_VIEW (view),0), TRUE);
    gtk_tree_path_free(path);
    // start editing accelerator
}
