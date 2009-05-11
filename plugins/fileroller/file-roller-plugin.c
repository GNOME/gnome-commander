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
#include "file-roller-plugin.h"
#include "file-roller.xpm"
#include "file-roller-small.xpm"

#define NAME "File Roller"
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


static gchar *handled_extensions[] =
{
    ".7z",          // 7-zip
    ".ar",          // ar
    ".arj",         // arj
    ".bin",         // stuffit
    ".deb",         // Debian archives
    ".ear",         // jar
    ".jar",         // jar
    ".lzh",         // lha
    ".rar",         // rar
    ".rpm",         // RPM archives
    ".sit",         // stuffit
    ".tar",         // tar
    ".tar.Z",       // tar+compress
    ".tar.bz",      // tar+bz
    ".tar.bz2",     // tar+bz2
    ".tar.gz",      // tar+gz
    ".tar.lzo",     // tar+lzop
    ".taz",         // tar+compress
    ".tbz",         // tar+bz
    ".tbz2",        // tar+bz2
    ".tgz",         // tar+gz
    ".tzo",         // tar+lzop
    ".war",         // jar
    ".zip",         // zip
    ".zoo",         // zoo
    NULL
};


struct _FileRollerPluginPrivate
{
    GtkWidget *conf_dialog;
    GtkWidget *conf_combo;

    GnomeCmdState *state;

    gchar *default_ext;
};

static GnomeCmdPluginClass *parent_class = NULL;


static void on_extract_cwd (GtkMenuItem *item, GnomeVFSURI *uri)
{
    gchar *target_arg, *archive_arg;
    gchar *uri_str = gnome_vfs_uri_to_string (uri, 0);
    gchar *local_path = gnome_vfs_get_local_path_from_uri (uri_str);
    gchar *target_name = gtk_object_get_data (GTK_OBJECT (item), "target_name");
    gchar *target_dir = gtk_object_get_data (GTK_OBJECT (item), "target_dir");
    gchar *cmd, *t;

    if (target_dir==NULL)
    {
        t = g_path_get_dirname (local_path);
        target_dir = target_name ? g_build_path (G_DIR_SEPARATOR_S, t, target_name, NULL) : g_strdup (t);
        g_free (t);
    }
    g_free (target_name);

    t = g_strdup_printf ("--extract-to=%s", target_dir);
    target_arg = g_shell_quote (t);
    g_free (t);

    archive_arg = g_shell_quote (local_path);
    cmd = g_strdup_printf ("file-roller %s %s", target_arg, archive_arg);

    t = g_path_get_dirname (local_path);
    gnome_execute_shell (t, cmd);
    g_free (t);

    g_free (target_arg);
    g_free (target_dir);
    g_free (archive_arg);
    g_free (local_path);
    g_free (uri_str);
    g_free (cmd);
}


static void do_add_to_archive (const gchar *name, GnomeCmdState *state)
{
    gchar *t = g_strdup_printf ("--add-to=%s", name);
    gchar *arg = g_shell_quote (t);
    gchar *cmd = g_strdup_printf ("file-roller %s ", arg);
    gchar *active_dir_path, *uri_str;
    GList *files;

    for (files = state->active_dir_selected_files; files; files = files->next)
    {
        GnomeVFSURI *uri = GNOME_CMD_FILE_INFO (files->data)->uri;
        gchar *uri_str = gnome_vfs_uri_to_string (uri, 0);
        gchar *path = gnome_vfs_get_local_path_from_uri (uri_str);
        gchar *tmp = cmd;
        gchar *arg = g_shell_quote (path);
        cmd = g_strdup_printf ("%s %s", tmp, arg);
        g_free (arg);
        g_free (path);
        g_free (tmp);
        g_free (uri_str);
    }

    g_printerr ("add: %s\n", cmd);
    uri_str = gnome_vfs_uri_to_string (state->active_dir_uri, 0);
    active_dir_path = gnome_vfs_get_local_path_from_uri (uri_str);
    gnome_execute_shell (active_dir_path, cmd);

    g_free (cmd);
    g_free (uri_str);
    g_free (active_dir_path);
}


static void on_add_to_archive (GtkMenuItem *item, FileRollerPlugin *plugin)
{
    gint ret;
    GtkWidget *dialog = NULL;
    GdkPixbuf *pixbuf;
    GtkWidget *entry;
    GtkWidget *hbox;
    gchar *t;
    const gchar *name;
    gboolean name_ok = FALSE;
    GList *files;

    files = plugin->priv->state->active_dir_selected_files;

    do
    {
        if (dialog)
            gtk_widget_destroy (dialog);

        dialog = gtk_message_dialog_new (
            NULL,
            0,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK_CANCEL,
            _("What file name should the new archive have?"));

        gtk_window_set_title (GTK_WINDOW (dialog), _("Create Archive"));

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_widget_ref (hbox);
        gtk_widget_show (hbox);
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, TRUE, 6);

        entry = gtk_entry_new ();
        gtk_widget_ref (entry);
        gtk_widget_show (entry);
        gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 6);

        t = g_strdup_printf (
            "%s%s",
            get_utf8 (GNOME_CMD_FILE_INFO (files->data)->info->name),
            plugin->priv->default_ext);

        gtk_entry_set_text (GTK_ENTRY (entry), t);
        g_free (t);

        pixbuf = gdk_pixbuf_new_from_xpm_data ((const char**)file_roller_xpm);
        gtk_image_set_from_pixbuf (GTK_IMAGE (GTK_MESSAGE_DIALOG (dialog)->image), pixbuf);
        gdk_pixbuf_unref (pixbuf);

        ret = gtk_dialog_run (GTK_DIALOG (dialog));

        name = gtk_entry_get_text (GTK_ENTRY (entry));
        if (name != NULL && strlen (name) > 0)
            name_ok = TRUE;
    }
    while (name_ok == FALSE && ret == GTK_RESPONSE_OK);

    if (ret == GTK_RESPONSE_OK)
        do_add_to_archive (name, plugin->priv->state);

    gtk_widget_destroy (dialog);
}


static GtkWidget *create_menu_item (const gchar *name, gboolean show_pixmap,
                                    GtkSignalFunc callback, gpointer data)
{
    GtkWidget *item, *label;

    if (show_pixmap)
    {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) file_roller_small_xpm);
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

    return item;
}


static GtkWidget *create_main_menu (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    return NULL;
}


static GList *create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GList *items = NULL;
    GtkWidget *item;
    gint num_files;
    GList *files;

    files = state->active_dir_selected_files;
    num_files = g_list_length (files);

    if (num_files <= 0)
        return NULL;

    if (!gnome_vfs_uri_is_local (GNOME_CMD_FILE_INFO (files->data)->uri))
        return NULL;

    FILE_ROLLER_PLUGIN (plugin)->priv->state = state;

    item = create_menu_item (_("Create Archive..."), TRUE, GTK_SIGNAL_FUNC (on_add_to_archive), plugin);
    items = g_list_append (items, item);

    if (num_files == 1)
    {
        GnomeCmdFileInfo *f = GNOME_CMD_FILE_INFO (files->data);
        gchar *fname = g_strdup (f->info->name);
        gint i;

        for (i=0; handled_extensions[i]; ++i)
            if (g_str_has_suffix (fname, handled_extensions[i]))
            {
                item = create_menu_item (_("Extract in Current Directory"), TRUE, GTK_SIGNAL_FUNC (on_extract_cwd), f->uri);
                items = g_list_append (items, item);

                fname[strlen(fname)-strlen(handled_extensions[i])] = '\0';

                gchar *text;

                text = g_strdup_printf (_("Extract to '%s'"), fname);
                item = create_menu_item (text, TRUE, GTK_SIGNAL_FUNC (on_extract_cwd), f->uri);
                gtk_object_set_data (GTK_OBJECT (item), "target_name", g_strdup (fname));
                items = g_list_append (items, item);
                g_free (text);

                if (!gnome_vfs_uri_equal (state->active_dir_uri, state->inactive_dir_uri))
                {
                    gchar *path = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (state->inactive_dir_uri), NULL);

                    text = g_strdup_printf (_("Extract to '%s'"), path);
                    item = create_menu_item (text, TRUE, GTK_SIGNAL_FUNC (on_extract_cwd), f->uri);
                    gtk_object_set_data (GTK_OBJECT (item), "target_dir", path);
                    items = g_list_append (items, item);
                    g_free (text);
                }

                break;
            }

        g_free (fname);
    }

    return items;
}


static void update_main_menu_state (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
}


static void on_configure_close (GtkButton *btn, FileRollerPlugin *plugin)
{
    plugin->priv->default_ext = g_strdup (gtk_entry_get_text (
        GTK_ENTRY (GTK_COMBO (plugin->priv->conf_combo)->entry)));

    gnome_cmd_data_set_string ("/file-runner-plugin/default_type", plugin->priv->default_ext);

    gtk_widget_hide (plugin->priv->conf_dialog);
}


static void configure (GnomeCmdPlugin *plugin)
{
    gint i;
    GList *items = NULL;
    GtkWidget *dialog, *table, *cat, *label, *combo, *vbox;

    dialog = gnome_cmd_dialog_new (_("Options"));
    gnome_cmd_dialog_set_transient_for (GNOME_CMD_DIALOG (dialog), GTK_WINDOW (main_win_widget));
    gnome_cmd_dialog_set_modal (GNOME_CMD_DIALOG (dialog));

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK,
                                 GTK_SIGNAL_FUNC (on_configure_close), plugin);

    vbox = create_vbox (dialog, FALSE, 12);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), vbox);


    table = create_table (dialog, 2, 2);
    cat = create_category (dialog, table, _("File-roller options"));
    gtk_box_pack_start (GTK_BOX (vbox), cat, FALSE, TRUE, 0);

    label = create_label (dialog, _("Default type"));
    table_add (table, label, 0, 1, 0);

    combo = create_combo (dialog);
    table_add (table, combo, 1, 1, GTK_FILL);

    for (i=0; handled_extensions[i] != NULL; i++)
        items = g_list_append (items, handled_extensions[i]);

    gtk_combo_set_popdown_strings (GTK_COMBO (combo), items);

    gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry),
                        FILE_ROLLER_PLUGIN (plugin)->priv->default_ext);

    FILE_ROLLER_PLUGIN (plugin)->priv->conf_dialog = dialog;
    FILE_ROLLER_PLUGIN (plugin)->priv->conf_combo = combo;

    gtk_widget_show (dialog);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    FileRollerPlugin *plugin = FILE_ROLLER_PLUGIN (object);

    g_free (plugin->priv->default_ext);
    g_free (plugin->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (FileRollerPluginClass *klass)
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


static void init (FileRollerPlugin *plugin)
{
    plugin->priv = g_new (FileRollerPluginPrivate, 1);

    plugin->priv->default_ext = gnome_cmd_data_get_string ("/file-runner-plugin/default_type", ".zip");
}


/***********************************
 * Public functions
 ***********************************/

GtkType file_roller_plugin_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "FileRollerPlugin",
            sizeof (FileRollerPlugin),
            sizeof (FileRollerPluginClass),
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


GnomeCmdPlugin *file_roller_plugin_new ()
{
    FileRollerPlugin *plugin = gtk_type_new (file_roller_plugin_get_type ());

    return GNOME_CMD_PLUGIN (plugin);
}


GnomeCmdPlugin *create_plugin ()
{
    return file_roller_plugin_new ();
}


PluginInfo *get_plugin_info ()
{
    if (!plugin_nfo.authors)
    {
        plugin_nfo.authors = g_new0 (gchar *, 2);
        plugin_nfo.authors[0] = AUTHOR;
        plugin_nfo.authors[1] = NULL;
        plugin_nfo.comments = g_strdup (_("A plugin that adds File Roller shortcuts for creating "
                                          "and extracting compressed archives."));
    }
    return &plugin_nfo;
}
