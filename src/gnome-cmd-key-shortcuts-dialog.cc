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
#define GNOME_CMD_IS_KEY_SHORTCUTS_DIALOG(obj)       (G_TYPE_INSTANCE_CHECK_TYPE ((obj), GNOME_CMD_TYPE_KEY_SHORTCUTS_DIALOG)


struct GnomeCmdKeyShortcutsDialogPrivate
{
    GnomeCmdKeyShortcutsDialogPrivate();
};


struct GnomeCmdKeyShortcutsDialog
{
    GtkDialog parent;

    GnomeCmdKeyShortcutsDialogPrivate *priv;

    static GnomeCmdUserActions *user_actions;
};


struct GnomeCmdKeyShortcutsDialogClass
{
    GtkDialogClass parent_class;
};


inline GnomeCmdKeyShortcutsDialogPrivate::GnomeCmdKeyShortcutsDialogPrivate()
{
}


GnomeCmdUserActions *GnomeCmdKeyShortcutsDialog::user_actions = NULL;


G_DEFINE_TYPE (GnomeCmdKeyShortcutsDialog, gnome_cmd_key_shortcuts_dialog, GTK_TYPE_DIALOG)


static void gnome_cmd_key_shortcuts_dialog_finalize (GObject *object)
{
    GnomeCmdKeyShortcutsDialog *dialog = GNOME_CMD_KEY_SHORTCUTS_DIALOG (object);

    delete dialog->priv;

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
    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            if (dialog->user_actions)
            {
                dialog->user_actions->clear();

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
                        dialog->user_actions->register_action(accel_mask, accel_key, name, g_strstrip (options));

                    g_free (name);
                    g_free (options);
                }

                dialog->user_actions->unregister(GDK_F3);
                dialog->user_actions->unregister(GDK_F4);
                dialog->user_actions->unregister(GDK_F5);
                dialog->user_actions->unregister(GDK_F6);
                dialog->user_actions->unregister(GDK_F7);
                dialog->user_actions->unregister(GDK_F8);
                dialog->user_actions->unregister(GDK_F9);
                dialog->user_actions->unregister(GDK_F10);

                dialog->user_actions->register_action(GDK_F3, "file.view");
                dialog->user_actions->register_action(GDK_F4, "file.edit");
                dialog->user_actions->register_action(GDK_F5, "file.copy");
                dialog->user_actions->register_action(GDK_F6, "file.rename");
                dialog->user_actions->register_action(GDK_F7, "file.mkdir");
                dialog->user_actions->register_action(GDK_F8, "file.delete");
                dialog->user_actions->register_action(GDK_F9, "edit.search");
                dialog->user_actions->register_action(GDK_F10, "file.exit");
            }

            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-user-actions");
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


inline GtkWidget *create_view_and_model (GnomeCmdUserActions &user_actions);
inline GtkTreeModel *create_and_fill_model (GnomeCmdUserActions &user_actions);

static void accel_edited_callback (GtkCellRendererAccel *accel, const char *path_string, guint accel_key, GdkModifierType accel_mask, guint hardware_keycode, GtkWidget *view);
static void cell_edited_callback (GtkCellRendererText *cell, gchar *path_string, gchar *new_text, GtkWidget *view);
static void add_clicked_callback (GtkButton *button, GtkWidget *view);
static void remove_clicked_callback (GtkButton *button, GtkWidget *view);


static void gnome_cmd_key_shortcuts_dialog_init (GnomeCmdKeyShortcutsDialog *dialog)
{
    dialog->priv = new GnomeCmdKeyShortcutsDialogPrivate;

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (dialog), _("Keyboard Shortcuts"));
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

    GtkWidget *vbox = gtk_vbox_new (FALSE, 12);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), vbox);
    gtk_widget_show (vbox);

    GtkWidget *hbox = gtk_hbox_new (FALSE, 12);
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    gtk_widget_show (hbox);

    GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);
    gtk_widget_show (scrolled_window);

    GtkWidget *view = create_view_and_model (*dialog->user_actions);
    gtk_widget_set_size_request (view, 600, 400);
    gtk_container_add (GTK_CONTAINER (scrolled_window), view);
    gtk_widget_show (view);

    GtkWidget *box = gnome_cmd_hint_box_new (_("To edit a shortcut key, click on the "
                                               "corresponding row and type a new "
                                               "accelerator, or press escape to "
                                               "cancel."));
    gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
    gtk_widget_show (box);

    vbox = gtk_vbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
    gtk_widget_show (vbox);

    GtkWidget *button = gtk_button_new_from_stock (GTK_STOCK_ADD);
    g_signal_connect (button, "clicked", G_CALLBACK (add_clicked_callback), view);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);

    button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    g_signal_connect (button, "clicked", G_CALLBACK (remove_clicked_callback), view);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), view);

    gtk_widget_grab_focus (view);
}


gboolean gnome_cmd_key_shortcuts_dialog_new (GnomeCmdUserActions &user_actions)
{
    GnomeCmdKeyShortcutsDialog::user_actions = &user_actions;        // ugly hack, but can't come to any better method of passing data to gnome_cmd_key_shortcuts_dialog_init ()

    GtkWidget *dialog = gtk_widget_new (GNOME_CMD_TYPE_KEY_SHORTCUTS_DIALOG, NULL);

    g_return_val_if_fail (dialog != NULL, FALSE);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return result==GTK_RESPONSE_OK;
}


inline GtkTreeViewColumn *create_new_accel_column (GtkTreeView *view, GtkCellRenderer *&renderer, gint COL_KEYS_ID, gint COL_MODS_ID, const gchar *title=NULL)
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


inline GtkTreeViewColumn *create_new_combo_column (GtkTreeView *view, GtkTreeModel *model, GtkCellRenderer *&renderer, gint COL_ID, const gchar *title=NULL)
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


inline GtkWidget *create_view_and_model (GnomeCmdUserActions &user_actions)
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "enable-search", TRUE,
                  "search-column", COL_ACTION,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    GtkTooltips *tips = gtk_tooltips_new ();

    col = create_new_accel_column (GTK_TREE_VIEW (view), renderer, COL_ACCEL_KEY, COL_ACCEL_MASK, _("Shortcut Key"));
    gtk_tooltips_set_tip (tips, col->button, _("Keyboard shortcut for selected action"), NULL);
    gtk_tree_view_column_set_sort_column_id (col, SORTID_ACCEL);

    g_signal_connect (renderer, "accel-edited", G_CALLBACK (accel_edited_callback), view);

    GtkTreeModel *combo_model = gnome_cmd_user_actions_create_model ();

    col = create_new_combo_column (GTK_TREE_VIEW (view), combo_model, renderer, COL_ACTION, _("Action"));
    gtk_tooltips_set_tip (tips, col->button, _("User action"), NULL);
    gtk_tree_view_column_set_sort_column_id (col, SORTID_ACTION);
    g_signal_connect (renderer, "edited", (GCallback) cell_edited_callback, view);

    g_object_unref (combo_model);          // destroy model automatically with view

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_OPTION, _("Options"));
    gtk_tooltips_set_tip (tips, col->button, _("Optional data"), NULL);
    gtk_tree_view_column_set_sort_column_id (col, SORTID_OPTION);
    g_signal_connect(renderer, "edited", (GCallback) cell_edited_callback, view);

    g_object_set (renderer,
                  "editable", TRUE,
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  NULL);

    GtkTreeModel *model = create_and_fill_model (user_actions);

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


inline GtkTreeModel *create_and_fill_model (GnomeCmdUserActions &user_actions)
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


inline gboolean conflict_confirm (GtkWidget *view, const gchar *action, guint accel_key, GdkModifierType accel_mask)
{
    gchar *accel_string = egg_accelerator_get_label (accel_key, accel_mask);

    GtkWidget *dlg = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                             (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                             GTK_MESSAGE_WARNING,
                                             GTK_BUTTONS_NONE,
                                             _("Shortcut \"%s\" is already taken by \"%s\"."),
                                             accel_string, action);
    gtk_dialog_add_buttons (GTK_DIALOG (dlg), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                              _("_Reassign shortcut"), GTK_RESPONSE_OK,
                                              NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_CANCEL);
    gtk_window_set_title (GTK_WINDOW (dlg), _("Conflicting Shortcuts"));
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg), _("Reassigning the shortcut will cause it "
                                                                          "to be removed from \"%s\"."), action);
    gtk_widget_show_all (dlg);
    gint response = gtk_dialog_run (GTK_DIALOG (dlg));

    gtk_widget_destroy (dlg);

    g_free (accel_string);

    return response==GTK_RESPONSE_OK;
}


inline gboolean equal_accel (GtkTreeModel *model, GtkTreeIter *i, guint key, GdkModifierType mask)
{
    guint accel_key  = 0;
    GdkModifierType accel_mask = (GdkModifierType) 0;

    gtk_tree_model_get (model, i,
                        COL_ACCEL_KEY, &accel_key,
                        COL_ACCEL_MASK, &accel_mask,
                        -1);

    return accel_key==key && accel_mask==mask;
}


inline gboolean find_accel (GtkTreeModel *model, GtkTreeIter *i, guint key, GdkModifierType mask)
{
    gboolean valid_iter;

    for (valid_iter=gtk_tree_model_get_iter_first (model, i); valid_iter; valid_iter=gtk_tree_model_iter_next (model, i))
        if (equal_accel (model, i, key, mask))
            return TRUE;

    return FALSE;
}


inline void set_accel (GtkTreeModel *model, GtkTreePath *path, guint accel_key, GdkModifierType accel_mask)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter (model, &iter, path))
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COL_ACCEL_KEY, accel_key,
                            COL_ACCEL_MASK, accel_mask,
                            -1);
}


static void accel_edited_callback (GtkCellRendererAccel *accel, const char *path_string, guint accel_key, GdkModifierType accel_mask, guint hardware_keycode, GtkWidget *view)
{
    DEBUG('a', "Key event:  %s (%#x)\n", key2str(accel_mask,accel_key).c_str(), accel_key);

    if (!accel_key)
        gnome_cmd_show_message (NULL, _("Invalid shortcut."));
    else
    {
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
        GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
        GtkTreeIter iter;

        // do nothing if accelerators haven't changed...
        if (gtk_tree_model_get_iter (model, &iter, path) && equal_accel (model, &iter, accel_key, accel_mask))
            return;

        if (!find_accel (model, &iter, accel_key, accel_mask))                  //  store new values if there isn't any duplicate...
            set_accel (model, path, accel_key, accel_mask);
        else
        {
            gchar *action;

            gtk_tree_model_get (model, &iter, COL_ACTION, &action, -1);         // ...otherwise retrieve conflicting action...

            if (conflict_confirm (view, action, accel_key, accel_mask))         // ...and ask user for confirmation
            {
                set_accel (model, path, accel_key, accel_mask);
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
            }

            g_free (action);
        }

        gtk_tree_path_free (path);
    }
}


static void cell_edited_callback (GtkCellRendererText *cell, gchar *path_string, gchar *new_text, GtkWidget *view)
{
    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter iter;

    gint col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

    if (gtk_tree_model_get_iter (model, &iter, path))
        if (col==COL_ACTION && GnomeCmdKeyShortcutsDialog::user_actions)
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                col, new_text,
                                COL_NAME, GnomeCmdKeyShortcutsDialog::user_actions->name(new_text),
                                -1);
        else
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                col, new_text,
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
