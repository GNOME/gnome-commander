/** 
 * @file gnome-cmd-file-popmenu.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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
#include "gnome-cmd-file-popmenu.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-main-win.h"
#include "plugin_manager.h"
#include "gnome-cmd-python-plugin.h"
#include "gnome-cmd-user-actions.h"
#include "utils.h"
#include "cap.h"

#include <fnmatch.h>

#include "../pixmaps/copy_file_names.xpm"

using namespace std;


#define MAX_OPEN_WITH_APPS 20


struct OpenWithData
{
    GList *files;
    GnomeCmdApp *app;
    GtkPixmap *pm;
};

struct ScriptData
{
    GList *files;
    const char *name;
    gboolean term;
};

static GtkMenuClass *parent_class = NULL;


struct GnomeCmdFilePopmenuPrivate
{
    GList *data_list;
};


inline void exec_with_app (GList *files, GnomeCmdApp *app)
{
    mime_exec_multiple (files, app);
}


/* Used by exec_default for each list of files that shares the same default application
 * This is a hash-table callback function
 */
static void htcb_exec_with_app (const gchar *key, OpenWithData *data, gpointer user_data)
{
    exec_with_app (data->files, data->app);
    g_free (data);
}


/* Executes a list of files with the same application
 *
 */
static void cb_exec_with_app (GtkMenuItem *menu_item, OpenWithData *data)
{
    exec_with_app (data->files, data->app);
}


/* Iterates through all files and gets their default application.
 * All files with the same default app are grouped together and opened in one call.
 */
static void cb_exec_default (GtkMenuItem *menu_item, GList *files)
{
    GHashTable *hash = g_hash_table_new (g_str_hash, g_str_equal);

    for (; files; files = files->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) files->data;
        GnomeVFSMimeApplication *vfs_app = f->get_default_application();

        if (vfs_app)
        {
            OpenWithData *data = (OpenWithData *) g_hash_table_lookup (hash, vfs_app->id);

            if (!data)
            {
                data = g_new0 (OpenWithData, 1);
                data->app = gnome_cmd_app_new_from_vfs_app (vfs_app);
                data->files = NULL;
                g_hash_table_insert (hash, vfs_app->id, data);
            }

            gnome_vfs_mime_application_free (vfs_app);
            data->files = g_list_append (data->files, f);
        }
        else
            gnome_cmd_show_message (NULL, f->info->name, _("Couldn't retrieve MIME type of the file."));
    }

    g_hash_table_foreach (hash, (GHFunc) htcb_exec_with_app, NULL);

    g_hash_table_destroy (hash);
}


static gboolean on_open_with_other_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GList *files)
{
    GtkWidget *term_check = lookup_widget (GTK_WIDGET (string_dialog), "term_check");

    if (!values[0] || strlen(values[0]) < 1)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("Invalid command")));
        return FALSE;
    }

    string cmd = values[0];

    for (; files; files = files->next)
    {
        cmd += ' ';
        cmd += stringify (GNOME_CMD_FILE (files->data)->get_quoted_real_path());
    }

    GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);
    GnomeCmdDir *dir = fs->get_directory();
    gchar *dpath = GNOME_CMD_FILE (dir)->get_real_path();
    run_command_indir (cmd.c_str(), dpath, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (term_check)));
    g_free (dpath);

    return TRUE;
}


static void on_open_with_other (GtkMenuItem *menu_item, GList *files)
{
    const gchar *labels[] = {_("Application:")};
    GtkWidget *dialog;

    dialog = gnome_cmd_string_dialog_new (_("Open with other..."), labels, 1,
                                          (GnomeCmdStringDialogCallback) on_open_with_other_ok, files);

    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    GtkWidget *term_check = create_check (dialog, _("Needs terminal"), "term_check");

    g_object_ref (dialog);
    g_object_set_data_full (G_OBJECT (menu_item), "new_textfile_dialog", dialog, g_object_unref);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), term_check);

    gtk_widget_show (dialog);
}


static void on_execute (GtkMenuItem *menu_item, GList *files)
{
    GnomeCmdFile *f = GNOME_CMD_FILE (files->data);

    f->execute();
}


static void on_execute_py_plugin (GtkMenuItem *menu_item, PythonPluginData *data)
{
#ifdef HAVE_PYTHON
    gnome_cmd_python_plugin_execute(data, main_win);
#endif
}


static void on_execute_script (GtkMenuItem *menu_item, ScriptData *data)
{
    GdkModifierType mask;

    gdk_window_get_pointer (NULL, NULL, NULL, &mask);

    GList *files = data->files;
    if (state_is_shift (mask))
    {
        // Run script per file
        for (; files; files = files->next)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) files->data;
            string command (data->name);
            command.append (" ").append (f->get_name ());

            run_command_indir (command.c_str (), f->get_dirname (), data->term);
        }
    }
    else
    {
        // Run script with list of files
        string command (data->name);
        command.append (" ");
        for (; files; files = files->next)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) files->data;
            command.append(f->get_name ()).append(" ");
        }

        run_command_indir (command.c_str (), ((GnomeCmdFile *) data->files->data)->get_dirname (), data->term);
    }

    g_list_free (data->files);
    g_free ((gpointer) data->name);
}


static void on_cut (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_cap_cut (fl);
}


static void on_copy (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_cap_copy (fl);
}


static void on_delete (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_show_delete_dialog (fl);
}


static void on_rename (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_show_rename_dialog (fl);
}


static void on_properties (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_show_properties_dialog (fl);
}


static void add_fav_app_menu_item (GnomeCmdFilePopmenu *menu, GnomeCmdApp *app, gint pos, GList *files)
{
    OpenWithData *data = g_new0 (OpenWithData, 1);

    data->app = gnome_cmd_app_dup (app);
    data->files = files;

    menu->priv->data_list = g_list_append (menu->priv->data_list, data);

    GtkWidget *item = gtk_image_menu_item_new ();

    if (GnomeCmdPixmap *pm = gnome_cmd_app_get_pixmap (app))
        if (GtkWidget *pixmap = gtk_pixmap_new (pm->pixmap, pm->mask))
        {
            gtk_widget_show (pixmap);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), pixmap);
        }

    gtk_widget_show (item);
    gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, pos);

    // Create the contents of the menu item
    GtkWidget *label = gtk_label_new (gnome_cmd_app_get_name (app));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (item), label);

    // Connect to the signal and set user data
    g_object_set_data (G_OBJECT (item), GNOMEUIINFO_KEY_UIDATA, data);

    g_signal_connect (item, "activate", G_CALLBACK (cb_exec_with_app), data);
}


inline gboolean fav_app_matches_files (GnomeCmdApp *app, GList *files)
{
    switch (gnome_cmd_app_get_target (app))
    {
        case APP_TARGET_ALL_DIRS:
            for (; files; files = files->next)
            {
                GnomeCmdFile *f = (GnomeCmdFile *) files->data;
                if (f->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_ALL_FILES:
            for (; files; files = files->next)
            {
                GnomeCmdFile *f = (GnomeCmdFile *) files->data;
                if (f->info->type != GNOME_VFS_FILE_TYPE_REGULAR)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_ALL_DIRS_AND_FILES:
            for (; files; files = files->next)
            {
                GnomeCmdFile *f = (GnomeCmdFile *) files->data;
                if (f->info->type != GNOME_VFS_FILE_TYPE_REGULAR
                    && f->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_SOME_FILES:
            for (; files; files = files->next)
            {
                gboolean ok = FALSE;
#ifdef FNM_CASEFOLD
                gint fn_flags = FNM_NOESCAPE | FNM_CASEFOLD;
#else
                gint fn_flags = FNM_NOESCAPE;
#endif

                GnomeCmdFile *f = (GnomeCmdFile *) files->data;
                if (f->info->type != GNOME_VFS_FILE_TYPE_REGULAR)
                    return FALSE;

                // Check that the file matches at least one pattern
                GList *patterns = gnome_cmd_app_get_pattern_list (app);
                for (; patterns; patterns = patterns->next)
                {
                    gchar *pattern = (gchar *) patterns->data;
                    ok |= fnmatch (pattern, f->info->name, fn_flags) == 0;
                }

                if (!ok) return FALSE;
            }
            return TRUE;
    }

    return FALSE;
}


inline void add_plugin_menu_items (GnomeCmdFilePopmenu *menu, GList *items, gint pos)
{
    for (; items; items = items->next)
    {
        GtkWidget *item = GTK_WIDGET (items->data);
        gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, pos++);
    }
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdFilePopmenu *menu = GNOME_CMD_FILE_POPMENU (object);

    g_list_foreach (menu->priv->data_list, (GFunc) g_free, NULL);
    g_list_free (menu->priv->data_list);

    g_free (menu->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdFilePopmenuClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkMenuClass *) gtk_type_class (gtk_menu_get_type ());

    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdFilePopmenu *menu)
{
    menu->priv = g_new0 (GnomeCmdFilePopmenuPrivate, 1);
}


inline gchar *string_double_underscores (const gchar *string)
{
    if (!string)
        return NULL;

    int underscores = 0;

    for (const gchar *p = string; *p; p++)
        underscores += (*p == '_');

    if (underscores == 0)
        return g_strdup (string);

    gchar *escaped = g_new (char, strlen (string) + underscores + 1);
    gchar *q = escaped;

    for (const gchar *p = string; *p; p++, q++)
    {
        /* Add an extra underscore. */
        if (*p == '_')
            *q++ = '_';
        *q = *p;
    }
    *q = '\0';

    return escaped;
}


inline gchar *get_default_application_action_name (GList *files, gchar **icon_path)
{
    if (g_list_length(files)>1)
        return g_strdup (_("_Open"));

    GnomeCmdFile *f = (GnomeCmdFile *) files->data;
    gchar *uri_str = f->get_uri_str();
    GnomeVFSMimeApplication *app = gnome_vfs_mime_get_default_application_for_uri (uri_str, f->info->mime_type);
    
    if (icon_path)
    {
        GnomeCmdApp *gapp = gnome_cmd_app_new_from_vfs_app (app);
        if (gapp)
        {
            *icon_path = g_strdup (gapp->icon_path);
            gnome_cmd_app_free (gapp);
        }
        else
            *icon_path = NULL;
    }
    
    g_free (uri_str);

    if (!app)
        return g_strdup (_("_Open"));

    gchar *escaped_app_name = string_double_underscores (app->name);
    gnome_vfs_mime_application_free (app);
    gchar *retval = g_strdup_printf (_("_Open with \"%s\""), escaped_app_name);
    g_free (escaped_app_name);

    return retval;
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_file_popmenu_new (GnomeCmdFileList *fl)
{
    gint pos, match_count;
    GList *vfs_apps, *tmp;

    // Make place for separator and open with other...
    static GnomeUIInfo apps_uiinfo[MAX_OPEN_WITH_APPS+2];

    static GnomeUIInfo open_uiinfo[] =
    {
        GNOMEUIINFO_ITEM_NONE(N_("_Open"), NULL, cb_exec_default),
        GNOMEUIINFO_SUBTREE(N_("Open Wit_h"), apps_uiinfo),
        GNOMEUIINFO_END
    };

    static GnomeUIInfo exec_uiinfo[] =
    {
        GNOMEUIINFO_ITEM_NONE(N_("E_xecute"), NULL, on_execute),
        GNOMEUIINFO_END
    };

    static GnomeUIInfo sep_uiinfo[] =
    {
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_END
    };

    static GnomeUIInfo other_uiinfo[] =
    {
        GNOMEUIINFO_ITEM_STOCK(N_("Cu_t"), NULL, on_cut, GTK_STOCK_CUT),
        GNOMEUIINFO_ITEM_STOCK(N_("_Copy"), NULL, on_copy, GTK_STOCK_COPY),
        GNOMEUIINFO_ITEM(N_("Copy file names"), NULL, edit_copy_fnames, copy_file_names_xpm),
        GNOMEUIINFO_ITEM_STOCK(N_("_Delete"), NULL, on_delete, GTK_STOCK_DELETE),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_ITEM_NONE (N_("Rename"), NULL, on_rename),
        GNOMEUIINFO_ITEM_STOCK(N_("Send files"), NULL, file_sendto, GTK_STOCK_EXECUTE),
        GNOMEUIINFO_ITEM_FILENAME (N_("Open this _folder"), NULL, command_open_nautilus, PACKAGE_NAME G_DIR_SEPARATOR_S "nautilus.svg"),
        GNOMEUIINFO_ITEM_FILENAME (N_("Open _terminal here"), NULL, command_open_terminal__internal, PACKAGE_NAME G_DIR_SEPARATOR_S "terminal.svg"),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_ITEM_STOCK(N_("_Properties..."), NULL, on_properties, GTK_STOCK_PROPERTIES),
        GNOMEUIINFO_END
    };

    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), NULL);
    GList *files = fl->get_selected_files();
    if (!files) return NULL;

    GnomeCmdFilePopmenu *menu = (GnomeCmdFilePopmenu *) g_object_new (GNOME_CMD_TYPE_FILE_POPMENU, NULL);

    GnomeCmdFile *f = (GnomeCmdFile *) files->data;


    // Fill the "Open With" menu with applications
    gint i = -1;
    menu->priv->data_list = NULL;

    vfs_apps = tmp = gnome_vfs_mime_get_all_applications (f->info->mime_type);
    for (; vfs_apps && i < MAX_OPEN_WITH_APPS; vfs_apps = vfs_apps->next)
    {
        GnomeVFSMimeApplication *vfs_app = (GnomeVFSMimeApplication *) vfs_apps->data;

        if (vfs_app)
        {
            OpenWithData *data = g_new0 (OpenWithData, 1);

            data->files = files;
            data->app = gnome_cmd_app_new_from_vfs_app (vfs_app);

            apps_uiinfo[++i].type = GNOME_APP_UI_ITEM;
            apps_uiinfo[i].label = g_strdup (gnome_cmd_app_get_name (data->app));
            apps_uiinfo[i].moreinfo = (gpointer) cb_exec_with_app;
            apps_uiinfo[i].user_data = data;
            if (data->app->icon_path)
            {
                apps_uiinfo[i].pixmap_type = GNOME_APP_PIXMAP_FILENAME;
                apps_uiinfo[i].pixmap_info = g_strdup (data->app->icon_path);
            }
        }
    }

    if (i >= 0)
        apps_uiinfo[++i].type = GNOME_APP_UI_SEPARATOR;

    // Add open with other
    apps_uiinfo[++i].type = GNOME_APP_UI_ITEM;
    apps_uiinfo[i].label = g_strdup (_("Other _Application..."));
    apps_uiinfo[i].moreinfo = (gpointer) on_open_with_other;
    apps_uiinfo[i].user_data = files;
    apps_uiinfo[i].pixmap_type = GNOME_APP_PIXMAP_NONE;

    gnome_vfs_mime_application_list_free (tmp);
    apps_uiinfo[++i].type = GNOME_APP_UI_ENDOFINFO;

    // Set default callback data
    for (gint i=0; open_uiinfo[i].type != GNOME_APP_UI_ENDOFINFO; ++i)
        if (open_uiinfo[i].type == GNOME_APP_UI_ITEM)
            open_uiinfo[i].user_data = fl;

    for (gint i=0; other_uiinfo[i].type != GNOME_APP_UI_ENDOFINFO; ++i)
        if (other_uiinfo[i].type == GNOME_APP_UI_ITEM)
            other_uiinfo[i].user_data = fl;

    open_uiinfo[0].label = get_default_application_action_name(files, (gchar **) &open_uiinfo[0].pixmap_info);  // must be freed after gnome_app_fill_menu ()
    open_uiinfo[0].pixmap_type = open_uiinfo[0].pixmap_info ? GNOME_APP_PIXMAP_FILENAME : GNOME_APP_PIXMAP_NONE;
    open_uiinfo[0].user_data = files;
    exec_uiinfo[0].user_data = files;

    // Fill the menu
    pos = 0;
    gnome_app_fill_menu (GTK_MENU_SHELL (menu), open_uiinfo, NULL, FALSE, pos);

    g_free ((gpointer) open_uiinfo[0].label);

    pos += 3;
    if (f->is_executable() && g_list_length (files) == 1)
        gnome_app_fill_menu (GTK_MENU_SHELL (menu), exec_uiinfo, NULL, FALSE, pos++);

    gnome_app_fill_menu (GTK_MENU_SHELL (menu), sep_uiinfo, NULL, FALSE, pos++);

    // Add favorite applications
    match_count = 0;
    for (GList *i=gnome_cmd_data.options.fav_apps; i; i = i->next)
    {
        GnomeCmdApp *app = (GnomeCmdApp *) i->data;
        if (fav_app_matches_files (app, files))
        {
            add_fav_app_menu_item (menu, app, pos++, files);
            match_count++;
        }
    }

    for (GList *i=plugin_manager_get_all (); i; i = i->next)
    {
        PluginData *data = (PluginData *) i->data;
        if (data->active)
        {
            GList *items = gnome_cmd_plugin_create_popup_menu_items (data->plugin, main_win->get_state());
            if (items)
            {
                add_plugin_menu_items (menu, items, pos);
                match_count++;
                pos += g_list_length (items);
            }
        }
    }

    if (match_count > 0)
        gnome_app_fill_menu (GTK_MENU_SHELL (menu), sep_uiinfo, NULL, FALSE, pos++);

    GList *py_plugins = NULL;
#ifdef HAVE_PYTHON
    py_plugins = gnome_cmd_python_plugin_get_list();
#endif
    guint n = g_list_length (py_plugins);

    if (n)
    {
        GnomeUIInfo *py_uiinfo = g_new0 (GnomeUIInfo, n+1);
        GnomeUIInfo *tmp = py_uiinfo;

        for (; py_plugins; py_plugins = py_plugins->next, ++tmp)
        {
            PythonPluginData *data = (PythonPluginData *) py_plugins->data;

            if (data)
            {
                tmp->type = GNOME_APP_UI_ITEM;
                tmp->label = _(data->name);
                tmp->moreinfo = (gpointer) on_execute_py_plugin;
                tmp->user_data = data;
                tmp->pixmap_type = GNOME_APP_PIXMAP_NONE; // GNOME_APP_PIXMAP_FILENAME;
                // tmp->pixmap_info = "pixmaps/python-plugin.svg";
            }
        }

        tmp->type = GNOME_APP_UI_ENDOFINFO;

        gnome_app_fill_menu (GTK_MENU_SHELL (menu), py_uiinfo, NULL, FALSE, pos);
        pos += n;
        gnome_app_fill_menu (GTK_MENU_SHELL (menu), sep_uiinfo, NULL, FALSE, pos++);

        g_free (py_uiinfo);
    }
	
    // Script actions
    gchar *user_dir = g_build_filename (g_get_home_dir (), "." PACKAGE "/scripts", NULL);
    DIR *dp = opendir (user_dir);
    GList *slist = NULL;
    if (dp != NULL)
    {
        struct dirent *ep;
        while (ep = readdir (dp))
        {
            struct stat buf;
            string s (user_dir);
            s.append ("/").append (ep->d_name);
            if (stat (s.c_str(), &buf) == 0)
            {
                if (buf.st_mode & S_IFREG)
                {
                    slist = g_list_append (slist, g_strdup(ep->d_name));
                }
            }
        }
        closedir (dp);
    }

    n = g_list_length(slist);
    if (n)
    {
        GnomeUIInfo *py_uiinfo = g_new0 (GnomeUIInfo, n+1);
        GnomeUIInfo *tmp = py_uiinfo;

        for (GList *l = slist; l; l = l->next)
        {
            ScriptData *data = g_new0 (ScriptData, 1);
            data->files = files;

            string s (user_dir);
            s.append ("/").append ((char*) l->data);

            FILE *f = fopen (s.c_str (), "r");
            if (f)
            {
                char buf[256];
                while (fgets (buf, 256, f))
                {
                    if (strncmp (buf, "#name:", 6) == 0)
                    {
                        buf[strlen (buf)-1] = '\0';
                        g_free ((gpointer) l->data);
                        l->data = g_strdup (buf+7);
                    }
                    else if (strncmp (buf, "#term:", 6) == 0)
                    {
                        data->term = strncmp (buf + 7, "true", 4) == 0;
                    }
                }
                fclose (f);
            }

            data->name = g_strdup (s.c_str ());
            tmp->type = GNOME_APP_UI_ITEM;
            tmp->label = (gchar*) l->data;
            tmp->moreinfo = (gpointer) on_execute_script;
            tmp->user_data = data;
            tmp->pixmap_type = GNOME_APP_PIXMAP_STOCK;
            tmp->pixmap_info = GTK_STOCK_EXECUTE;
            tmp++;
        }
        tmp++;
        tmp->type = GNOME_APP_UI_ENDOFINFO;

        gnome_app_fill_menu (GTK_MENU_SHELL (menu), py_uiinfo, NULL, FALSE, pos);
        pos += n;
        gnome_app_fill_menu (GTK_MENU_SHELL (menu), sep_uiinfo, NULL, FALSE, pos++);

        g_free (py_uiinfo);
    }
    g_free (user_dir);
    g_list_free (slist);

    gnome_app_fill_menu (GTK_MENU_SHELL (menu), other_uiinfo, NULL, FALSE, pos++);

    return GTK_WIDGET (menu);
}


GtkType gnome_cmd_file_popmenu_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdFilePopmenu",
            sizeof (GnomeCmdFilePopmenu),
            sizeof (GnomeCmdFilePopmenuClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gtk_menu_get_type (), &dlg_info);
    }
    return dlg_type;
}
