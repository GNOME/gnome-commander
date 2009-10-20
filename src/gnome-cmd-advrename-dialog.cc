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
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtkdialog.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-convert.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-profile-component.h"
#include "dialogs/gnome-cmd-advrename-regex-dialog.h"
#include "dialogs/gnome-cmd-manage-profiles-dialog.h"
#include "gnome-cmd-advrename-lexer.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-treeview.h"
#include "gnome-cmd-menu-button.h"
#include "gnome-cmd-data.h"
#include "tags/gnome-cmd-tags.h"
#include "utils.h"

using namespace std;


struct GnomeCmdAdvrenameDialogClass
{
    GtkDialogClass parent_class;
};


struct GnomeCmdAdvrenameDialog::Private
{
    gboolean template_has_counters;

    GtkWidget *vbox;
    GnomeCmdProfileComponent *profile_component;
    GtkWidget *files_view;
    GtkWidget *profile_menu_button;

    Private();
    ~Private();

    static gchar *translate_menu (const gchar *path, gpointer data);

    GtkWidget *create_placeholder_menu(GnomeCmdData::AdvrenameConfig *cfg);
    GtkWidget *create_button_with_menu(gchar *label_text, GnomeCmdData::AdvrenameConfig *cfg=NULL);

    static void manage_profiles(GnomeCmdAdvrenameDialog::Private *priv, guint unused, GtkWidget *menu);
    static void load_profile(GnomeCmdAdvrenameDialog::Private *priv, guint profile_idx, GtkWidget *menu);

    void files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog, GdkEventButton *event=NULL);

    static void on_profile_template_changed (GnomeCmdProfileComponent *component, GnomeCmdAdvrenameDialog *dialog);
    static void on_profile_counter_changed (GnomeCmdProfileComponent *component, GnomeCmdAdvrenameDialog *dialog);
    static void on_profile_regex_changed (GnomeCmdProfileComponent *component, GnomeCmdAdvrenameDialog *dialog);
    static void on_files_model_row_deleted (GtkTreeModel *files, GtkTreePath *path, GnomeCmdAdvrenameDialog *dialog);
    static void on_files_view_popup_menu__remove (GtkWidget *menuitem, GtkTreeView *treeview);
    static void on_files_view_popup_menu__view_file (GtkWidget *menuitem, GtkTreeView *treeview);
    static void on_files_view_popup_menu__show_properties (GtkWidget *menuitem, GtkTreeView *treeview);
    static void on_files_view_popup_menu__update_files (GtkWidget *menuitem, GnomeCmdAdvrenameDialog *dialog);
    static gboolean on_files_view_button_pressed (GtkWidget *treeview, GdkEventButton *event, GnomeCmdAdvrenameDialog *dialog);
    static gboolean on_files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog);
    static void on_files_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameDialog *dialog);

    static gboolean on_dialog_delete (GtkWidget *widget, GdkEvent *event, GnomeCmdAdvrenameDialog *dialog);
    static void on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdAdvrenameDialog *dialog);
    static void on_dialog_response (GnomeCmdAdvrenameDialog *dialog, int response_id, gpointer data);
};


inline GnomeCmdAdvrenameDialog::Private::Private()
{
    profile_menu_button = NULL;
    template_has_counters = FALSE;
}


inline GnomeCmdAdvrenameDialog::Private::~Private()
{
}


inline gboolean model_is_empty(GtkTreeModel *tree_model)
{
    GtkTreeIter iter;

    return !gtk_tree_model_get_iter_first (tree_model, &iter);
}


gchar *GnomeCmdAdvrenameDialog::Private::translate_menu (const gchar *path, gpointer data)
{
    return _(path);
}


inline GtkWidget *GnomeCmdAdvrenameDialog::Private::create_placeholder_menu(GnomeCmdData::AdvrenameConfig *cfg)
{
    guint items_size = cfg->profiles.empty() ? 1 : cfg->profiles.size()+3;
    GtkItemFactoryEntry *items = g_try_new0 (GtkItemFactoryEntry, items_size);
    GtkItemFactoryEntry *i = items;

    g_return_val_if_fail (items!=NULL, NULL);

    i->path = g_strdup (_("/_Save Profile As..."));
    i->callback = (GtkItemFactoryCallback) manage_profiles;
    i->callback_action = TRUE;
    i->item_type = "<StockItem>";
    i->extra_data = GTK_STOCK_SAVE_AS;
    ++i;

    if (!cfg->profiles.empty())
    {
        i->path = g_strdup (_("/_Manage Profiles..."));
        i->callback = (GtkItemFactoryCallback) manage_profiles;
        i->item_type = "<StockItem>";
        i->extra_data = GTK_STOCK_EDIT;
        ++i;

        i->path = g_strdup ("/");
        i->item_type = "<Separator>";
        ++i;

        for (vector<GnomeCmdData::AdvrenameConfig::Profile>::const_iterator p=cfg->profiles.begin(); p!=cfg->profiles.end(); ++p, ++i)
        {
            i->path = g_strconcat ("/", p->name.c_str(), NULL);
            i->callback = (GtkItemFactoryCallback) load_profile;
            i->callback_action = (i-items)-3;
            i->item_type = "<StockItem>";
            i->extra_data = GTK_STOCK_REVERT_TO_SAVED;
        }
    }

    GtkItemFactory *ifac = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);

    gtk_item_factory_create_items (ifac, items_size, items, this);

    for (guint i=0; i<items_size; ++i)
        g_free (items[i].path);

    g_free (items);

    return gtk_item_factory_get_widget (ifac, "<main>");
}


inline GtkWidget *GnomeCmdAdvrenameDialog::Private::create_button_with_menu(gchar *label_text, GnomeCmdData::AdvrenameConfig *cfg)
{
    profile_menu_button = gnome_cmd_button_menu_new (label_text, create_placeholder_menu(cfg));

    return profile_menu_button;
}


void GnomeCmdAdvrenameDialog::Private::manage_profiles(GnomeCmdAdvrenameDialog::Private *priv, guint new_profile, GtkWidget *widget)
{
    GtkWidget *dialog = gtk_widget_get_ancestor (priv->profile_menu_button, GNOME_CMD_TYPE_ADVRENAME_DIALOG);

    g_return_if_fail (dialog!=NULL);

    GnomeCmdData::AdvrenameConfig &cfg = GNOME_CMD_ADVRENAME_DIALOG(dialog)->defaults;

    if (new_profile)
        priv->profile_component->copy();

    if (gnome_cmd_manage_profiles_dialog_new (_("Profiles"), GTK_WINDOW (dialog), cfg,  new_profile))
    {
        GtkWidget *menu = widget->parent;

        gnome_cmd_button_menu_disconnect_handler (priv->profile_menu_button, menu);
        g_object_unref (gtk_item_factory_from_widget (menu));
        gnome_cmd_button_menu_connect_handler (priv->profile_menu_button, priv->create_placeholder_menu(&cfg));
    }
}


void GnomeCmdAdvrenameDialog::Private::load_profile(GnomeCmdAdvrenameDialog::Private *priv, guint profile_idx, GtkWidget *widget)
{
    GtkWidget *dialog = gtk_widget_get_ancestor (priv->profile_menu_button, GNOME_CMD_TYPE_ADVRENAME_DIALOG);

    g_return_if_fail (dialog!=NULL);

    GnomeCmdData::AdvrenameConfig &cfg = GNOME_CMD_ADVRENAME_DIALOG(dialog)->defaults;

    g_return_if_fail (profile_idx<cfg.profiles.size());

    cfg.default_profile = cfg.profiles[profile_idx];
    priv->profile_component->update();

    GNOME_CMD_ADVRENAME_DIALOG(dialog)->update_new_filenames();     //  FIXME:  ??
}


inline GtkTreeModel *create_files_model ();
inline GtkWidget *create_files_view ();


G_DEFINE_TYPE (GnomeCmdAdvrenameDialog, gnome_cmd_advrename_dialog, GTK_TYPE_DIALOG)


void GnomeCmdAdvrenameDialog::Private::on_profile_template_changed (GnomeCmdProfileComponent *component, GnomeCmdAdvrenameDialog *dialog)
{
    gnome_cmd_advrename_parse_template (component->get_template_entry(), dialog->priv->template_has_counters);
    dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_profile_counter_changed (GnomeCmdProfileComponent *component, GnomeCmdAdvrenameDialog *dialog)
{
    if (dialog->priv->template_has_counters)
        dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_profile_regex_changed (GnomeCmdProfileComponent *component, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_files_model_row_deleted (GtkTreeModel *files, GtkTreePath *path, GnomeCmdAdvrenameDialog *dialog)
{
    if (dialog->priv->template_has_counters)
        dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu__remove (GtkWidget *menuitem, GtkTreeView *treeview)
{
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (treeview);

        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

        gnome_cmd_file_unref (f);
    }
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu__view_file (GtkWidget *menuitem, GtkTreeView *treeview)
{
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (treeview);
        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);

        if (f)
            gnome_cmd_file_view (f, -1);
    }
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu__show_properties (GtkWidget *menuitem, GtkTreeView *treeview)
{
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (treeview);
        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);

        if (f)
            gnome_cmd_file_show_properties (f);
    }
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu__update_files (GtkWidget *menuitem, GnomeCmdAdvrenameDialog *dialog)
{
    GtkTreeIter i;
    GnomeCmdFile *f;

    //  re-read file attributes, as they could be changed...
    for (gboolean valid_iter=gtk_tree_model_get_iter_first (dialog->files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (dialog->files, &i))
    {
        gtk_tree_model_get (dialog->files, &i,
                            COL_FILE, &f,
                            -1);

        gtk_list_store_set (GTK_LIST_STORE (dialog->files), &i,
                            COL_NAME, gnome_cmd_file_get_name (f),
                            COL_SIZE, gnome_cmd_file_get_size (f),
                            COL_DATE, gnome_cmd_file_get_mdate (f, FALSE),
                            COL_RENAME_FAILED, FALSE,
                            -1);
    }

    gnome_cmd_advrename_parse_template (dialog->priv->profile_component->get_template_entry(), dialog->priv->template_has_counters);
    dialog->update_new_filenames();
}


inline void GnomeCmdAdvrenameDialog::Private::files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog, GdkEventButton *event)
{
    GtkWidget *menu = gtk_menu_new ();
    GtkWidget *menuitem;

    menuitem = gtk_menu_item_new_with_label (_("Remove from file list"));
    g_signal_connect (menuitem, "activate", G_CALLBACK (on_files_view_popup_menu__remove), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    menuitem = gtk_menu_item_new_with_label (_("View file"));
    g_signal_connect (menuitem, "activate", G_CALLBACK (on_files_view_popup_menu__view_file), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    menuitem = gtk_menu_item_new_with_label (_("File properties"));
    g_signal_connect (menuitem, "activate", G_CALLBACK (on_files_view_popup_menu__show_properties), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

    menuitem = gtk_menu_item_new_with_label (_("Update file list"));
    g_signal_connect (menuitem, "activate", G_CALLBACK (on_files_view_popup_menu__update_files), dialog);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    gtk_widget_show_all (menu);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                    (event != NULL) ? event->button : 0, gdk_event_get_time ((GdkEvent*) event));
}


gboolean GnomeCmdAdvrenameDialog::Private::on_files_view_button_pressed (GtkWidget *treeview, GdkEventButton *event, GnomeCmdAdvrenameDialog *dialog)
{
    if (event->type==GDK_BUTTON_PRESS && event->button==3)
    {
        // optional: select row if no row is selected or only one other row is selected
        // (will only do something if you set a tree selection mode
        // as described later in the tutorial)
        if (1)
        {
            GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
            if (gtk_tree_selection_count_selected_rows (selection) <= 1)
            {
                GtkTreePath *path;

                if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
                                                   (gint) event->x, (gint) event->y,
                                                   &path,
                                                   NULL, NULL, NULL))
                {
                    gtk_tree_selection_unselect_all (selection);
                    gtk_tree_selection_select_path (selection, path);
                    gtk_tree_path_free (path);
                }
            }
        }
        dialog->priv->files_view_popup_menu (treeview, dialog, event);

        return TRUE;
    }

    return FALSE;
}


gboolean GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->priv->files_view_popup_menu (treeview, dialog);

    return TRUE;
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameDialog *dialog)
{
    on_files_view_popup_menu__show_properties (NULL, view);
}


gboolean GnomeCmdAdvrenameDialog::Private::on_dialog_delete (GtkWidget *widget, GdkEvent *event, GnomeCmdAdvrenameDialog *dialog)
{
    return event->type==GDK_DELETE;
}


void GnomeCmdAdvrenameDialog::Private::on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->defaults.width  = allocation->width;
    dialog->defaults.height = allocation->height;
}


void GnomeCmdAdvrenameDialog::Private::on_dialog_response (GnomeCmdAdvrenameDialog *dialog, int response_id, gpointer unused)
{
    GtkTreeIter i;

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
        case GTK_RESPONSE_APPLY:
            for (gboolean valid_iter=gtk_tree_model_get_iter_first (dialog->files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (dialog->files, &i))
            {
                GnomeCmdFile *f;
                gchar *new_name;

                gtk_tree_model_get (dialog->files, &i,
                                    COL_FILE, &f,
                                    COL_NEW_NAME, &new_name,
                                    -1);

                GnomeVFSResult result = GNOME_VFS_OK;

                if (strcmp (f->info->name, new_name) != 0)
                    result = gnome_cmd_file_rename (f, new_name);

                gtk_list_store_set (GTK_LIST_STORE (dialog->files), &i,
                                    COL_NAME, gnome_cmd_file_get_name (f),
                                    COL_RENAME_FAILED, result!=GNOME_VFS_OK,
                                    -1);

                g_free (new_name);
            }
            dialog->update_new_filenames();
            dialog->defaults.templates.add(dialog->priv->profile_component->get_template_entry());
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_CLOSE:
            dialog->defaults.templates.add(dialog->priv->profile_component->get_template_entry());
            gtk_widget_hide (*dialog);
            dialog->unset();
            g_signal_stop_emission_by_name (dialog, "response");        //  FIXME:  ???
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-advanced-rename");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GCMD_RESPONSE_PROFILES:
            break;

        case GCMD_RESPONSE_RESET:
            dialog->defaults.default_profile.reset();
            dialog->priv->profile_component->update();
            break;

        default :
            g_assert_not_reached ();
    }
}


static void gnome_cmd_advrename_dialog_init (GnomeCmdAdvrenameDialog *dialog)
{
    dialog->priv = new GnomeCmdAdvrenameDialog::Private;

    gtk_window_set_title (*dialog, _("Advanced Rename Tool"));
    gtk_window_set_resizable (*dialog, TRUE);
    gtk_dialog_set_has_separator (*dialog, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

    GtkWidget *vbox = dialog->priv->vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

    // Results
    gchar *str = g_strdup_printf ("<b>%s</b>", _("Results"));
    GtkWidget *label = gtk_label_new_with_mnemonic (str);
    g_free (str);

    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    GtkWidget *align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 6, 12, 0);
    gtk_container_add (GTK_CONTAINER (vbox), align);

    GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (align), scrolled_window);

    dialog->priv->files_view = create_files_view ();
    gtk_container_add (GTK_CONTAINER (scrolled_window), dialog->priv->files_view);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->files_view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
}


static void gnome_cmd_advrename_dialog_finalize (GObject *object)
{
    GnomeCmdAdvrenameDialog *dialog = GNOME_CMD_ADVRENAME_DIALOG (object);

    delete dialog->priv;

    G_OBJECT_CLASS (gnome_cmd_advrename_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_advrename_dialog_class_init (GnomeCmdAdvrenameDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_advrename_dialog_finalize;
}


inline GtkTreeModel *create_files_model ()
{
    GtkListStore *store = gtk_list_store_new (GnomeCmdAdvrenameDialog::NUM_FILE_COLS,
                                              G_TYPE_POINTER,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_BOOLEAN);

    return GTK_TREE_MODEL (store);
}


inline GtkWidget *create_files_view ()
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "reorderable", TRUE,
                  "enable-search", TRUE,
                  "search-column", GnomeCmdAdvrenameDialog::COL_NAME,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    GtkTooltips *tips = gtk_tooltips_new ();

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_NAME, _("Old name"));
    g_object_set (renderer, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tooltips_set_tip (tips, col->button, _("Current file name"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_NEW_NAME, _("New name"));
    g_object_set (renderer, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tooltips_set_tip (tips, col->button, _("New file name"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_SIZE, _("Size"));
    g_object_set (renderer, "xalign", 1.0, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tooltips_set_tip (tips, col->button, _("File size"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_DATE, _("Date"));
    g_object_set (renderer, "foreground", "red", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tooltips_set_tip (tips, col->button, _("File modification date"), NULL);

    return view;
}


void GnomeCmdAdvrenameDialog::update_new_filenames()
{
    gnome_cmd_advrename_reset_counter (defaults.default_profile.counter_start,
                                       defaults.default_profile.counter_width,
                                       defaults.default_profile.counter_step);
    GtkTreeIter i;

    vector<GnomeCmd::RegexReplace *> rx;

    GtkTreeModel *regexes = priv->profile_component->get_regex_model();

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (regexes, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (regexes, &i))
    {
        GnomeCmd::RegexReplace *r;

        gtk_tree_model_get (regexes, &i,
                            GnomeCmdProfileComponent::COL_REGEX, &r,
                            -1);
        if (r && *r)                            //  ignore regex pattern if it can't be retrieved or if it is malformed
            rx.push_back(r);
    }

#if !GLIB_CHECK_VERSION (2, 14, 0)
    vector<pair<int,int> > match;
#endif

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (files, &i))
    {
        GnomeCmdFile *f;

        gtk_tree_model_get (files, &i,
                            COL_FILE, &f,
                            -1);
        if (!f)
            continue;

        gchar *fname = gnome_cmd_advrename_gen_fname (f);

        for (vector<GnomeCmd::RegexReplace *>::iterator j=rx.begin(); j!=rx.end(); ++j)
        {
            GnomeCmd::RegexReplace *&r = *j;

            gchar *prev_fname = fname;

#if GLIB_CHECK_VERSION (2, 14, 0)
            fname = r->replace(prev_fname);
#else
            match.clear();

            for (gchar *s=prev_fname; *s && r->match(s); s+=r->end())
                if (r->length()>0)
                    match.push_back(make_pair(r->start(), r->end()));

            gchar *src = prev_fname;
            gchar *dest = fname = (gchar *) g_malloc (strlen(prev_fname) + match.size()*r->to.size() + 1);    // allocate new fname big enough to hold all data

            for (vector<pair<int,int> >::const_iterator i=match.begin(); i!=match.end(); ++i)
            {
                memcpy(dest, src, i->first);
                dest += i->first;
                src += i->second;
                memcpy(dest, r->to.c_str(), r->to.size());
                dest += r->to.size();
            }

            strcpy(dest, src);
#endif
            g_free (prev_fname);
        }

        fname = priv->profile_component->trim_blanks (priv->profile_component->convert_case (fname));
        gtk_list_store_set (GTK_LIST_STORE (files), &i,
                            COL_NEW_NAME, fname,
                            -1);
        g_free (fname);
    }
}


GnomeCmdAdvrenameDialog::GnomeCmdAdvrenameDialog(GnomeCmdData::AdvrenameConfig &cfg): defaults(cfg)
{
    gtk_window_set_default_size (*this, cfg.width, cfg.height);

    gtk_dialog_add_action_widget (*this,
                                  priv->create_button_with_menu (_("Profiles..."), &cfg),
                                  GCMD_RESPONSE_PROFILES);

    gtk_dialog_add_buttons (*this,
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                            _("Reset"), GCMD_RESPONSE_RESET,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                            GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                            NULL);

    gtk_dialog_set_default_response (*this, GTK_RESPONSE_APPLY);

    priv->profile_component = new GnomeCmdProfileComponent(cfg.default_profile);

    gtk_box_pack_start (GTK_BOX (priv->vbox), *priv->profile_component, FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (priv->vbox), *priv->profile_component, 0);

    // Template
    priv->profile_component->set_template_history(defaults.templates.ents);

    // Results
    files = create_files_model ();

    g_signal_connect (priv->profile_component, "template-changed", G_CALLBACK (Private::on_profile_template_changed), this);
    g_signal_connect (priv->profile_component, "counter-changed", G_CALLBACK (Private::on_profile_counter_changed), this);
    g_signal_connect (priv->profile_component, "regex-changed", G_CALLBACK (Private::on_profile_regex_changed), this);
    g_signal_connect (files, "row-deleted", G_CALLBACK (Private::on_files_model_row_deleted), this);
    g_signal_connect (priv->files_view, "button-press-event", G_CALLBACK (Private::on_files_view_button_pressed), this);
    g_signal_connect (priv->files_view, "popup-menu", G_CALLBACK (Private::on_files_view_popup_menu), this);
    g_signal_connect (priv->files_view, "row-activated", G_CALLBACK (Private::on_files_view_row_activated), this);

    g_signal_connect (this, "delete-event", G_CALLBACK (Private::on_dialog_delete), this);
    g_signal_connect (this, "size-allocate", G_CALLBACK (Private::on_dialog_size_allocate), this);
    g_signal_connect (this, "response", G_CALLBACK (Private::on_dialog_response), this);

    gnome_cmd_advrename_parse_template (priv->profile_component->get_template_entry(), priv->template_has_counters);
}


void GnomeCmdAdvrenameDialog::set(GList *file_list)
{
    for (GtkTreeIter iter; file_list; file_list=file_list->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) file_list->data;

        gtk_list_store_append (GTK_LIST_STORE (files), &iter);
        gtk_list_store_set (GTK_LIST_STORE (files), &iter,
                            COL_FILE, gnome_cmd_file_ref (f),
                            COL_NAME, gnome_cmd_file_get_name (f),
                            COL_SIZE, gnome_cmd_file_get_size (f),
                            COL_DATE, gnome_cmd_file_get_mdate (f, FALSE),
                            -1);
    }

    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->files_view), files);
    // g_object_unref (files);          // destroy model automatically with view

    update_new_filenames();
}


void GnomeCmdAdvrenameDialog::unset()
{
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->files_view), NULL);       // unset the model

    GnomeCmdFile *f;
    GtkTreeIter i;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (files, &i))
    {
        gtk_tree_model_get (files, &i,
                            COL_FILE, &f,
                            -1);

        gnome_cmd_file_unref (f);
    }

    g_signal_handlers_block_by_func (files, gpointer (Private::on_files_model_row_deleted), this);
    gtk_list_store_clear (GTK_LIST_STORE (files));
    g_signal_handlers_unblock_by_func (files, gpointer (Private::on_files_model_row_deleted), this);
}
