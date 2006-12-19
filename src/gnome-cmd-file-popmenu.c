/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#include "gnome-cmd-file-popmenu.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-main-win.h"
#include "plugin_manager.h"
#include "utils.h"
#include "cap.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fnmatch.h>


#define MAX_OPEN_WITH_APPS 20


typedef struct
{
    GList *files;
    GnomeCmdApp *app;
    GtkPixmap *pm;
} OpenWithData;


static GtkMenuClass *parent_class = NULL;

struct _GnomeCmdFilePopmenuPrivate
{
    GList *data_list;
};


static void
exec_with_app (GList *files, GnomeCmdApp *app)
{
    mime_exec_multiple (files, app);
}


/* Used by exec_default for each list of files that shares the same default application
 * This is a hash-table callback function
 */
static void
htcb_exec_with_app (const gchar* key,
                    OpenWithData *data,
                    gpointer user_data)
{
    exec_with_app (data->files, data->app);
    g_free (data);
}


/* Executes a list of files with the same application
 *
 */
static void
cb_exec_with_app (GtkMenuItem *menu_item,
                  OpenWithData *data)
{
     exec_with_app (data->files, data->app);
}


/* Iterates through all files and gets their default application.
 * All files with the same default app are grouped together and opened in one call.
 */
static void
cb_exec_default (GtkMenuItem *menu_item,
                 GList *files)
{
    GHashTable *hash = g_hash_table_new (g_str_hash, g_str_equal);

    for (; files; files = files->next)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile*)files->data;
        GnomeVFSMimeApplication *vfs_app =
            gnome_vfs_mime_get_default_application (finfo->info->mime_type);
        if (vfs_app) {
            OpenWithData *data = (OpenWithData*)g_hash_table_lookup (hash, vfs_app->id);
            if (!data) {
                data = g_new (OpenWithData, 1);
                data->app = gnome_cmd_app_new_from_vfs_app (vfs_app);
                data->files = NULL;
                g_hash_table_insert (hash, vfs_app->id, data);
            }

            gnome_vfs_mime_application_free (vfs_app);
            data->files = g_list_append (data->files, finfo);
        }
    }

    g_hash_table_foreach (hash, (GHFunc)htcb_exec_with_app, NULL);
    g_hash_table_destroy (hash);
}


static gboolean
on_open_with_other_ok (GnomeCmdStringDialog *string_dialog,
                       const gchar **values,
                       GList *files)
{
    gchar *cmd;
    GtkWidget *term_check = lookup_widget (GTK_WIDGET (string_dialog), "term_check");

    if (!values[0] || strlen(values[0]) < 1) {
        gnome_cmd_string_dialog_set_error_desc (
            string_dialog, g_strdup (_("Invalid command")));
        return FALSE;
    }

    cmd = g_strdup_printf ("%s ", values[0]);

    for (; files; files = files->next)
    {
        gchar *path = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (files->data));
        gchar *tmp = cmd;
        gchar *arg = g_shell_quote (path);
        cmd = g_strdup_printf ("%s %s", tmp, arg);
        g_free (arg);
        g_free (path);
        g_free (tmp);
    }

    run_command (cmd, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (term_check)));
    g_free (cmd);

    return TRUE;
}


static void
on_open_with_other (GtkMenuItem *menu_item,
                    GList *files)
{
    const gchar *labels[] = {_("Application:")};
    GtkWidget *dialog, *term_check;

    dialog = gnome_cmd_string_dialog_new (
        _("Open with other..."), labels, 1,
        (GnomeCmdStringDialogCallback)on_open_with_other_ok, files);
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    term_check = create_check (dialog, _("Needs terminal"), "term_check");

    gtk_widget_ref (dialog);
    gtk_object_set_data_full (GTK_OBJECT (menu_item), "new_textfile_dialog", dialog,
                              (GtkDestroyNotify)gtk_widget_unref);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), term_check);

    gtk_widget_show (dialog);
}


static void
on_execute (GtkMenuItem *menu_item,
            GList *files)
{
    GnomeCmdFile *finfo = GNOME_CMD_FILE (files->data);

    gnome_cmd_file_execute (finfo);
}


static void
on_cut (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_cap_cut (fl);
}


static void
on_copy (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_cap_copy (fl);
}


static void
on_delete (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_show_delete_dialog (fl);
}


static void
on_rename (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_show_rename_dialog (fl);
}


static void
on_properties (GtkMenuItem *item, GnomeCmdFileList *fl)
{
    gnome_cmd_file_list_show_properties_dialog (fl);
}


static void
add_fav_app_menu_item (GnomeCmdFilePopmenu *menu,
                       GnomeCmdApp *app,
                       gint pos,
                       GList *files)
{
    GtkWidget *item, *label;
    GnomeCmdPixmap *pm;
    GtkWidget *pixmap = NULL;

    OpenWithData *data = g_new (OpenWithData, 1);

    data->app = gnome_cmd_app_dup (app);
    data->files = files;

    menu->priv->data_list = g_list_append (menu->priv->data_list, data);

    item = gtk_image_menu_item_new ();
    pm = gnome_cmd_app_get_pixmap (app);
    if (pm) {
        pixmap = gtk_pixmap_new (pm->pixmap, pm->mask);
        if (pixmap) {
            gtk_widget_show (pixmap);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), pixmap);
        }
    }


    gtk_widget_show (item);
    gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, pos);


    // Create the contents of the menu item
    label = gtk_label_new (gnome_cmd_app_get_name (app));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (item), label);

    // Connect to the signal and set user data
    gtk_object_set_data (GTK_OBJECT (item),
                         GNOMEUIINFO_KEY_UIDATA,
                         data);

    gtk_signal_connect (GTK_OBJECT (item), "activate",
                        GTK_SIGNAL_FUNC (cb_exec_with_app), data);
}


static gboolean
fav_app_matches_files (GnomeCmdApp *app, GList *files)
{
    GnomeCmdFile *finfo;

    switch (gnome_cmd_app_get_target (app))
    {
        case APP_TARGET_ALL_DIRS:
            for (; files; files = files->next)
            {
                finfo = (GnomeCmdFile*)files->data;
                if (finfo->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_ALL_FILES:
            for (; files; files = files->next)
            {
                finfo = (GnomeCmdFile*)files->data;
                if (finfo->info->type != GNOME_VFS_FILE_TYPE_REGULAR)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_ALL_DIRS_AND_FILES:
            for (; files; files = files->next)
            {
                finfo = (GnomeCmdFile*)files->data;
                if (finfo->info->type != GNOME_VFS_FILE_TYPE_REGULAR
                    && finfo->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
                    return FALSE;
            }
            return TRUE;

        case APP_TARGET_SOME_FILES:
            for (; files; files = files->next)
            {
                gboolean ok = FALSE;
                gint fn_flags = FNM_NOESCAPE | FNM_CASEFOLD;

                finfo = (GnomeCmdFile*)files->data;
                if (finfo->info->type != GNOME_VFS_FILE_TYPE_REGULAR)
                    return FALSE;

                // Check that the file matches atleast one pattern
                GList *patterns = gnome_cmd_app_get_pattern_list (app);
                for (; patterns; patterns = patterns->next)
                {
                    char *pattern = (gchar*)patterns->data;
                    if (fnmatch (pattern, finfo->info->name, fn_flags) == 0)
                        ok = TRUE;
                }

                if (!ok) return FALSE;
            }
            return TRUE;
    }

    return FALSE;
}


static void
add_plugin_menu_items (GnomeCmdFilePopmenu *menu, GList *items, gint pos)
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

static void
destroy (GtkObject *object)
{
    GnomeCmdFilePopmenu *menu = GNOME_CMD_FILE_POPMENU (object);

    g_list_foreach (menu->priv->data_list, (GFunc)g_free, NULL);
    g_list_free (menu->priv->data_list);

    g_free (menu->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdFilePopmenuClass *klass)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = GTK_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = gtk_type_class (gtk_menu_get_type ());
    object_class->destroy = destroy;
    widget_class->map = map;
}


static void
init (GnomeCmdFilePopmenu *menu)
{
    menu->priv = g_new (GnomeCmdFilePopmenuPrivate, 1);
}




/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_file_popmenu_new (GnomeCmdFileList *fl)
{
    gint i, pos, match_count;
    GList *vfs_apps, *files, *tmp;
    GnomeCmdFilePopmenu *menu;
    GnomeCmdFile *finfo;

    // Make place for separator and open with other...
    static GnomeUIInfo apps_uiinfo[MAX_OPEN_WITH_APPS+2];

    static GnomeUIInfo open_uiinfo[] =
    {
        {
            GNOME_APP_UI_ITEM, N_("_Open"),
            NULL,
            cb_exec_default, NULL, NULL,
            GNOME_APP_PIXMAP_NONE, NULL,
            0, 0, NULL
        },
        {
            GNOME_APP_UI_SUBTREE, N_("_Open with..."),
            NULL,
            apps_uiinfo, NULL, NULL,
            GNOME_APP_PIXMAP_NONE, NULL,
            0, 0, NULL
        },
        GNOMEUIINFO_END
    };

    static GnomeUIInfo exec_uiinfo[] =
    {
        {
            GNOME_APP_UI_ITEM, N_("_Execute"),
            NULL,
            on_execute, NULL, NULL,
            GNOME_APP_PIXMAP_NONE, NULL,
            0, 0, NULL
        },
        GNOMEUIINFO_END
    };

    static GnomeUIInfo sep_uiinfo[] =
    {
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_END
    };

    static GnomeUIInfo other_uiinfo[] =
    {
        {
            GNOME_APP_UI_ITEM, N_("_Cut"),
            NULL,
            on_cut, NULL, NULL,
            GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT,
            0, 0, NULL
        },
        {
            GNOME_APP_UI_ITEM, N_("_Copy"),
            NULL,
            on_copy, NULL, NULL,
            GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY,
            0, 0, NULL
        },
        {
            GNOME_APP_UI_ITEM, N_("_Delete"),
            NULL,
            on_delete, NULL, NULL,
            GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TRASH,
            0, 0, NULL
        },
        {
            GNOME_APP_UI_ITEM, N_("Rename"),
            NULL,
            on_rename, NULL, NULL,
            GNOME_APP_PIXMAP_NONE, NULL,
            0, 0, NULL
        },
        GNOMEUIINFO_SEPARATOR,
        {
            GNOME_APP_UI_ITEM, N_("Properties"),
            NULL,
            on_properties, NULL, NULL,
            GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP,
            0, 0, NULL
        },
        GNOMEUIINFO_END
    };

    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), NULL);
    files = gnome_cmd_file_list_get_selected_files (fl);
    if (!files) return NULL;

    menu = gtk_type_new (gnome_cmd_file_popmenu_get_type ());

    finfo = (GnomeCmdFile*)files->data;


    /* Fill the "Open with..." menu with applications
     *
     */
    i = -1;
    menu->priv->data_list = NULL;

    vfs_apps = tmp = gnome_vfs_mime_get_all_applications (finfo->info->mime_type);
    for (; vfs_apps; vfs_apps = vfs_apps->next)
    {
        GnomeVFSMimeApplication *vfs_app = (GnomeVFSMimeApplication*)vfs_apps->data;

        if (vfs_app)
        {
            OpenWithData *data = g_new (OpenWithData, 1);

            data->files = files;
            data->app = gnome_cmd_app_new_from_vfs_app (vfs_app);

            i++;
            apps_uiinfo[i].type = GNOME_APP_UI_ITEM;
            apps_uiinfo[i].label = g_strdup (gnome_cmd_app_get_name (data->app));
            apps_uiinfo[i].moreinfo = cb_exec_with_app;
            apps_uiinfo[i].user_data = data;

            menu->priv->data_list = g_list_append (
                menu->priv->data_list, data);
        }

        if (i >= MAX_OPEN_WITH_APPS)
            break;
    }

    if (i >= 0)
        apps_uiinfo[i++].type = GNOME_APP_UI_SEPARATOR;

    // Add open with other
    i++;
    apps_uiinfo[i].type = GNOME_APP_UI_ITEM;
    apps_uiinfo[i].label = g_strdup (_("Other..."));
    apps_uiinfo[i].moreinfo = on_open_with_other;
    apps_uiinfo[i].user_data = files;

    gnome_vfs_mime_application_list_free (tmp);
    apps_uiinfo[i+1].type = GNOME_APP_UI_ENDOFINFO;


    /* Set default callback data
     */
    i = 0;
    while (open_uiinfo[i].type != GNOME_APP_UI_ENDOFINFO) {
        if (open_uiinfo[i].type == GNOME_APP_UI_ITEM) {
            open_uiinfo[i].user_data = fl;
        }
        i++;
    }
    i = 0;
    while (other_uiinfo[i].type != GNOME_APP_UI_ENDOFINFO) {
        if (other_uiinfo[i].type == GNOME_APP_UI_ITEM) {
            other_uiinfo[i].user_data = fl;
        }
        i++;
    }

    open_uiinfo[0].user_data = files;
    exec_uiinfo[0].user_data = files;



    /* Fill the menu
     */
    pos = 0;
    gnome_app_fill_menu (GTK_MENU_SHELL (menu), open_uiinfo,
                         NULL, FALSE, pos);
    pos += 3;
    if (gnome_cmd_file_is_executable (finfo)
        && g_list_length (files) == 1) {
        gnome_app_fill_menu (GTK_MENU_SHELL (menu), exec_uiinfo,
                             NULL, FALSE, pos);
        pos++;
    }

    gnome_app_fill_menu (GTK_MENU_SHELL (menu), sep_uiinfo,
                         NULL, FALSE, pos++);

    /*
     * Add favorite applications
     */
    tmp = gnome_cmd_data_get_fav_apps ();
    match_count = 0;
    while (tmp) {
        GnomeCmdApp *app = (GnomeCmdApp*)tmp->data;
        if (fav_app_matches_files (app, files)) {
            add_fav_app_menu_item (menu, app, pos, files);
            pos++;
            match_count++;
        }
        tmp = tmp->next;
    }

    tmp = plugin_manager_get_all ();
    while (tmp) {
        PluginData *data = (PluginData*)tmp->data;
        if (data->active) {
            GList *items = gnome_cmd_plugin_create_popup_menu_items (
                data->plugin,
                gnome_cmd_main_win_get_state (main_win));
            if (items) {
                add_plugin_menu_items (menu, items, pos);
                match_count++;
                pos += g_list_length (items);
            }
        }
        tmp = tmp->next;
    }

    if (match_count > 0) {
        gnome_app_fill_menu (GTK_MENU_SHELL (menu), sep_uiinfo,
                             NULL, FALSE, pos++);
    }

    gnome_app_fill_menu (GTK_MENU_SHELL (menu), other_uiinfo,
                         NULL, FALSE, pos);

    return GTK_WIDGET (menu);
}



GtkType
gnome_cmd_file_popmenu_get_type         (void)
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
