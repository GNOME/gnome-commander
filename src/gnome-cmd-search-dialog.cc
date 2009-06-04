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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-search-dialog.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "filter.h"
#include "utils.h"

using namespace std;


#if 0
static char *msgs[] = {N_("Search _recursively:"),
                       N_("_Unlimited depth"),
                       N_("Current _directory only"),
                       N_("_Limited depth"),
//                       N_("Search local directories only"),
                       N_("Files _not containing text")};
#endif


static GnomeCmdDialogClass *parent_class = NULL;

#define PBAR_MAX   50

#define SEARCH_JUMP_SIZE     4096U
#define SEARCH_BUFFER_SIZE  (SEARCH_JUMP_SIZE * 10U)

struct ProtectedData
{
    GList *files;
    gchar *msg;
    GMutex *mutex;
};

struct SearchData
{
    const gchar *name_pattern;              // the pattern that file names should match to end up in the file list
    const gchar *content_pattern;           // the pattern that the content of a file should match to end up in the file list
    const gchar *dir;                       // the current dir of the search routine

    Filter *name_filter;
    regex_t *content_regex;
    gboolean content_search;                // should we do content search?
    gint matches;                           // the number of matching files
    gint context_id;                        // the context id of the status bar
    GnomeCmdSearchDialog *dialog;
    gboolean recurse;                       // should we recurse or just search in the selected directory?
    gboolean case_sens;
    GList *match_dirs;                      // the directories which we found matching files in
    GnomeCmdDir *start_dir;                 // the directory to start searching from
    GThread *thread;
    ProtectedData pdata;
    gint update_gui_timeout_id;

    gboolean search_done;
    gboolean stopped;                       // stops the search routine if set to TRUE. This is done by the stop_button
    gboolean dialog_destroyed;              // set when the search dialog is destroyed, also stops the search of course

    gchar  *search_mem;                     // memory to search in the content of a file
};

struct SearchFileData
{
    gchar          *uri_str;
    GnomeVFSHandle *handle;
    gint            offset;
    guint           len;
};

struct GnomeCmdSearchDialogPrivate
{
    SearchData *data;                       // holds data needed by the search routines

    GnomeCmdCon *con;

    GtkWidget *pattern_combo;
    GtkWidget *dir_browser;
    GtkWidget *dir_entry;
    GtkWidget *find_text_combo;
    GtkWidget *find_text_check;
    GnomeCmdFileList *result_list;
    GtkWidget *statusbar;

    GtkWidget *help_button;
    GtkWidget *close_button;
    GtkWidget *goto_button;
    GtkWidget *stop_button;
    GtkWidget *search_button;

    GtkWidget *recurse_check;
    GtkWidget *case_check;
    GtkWidget *pbar;
};


/**
 * Puts a string in the statusbar.
 *
 */
inline void set_statusmsg (SearchData *data, gchar *msg)
{
    if (msg)
        gtk_statusbar_push (GTK_STATUSBAR (data->dialog->priv->statusbar), data->context_id, msg);
}


inline void search_file_data_free (SearchFileData  *searchfile_data)
{
    if (searchfile_data->handle != NULL)
        gnome_vfs_close (searchfile_data->handle);

    g_free (searchfile_data->uri_str);
    g_free (searchfile_data);
}

/**
 * Loads a file in chunks and returns the content.
 */
static SearchFileData *read_search_file (SearchData *data, SearchFileData *searchfile_data, GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);
    g_return_val_if_fail (f->info != NULL, NULL);

    GnomeVFSResult  result;

    if (searchfile_data == NULL)
    {
        searchfile_data          = g_new0 (SearchFileData, 1);
        searchfile_data->uri_str = gnome_cmd_file_get_uri_str (f);
        result                   = gnome_vfs_open (&searchfile_data->handle, searchfile_data->uri_str, GNOME_VFS_OPEN_READ);

        if (result != GNOME_VFS_OK)
        {
           warn_print (_("Failed to open file %s: %s\n"), searchfile_data->uri_str, gnome_vfs_result_to_string (result));
           search_file_data_free (searchfile_data);
           return NULL;
        }
    }

    // If the stop button was pressed let's abort here
    if (data->stopped)
    {
        search_file_data_free (searchfile_data);
        return NULL;
    }

    if (searchfile_data->len)
    {
      if ((searchfile_data->offset + searchfile_data->len) >= f->info->size)
      {   // end, all has been read
          search_file_data_free (searchfile_data);
          return NULL;
      }
      else
      {   // jump a big step backward to give the regex a chance
          searchfile_data->offset += searchfile_data->len - SEARCH_JUMP_SIZE;
          if (f->info->size < (searchfile_data->offset + (SEARCH_BUFFER_SIZE - 1)))
              searchfile_data->len = f->info->size - searchfile_data->offset;
          else
              searchfile_data->len = SEARCH_BUFFER_SIZE - 1;
      }
    }
    else
    {   // first time call of this function
        if (f->info->size < (SEARCH_BUFFER_SIZE - 1))
            searchfile_data->len = f->info->size;
        else
            searchfile_data->len = SEARCH_BUFFER_SIZE - 1;
    }

    GnomeVFSFileSize ret;
    result = gnome_vfs_seek (searchfile_data->handle, GNOME_VFS_SEEK_START, searchfile_data->offset);
    if (result != GNOME_VFS_OK)
    {
        warn_print (_("Failed to seek in file %s: %s\n"), searchfile_data->uri_str, gnome_vfs_result_to_string (result));
        search_file_data_free (searchfile_data);
        return NULL;
    }
    result = gnome_vfs_read (searchfile_data->handle, data->search_mem, searchfile_data->len, &ret);
    if (result != GNOME_VFS_OK)
    {
        warn_print (_("Failed to read from file %s: %s\n"), searchfile_data->uri_str, gnome_vfs_result_to_string (result));
        search_file_data_free (searchfile_data);
        return NULL;
    }
    data->search_mem[searchfile_data->len] = '\0';

    return searchfile_data;
}


/**
 * Determines if the content of a file matches an regexp
 *
 */
inline gboolean content_matches (GnomeCmdFile *f, SearchData *data)
{
    gint   ret = REG_NOMATCH;

    if (f->info->size > 0)
    {
        regmatch_t       match;
        SearchFileData  *search_file = NULL;

        while ((search_file = read_search_file (data, search_file, f)) != NULL)
            ret = regexec (data->content_regex, data->search_mem, 1, &match, 0);
    }

    return ret != REG_NOMATCH;
}

/**
 * Determines if the name of a file matches an regexp
 *
 */
inline gboolean name_matches (gchar *name, SearchData *data)
{
    return data->name_filter->match(name);
}


/**
 * Searches a given directory for files that matches the criteria given by data.
 *
 */
static void search_dir_r (GnomeCmdDir *dir, SearchData *data)
{
    if (!dir)
        return;

    // update the search status data
    if (!data->dialog_destroyed)
    {
        g_mutex_lock (data->pdata.mutex);

        gchar *path = gnome_cmd_file_get_path (GNOME_CMD_FILE (dir));
        g_free (data->pdata.msg);
        data->pdata.msg = g_strdup_printf (_("Searching in: %s"), path);
        g_free (path);

        g_mutex_unlock (data->pdata.mutex);
    }


    // If the stop button was pressed let's abort here
    if (data->stopped)
        return;

    GList *files;

    gnome_cmd_dir_list_files (dir, FALSE);
    gnome_cmd_dir_get_files (dir, &files);


    // Let's iterate through all files
    for (GList *tmp=files; tmp; tmp=tmp->next)
    {
        // If the stop button was pressed let's abort here
        if (data->stopped)
            return;

        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;

        // If the current file is a directory lets continue our recursion
        if (GNOME_CMD_IS_DIR (f) && data->recurse)
        {
            // we don't want to go backwards or follow symlinks
            if (strcmp (f->info->name, ".") != 0 &&
                strcmp (f->info->name, "..") != 0 &&
                !GNOME_VFS_FILE_INFO_SYMLINK (f->info))
            {
                GnomeCmdDir *new_dir = GNOME_CMD_DIR (f);

                if (new_dir)
                {
                    gnome_cmd_dir_ref (new_dir);
                    search_dir_r (new_dir, data);
                    gnome_cmd_dir_unref (new_dir);
                }
            }
        }
        // if the file is a regular one it might match the search criteria
        else
            if (f->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
            {
                // if the name doesn't match lets go to the next file
                if (!name_matches (f->info->name, data))
                    continue;

                // if the user wants to we should do some content matching here
                if (data->content_search && !content_matches (f, data))
                    continue;

                // the file matched the search criteria, lets add it to the list
                g_mutex_lock (data->pdata.mutex);
                data->pdata.files = g_list_append (data->pdata.files, f);
                g_mutex_unlock (data->pdata.mutex);

                // also ref each directory that has a matching file
                if (g_list_index (data->match_dirs, dir) == -1)
                {
                    gnome_cmd_dir_ref (dir);
                    data->match_dirs = g_list_append (data->match_dirs, dir);
                }

                // count the match
                data->matches++;
            }
    }
}


static gpointer perform_search_operation (SearchData *data)
{
    // Unref all directories which contained matching files from last search
    if (data->match_dirs)
    {
        g_list_foreach (data->match_dirs, (GFunc) gnome_cmd_dir_unref, NULL);
        g_list_free (data->match_dirs);
        data->match_dirs = NULL;
    }

    search_dir_r (data->start_dir, data);

    // free regexps
    delete data->name_filter;
    data->name_filter = NULL;

    if (data->content_search)
    {
        regfree (data->content_regex);
        g_free (data->content_regex);
        data->content_regex = NULL;
    }

    gnome_cmd_dir_unref (data->start_dir);
    data->start_dir = NULL;
    data->dir = NULL;

    data->search_done = TRUE;
    gtk_widget_set_sensitive (data->dialog->priv->goto_button, TRUE);

    return NULL;
}


static gboolean update_search_status_widgets (SearchData *data)
{
    g_mutex_lock (data->pdata.mutex);

    // Add all files found since last update to the list
    for (GList *files = data->pdata.files; files; files = files->next)
        data->dialog->priv->result_list->append_file(GNOME_CMD_FILE (files->data));

    if (data->pdata.files)
    {
        g_list_free (data->pdata.files);
        data->pdata.files = NULL;
    }

    // Update status bar with the latest message
    set_statusmsg (data, data->pdata.msg);

    // Update the progress bar
    progress_bar_update (data->dialog->priv->pbar, PBAR_MAX);

    g_mutex_unlock (data->pdata.mutex);

    if (data->search_done)
    {
        if (!data->dialog_destroyed)
        {
            gchar *msg;
            if (data->stopped)
                msg = g_strdup_printf (
                    ngettext("Found %d match - search aborted",
                             "Found %d matches - search aborted",
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

            // set focus to result list
            gtk_widget_grab_focus (GTK_WIDGET (data->dialog->priv->result_list));

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
static gboolean join_thread_func (SearchData *data)
{
    if (data->thread)
        g_thread_join (data->thread);

    if (data->pdata.mutex != NULL)
      g_mutex_free (data->pdata.mutex);

    g_free (data->search_mem);

    g_free (data);

    return FALSE;
}


static void on_dialog_destroy (GnomeCmdSearchDialog *dialog, gpointer user_data)
{
    SearchData *data = dialog->priv->data;

    if (data)
    {
        if (!data->search_done)
            g_source_remove (data->update_gui_timeout_id);

        // Stop and wait for search thread to exit
        data->stopped = TRUE;
        data->dialog_destroyed = TRUE;
        g_timeout_add (1, (GSourceFunc) join_thread_func, data);

        // Unref all directories which contained matching files from last search
        if (data->pdata.mutex != NULL)
        {
            g_mutex_lock (data->pdata.mutex);
            if (data->match_dirs)
            {
                g_list_foreach (data->match_dirs, (GFunc) gnome_cmd_dir_unref, NULL);
                g_list_free (data->match_dirs);
                data->match_dirs = NULL;
            }
            g_mutex_unlock (data->pdata.mutex);
        }
    }
}


static void on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdSearchDialog *dialog)
{
    gnome_cmd_data.search_defaults.width = allocation->width;
    gnome_cmd_data.search_defaults.height = allocation->height;
}


static gboolean start_search (GnomeCmdSearchDialog *dialog)
{
    SearchData *data = dialog->priv->data;

    if (data->thread)
    {
        g_thread_join (data->thread);
        data->thread = NULL;
    }

    data->dialog = dialog;
    data->name_pattern = gtk_combo_box_get_active_text (GTK_COMBO_BOX (dialog->priv->pattern_combo));
    data->content_pattern = gtk_combo_box_get_active_text (GTK_COMBO_BOX (dialog->priv->find_text_combo));
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
    gnome_cmd_data.search_defaults.case_sens = data->case_sens;
    gnome_cmd_data.search_defaults.recursive = data->recurse;
    gnome_cmd_data.search_defaults.name_patterns.add(data->name_pattern);
    gnome_cmd_data.search_defaults.directories.add(data->dir);
    if (data->content_search)
    {
        gnome_cmd_data.search_defaults.content_patterns.add(data->content_pattern);
        gnome_cmd_data.intviewer_defaults.text_patterns.add(data->content_pattern);
    }

    dialog->priv->result_list->remove_all_files();

    gtk_widget_set_sensitive (data->dialog->priv->search_button, FALSE);
    gtk_widget_set_sensitive (data->dialog->priv->stop_button, TRUE);

    // create an re for file name matching
    GtkWidget *regex_radio = lookup_widget (GTK_WIDGET (dialog), "regex_radio");

    data->name_filter = new Filter(data->name_pattern, data->case_sens,
                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (regex_radio)) ? Filter::TYPE_REGEX
                                                                                                   : Filter::TYPE_FNMATCH);

    // if we're going to search through file content create an re for that too
    if (data->content_search)
    {
        data->content_regex = g_new0 (regex_t, 1);
        regcomp (data->content_regex, data->content_pattern, data->case_sens ? 0 : REG_ICASE);
    }

    if (dialog->priv->data->search_mem == NULL)
        dialog->priv->data->search_mem = (gchar *) g_malloc (SEARCH_BUFFER_SIZE);

    // start the search
    GnomeCmdPath *path = gnome_cmd_con_create_path (dialog->priv->con, data->dir);
    data->start_dir = gnome_cmd_dir_new (dialog->priv->con, path);
    gnome_cmd_dir_ref (data->start_dir);

    data->search_done = FALSE;

    if (data->pdata.mutex == NULL)
      data->pdata.mutex = g_mutex_new ();

    data->thread = g_thread_create ((GThreadFunc) perform_search_operation, data, TRUE, NULL);

    gtk_widget_show (data->dialog->priv->pbar);
    data->update_gui_timeout_id = g_timeout_add (gnome_cmd_data.gui_update_rate,
                                                 (GSourceFunc) update_search_status_widgets,
                                                 dialog->priv->data);
    return FALSE;
}


/**
 * The user has clicked on the search button
 *
 */
static void on_search (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    g_timeout_add (1, (GSourceFunc) start_search, dialog);

    gtk_widget_set_sensitive (dialog->priv->search_button, FALSE);
    gtk_widget_set_sensitive (dialog->priv->goto_button, FALSE);
}


/**
 * The user has clicked on the help button
 *
 */
static void on_help (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-search");
}


/**
 * The user has clicked on the close button
 *
 */
static void on_close (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


/**
 * The user has clicked on the stop button
 *
 */
static void on_stop (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    g_return_if_fail (dialog != NULL);
    g_return_if_fail (dialog->priv != NULL);
    g_return_if_fail (dialog->priv->data != NULL);

    dialog->priv->data->stopped = TRUE;

    gtk_widget_set_sensitive (dialog->priv->stop_button, FALSE);
}


//  The user has clicked on the "go to" button
static void on_goto (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    GnomeCmdFile *f = dialog->priv->result_list->get_selected_file();

    if (!f)
        return;

    gchar *fpath = gnome_cmd_file_get_path (f);
    gchar *dpath = g_path_get_dirname (fpath);

    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_fs (main_win, ACTIVE);
    fs->goto_directory(dpath);
    fs->file_list()->focus_file(gnome_cmd_file_get_name (f), TRUE);

    g_free (fpath);
    g_free (dpath);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}


inline gboolean handle_list_keypress (GnomeCmdFileList *fl, GdkEventKey *event, GnomeCmdSearchDialog *dialog)
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


static gboolean on_list_keypressed (GtkWidget *result_list,  GdkEventKey *event, gpointer dialog)
{
    if (GNOME_CMD_FILE_LIST (result_list)->key_pressed(event) ||
        handle_list_keypress (GNOME_CMD_FILE_LIST (result_list), event, GNOME_CMD_SEARCH_DIALOG (dialog)))
    {
        stop_kp (GTK_OBJECT (result_list));
        return TRUE;
    }

    return FALSE;
}


// The user has clicked on the "search by content" checkbutton.
static void find_text_toggled (GtkToggleButton *togglebutton, GnomeCmdSearchDialog *dialog)
{
    if (gtk_toggle_button_get_active (togglebutton))
    {
        gtk_widget_set_sensitive (dialog->priv->find_text_combo, TRUE);
        gtk_widget_grab_focus (dialog->priv->find_text_combo);
    }
    else
        gtk_widget_set_sensitive (dialog->priv->find_text_combo, FALSE);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdSearchDialog *dialog = GNOME_CMD_SEARCH_DIALOG (object);

    g_free (dialog->priv);
    dialog->priv = NULL;

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdSearchDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (gnome_cmd_dialog_get_type ());

    object_class->destroy = destroy;

    widget_class->map = ::map;
}

/*
 * create a label with keyboard shortcut and a widget to activate if shortcut is pressed.
 */
inline GtkWidget *create_label_with_mnemonic (GtkWidget *parent, const gchar *text, GtkWidget *for_widget)
{
    GtkWidget *label = gtk_label_new_with_mnemonic (text);

    if (for_widget != NULL)
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), for_widget);

    gtk_widget_ref (label);
    gtk_object_set_data_full (GTK_OBJECT (parent), "label", label, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (label);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    return label;
}

/*
 * create a check_box_button with keyboard shortcut
 */
inline GtkWidget *create_check_with_mnemonic (GtkWidget *parent, gchar *text, gchar *name)
{
    GtkWidget *btn = gtk_check_button_new_with_mnemonic (text);

    gtk_widget_ref (btn);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, btn, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (btn);

    return btn;
}

/*
 * create a GtkRadioButton with keyboard shortcut
 */
inline GtkWidget *create_radio_with_mnemonic (GtkWidget *parent, GSList *group, gchar *text, gchar *name)
{
    GtkWidget *radio = gtk_radio_button_new_with_mnemonic (group, text);

    gtk_widget_ref (radio);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, radio, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (radio);

    return radio;
}

/*
 * create a gtk_combo_box_entry_new_text. gtk_combo is deprecated.
 */
inline GtkWidget *create_combo_box_entry (GtkWidget *parent)
{
    GtkWidget *combo = gtk_combo_box_entry_new_text ();
    gtk_widget_ref (combo);
    gtk_object_set_data_full (GTK_OBJECT (parent), "combo", combo, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (combo);
    return combo;
}

/*
 * callback function for 'g_list_foreach' to add default value to dropdownbox
 */
static void combo_box_insert_text (gpointer  data, gpointer  user_data)
{
  gtk_combo_box_append_text (GTK_COMBO_BOX (user_data), (gchar *) data);
}


static void init (GnomeCmdSearchDialog *dialog)
{
    GnomeCmdData::SearchConfig &defaults = gnome_cmd_data.search_defaults;

    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *sw;
    GtkWidget *radio;
    GtkWidget *pbar;

    dialog->priv = g_new0 (GnomeCmdSearchDialogPrivate, 1);
    dialog->priv->data = g_new0 (SearchData, 1);

    window = GTK_WIDGET (dialog);
    gtk_object_set_data (GTK_OBJECT (window), "window", window);
    gtk_window_set_title (GTK_WINDOW (window), _("Search..."));
    gnome_cmd_dialog_set_resizable (GNOME_CMD_DIALOG (dialog), TRUE);
    gtk_window_set_default_size (GTK_WINDOW (window), defaults.width, defaults.height);

    vbox = create_vbox (window, FALSE, 0);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), vbox);

    table = create_table (window, 5, 2);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 6);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);

    // Search for
    dialog->priv->pattern_combo = create_combo_box_entry (window);
    label = create_label_with_mnemonic (window, _("Search _for: "), dialog->priv->pattern_combo);
    table_add (table, label, 0, 0, GTK_FILL);

    table_add (table, dialog->priv->pattern_combo, 1, 0, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    if (!defaults.name_patterns.empty())
        g_list_foreach (defaults.name_patterns.ents, combo_box_insert_text, dialog->priv->pattern_combo);

    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->pattern_combo), 0);
    gnome_cmd_dialog_editable_enters (GNOME_CMD_DIALOG (dialog), GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (dialog->priv->pattern_combo))));

    // Search in
    dialog->priv->dir_browser = create_file_entry (window, "dir_browser", "");
    label = create_label_with_mnemonic (window, _("Search _in: "), dialog->priv->dir_browser);
    table_add (table, label, 0, 1, GTK_FILL);

    table_add (table, dialog->priv->dir_browser, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    if (!defaults.directories.empty())
        gtk_combo_set_popdown_strings (
            GTK_COMBO (gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (dialog->priv->dir_browser))),
            defaults.directories.ents);

    dialog->priv->dir_entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (dialog->priv->dir_browser));

    hbox = create_hbox (window, FALSE, 0);


    // Recurse check
    dialog->priv->recurse_check = create_check_with_mnemonic (window, _("Search _recursively"), "recurse_check");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->recurse_check), defaults.recursive);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->priv->recurse_check, FALSE, FALSE, 0);


    // File name matching
    radio = create_radio_with_mnemonic (window, NULL, _("Rege_x syntax"), "regex_radio");
    gtk_box_pack_end (GTK_BOX (hbox), radio, FALSE, FALSE, 12);
    if (gnome_cmd_data.filter_type == Filter::TYPE_REGEX)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio_with_mnemonic (window, get_radio_group (radio), _("She_ll syntax"), "shell_radio");
    gtk_box_pack_end (GTK_BOX (hbox), radio, FALSE, FALSE, 12);
    if (gnome_cmd_data.filter_type == Filter::TYPE_FNMATCH)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    table_add (table, hbox, 1, 2, GTK_FILL);


    // Find text
    dialog->priv->find_text_check = create_check_with_mnemonic (window, _("Find _text: "), "find_text");
    table_add (table, dialog->priv->find_text_check, 0, 3, GTK_FILL);

    dialog->priv->find_text_combo = create_combo_box_entry (window);
    table_add (table, dialog->priv->find_text_combo, 1, 3, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    gtk_widget_set_sensitive (dialog->priv->find_text_combo, FALSE);
    if (!defaults.content_patterns.empty())
        g_list_foreach (defaults.content_patterns.ents, combo_box_insert_text, dialog->priv->find_text_combo);

    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->find_text_combo), 0);
    gnome_cmd_dialog_editable_enters (GNOME_CMD_DIALOG (dialog), GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (dialog->priv->find_text_combo))));

    // Case check
    dialog->priv->case_check = create_check_with_mnemonic (window, _("Case sensiti_ve"), "case_check");
    gtk_table_attach (GTK_TABLE (table), dialog->priv->case_check, 1, 2, 4, 5,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->case_check), defaults.case_sens);

    dialog->priv->help_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_HELP, GTK_SIGNAL_FUNC (on_help), dialog);
    dialog->priv->close_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_SIGNAL_FUNC (on_close), dialog);
    dialog->priv->goto_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_Go to"), GTK_SIGNAL_FUNC (on_goto), dialog);
    dialog->priv->stop_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_STOP, GTK_SIGNAL_FUNC (on_stop), dialog);
    dialog->priv->search_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_FIND, GTK_SIGNAL_FUNC (on_search), dialog);

    gtk_widget_set_sensitive (dialog->priv->stop_button, FALSE);
    gtk_widget_set_sensitive (dialog->priv->goto_button, FALSE);


    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (sw);
    gtk_object_set_data_full (GTK_OBJECT (window), "sw", sw, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (sw);
    gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    dialog->priv->result_list = new GnomeCmdFileList;
    gtk_widget_ref (GTK_WIDGET (dialog->priv->result_list));
    gtk_object_set_data_full (GTK_OBJECT (window), "result_list", GTK_WIDGET (dialog->priv->result_list), (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_set_size_request (GTK_WIDGET (dialog->priv->result_list), -1, 200);
    gtk_widget_show (GTK_WIDGET (dialog->priv->result_list));
    gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (dialog->priv->result_list));
    gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->result_list), 4);


    dialog->priv->statusbar = gtk_statusbar_new ();
    gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (dialog->priv->statusbar), FALSE);
    gtk_widget_ref (dialog->priv->statusbar);
    gtk_object_set_data_full (GTK_OBJECT (window), "statusbar", dialog->priv->statusbar,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (dialog->priv->statusbar);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->statusbar, FALSE, TRUE, 0);

    pbar = create_progress_bar (window);
    gtk_widget_hide (pbar);
    gtk_progress_set_show_text (GTK_PROGRESS (pbar), FALSE);
    gtk_progress_set_activity_mode (GTK_PROGRESS (pbar), TRUE);
    gtk_progress_configure (GTK_PROGRESS (pbar), 0, 0, PBAR_MAX);
    gtk_box_pack_start (GTK_BOX (dialog->priv->statusbar), pbar, FALSE, TRUE, 0);
    dialog->priv->pbar = pbar;


    g_signal_connect (G_OBJECT (dialog), "destroy", G_CALLBACK (on_dialog_destroy), NULL);
    g_signal_connect (G_OBJECT (dialog), "size-allocate", G_CALLBACK (on_dialog_size_allocate), NULL);
    g_signal_connect (G_OBJECT (dialog->priv->result_list), "key-press-event", G_CALLBACK (on_list_keypressed), dialog);

    gtk_signal_connect (GTK_OBJECT (dialog->priv->find_text_check), "toggled", GTK_SIGNAL_FUNC (find_text_toggled), dialog);

    gtk_window_set_keep_above (GTK_WINDOW (dialog), FALSE);

    gtk_widget_grab_focus (dialog->priv->pattern_combo);
    dialog->priv->result_list->update_style();
}


/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_search_dialog_get_type ()
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


GtkWidget *gnome_cmd_search_dialog_new (GnomeCmdDir *default_dir)
{
    gchar *new_path;
    GnomeCmdSearchDialog *dialog = (GnomeCmdSearchDialog *) gtk_type_new (gnome_cmd_search_dialog_get_type ());

    gchar *path = gnome_cmd_file_get_path (GNOME_CMD_FILE (default_dir));
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
