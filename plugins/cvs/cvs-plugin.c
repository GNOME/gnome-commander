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
#include <gnome.h>
#include <libgcmd/libgcmd.h>
#include "cvs-plugin.h"
#include "cvs-plugin.xpm"
#include "interface.h"
#include "parser.h"

#define NAME "CVS"
#define COPYRIGHT "Copyright \xc2\xa9 2003-2006 Marcus Bjurman"
#define AUTHOR "Marcus Bjurman <marbj499@student.liu.se>"
#define WEBPAGE "http://www.nongnu.org/gcmd"


static PluginInfo plugin_nfo = {
    GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
    NAME,
    VERSION,
    COPYRIGHT,
    NULL,
    NULL,
    NULL,
    NULL,
    WEBPAGE
};


static gchar *compression_level_strings[] = {
    "0 - No compression",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6 - Default compression level",
    "7",
    "8",
    "9 - Maximum compression",
    NULL
};


struct _CvsPluginPrivate
{
    GtkWidget *update;
    GtkWidget *log;
    GtkWidget *diff;
    GtkWidget *last_log;
    GtkWidget *last_change;

    GtkWidget *conf_dialog;
    GtkWidget *compression_level_menu;
    GtkWidget *unidiff_check;
};


static GnomeCmdPluginClass *parent_class = NULL;


static void on_dummy (GtkMenuItem *item, gpointer data)
{
    gnome_ok_dialog ("CVS plugin dummy operation");
}


static gboolean change_cwd (const gchar *fpath)
{
    gchar *dir = g_path_get_dirname (fpath);

    if (dir)
    {
        gint ret = chdir (dir);
        g_free (dir);

        return ret == 0;
    }

    return FALSE;
}


static void on_diff (GtkMenuItem *item, GnomeCmdState *state)
{
    GList *files = state->active_dir_selected_files;
    CvsPlugin *plugin = gtk_object_get_data (GTK_OBJECT (item), "plugin");

    if (files && !plugin->diff_win)
        plugin->diff_win = create_diff_win (plugin);

    for (; files; files = files->next)
    {
        gchar *cmd;
        GnomeCmdFileInfo *f = GNOME_CMD_FILE_INFO (files->data);
        GnomeVFSURI *uri = gnome_vfs_uri_append_file_name (state->active_dir_uri, f->info->name);
        const gchar *fpath = gnome_vfs_uri_get_path (uri);

        change_cwd (fpath);
        cmd = g_strdup_printf ("cvs -z%d diff %s %s",
                               plugin->compression_level,
                               plugin->unidiff?"-u":"",
                               g_basename (fpath));
        add_diff_tab (plugin, cmd, g_basename (fpath));
        g_free (cmd);
    }
}


static void on_log (GtkMenuItem *item, GnomeCmdState *state)
{
    GList *files = state->active_dir_selected_files;
    CvsPlugin *plugin = gtk_object_get_data (GTK_OBJECT (item), "plugin");

    if (files && !plugin->log_win)
        plugin->log_win = create_log_win (plugin);

    for (; files; files = files->next)
    {
        GnomeCmdFileInfo *f = GNOME_CMD_FILE_INFO (files->data);
        GnomeVFSURI *uri = gnome_vfs_uri_append_file_name (state->active_dir_uri, f->info->name);
        const gchar *fpath = gnome_vfs_uri_get_path (uri);

        change_cwd (fpath);
        add_log_tab (plugin, g_basename (fpath));
    }
}


static GtkWidget *create_menu_item (const gchar *name, gboolean show_pixmap,
                                    GtkSignalFunc callback, gpointer data,
                                    CvsPlugin *plugin)
{
    GtkWidget *item, *label;

    if (show_pixmap)
    {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) cvs_plugin_xpm);
        GtkWidget *pixmap = gtk_image_new_from_pixbuf (pixbuf);
        g_object_unref (G_OBJECT (pixbuf));
        item = gtk_image_menu_item_new ();
        if (pixmap)
        {
            gtk_widget_show (pixmap);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), pixmap);
        }
    }
    else
        item = gtk_menu_item_new ();

    gtk_widget_show (item);

    // Create the contents of the menu item
    label = gtk_label_new (name);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (item), label);

    // Connect to the signal and set user data
    gtk_object_set_data (GTK_OBJECT (item), GNOMEUIINFO_KEY_UIDATA, data);

    if (callback)
        gtk_signal_connect (GTK_OBJECT (item), "activate", callback, data);

    gtk_object_set_data (GTK_OBJECT (item), "plugin", plugin);

    return item;
}


static GtkWidget *create_main_menu (GnomeCmdPlugin *p, GnomeCmdState *state)
{
    CvsPlugin *plugin = CVS_PLUGIN (p);
    GtkMenu *submenu = GTK_MENU (gtk_menu_new ());

    GtkWidget *item = create_menu_item ("CVS", FALSE, NULL, NULL, plugin);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));

    plugin->priv->update = create_menu_item ("Update", FALSE, GTK_SIGNAL_FUNC (on_dummy), state, plugin);
    gtk_menu_append (submenu, plugin->priv->update);
    plugin->priv->diff = create_menu_item ("Diff", FALSE, GTK_SIGNAL_FUNC (on_diff), state, plugin);
    gtk_menu_append (submenu, plugin->priv->diff);
    plugin->priv->log = create_menu_item ("Log", FALSE, GTK_SIGNAL_FUNC (on_log), state, plugin);
    gtk_menu_append (submenu, plugin->priv->log);
    plugin->priv->last_log = create_menu_item ("Last Log", FALSE, GTK_SIGNAL_FUNC (on_dummy), state, plugin);
    gtk_menu_append (submenu, plugin->priv->last_log);
    plugin->priv->last_change = create_menu_item ("Last Change", FALSE, GTK_SIGNAL_FUNC (on_dummy), state, plugin);
    gtk_menu_append (submenu, plugin->priv->last_change);

    return item;
}


static gboolean cvs_dir_exists (GList *files)
{
    for (; files; files = files->next)
    {
        GnomeCmdFileInfo *f = GNOME_CMD_FILE_INFO (files->data);
        if (f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY &&
            strcmp (f->info->name, "CVS") == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}


static GList *create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GtkWidget *item;
    GList *l = NULL;

    if (cvs_dir_exists (state->active_dir_files))
    {
        item = create_menu_item ("CVS Diff", TRUE, GTK_SIGNAL_FUNC (on_diff), state, CVS_PLUGIN (plugin));
        l = g_list_append (l, item);
        item = create_menu_item ("CVS Log", TRUE, GTK_SIGNAL_FUNC (on_log), state, CVS_PLUGIN (plugin));
        l = g_list_append (l, item);
    }

    return l;
}


static void update_main_menu_state (GnomeCmdPlugin *p, GnomeCmdState *state)
{
    CvsPlugin *plugin = CVS_PLUGIN (p);

    gboolean dir_exists = cvs_dir_exists (state->active_dir_files);

    gtk_widget_set_sensitive (plugin->priv->update, dir_exists);
    gtk_widget_set_sensitive (plugin->priv->diff, dir_exists);
    gtk_widget_set_sensitive (plugin->priv->log, dir_exists);
    gtk_widget_set_sensitive (plugin->priv->last_log, dir_exists);
    gtk_widget_set_sensitive (plugin->priv->last_change, dir_exists);
}


static void on_configure_close (GtkButton *btn, CvsPlugin *plugin)
{
    plugin->compression_level = gtk_option_menu_get_history (GTK_OPTION_MENU (plugin->priv->compression_level_menu));
    plugin->unidiff = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (plugin->priv->unidiff_check));

    gnome_cmd_data_set_int ("/cvs-plugin/compression_level", plugin->compression_level);

    gtk_widget_destroy (plugin->priv->conf_dialog);
}


static void configure (GnomeCmdPlugin *p)
{
    GtkWidget *dialog, *table, *cat, *label, *vbox, *optmenu, *check;
    CvsPlugin *plugin = CVS_PLUGIN (p);

    dialog = gnome_cmd_dialog_new (_("Options"));
    plugin->priv->conf_dialog = dialog;
    gtk_widget_ref (dialog);
    gnome_cmd_dialog_set_transient_for (GNOME_CMD_DIALOG (dialog), GTK_WINDOW (main_win_widget));
    gnome_cmd_dialog_set_modal (GNOME_CMD_DIALOG (dialog));

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK,
                                 GTK_SIGNAL_FUNC (on_configure_close), plugin);

    vbox = create_vbox (dialog, FALSE, 12);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), vbox);


    table = create_table (dialog, 2, 2);
    cat = create_category (dialog, table, _("CVS options"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    label = create_label (dialog, _("Compression level"));
    table_add (table, label, 0, 1, 0);

    optmenu = create_option_menu (dialog, compression_level_strings);
    plugin->priv->compression_level_menu = optmenu;
    gtk_option_menu_set_history (GTK_OPTION_MENU (optmenu), plugin->compression_level);
    table_add (table, optmenu, 1, 1, GTK_FILL);

    check = create_check (dialog, _("Unified diff format"), "check");
    plugin->priv->unidiff_check = check;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), plugin->unidiff);
    gtk_table_attach (GTK_TABLE (table), check, 0, 2, 0, 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);

    gtk_widget_show (dialog);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    //CvsPlugin *plugin = CVS_PLUGIN (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (CvsPluginClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GnomeCmdPluginClass *plugin_class = GNOME_CMD_PLUGIN_CLASS (klass);

    parent_class = gtk_type_class (gnome_cmd_plugin_get_type ());

    object_class->destroy = destroy;

    plugin_class->create_main_menu = create_main_menu;
    plugin_class->create_popup_menu_items = create_popup_menu_items;
    plugin_class->update_main_menu_state = update_main_menu_state;
    plugin_class->configure = configure;
}


static void init (CvsPlugin *plugin)
{
    plugin->priv = g_new0 (CvsPluginPrivate, 1);

    plugin->diff_win = NULL;
    plugin->log_win = NULL;

    plugin->compression_level = gnome_cmd_data_get_int ("/cvs-plugin/compression_level", 6);
    plugin->unidiff = gnome_cmd_data_get_bool ("/cvs-plugin/unidiff", TRUE);
}



/***********************************
 * Public functions
 ***********************************/

GtkType cvs_plugin_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "CvsPlugin",
            sizeof (CvsPlugin),
            sizeof (CvsPluginClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gnome_cmd_plugin_get_type (), &info);
    }
    return type;
}


GnomeCmdPlugin *cvs_plugin_new ()
{
    CvsPlugin *plugin = gtk_type_new (cvs_plugin_get_type ());

    return GNOME_CMD_PLUGIN (plugin);
}


GnomeCmdPlugin *create_plugin ()
{
    return cvs_plugin_new ();
}


PluginInfo *get_plugin_info ()
{
    if (!plugin_nfo.authors)
    {
        plugin_nfo.authors = g_new0 (gchar *, 2);
        plugin_nfo.authors[0] = AUTHOR;
        plugin_nfo.authors[1] = NULL;
        plugin_nfo.comments = g_strdup (_("A plugin that eventually will be a simple CVS client"));
    }
    return &plugin_nfo;
}
