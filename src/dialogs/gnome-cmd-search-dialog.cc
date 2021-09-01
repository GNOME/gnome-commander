/**
 * @file gnome-cmd-search-dialog.cc
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
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-selection-profile-component.h"
#include "gnome-cmd-manage-profiles-dialog.h"
#include "filter.h"
#include "utils.h"

using namespace std;


#if 0
static char *msgs[] = {N_("Search local directories only"),
                       N_("Files _not containing text")};
#endif


#define PBAR_MAX   50 // Absolute width of a progress bar

#define SEARCH_JUMP_SIZE     4096U
#define SEARCH_BUFFER_SIZE  (SEARCH_JUMP_SIZE * 10U)

#define GNOME_SEARCH_TOOL_REFRESH_DURATION  50000


struct GnomeCmdSearchDialogClass
{
    GtkDialogClass parent_class;
};


struct SearchFileData
{
    GFileInputStream *inputStream;
    gchar          *uriString;
    gint            offset;
    guint           len;
    gchar           mem[SEARCH_BUFFER_SIZE];     // memory to search in the content of a file
};


struct SearchData
{
    struct ProtectedData
    {
        GList  *files;
        gchar  *msg;
        GMutex  mutex;

        ProtectedData() : files(nullptr), msg(nullptr) {}
    };

    GnomeCmdSearchDialog *dialog;

    // the directory to start searching from
    GnomeCmdDir *start_dir = nullptr;

    Filter *name_filter = nullptr;
    regex_t *content_regex = nullptr;

    // the context id of the status bar
    gint context_id = 0;

    //the directories which we found matching files in
    GList *match_dirs = nullptr;
    GThread *thread = nullptr;
    ProtectedData pdata;
    gint update_gui_timeout_id {0};

    gboolean search_done = TRUE;

    //stops the search routine if set to TRUE. This is done by the stop_button
    gboolean stopped = TRUE;

    // set when the search dialog is destroyed, also stops the search of course
    gboolean dialog_destroyed = FALSE;

    explicit SearchData(GnomeCmdSearchDialog *dlg);

    void set_statusmsg(const gchar *msg = nullptr);

    // searches a given directory for files that matches the criteria given by data
    void SearchDirRecursive(GnomeCmdDir *dir, long level);

    // determines if the name of a file matches an regexp
    gboolean NameMatches(const char *name)  {  return name_filter->match(name);  }

    // determines if the content of a file matches an regexp
    gboolean ContentMatches(GnomeCmdFile *f);

    // loads a file in chunks and returns the content
    gboolean ReadSearchFile(SearchFileData *, GnomeCmdFile *f);
    gboolean StartSearch();

    static gboolean join_thread_func(SearchData *data);
};


struct GnomeCmdSearchDialog::Private
{
    // holds data needed by the search routines
    SearchData data;

    GtkWidget *vbox;
    GnomeCmdSelectionProfileComponent *profile_component;
    GtkWidget *dir_browser;
    GnomeCmdFileList *result_list;
    GtkWidget *statusbar;
    GtkWidget *pbar;
    GtkWidget *profile_menu_button;

    explicit Private(GnomeCmdSearchDialog *dlg);
    ~Private();

    static gchar *translate_menu(const gchar *path, gpointer data);

    GtkWidget *create_placeholder_menu(GnomeCmdData::SearchConfig &cfg);
    GtkWidget *create_button_with_menu(gchar *label_text, GnomeCmdData::SearchConfig &cfg);

    static void manage_profiles(GnomeCmdSearchDialog::Private *priv, guint unused, GtkWidget *menu);
    static void load_profile(GnomeCmdSearchDialog::Private *priv, guint profile_idx, GtkWidget *menu);

    static gboolean on_list_keypressed (GtkWidget *result_list, GdkEventKey *event, gpointer unused);

    static void on_dialog_show (GtkWidget *widget, GnomeCmdSearchDialog *dialog);
    static void on_dialog_hide (GtkWidget *widget, GnomeCmdSearchDialog *dialog);
    static gboolean on_dialog_delete (GtkWidget *widget, GdkEvent *event, GnomeCmdSearchDialog *dialog);
    static void on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdSearchDialog *dialog);
    static void on_dialog_response (GtkDialog *window, int response_id, GnomeCmdSearchDialog *dialog);
};


inline GnomeCmdSearchDialog::Private::Private(GnomeCmdSearchDialog *dlg): data(dlg)
{
    vbox = nullptr;
    profile_component = nullptr;
    dir_browser = nullptr;
    result_list = nullptr;
    statusbar = nullptr;
    pbar = nullptr;
    profile_menu_button = nullptr;
}


inline GnomeCmdSearchDialog::Private::~Private()
{
}


gchar *GnomeCmdSearchDialog::Private::translate_menu(const gchar *path, gpointer data)
{
    return _(path);
}


inline GtkWidget *GnomeCmdSearchDialog::Private::create_placeholder_menu(GnomeCmdData::SearchConfig &cfg)
{
    guint items_size = cfg.profiles.empty() ? 1 : cfg.profiles.size()+3;
    GtkItemFactoryEntry *items = g_try_new0 (GtkItemFactoryEntry, items_size);

    g_return_val_if_fail (items!=nullptr, nullptr);

    GtkItemFactoryEntry *i = items;

    i->path = g_strdup (_("/_Save Profile As…"));
    i->callback = (GtkItemFactoryCallback) manage_profiles;
    i->callback_action = TRUE;
    i->item_type = (gchar*) "<StockItem>";
    i->extra_data = GTK_STOCK_SAVE_AS;
    ++i;

    if (!cfg.profiles.empty())
    {
        i->path = g_strdup (_("/_Manage Profiles…"));
        i->callback = (GtkItemFactoryCallback) manage_profiles;
        i->item_type = (gchar*) "<StockItem>";
        i->extra_data = GTK_STOCK_EDIT;
        ++i;

        i->path = g_strdup ("/");
        i->item_type = (gchar*) "<Separator>";
        ++i;

        for (vector<GnomeCmdData::SearchProfile>::const_iterator p=cfg.profiles.begin(); p!=cfg.profiles.end(); ++p, ++i)
        {
            i->path = g_strconcat ("/", p->name.c_str(), nullptr);
            i->callback = (GtkItemFactoryCallback) load_profile;
            i->callback_action = (i-items)-3;
            i->item_type = (gchar*) "<StockItem>";
            i->extra_data = GTK_STOCK_REVERT_TO_SAVED;
        }
    }

    GtkItemFactory *ifac = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", nullptr);

    gtk_item_factory_create_items (ifac, items_size, items, this);

    for (guint ii=0; ii<items_size; ++ii)
        g_free (items[ii].path);

    g_free (items);

    return gtk_item_factory_get_widget (ifac, "<main>");
}


inline GtkWidget *GnomeCmdSearchDialog::Private::create_button_with_menu(gchar *label_text, GnomeCmdData::SearchConfig &cfg)
{
    profile_menu_button = gnome_cmd_button_menu_new (label_text, create_placeholder_menu(cfg));

    return profile_menu_button;
}


void GnomeCmdSearchDialog::Private::manage_profiles(GnomeCmdSearchDialog::Private *priv, guint new_profile, GtkWidget *widget)
{
    auto *dialog = reinterpret_cast<GnomeCmdSearchDialog*> (gtk_widget_get_ancestor (priv->profile_menu_button, GNOME_CMD_TYPE_SEARCH_DIALOG));

    g_return_if_fail (dialog != nullptr);

    if (new_profile)
        priv->profile_component->copy();

    if (GnomeCmd::ManageProfilesDialog<GnomeCmdData::SearchConfig,GnomeCmdData::SearchProfile,GnomeCmdSelectionProfileComponent> (*dialog,dialog->defaults,new_profile,_("Profiles"),"gnome-commander-search"))
    {
        GtkWidget *menu = widget->parent;

        gnome_cmd_button_menu_disconnect_handler (priv->profile_menu_button, menu);
        g_object_unref (gtk_item_factory_from_widget (menu));
        gnome_cmd_button_menu_connect_handler (priv->profile_menu_button, priv->create_placeholder_menu(dialog->defaults));
    }
}


void GnomeCmdSearchDialog::Private::load_profile(GnomeCmdSearchDialog::Private *priv, guint profile_idx, GtkWidget *widget)
{
    GtkWidget *dialog = gtk_widget_get_ancestor (priv->profile_menu_button, GNOME_CMD_TYPE_SEARCH_DIALOG);

    g_return_if_fail (dialog!=nullptr);

    GnomeCmdData::SearchConfig &cfg = GNOME_CMD_SEARCH_DIALOG(dialog)->defaults;

    g_return_if_fail (profile_idx<cfg.profiles.size());

    cfg.default_profile = cfg.profiles[profile_idx];
    priv->profile_component->update();
}


inline SearchData::SearchData(GnomeCmdSearchDialog *dlg): dialog(dlg)
{
}


G_DEFINE_TYPE (GnomeCmdSearchDialog, gnome_cmd_search_dialog, GTK_TYPE_DIALOG)


inline void free_search_file_data (SearchFileData *searchfileData)
{
    if (searchfileData->inputStream)
        g_object_unref(searchfileData->inputStream);

    g_free (searchfileData->uriString);
    g_free (searchfileData);
}


gboolean SearchData::ReadSearchFile(SearchFileData *searchFileData, GnomeCmdFile *f)
{
    if (stopped)     // if the stop button was pressed, let's abort here
    {
        return FALSE;
    }

    auto size = get_gfile_attribute_uint64(f->gFile, G_FILE_ATTRIBUTE_STANDARD_SIZE);

    if (searchFileData->len)
    {
        if ((searchFileData->offset + searchFileData->len) >= size)   // end, all has been read
        {
            return FALSE;
        }

      // jump a big step backward to give the regex a chance
      searchFileData->offset += searchFileData->len - SEARCH_JUMP_SIZE;
      if (size < (searchFileData->offset + (SEARCH_BUFFER_SIZE - 1)))
          searchFileData->len = size - searchFileData->offset;
      else
          searchFileData->len = SEARCH_BUFFER_SIZE - 1;
    }
    else   // first time call of this function
        searchFileData->len = MIN (size, SEARCH_BUFFER_SIZE - 1);

    if (!g_seekable_can_seek ((GSeekable *) searchFileData->inputStream))
    {
        g_warning (_("Failed to read file %s: File is not searchable"), searchFileData->uriString);
        return FALSE;
    }

    GError *error = nullptr;

    if (!g_seekable_seek ((GSeekable *) searchFileData->inputStream, searchFileData->offset, G_SEEK_SET, nullptr, &error)
        || error)
    {
        g_warning (_("Failed to read file %s: %s"), searchFileData->uriString, error->message);
        g_error_free(error);
        return FALSE;
    }

    g_input_stream_read ((GInputStream*) searchFileData->inputStream,
                         searchFileData->mem,
                         searchFileData->len,
                         nullptr,
                         &error);
    if (error)
    {
        g_warning (_("Failed to read file %s: %s"), searchFileData->uriString, error->message);
        g_error_free(error);
        return FALSE;
    }

    searchFileData->mem[searchFileData->len < SEARCH_BUFFER_SIZE
                        ? searchFileData->len
                        : SEARCH_BUFFER_SIZE] = '\0';

    return TRUE;
}


inline gboolean SearchData::ContentMatches(GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, FALSE);

    if (get_gfile_attribute_uint64(f->gFile, G_FILE_ATTRIBUTE_STANDARD_SIZE) == 0)
        return FALSE;

    GError *error = nullptr;

    auto searchFileData = g_new0(SearchFileData, 1);
    searchFileData->uriString = f->get_uri_str();
    searchFileData->inputStream = g_file_read (f->gFile, nullptr, &error);

    if (error)
    {
        g_warning (_("Failed to read file %s: %s"), searchFileData->uriString, error->message);
        free_search_file_data (searchFileData);
        g_error_free(error);
        return FALSE;
    }

    regmatch_t match;
    gboolean retValue = FALSE;

    while (ReadSearchFile(searchFileData, f))
        if (regexec (content_regex, searchFileData->mem, 1, &match, 0) != REG_NOMATCH)
        {
            retValue = TRUE;        // stop on first match
        }

    // Todo: Fix this later
    //free_search_file_data (searchFileData);
    return retValue;
}


inline gboolean handle_list_keypress (GnomeCmdFileList *fl, GdkEventKey *event)
{
    switch (event->keyval)
    {
        case GDK_F3:
            gnome_cmd_file_list_view (fl, -1);
            return TRUE;

        case GDK_F4:
            gnome_cmd_file_list_edit (fl);
            return TRUE;
        default:
            return FALSE;
    }
}


inline void SearchData::set_statusmsg(const gchar *msg)
{
    gtk_statusbar_push (GTK_STATUSBAR (dialog->priv->statusbar), context_id, msg ? msg : "");
}


void SearchData::SearchDirRecursive(GnomeCmdDir *dir, long level)
{
    if (!dir)
        return;

    if (stopped)     // if the stop button was pressed, let's abort here
        return;

    // update the search status data
    if (!dialog_destroyed)
    {
        g_mutex_lock (&pdata.mutex);

        g_free (pdata.msg);
        pdata.msg = g_strdup_printf (_("Searching in: %s"), gnome_cmd_dir_get_display_path (dir));

        g_mutex_unlock (&pdata.mutex);
    }

    gnome_cmd_dir_list_files (dir, FALSE);

    // let's iterate through all files
    for (GList *i=gnome_cmd_dir_get_files (dir); i; i=i->next)
    {
        if (stopped)         // if the stop button was pressed, let's abort here
            return;

        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        // if the current file is a directory, let's continue our recursion
        if (GNOME_CMD_IS_DIR (f) && level!=0)
        {
            // we don't want to go backwards or to follow symlinks
            if (!f->is_dotdot
                && strcmp (g_file_info_get_display_name(f->gFileInfo), ".") != 0
                && !get_gfile_attribute_boolean(f->gFile, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK))
            {
                GnomeCmdDir *new_dir = GNOME_CMD_DIR (f);

                if (new_dir)
                {
                    gnome_cmd_dir_ref (new_dir);
                    SearchDirRecursive(new_dir, level-1);
                    gnome_cmd_dir_unref (new_dir);
                }
            }
        }

        // if the file is a regular one, it might match the search criteria
        if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_REGULAR)
        {
            // if the name doesn't match, let's go to the next file
            if (!NameMatches(g_file_info_get_display_name(f->gFileInfo)))
                continue;

            // if the user wants to we should do some content matching here
            if (dialog->defaults.default_profile.content_search && !ContentMatches(f))
                continue;

            // the file matched the search criteria, let's add it to the list
            g_mutex_lock (&pdata.mutex);
            pdata.files = g_list_append (pdata.files, f->ref());
            g_mutex_unlock (&pdata.mutex);

            // also ref each directory that has a matching file
            if (g_list_index (match_dirs, dir) == -1)
                match_dirs = g_list_append (match_dirs, gnome_cmd_dir_ref (dir));
        }
    }
}


static gpointer perform_search_operation (SearchData *data)
{
    // unref all directories which contained matching files from last search
    if (data->match_dirs)
    {
        g_list_foreach (data->match_dirs, (GFunc) gnome_cmd_dir_unref, nullptr);
        g_list_free (data->match_dirs);
        data->match_dirs = nullptr;
    }

    data->SearchDirRecursive(data->start_dir, data->dialog->defaults.default_profile.max_depth);

    // free regexps
    delete data->name_filter;
    data->name_filter = nullptr;

    if (data->dialog->defaults.default_profile.content_search)
    {
        regfree (data->content_regex);
        g_free (data->content_regex);
        data->content_regex = nullptr;
    }

    gnome_cmd_dir_unref (data->start_dir);      //  FIXME:  ???
    data->start_dir = nullptr;

    data->search_done = TRUE;

    return nullptr;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static gboolean update_search_status_widgets (SearchData *data)
{
    // update the progress bar
    progress_bar_update (data->dialog->priv->pbar, PBAR_MAX);

    g_mutex_lock (&data->pdata.mutex);

    GList *files = data->pdata.files;
    data->pdata.files = nullptr;

    // update status bar with the latest message
    data->set_statusmsg(data->pdata.msg);

    g_mutex_unlock (&data->pdata.mutex);

    GnomeCmdFileList *fl = data->dialog->priv->result_list;

    // add all files found since last update to the list
    for (GList *i = files; i; i = i->next)
        fl->append_file(GNOME_CMD_FILE (i->data));

    gnome_cmd_file_list_free (files);

    if ((!data->search_done && !data->stopped) || data->pdata.files)
        return TRUE;

    if (!data->dialog_destroyed)
    {
        int matches = data->dialog->priv->result_list->size();

        gchar *fmt = data->stopped ? ngettext("Found %d match — search aborted", "Found %d matches — search aborted", matches) :
                                     ngettext("Found %d match", "Found %d matches", matches);

        gchar *msg = g_strdup_printf (fmt, matches);

        data->set_statusmsg(msg);
        g_free (msg);

        gtk_widget_hide (data->dialog->priv->pbar);

        gtk_dialog_set_response_sensitive (*data->dialog, GnomeCmdSearchDialog::GCMD_RESPONSE_GOTO, matches>0);
        gtk_dialog_set_response_sensitive (*data->dialog, GnomeCmdSearchDialog::GCMD_RESPONSE_STOP, FALSE);
        gtk_dialog_set_response_sensitive (*data->dialog, GnomeCmdSearchDialog::GCMD_RESPONSE_FIND, TRUE);
        gtk_dialog_set_default_response (*data->dialog, GnomeCmdSearchDialog::GCMD_RESPONSE_FIND);

        if (matches)
        {
            GnomeCmdFileList *flmatches = data->dialog->priv->result_list;
            gtk_widget_grab_focus (*flmatches);         // set focus to result list
            // select one file, as matches is non-zero, there should be at least one entry
            if (!flmatches->get_focused_file())
            {
                flmatches->select_row(0);
            }
        }
    }
    return FALSE;    // returning FALSE here stops the timeout callbacks
}

#pragma GCC diagnostic pop

/**
 * This function gets called then the search-dialog is about the be destroyed.
 * The function waits for the last search-thread to finish and then frees the
 * data structure that has been shared between the search threads and the
 * main thread.
 */
gboolean SearchData::join_thread_func (SearchData *data)
{
    if (data->thread)
        g_thread_join (data->thread);

    g_mutex_clear (&data->pdata.mutex);

    return FALSE;
}


gboolean SearchData::StartSearch()
{
    // create an regex for file name matching
    name_filter = new Filter(dialog->defaults.default_profile.filename_pattern.c_str(), dialog->defaults.default_profile.match_case, dialog->defaults.default_profile.syntax);

    // if we're going to search through file content create an regex for that too
    if (dialog->defaults.default_profile.content_search)
    {
        content_regex = g_new0 (regex_t, 1);
        regcomp (content_regex, dialog->defaults.default_profile.text_pattern.c_str(), dialog->defaults.default_profile.match_case ? 0 : REG_ICASE);
    }

    g_mutex_init(&pdata.mutex);

    thread = g_thread_new (nullptr, (GThreadFunc) perform_search_operation, this);

    return TRUE;
}


gboolean GnomeCmdSearchDialog::Private::on_list_keypressed(GtkWidget *result_list, GdkEventKey *event, gpointer unused)
{
    if (GNOME_CMD_FILE_LIST (result_list)->key_pressed(event) ||
        handle_list_keypress (GNOME_CMD_FILE_LIST (result_list), event))
    {
        g_signal_stop_emission_by_name (result_list, "key-press-event");
        return TRUE;
    }

    return FALSE;
}


void GnomeCmdSearchDialog::Private::on_dialog_show(GtkWidget *widget, GnomeCmdSearchDialog *dialog)
{
    dialog->priv->profile_component->update();

    dialog->priv->data.start_dir = main_win->fs(ACTIVE)->get_directory();

    gchar *uri = gnome_cmd_dir_get_uri_str (dialog->priv->data.start_dir);
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog->priv->dir_browser), uri);
    g_free (uri);
}


void GnomeCmdSearchDialog::Private::on_dialog_hide(GtkWidget *widget, GnomeCmdSearchDialog *dialog)
{
    dialog->priv->profile_component->copy();

    dialog->priv->result_list->remove_all_files();
}


gboolean GnomeCmdSearchDialog::Private::on_dialog_delete(GtkWidget *widget, GdkEvent *event, GnomeCmdSearchDialog *dialog)
{
    return event->type==GDK_DELETE;
}


void GnomeCmdSearchDialog::Private::on_dialog_size_allocate(GtkWidget *widget, GtkAllocation *allocation, GnomeCmdSearchDialog *dialog)
{
    dialog->defaults.width  = allocation->width;
    dialog->defaults.height = allocation->height;
}


void GnomeCmdSearchDialog::Private::on_dialog_response(GtkDialog *window, int response_id, GnomeCmdSearchDialog *dialog)
{
    switch (response_id)
    {
        case GCMD_RESPONSE_STOP:
            {
                dialog->priv->data.stopped = TRUE;
                gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_STOP, FALSE);
                gtk_dialog_set_default_response (*dialog, GCMD_RESPONSE_FIND);
            }
            break;

        case GCMD_RESPONSE_FIND:
            {
                SearchData &data = dialog->priv->data;

                if (data.thread)
                {
                    g_thread_join (data.thread);
                    data.thread = nullptr;
                }

                data.search_done = TRUE;
                data.stopped = TRUE;
                data.dialog_destroyed = FALSE;

                data.context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (dialog->priv->statusbar), "info");
                data.content_regex = nullptr;
                data.match_dirs = nullptr;

                auto uriString = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog->priv->dir_browser));
                auto dirGFile = g_file_new_for_uri (uriString);
                g_free (uriString);

                auto dirPathString = g_file_get_path (dirGFile);
                gchar *dir_path = g_strconcat (dirPathString, G_DIR_SEPARATOR_S, nullptr);
                g_free (dirPathString);

                GnomeCmdCon *con = gnome_cmd_dir_get_connection (data.start_dir);

                if (strncmp(dir_path, gnome_cmd_con_get_root_path (con), con->root_path->len) != 0)
                {
                    //if (!gnome_vfs_uri_is_local (dirGFile))
                    //{
                    //    gnome_cmd_show_message (*dialog, stringify(g_strdup_printf (_("Failed to change directory outside of %s"),
                    //                                                                          gnome_cmd_con_get_root_path (con))));
                    //    gnome_vfs_uri_unref (dirGFile);
                    //    g_free (dir_path);
                    //
                    //    break;
                    //}
                    //else
                        data.start_dir = gnome_cmd_dir_new (get_home_con (), gnome_cmd_con_create_path (get_home_con (), dir_path));
                }
                else
                    data.start_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dir_path + con->root_path->len));

                gnome_cmd_dir_ref (data.start_dir);

                g_object_unref (dirGFile);
                g_free (dir_path);

                // save default settings
                dialog->priv->profile_component->copy();
                if (!dialog->defaults.default_profile.filename_pattern.empty())
                {
                    gnome_cmd_data.search_defaults.name_patterns.add(dialog->defaults.default_profile.filename_pattern.c_str());
                    dialog->priv->profile_component->set_name_patterns_history(dialog->defaults.name_patterns.ents);
                }

                if (dialog->defaults.default_profile.content_search && !dialog->defaults.default_profile.text_pattern.empty())
                {
                    gnome_cmd_data.search_defaults.content_patterns.add(dialog->defaults.default_profile.text_pattern.c_str());
                    gnome_cmd_data.intviewer_defaults.text_patterns.add(dialog->defaults.default_profile.text_pattern.c_str());
                    dialog->priv->profile_component->set_content_patterns_history(dialog->defaults.content_patterns.ents);
                }

                data.search_done = FALSE;
                data.stopped = FALSE;

                dialog->priv->result_list->remove_all_files();

                gchar *base_dir_utf8 = GNOME_CMD_FILE (data.start_dir)->get_real_path();
                dialog->priv->result_list->set_base_dir(base_dir_utf8);

                if (data.StartSearch())
                {
                    data.set_statusmsg();
                    gtk_widget_show (dialog->priv->pbar);
                    data.update_gui_timeout_id = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_search_status_widgets, &data);

                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_GOTO, FALSE);
                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_STOP, TRUE);
                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_FIND, FALSE);
                    gtk_dialog_set_default_response (*dialog, GCMD_RESPONSE_STOP);
                }
            }
            break;

        case GCMD_RESPONSE_GOTO:
            {
                GnomeCmdFile *f = dialog->priv->result_list->get_selected_file();

                if (!f)
                    break;

                GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);
                GnomeCmdCon *con = fs->get_connection();

                gchar *fpath = f->get_path();
                gsize offset = strncmp(fpath, gnome_cmd_con_get_root_path (con), con->root_path->len)==0 ? con->root_path->len : 0;
                gchar *dpath = g_path_get_dirname (fpath + offset);

                if (fs->file_list()->locked)
                {
                    GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dpath));
                    fs->new_tab(dir);
                }
                else
                    fs->file_list()->goto_directory(dpath);

                fs->file_list()->focus_file(f->get_name(), TRUE);

                g_free (fpath);
                g_free (dpath);
            }

#if defined (__GNUC__) && __GNUC__ >= 7
        __attribute__ ((fallthrough));
#endif
        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_CLOSE:
            gtk_widget_hide (*dialog);
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-search");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GCMD_RESPONSE_PROFILES:
            break;

        default :
            g_assert_not_reached ();
    }
}


static void gnome_cmd_search_dialog_init (GnomeCmdSearchDialog *dialog)
{
    dialog->priv = new GnomeCmdSearchDialog::Private(dialog);

    gtk_window_set_title (*dialog, _("Search…"));
    gtk_window_set_resizable (*dialog, TRUE);
    gtk_dialog_set_has_separator (*dialog, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

    dialog->priv->vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->vbox), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), dialog->priv->vbox, TRUE, TRUE, 0);


    // file list
    GtkWidget *sw = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_box_pack_start (GTK_BOX (dialog->priv->vbox), sw, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    dialog->priv->result_list = new GnomeCmdFileList(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING);
    gtk_widget_set_size_request (*dialog->priv->result_list, -1, 200);
    gtk_container_add (GTK_CONTAINER (sw), *dialog->priv->result_list);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->result_list), 4);


    // status
    dialog->priv->statusbar = gtk_statusbar_new ();
    gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (dialog->priv->statusbar), FALSE);
    gtk_box_pack_start (GTK_BOX (dialog->priv->vbox), dialog->priv->statusbar, FALSE, TRUE, 0);


    // progress
    dialog->priv->pbar = create_progress_bar (*dialog);
    gtk_progress_set_show_text (GTK_PROGRESS (dialog->priv->pbar), FALSE);
    gtk_progress_set_activity_mode (GTK_PROGRESS (dialog->priv->pbar), TRUE);
    gtk_progress_configure (GTK_PROGRESS (dialog->priv->pbar), 0, 0, PBAR_MAX);
    gtk_box_pack_start (GTK_BOX (dialog->priv->statusbar),dialog->priv-> pbar, FALSE, TRUE, 0);


    dialog->priv->result_list->update_style();

    gtk_widget_show (dialog->priv->vbox);
    gtk_widget_show_all (sw);
    gtk_widget_show (dialog->priv->statusbar);
    gtk_widget_hide (dialog->priv->pbar);
}


static void gnome_cmd_search_dialog_finalize (GObject *object)
{
    GnomeCmdSearchDialog *dialog = GNOME_CMD_SEARCH_DIALOG (object);
    SearchData &data = dialog->priv->data;

    if (!data.search_done)
        g_source_remove (data.update_gui_timeout_id);

    // stop and wait for search thread to exit
    data.stopped = TRUE;
    data.dialog_destroyed = TRUE;
    g_timeout_add (1, (GSourceFunc) SearchData::join_thread_func, &data);

    // unref all directories which contained matching files from last search
    g_mutex_lock (&data.pdata.mutex);
    if (data.match_dirs)
    {
        g_list_foreach (data.match_dirs, (GFunc) gnome_cmd_dir_unref, nullptr);
        g_list_free (data.match_dirs);
        data.match_dirs = nullptr;
    }
    g_mutex_unlock (&data.pdata.mutex);

    delete dialog->priv;

    G_OBJECT_CLASS (gnome_cmd_search_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_search_dialog_class_init (GnomeCmdSearchDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_search_dialog_finalize;
}


void GnomeCmdSearchDialog::show_and_set_focus()
{
    gtk_widget_show(*this);
    priv->profile_component->set_focus();
}

GnomeCmdSearchDialog::GnomeCmdSearchDialog(GnomeCmdData::SearchConfig &cfg): defaults(cfg)
{
    gtk_window_set_default_size (*this, defaults.width, defaults.height);
    if (gnome_cmd_data.options.search_window_is_transient)
    {
        gtk_window_set_transient_for (*this, *main_win);
    }
    else
    {
        gtk_window_set_type_hint (*this, GDK_WINDOW_TYPE_HINT_NORMAL);
    }

    GtkWidget *button = priv->create_button_with_menu(_("Profiles…"), cfg);

    gtk_dialog_add_action_widget (*this, button, GCMD_RESPONSE_PROFILES);

    gtk_dialog_add_buttons (*this,
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                            GTK_STOCK_JUMP_TO, GCMD_RESPONSE_GOTO,
                            GTK_STOCK_STOP, GCMD_RESPONSE_STOP,
                            GTK_STOCK_FIND, GCMD_RESPONSE_FIND,
                            nullptr);

    gtk_dialog_set_response_sensitive (*this, GCMD_RESPONSE_GOTO, FALSE);
    gtk_dialog_set_response_sensitive (*this, GCMD_RESPONSE_STOP, FALSE);

    gtk_dialog_set_default_response (*this, GCMD_RESPONSE_FIND);

    // search in
    priv->dir_browser =  gtk_file_chooser_button_new (_("Select Directory"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (priv->dir_browser), FALSE);

    priv->profile_component = new GnomeCmdSelectionProfileComponent(cfg.default_profile, priv->dir_browser, _("_Look in folder:"));

    gtk_box_pack_start (GTK_BOX (priv->vbox), *priv->profile_component, FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (priv->vbox), *priv->profile_component, 0);

    if (!defaults.name_patterns.empty())
        priv->profile_component->set_name_patterns_history(defaults.name_patterns.ents);

    priv->profile_component->set_content_patterns_history(defaults.content_patterns.ents);

    priv->profile_component->set_default_activation(*this);

    gtk_widget_show_all (button);
    gtk_widget_show (*priv->profile_component);

    g_signal_connect (priv->result_list, "key-press-event", G_CALLBACK (Private::on_list_keypressed), this);

    g_signal_connect (this, "show", G_CALLBACK (Private::on_dialog_show), this);
    g_signal_connect (this, "hide", G_CALLBACK (Private::on_dialog_hide), this);
    g_signal_connect (this, "delete-event", G_CALLBACK (Private::on_dialog_delete), this);
    g_signal_connect (this, "size-allocate", G_CALLBACK (Private::on_dialog_size_allocate), this);
    g_signal_connect (this, "response", G_CALLBACK (Private::on_dialog_response), this);
}


GnomeCmdSearchDialog::~GnomeCmdSearchDialog()
{
}
