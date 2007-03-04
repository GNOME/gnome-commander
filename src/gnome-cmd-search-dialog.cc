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
#include <sys/types.h>
#include <regex.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-search-dialog.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-main-win.h"
#include "filter.h"
#include "utils.h"

using namespace std;


static GnomeCmdDialogClass *parent_class = NULL;

#define PBAR_MAX 50

typedef struct
{
    GList *files;
    gchar *msg;
    GMutex *mutex;

} ProtectedData;

typedef struct
{
    // the pattern that filenames should match to end up in the file-list
    const gchar *name_pattern;

    // the pattern that the content of a file should match to end up in the file-list.
    const gchar *content_pattern;

    // the current dir of the search routine.
    const gchar *dir;

    Filter *name_filter;
    regex_t *content_regex;

    // should we do content search?
    gboolean content_search;

    // the number of matching files
    gint matches;

    // the context id of the statusbar
    gint context_id;

    GnomeCmdSearchDialog *dialog;

    // stopps the search routine if set to TRUE. This is done by the stop_button
    gboolean stopped;

    // set when the search dialog is destroyed, also stopps the search of course
    gboolean dialog_destroyed;

    // should we recurse or just search in the selected directory?
    gboolean recurse;

    gboolean case_sens;

    // The directories which we found matching files in
    GList *match_dirs;

    // The directory to start searching from
    GnomeCmdDir *start_dir;

    GThread *thread;

    ProtectedData pdata;

    gboolean search_done;

    gint update_gui_timeout_id;

} SearchData;

struct _GnomeCmdSearchDialogPrivate
{
    SearchData *data;              // holds data needed by the search routines

    GnomeCmdCon *con;
    GtkWidget *pattern_combo;
    GtkWidget *pattern_entry;
    GtkWidget *dir_browser;
    GtkWidget *dir_entry;
    GtkWidget *find_text_combo;
    GtkWidget *find_text_entry;
    GtkWidget *find_text_check;
    GtkWidget *result_list;
    GtkWidget *statusbar;
    GtkWidget *goto_button;
    GtkWidget *close_button;
    GtkWidget *stop_button;
    GtkWidget *search_button;
    GtkWidget *recurse_check;
    GtkWidget *case_check;
    GtkWidget *pbar;
};

static void
on_search (GtkButton *button, GnomeCmdSearchDialog *dialog);


static gboolean
on_pattern_entry_keypressed (GtkWidget            *entry,
                             GdkEventKey          *event,
                             GnomeCmdSearchDialog *dialog)
{
    if (state_is_blank (event->state) && event->keyval == GDK_Return)
    {
        on_search (NULL, dialog);
        event->keyval = GDK_Escape;
        return TRUE;
    }

    return FALSE;
}


static void
on_list_file_clicked (GnomeCmdFileList *fl,
                      GnomeCmdFile *finfo,
                      GdkEventButton *button,
                      GnomeCmdSearchDialog *dialog)
{
    if (dialog->priv->data->search_done)
        gtk_widget_set_sensitive (dialog->priv->goto_button, TRUE);
}


/**
 * Puts a string in the statusbar.
 *
 */
static void
set_statusmsg (SearchData *data, gchar *msg)
{
    if (!msg) return;

    gtk_statusbar_push (GTK_STATUSBAR (data->dialog->priv->statusbar),
                        data->context_id, msg);
}


/**
 * Loads a file and returns the content.
 * FIXME: Recode this so not the whole file has
 *        to be loaded in memory.
 */
static gchar*
load_file (GnomeCmdFile *finfo)
{
    GnomeVFSFileSize ret;
    GnomeVFSHandle *handle;

    g_return_val_if_fail (finfo != NULL, NULL);
    g_return_val_if_fail (finfo->info != NULL, NULL);

    gchar *buf = (gchar *) g_malloc (finfo->info->size+1);
    gchar *uri_str = gnome_cmd_file_get_uri_str (finfo);
    GnomeVFSResult result = gnome_vfs_open (&handle, uri_str, GNOME_VFS_OPEN_READ);

    if (result != GNOME_VFS_OK)
    {
        warn_print (_("Failed to open file %s: %s\n"),
                    uri_str, gnome_vfs_result_to_string (result));
        g_free (uri_str);
        g_free (buf);
        return NULL;
    }

    result = gnome_vfs_read (handle, buf, finfo->info->size, &ret);
    if (result != GNOME_VFS_OK)
    {
        warn_print (_("Failed to read from file %s: %s\n"),
                    uri_str, gnome_vfs_result_to_string (result));
        g_free (uri_str);
        g_free (buf);
        gnome_vfs_close (handle);
        return NULL;
    }

    buf[ret] = '\0';

    g_free (uri_str);
    gnome_vfs_close (handle);

    return buf;
}


/**
 * Determinates if the name of a file matches an regexp
 *
 */
inline gboolean name_matches (gchar *name, SearchData *data)
{
    return filter_match (data->name_filter, name);
}


/**
 * Determinates if the content of a file matches an regexp
 *
 */
inline gboolean
content_matches (GnomeCmdFile *finfo, SearchData *data)
{
    static regmatch_t match;
    gint ret = REG_NOMATCH;
    gchar *buf = load_file (finfo);
    if (buf)
    {
        ret = regexec (data->content_regex, buf, 1, &match, 0);
        g_free (buf);
    }
    return ret == 0;
}


/**
 * Searches a given directory for files that matches the criteria given by data.
 *
 */
static void
search_dir_r (GnomeCmdDir *dir, SearchData *data)
{
    GList *tmp, *files;
    gchar *path;


    // update the search status data
    if (!data->dialog_destroyed)
    {
        g_mutex_lock (data->pdata.mutex);

        path = gnome_cmd_file_get_path (GNOME_CMD_FILE (dir));
        if (data->pdata.msg)
            g_free (data->pdata.msg);
        data->pdata.msg = g_strdup_printf (_("Searching in: %s"), path);
        g_free (path);

        g_mutex_unlock (data->pdata.mutex);
    }


    // If the stopbutton was pressed let's abort here
    if (data->stopped)
        return;

    if (dir==NULL)
    return;

    gnome_cmd_dir_list_files (dir, FALSE);
    gnome_cmd_dir_get_files (dir, &files);
    tmp = files;

    if (tmp==NULL)
    return;

    // Let's iterate through all files
    while (tmp)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) tmp->data;

        // If the stopbutton was pressed let's abort here
        if (data->stopped)
            return;

        // If the current file is a directory lets continue our recursion
        if (GNOME_CMD_IS_DIR (finfo) && data->recurse)
        {
            // we don't want to go backwards or follow symlinks
            if (strcmp (finfo->info->name, ".") != 0
                && strcmp (finfo->info->name, "..") != 0
                && !GNOME_VFS_FILE_INFO_SYMLINK(finfo->info))
            {
                GnomeCmdDir *new_dir = GNOME_CMD_DIR (finfo);

                if (new_dir)
                {
                    gnome_cmd_dir_ref (new_dir);
                    search_dir_r (new_dir, data);
                    gnome_cmd_dir_unref (new_dir);
                }

                // If the stopbutton was pressed let's abort here
                if (data->stopped)
                    return;
            }
        }
        // if the file is a regular one it might match the search criteria
        else if (finfo->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
        {
            // if the name doesn't match lets go to the next file
            if (!name_matches (finfo->info->name, data))
                goto next;

            // if the user wants to we should do some content matching here
            if (data->content_search)
            {
                if (!content_matches (finfo, data))
                    goto next;
            }

            // the file matched the search criteria, lets add it to the list
            g_mutex_lock (data->pdata.mutex);
            data->pdata.files = g_list_append (data->pdata.files, finfo);
            g_mutex_unlock (data->pdata.mutex);

            // also ref each directory that has a matching file
            if (g_list_index (data->match_dirs, dir) == -1) {
                gnome_cmd_dir_ref (dir);
                data->match_dirs = g_list_append (data->match_dirs, dir);
            }

            // count the match
            data->matches++;
        }
      next:
        tmp = tmp->next;
    }
}


static gpointer
perform_search_operation (SearchData *data)
{
    // Unref all directories which contained matching files from last search
    if (data->match_dirs) {
        g_list_foreach (data->match_dirs, (GFunc)gnome_cmd_dir_unref, NULL);
        g_list_free (data->match_dirs);
        data->match_dirs = NULL;
    }

    search_dir_r (data->start_dir, data);

    // free regexps
    filter_free (data->name_filter);
    data->name_filter = NULL;

    if (data->content_search) {
        regfree (data->content_regex);
        g_free (data->content_regex);
        data->content_regex = NULL;
    }

    gnome_cmd_dir_unref (data->start_dir);
    data->start_dir = NULL;
    data->dir = NULL;

    data->search_done = TRUE;
    return NULL;
}


static gboolean
update_search_status_widgets (SearchData *data)
{
    g_mutex_lock (data->pdata.mutex);

    // Add all files found since last update to the list
    for (GList *files = data->pdata.files; files; files = files->next)
        gnome_cmd_file_list_add_file (GNOME_CMD_FILE_LIST (data->dialog->priv->result_list),
                                      GNOME_CMD_FILE (files->data), -1);
    if (data->pdata.files)
    {
        g_list_free (data->pdata.files);
        data->pdata.files = NULL;
    }

    // Update statusbar with the latest message
    set_statusmsg (data, data->pdata.msg);

    // Update the progressbar
    progress_bar_update (data->dialog->priv->pbar, PBAR_MAX);

    g_mutex_unlock (data->pdata.mutex);

    if (data->search_done) {
        if (!data->dialog_destroyed) {
            gchar *msg;
            if (data->stopped)
                msg = g_strdup_printf (
                    ngettext(
                        "Found %d match before I was stopped",
                        "Found %d matches before I was stopped",
                        data->matches),
                    data->matches);
            else
                msg = g_strdup_printf (
                    ngettext(
                        "Found %d match",
                        "Found %d matches",
                        data->matches),
                    data->matches);

            set_statusmsg (data, msg);
            g_free (msg);

            gtk_widget_set_sensitive (data->dialog->priv->search_button, TRUE);
            gtk_widget_set_sensitive (data->dialog->priv->stop_button, FALSE);

            gtk_widget_hide (data->dialog->priv->pbar);
        }

        // Returning FALSE here stops the timeout callbacks
        return FALSE;
    }

    return TRUE;
}


/*
 * This function gets called then the search-dialog is about the be destroyed.
 * The function waits for the last search-thread to finish and then frees the
 * data structure that has been shared between the search threads and the
 * main thread.
 */
static gboolean
join_thread_func (SearchData *data)
{
    if (data->thread)
        g_thread_join (data->thread);

    g_free (data);

    return FALSE;
}


static void
on_dialog_destroy (GnomeCmdSearchDialog *dialog, gpointer user_data)
{
    SearchData *data = dialog->priv->data;

    if (data)
    {
        if (!data->search_done)
            gtk_timeout_remove (data->update_gui_timeout_id);

        // Stop and wait for search thread to exit
        data->stopped = TRUE;
        data->dialog_destroyed = TRUE;
        gtk_timeout_add (1, (GtkFunction)join_thread_func, data);

        // Unref all directories which contained matching files from last search
        if (data->pdata.mutex != NULL) {
            g_mutex_lock (data->pdata.mutex);
            if (data->match_dirs) {
                g_list_foreach (data->match_dirs, (GFunc)gnome_cmd_dir_unref, NULL);
                g_list_free (data->match_dirs);
                data->match_dirs = NULL;
            }
            g_mutex_unlock (data->pdata.mutex);
        }
    }
}


static void
on_dialog_size_allocate (GtkWidget       *widget,
                         GtkAllocation   *allocation,
                         GnomeCmdSearchDialog *dialog)
{
    SearchDefaults *defaults = gnome_cmd_data_get_search_defaults ();
    defaults->width  = allocation->width;
    defaults->height = allocation->height;
}


static gboolean
start_search (GnomeCmdSearchDialog *dialog)
{
    GnomeCmdPath *path;
    SearchData *data = dialog->priv->data;
    SearchDefaults *defaults = gnome_cmd_data_get_search_defaults ();

    if (data->thread) {
        g_thread_join (data->thread);
        data->thread = NULL;
    }

    data->dialog = dialog;
    data->name_pattern = gtk_entry_get_text (GTK_ENTRY (dialog->priv->pattern_entry));
    data->content_pattern = gtk_entry_get_text (GTK_ENTRY (dialog->priv->find_text_entry));
    data->dir = gtk_entry_get_text (GTK_ENTRY (dialog->priv->dir_entry));
    data->context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (data->dialog->priv->statusbar), "info");
    data->content_search = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->find_text_check));
    data->content_regex = NULL;
    data->matches = 0;
    data->match_dirs = NULL;
    data->stopped = FALSE;
    data->dialog_destroyed = FALSE;
    data->recurse = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->recurse_check));
    data->case_sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->case_check));

    // Save default settings
    defaults->case_sens = data->case_sens;
    defaults->recursive = data->recurse;
    defaults->name_patterns = string_history_add (defaults->name_patterns, data->name_pattern, PATTERN_HISTORY_SIZE);
    defaults->directories = string_history_add (defaults->directories, data->dir, PATTERN_HISTORY_SIZE);
    if (data->content_search)
        defaults->content_patterns = string_history_add (
            defaults->content_patterns, data->content_pattern, PATTERN_HISTORY_SIZE);

    gnome_cmd_file_list_remove_all_files (GNOME_CMD_FILE_LIST (dialog->priv->result_list));

    gtk_widget_set_sensitive (data->dialog->priv->search_button, FALSE);
    gtk_widget_set_sensitive (data->dialog->priv->stop_button, TRUE);

    // create an re for filename matching
    data->name_filter = filter_new (data->name_pattern, data->case_sens);


    // if we're going to search through file content create an re for that too
    if (data->content_search)
    {
        data->content_regex = g_new (regex_t, 1);
        regcomp (data->content_regex, data->content_pattern, 0);
    }


    // start the search
    path = gnome_cmd_con_create_path (dialog->priv->con, data->dir);
    data->start_dir = gnome_cmd_dir_new (dialog->priv->con, path);
    gnome_cmd_dir_ref (data->start_dir);

    data->search_done = FALSE;
    data->pdata.mutex = g_mutex_new ();
    data->thread = g_thread_create ((GThreadFunc)perform_search_operation, data, TRUE, NULL);

    gtk_widget_show (data->dialog->priv->pbar);
    data->update_gui_timeout_id = gtk_timeout_add (
        gnome_cmd_data_get_gui_update_rate (),
        (GtkFunction)update_search_status_widgets,
        dialog->priv->data);

    return FALSE;
}


/**
 * The user has clicked on the search button
 *
 */
static void
on_search (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    gtk_timeout_add (1, (GtkFunction)start_search, dialog);

    gtk_widget_set_sensitive (dialog->priv->search_button, FALSE);
    gtk_widget_set_sensitive (dialog->priv->goto_button, FALSE);
}


/**
 * The user has clicked on the close button
 *
 */
static void
on_close (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


/**
 * The user has clicked on the stop button
 *
 */
static void
on_stop (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    g_return_if_fail (dialog != NULL);
    g_return_if_fail (dialog->priv != NULL);
    g_return_if_fail (dialog->priv->data != NULL);

    dialog->priv->data->stopped = TRUE;

    gtk_widget_set_sensitive (dialog->priv->stop_button, FALSE);
}


/**
 * The user has clicked on the goto button
 *
 */
static void
on_goto (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    GnomeCmdFile *finfo;
    GnomeCmdFileSelector *fs;
    gchar *dpath, *fpath;

    finfo = gnome_cmd_file_list_get_selected_file (GNOME_CMD_FILE_LIST (dialog->priv->result_list));
    if (!finfo)
        return;

    fpath = gnome_cmd_file_get_path (finfo);
    dpath = g_path_get_dirname (fpath);

    fs = gnome_cmd_main_win_get_active_fs (main_win);
    gnome_cmd_file_selector_goto_directory (fs, dpath);
    gnome_cmd_file_list_focus_file (fs->list, gnome_cmd_file_get_name (finfo), TRUE);

    g_free (fpath);
    g_free (dpath);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static gboolean
handle_list_keypress (GnomeCmdFileList *fl, GdkEventKey *event, GnomeCmdSearchDialog *dialog)
{
    switch (event->keyval)
    {
        case GDK_F3:
            gnome_cmd_file_list_view (fl, -1);
            return TRUE;

        case GDK_F4:
            gnome_cmd_file_list_edit (fl);
            return TRUE;
    }

    return FALSE;
}


static gboolean
on_list_keypressed (GnomeCmdFileList *fl, GdkEventKey *event, GnomeCmdSearchDialog *dialog)
{
    if (gnome_cmd_file_list_keypressed (fl, event) || handle_list_keypress (fl, event, dialog))
        stop_kp (GTK_OBJECT (fl));

    return TRUE;
}


/**
 * The user has clicked on the "search by content" checkbutton.
 *
 */
static void
find_text_toggled (GtkToggleButton *togglebutton, GnomeCmdSearchDialog *dialog)
{
    if (gtk_toggle_button_get_active (togglebutton))
    {
        gtk_widget_set_sensitive (dialog->priv->find_text_combo, TRUE);
        gtk_widget_grab_focus (dialog->priv->find_text_entry);
    }
    else
        gtk_widget_set_sensitive (dialog->priv->find_text_combo, FALSE);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdSearchDialog *dialog = GNOME_CMD_SEARCH_DIALOG (object);
    if (dialog->priv)
        g_free (dialog->priv);

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
class_init (GnomeCmdSearchDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (gnome_cmd_dialog_get_type ());

    object_class->destroy = destroy;

    widget_class->map = ::map;
}


static void
init (GnomeCmdSearchDialog *dialog)
{
    GtkWidget *window;
    GtkWidget *vbox1;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *sw;
    GtkWidget *pbar;
    SearchDefaults *defaults = gnome_cmd_data_get_search_defaults ();


    dialog->priv = g_new (GnomeCmdSearchDialogPrivate, 1);
    dialog->priv->data = g_new (SearchData, 1);
    dialog->priv->data->match_dirs = NULL;
    dialog->priv->data->thread = NULL;
    dialog->priv->data->search_done = TRUE;

    dialog->priv->data->pdata.files = NULL;
    dialog->priv->data->pdata.msg = NULL;
    dialog->priv->data->pdata.mutex = NULL;

    window = GTK_WIDGET (dialog);
    gtk_object_set_data (GTK_OBJECT (window), "window", window);
    gtk_window_set_title (GTK_WINDOW (window), _("Search..."));
    gnome_cmd_dialog_set_resizable (GNOME_CMD_DIALOG (dialog), TRUE);
    gtk_window_set_default_size (GTK_WINDOW (window), defaults->width, defaults->height);

    vbox1 = create_vbox (window, FALSE, 0);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), vbox1);

    table = create_table (window, 5, 2);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 6);
    gtk_box_pack_start (GTK_BOX (vbox1), table, FALSE, TRUE, 0);

    // Search for
    label = create_label (window, _("Search for: "));
    table_add (table, label, 0, 0, GTK_FILL);

    dialog->priv->pattern_combo = create_combo (window);
    table_add (table, dialog->priv->pattern_combo, 1, 0, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    if (defaults->name_patterns)
        gtk_combo_set_popdown_strings (GTK_COMBO (dialog->priv->pattern_combo), defaults->name_patterns);

    dialog->priv->pattern_entry = GTK_COMBO (dialog->priv->pattern_combo)->entry;
    gnome_cmd_dialog_editable_enters (GNOME_CMD_DIALOG (dialog), GTK_EDITABLE (dialog->priv->pattern_entry));

    // Search in
    label = create_label (window, _("Search in: "));
    table_add (table, label, 0, 1, GTK_FILL);

    dialog->priv->dir_browser = create_file_entry (window, "dir_browser", "");
    table_add (table, dialog->priv->dir_browser, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    if (defaults->directories)
        gtk_combo_set_popdown_strings (
            GTK_COMBO (gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (dialog->priv->dir_browser))),
            defaults->directories);

    dialog->priv->dir_entry =
        gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (dialog->priv->dir_browser));

    // Find text
    dialog->priv->find_text_check = create_check (window, _("Find text: "), "find_text");
    table_add (table, dialog->priv->find_text_check, 0, 2, GTK_FILL);

    dialog->priv->find_text_combo = create_combo (window);
    table_add (table, dialog->priv->find_text_combo, 1, 2, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    gtk_widget_set_sensitive (dialog->priv->find_text_combo, FALSE);
    if (defaults->content_patterns)
        gtk_combo_set_popdown_strings (GTK_COMBO (dialog->priv->find_text_combo), defaults->content_patterns);

    dialog->priv->find_text_entry = GTK_COMBO (dialog->priv->find_text_combo)->entry;
    gnome_cmd_dialog_editable_enters (
        GNOME_CMD_DIALOG (dialog), GTK_EDITABLE (dialog->priv->find_text_entry));


    // Recurse check
    dialog->priv->recurse_check = create_check (window, _("Search Recursively"), "recurse_check");
    gtk_table_attach (GTK_TABLE (table), dialog->priv->recurse_check, 0, 2, 3, 4,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->recurse_check), defaults->recursive);

    // Case check
    dialog->priv->case_check = create_check (window, _("Case Sensitive"), "case_check");
    gtk_table_attach (GTK_TABLE (table), dialog->priv->case_check, 0, 2, 4, 5,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->case_check), defaults->case_sens);


    dialog->priv->close_button = gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), GNOME_STOCK_PIXMAP_CLOSE,
        GTK_SIGNAL_FUNC (on_close), dialog);
    dialog->priv->goto_button = gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), _("Goto"),
        GTK_SIGNAL_FUNC (on_goto), dialog);
    dialog->priv->stop_button = gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), GNOME_STOCK_PIXMAP_STOP,
        GTK_SIGNAL_FUNC (on_stop), dialog);
    dialog->priv->search_button = gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), GNOME_STOCK_PIXMAP_SEARCH,
        GTK_SIGNAL_FUNC (on_search), dialog);

    gtk_widget_set_sensitive (dialog->priv->stop_button, FALSE);
    gtk_widget_set_sensitive (dialog->priv->goto_button, FALSE);


    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (sw);
    gtk_object_set_data_full (GTK_OBJECT (window), "sw", sw, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (sw);
    gtk_box_pack_start (GTK_BOX (vbox1), sw, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    dialog->priv->result_list = gnome_cmd_file_list_new ();
    gtk_widget_ref (dialog->priv->result_list);
    gtk_object_set_data_full (
        GTK_OBJECT (window), "result_list", dialog->priv->result_list,
        (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_set_size_request (dialog->priv->result_list, -1, 200);
    gtk_widget_show (dialog->priv->result_list);
    gtk_container_add (GTK_CONTAINER (sw), dialog->priv->result_list);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->result_list), 4);


    dialog->priv->statusbar = gtk_statusbar_new ();
    gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (dialog->priv->statusbar), FALSE);
    gtk_widget_ref (dialog->priv->statusbar);
    gtk_object_set_data_full (GTK_OBJECT (window), "statusbar", dialog->priv->statusbar,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (dialog->priv->statusbar);
    gtk_box_pack_start (GTK_BOX (vbox1), dialog->priv->statusbar, FALSE, TRUE, 0);

    pbar = create_progress_bar (window);
    gtk_widget_hide (pbar);
    gtk_progress_set_show_text (GTK_PROGRESS (pbar), FALSE);
    gtk_progress_set_activity_mode (GTK_PROGRESS (pbar), TRUE);
    gtk_progress_configure (GTK_PROGRESS (pbar), 0, 0, PBAR_MAX);
    gtk_box_pack_start (GTK_BOX (dialog->priv->statusbar), pbar, FALSE, TRUE, 0);
    dialog->priv->pbar = pbar;


    gtk_signal_connect (GTK_OBJECT (dialog), "destroy", GTK_SIGNAL_FUNC (on_dialog_destroy), NULL);
    gtk_signal_connect (GTK_OBJECT (dialog), "size-allocate", GTK_SIGNAL_FUNC (on_dialog_size_allocate), NULL);
    gtk_signal_connect (GTK_OBJECT (dialog->priv->result_list), "key-press-event", GTK_SIGNAL_FUNC (on_list_keypressed), dialog);
    gtk_signal_connect (GTK_OBJECT (dialog->priv->result_list), "file-clicked", GTK_SIGNAL_FUNC (on_list_file_clicked), dialog);

    gtk_signal_connect (GTK_OBJECT (dialog->priv->pattern_entry),
                        "key-press-event",
                        GTK_SIGNAL_FUNC (on_pattern_entry_keypressed), dialog);
    gtk_signal_connect (GTK_OBJECT (dialog->priv->find_text_check), "toggled", GTK_SIGNAL_FUNC (find_text_toggled), dialog);

    gtk_widget_grab_focus (dialog->priv->pattern_entry);
    gnome_cmd_file_list_update_style (GNOME_CMD_FILE_LIST (dialog->priv->result_list));
}


/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_search_dialog_get_type         (void)
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdSearchDialog",
            sizeof (GnomeCmdSearchDialog),
            sizeof (GnomeCmdSearchDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gnome_cmd_dialog_get_type (), &dlg_info);
    }
    return dlg_type;
}


GtkWidget*
gnome_cmd_search_dialog_new (GnomeCmdDir *default_dir)
{
    gchar *path;
    gchar *new_path;
    GnomeCmdSearchDialog *dialog = (GnomeCmdSearchDialog *) gtk_type_new (gnome_cmd_search_dialog_get_type ());

    path = gnome_cmd_file_get_path (GNOME_CMD_FILE (default_dir));
    if (path[strlen(path)-1] != '/')
    {
        new_path = g_strdup_printf ("%s/", path);
        g_free (path);
    }
    else
        new_path = path;

    gtk_entry_set_text (GTK_ENTRY (dialog->priv->dir_entry), new_path);

    g_free (new_path);

    dialog->priv->con = gnome_cmd_dir_get_connection (default_dir);

    return GTK_WIDGET (dialog);
}
