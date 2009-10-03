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
#include <locale.h>
#include <errno.h>
#include <dirent.h>
#include <fnmatch.h>

#include <set>

#include "gnome-cmd-includes.h"
#include "utils.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-plain-path.h"
#include "imageloader.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-xfer.h"

using namespace std;


typedef struct
{
    GnomeCmdFile *f;
    GtkWidget *dialog;
    gpointer *args;
} TmpDlData;

#ifdef HAVE_LOCALE_H
extern struct lconv *locale_information;
#endif

#define FIX_PW_HACK
#define STRINGS_TO_URIS_CHUNK 1024

static GdkCursor *cursor_busy = NULL;
extern gchar *debug_flags;
static gchar *tmp_file_dir = NULL;


gboolean DEBUG_ENABLED (gchar flag)
{
    if (debug_flags != NULL)
       return strchr(debug_flags, flag) != 0;

    return FALSE;
}

/**
 * The already reserved debug flags:
 * --------------------------------
 * a: user actions debugging
 * c: file and directory counting
 * d: directory ref-counting
 * f: file ref-counting
 * g: run_command debugging
 * i: imageloader
 * k: directory pool
 * l: directory listings
 * m: connection debugging
 * n: directory monitoring
 * p: python plugins
 * s: smb network browser
 * t: metadata tags
 * v: internal viewer
 * w: widget_lookup
 * y: brief mime-based imageload
 * z: detailed mime-based imageload
 * x: xfer
 *
 */
void DEBUG (gchar flag, const gchar *format, ...)
{
    if (DEBUG_ENABLED (flag))
    {
        va_list ap;
        va_start(ap, format);
        fprintf (stderr, "[%c%c] ", flag-32, flag-32);
        vfprintf(stderr, format, ap);
        va_end(ap);
    }
}


void warn_print (const gchar *fmt, ...)
{
    va_list     argptr;
    char        string[1024];

    va_start (argptr,fmt);
    vsnprintf (string,sizeof(string),fmt,argptr);
    va_end (argptr);
    g_printerr("WARNING: %s", string);
}


void run_command_indir (const gchar *in_command, const gchar *dir, gboolean term)
{
    gchar *command;

    if (term)
    {
        gchar *arg;

        if (gnome_cmd_data.use_gcmd_block)
        {
            gchar *s = g_strdup_printf ("%s; %s/bin/gcmd-block", in_command, PREFIX);
            arg = g_shell_quote (s);
            g_free (s);
        }
        else
            arg = g_shell_quote (in_command);

        command = g_strdup_printf (gnome_cmd_data.get_term(), arg);

        g_free (arg);
    }
    else
        command = g_strdup (in_command);

    DEBUG ('g', "running%s: %s\n", (term?" in terminal":""), command);

    gnome_execute_shell (dir, command);
    g_free (command);
}


const char **convert_varargs_to_name_array (va_list args)
{
    const char *name;

    GPtrArray *resizeable_array = g_ptr_array_new ();

    do
    {
        name = va_arg (args, const char *);
        g_ptr_array_add (resizeable_array, (gpointer) name);
    }
    while (name);

    const char **plain_ole_array = (const char **) resizeable_array->pdata;

    g_ptr_array_free (resizeable_array, FALSE);

    return plain_ole_array;
}


static gboolean delete_event_callback (gpointer data, gpointer user_data)
{
    g_return_val_if_fail (GTK_IS_DIALOG (data), FALSE);

    gtk_signal_emit_stop_by_name (GTK_OBJECT (data), "delete-event");

    return TRUE;
}


static gboolean on_run_dialog_keypress (GtkWidget *dialog, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_Escape)
    {
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_NONE);
        return TRUE;
    }

    return FALSE;
}


gint run_simple_dialog (GtkWidget *parent, gboolean ignore_close_box,
                        GtkMessageType msg_type,
                        const char *text, const char *title, gint def_response, ...)
{
    va_list button_title_args;
    const char **button_titles;
    GtkWidget *dialog;
    // GtkWidget *top_widget;
    int result;

    // Create the dialog.
    va_start (button_title_args, def_response);
    button_titles = convert_varargs_to_name_array (button_title_args);
    va_end (button_title_args);

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_win), GTK_DIALOG_MODAL, msg_type, GTK_BUTTONS_NONE, text);
    if (title)
        gtk_window_set_title (GTK_WINDOW (dialog), title);

    for (int i=0; button_titles[i]; i++)
        gtk_dialog_add_button (GTK_DIALOG (dialog), button_titles[i], i);

    g_free (button_titles);

    if (def_response>=0)
        gtk_dialog_set_default_response(GTK_DIALOG (dialog), def_response);

    // Allow close.
    if (ignore_close_box)
        gtk_signal_connect (GTK_OBJECT (dialog), "delete-event", GTK_SIGNAL_FUNC (delete_event_callback), NULL);
    else
        gtk_signal_connect (GTK_OBJECT (dialog), "key-press-event", GTK_SIGNAL_FUNC (on_run_dialog_keypress), dialog);

    gtk_window_set_wmclass (GTK_WINDOW (dialog), "dialog", "Eel");

    // Run it.
    do
    {
        gtk_widget_show (dialog);
        result = gtk_dialog_run (GTK_DIALOG (dialog));
    }
    while (ignore_close_box && result == GTK_RESPONSE_DELETE_EVENT);

    gtk_widget_destroy(dialog);

    return result;
}


gchar *str_uri_basename (const gchar *uri)
{
    if (!uri)
        return NULL;

    int len = strlen (uri),
        last_slash=0;

    if (len < 2)
        return NULL;

    for (int i=0; i<len; i++)
        if (uri[i] == '/')
            last_slash = i;

    return gnome_vfs_unescape_string (&uri[last_slash+1], NULL);
}


const gchar *type2string (GnomeVFSFileType type, gchar *buf, guint max)
{
    const char *s;

    switch (type)
    {
        case GNOME_VFS_FILE_TYPE_UNKNOWN:
            s = "?";
            break;
        case GNOME_VFS_FILE_TYPE_REGULAR:
            s = " ";
            break;
        case GNOME_VFS_FILE_TYPE_DIRECTORY:
            s = G_DIR_SEPARATOR_S;
            break;
        case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
            s = "@";
            break;
        case GNOME_VFS_FILE_TYPE_FIFO:
            s = "F";
            break;
        case GNOME_VFS_FILE_TYPE_SOCKET:
            s = "S";
            break;
        case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
            s = "C";
            break;
        case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
            s = "B";
            break;

        default:
             s = "?";
    }

    g_snprintf (buf, max, "%s", s);

    return buf;
}


const gchar *name2string (gchar *filename, gchar *buf, guint max)
{
    g_snprintf (buf, max, "%s", filename);

    return buf;
}


const gchar *perm2string (GnomeVFSFilePermissions p, gchar *buf, guint max)
{
    switch (gnome_cmd_data.perm_disp_mode)
    {
        case GNOME_CMD_PERM_DISP_MODE_TEXT:
            return perm2textstring (p, buf, max);

        case GNOME_CMD_PERM_DISP_MODE_NUMBER:
            return perm2numstring (p, buf, max);

        default:
            return perm2textstring (p, buf, max);
    }
}


const gchar *perm2textstring (GnomeVFSFilePermissions p, gchar *buf, guint max)
{
    g_snprintf (buf, max, "%s%s%s%s%s%s%s%s%s",
                (p & GNOME_VFS_PERM_USER_READ) ? "r" : "-",
                (p & GNOME_VFS_PERM_USER_WRITE) ? "w" : "-",
                (p & GNOME_VFS_PERM_USER_EXEC) ? "x" : "-",
                (p & GNOME_VFS_PERM_GROUP_READ) ? "r" : "-",
                (p & GNOME_VFS_PERM_GROUP_WRITE) ? "w" : "-",
                (p & GNOME_VFS_PERM_GROUP_EXEC) ? "x" : "-",
                (p & GNOME_VFS_PERM_OTHER_READ) ? "r" : "-",
                (p & GNOME_VFS_PERM_OTHER_WRITE) ? "w" : "-",
                (p & GNOME_VFS_PERM_OTHER_EXEC) ? "x" : "-");

    return buf;
}


const gchar *perm2numstring (GnomeVFSFilePermissions p, gchar *buf, guint max)
{
    gint i = 0;

    if (p & GNOME_VFS_PERM_USER_READ) i += 400;
    if (p & GNOME_VFS_PERM_USER_WRITE) i += 200;
    if (p & GNOME_VFS_PERM_USER_EXEC) i += 100;
    if (p & GNOME_VFS_PERM_GROUP_READ) i += 40;
    if (p & GNOME_VFS_PERM_GROUP_WRITE) i += 20;
    if (p & GNOME_VFS_PERM_GROUP_EXEC) i += 10;
    if (p & GNOME_VFS_PERM_OTHER_READ) i += 4;
    if (p & GNOME_VFS_PERM_OTHER_WRITE) i += 2;
    if (p & GNOME_VFS_PERM_OTHER_EXEC) i += 1;

    g_snprintf (buf, max, "%d", i);

    return buf;
}


const gchar *size2string (GnomeVFSFileSize size, GnomeCmdSizeDispMode size_disp_mode)
{
    static gchar buf0[64];
    static gchar buf1[128];


    switch (size_disp_mode)
    {
        case GNOME_CMD_SIZE_DISP_MODE_POWERED:
            {
                static const gchar *prefixes[] = {"B","kB","MB","GB","TB","PB"};
                gdouble dsize = (gdouble) size;
                guint i;

                for (i=0; i<G_N_ELEMENTS(prefixes) && dsize>1024; i++)
                    dsize /= 1024;

                if (i)
                    g_snprintf (buf0, sizeof(buf0), "%.1f %s ", dsize, prefixes[i]);
                else
                    g_snprintf (buf0, sizeof(buf0), "%llu %s ", size, prefixes[0]);
            }
            break;

        case GNOME_CMD_SIZE_DISP_MODE_GROUPED:
            {
                gint len = g_snprintf (buf0, sizeof(buf0), "%llu ", size);

                if (len < 5)
                    return buf0;

                gchar *sep = " ";

                gchar *src  = buf0;
                gchar *dest = buf1;

                *dest++ = *src++;

                for (gint i=len; i>0; i--)
                {
                    if (i>2 && i%3 == 2)
                        dest = g_stpcpy (dest, sep);
                    *dest++ = *src++;
                }
            }

            return buf1;

        case GNOME_CMD_SIZE_DISP_MODE_LOCALE:
            g_snprintf (buf0, sizeof(buf0), "%'llu ", size);
            break;

        case GNOME_CMD_SIZE_DISP_MODE_PLAIN:
            g_snprintf (buf0, sizeof(buf0), "%llu ", size);
            break;
    }

    return buf0;
}


const gchar *time2string (time_t t, const gchar *date_format)
{
    // NOTE: date_format is passed in current locale format

    static gchar buf[64];
    struct tm lt;

    localtime_r (&t, &lt);
    strftime (buf, sizeof(buf), date_format, &lt);

    // convert formatted date from current locale to UTF8
    gchar *loc_date = g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
    if (loc_date)
        strncpy (buf, loc_date, sizeof(buf)-1);

    g_free (loc_date);

    return buf;
}


inline void no_mime_app_found_error (gchar *mime_type)
{
    gchar *msg;

    msg = g_strdup_printf (_("No default application found for the MIME type %s.\nOpen the \"File types and programs\" page in the Control Center to add one."), mime_type);
    create_error_dialog (msg);
    g_free (msg);
}


static void do_mime_exec_single (gpointer *args)
{
    g_return_if_fail (args != NULL);

    GnomeCmdApp *app = (GnomeCmdApp *) args[0];
    gchar *path = (gchar *) args[1];
    gchar *dpath = (gchar *) args[2];

    string cmd = gnome_cmd_app_get_command (app);
    cmd += ' ';
    cmd += stringify (g_shell_quote (path));

    run_command_indir (cmd.c_str(), dpath, gnome_cmd_app_get_requires_terminal (app));

    g_free (path);
    g_free (dpath);
    gnome_cmd_app_free (app);
    g_free (args);
}


static void on_tmp_download_response (GtkWidget *w, gint id, TmpDlData *dldata)
{
    if (id == GTK_RESPONSE_YES)
    {
        gchar *path_str = get_temp_download_filepath (gnome_cmd_file_get_name (dldata->f));

        if (!path_str) return;

        dldata->args[1] = (gpointer) path_str;

        GnomeVFSURI *src_uri = gnome_vfs_uri_dup (gnome_cmd_file_get_uri (dldata->f));
        GnomeCmdPath *path = gnome_cmd_plain_path_new (path_str);
        GnomeCmdCon *con = get_home_con ();
        GnomeVFSURI *dest_uri = gnome_cmd_con_create_uri (con, path);
        gtk_object_destroy (GTK_OBJECT (path));

        gnome_cmd_xfer_tmp_download (src_uri,
                                     dest_uri,
                                     GNOME_VFS_XFER_FOLLOW_LINKS,
                                     GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
                                     GTK_SIGNAL_FUNC (do_mime_exec_single),
                                     dldata->args);
    }
    else
    {
        gnome_cmd_app_free ((GnomeCmdApp *) dldata->args[0]);
        g_free (dldata->args);
    }

    g_free (dldata);
    gtk_widget_destroy (dldata->dialog);
}


void mime_exec_single (GnomeCmdFile *f)
{
    g_return_if_fail (f != NULL);
    g_return_if_fail (f->info != NULL);

    gpointer *args;
    GnomeVFSMimeApplication *vfs_app;
    GnomeCmdApp *app;

    if (!f->info->mime_type)
        return;

    // Check if the file is a binary executable that lacks the executable bit

    if (!gnome_cmd_file_is_executable (f))
    {
        if (gnome_cmd_file_has_mime_type (f, "application/x-executable") || gnome_cmd_file_has_mime_type (f, "application/x-executable-binary"))
        {
            gchar *fname = get_utf8 (f->info->name);
            gchar *msg = g_strdup_printf (_("\"%s\" seems to be a binary executable file but it lacks the executable bit. Do you want to set it and then run the file?"), fname);
            gint ret = run_simple_dialog (GTK_WIDGET (main_win), FALSE, GTK_MESSAGE_QUESTION, msg,
                                          _("Make Executable?"),
                                          -1, _("Cancel"), _("OK"), NULL);
            g_free (fname);
            g_free (msg);

            if (ret != 1)  return;  else
            {
                GnomeVFSResult result = gnome_cmd_file_chmod (f, (GnomeVFSFilePermissions) (f->info->permissions|GNOME_VFS_PERM_USER_EXEC));
                if (result != GNOME_VFS_OK)
                    return;
            }
        }
    }

    // If the file is executable but not a binary file, check if the user wants to exec it or open it

    if (gnome_cmd_file_is_executable (f))
    {
        if (gnome_cmd_file_has_mime_type (f, "application/x-executable") || gnome_cmd_file_has_mime_type (f, "application/x-executable-binary") )
        {
            gnome_cmd_file_execute (f);
            return;
        }
        else
            if (gnome_cmd_file_mime_begins_with (f, "text/"))
            {
                gchar *fname = get_utf8 (f->info->name);
                gchar *msg = g_strdup_printf (_("\"%s\" is an executable text file. Do you want to run it, or display its contents?"), fname);
                gint ret = run_simple_dialog (GTK_WIDGET (main_win), FALSE, GTK_MESSAGE_QUESTION, msg, _("Run or Display"),
                                              -1, _("Cancel"), _("Display"), _("Run"), NULL);
                g_free (fname);
                g_free (msg);

                if (ret != 1)
                {
                    if (ret == 2)
                        gnome_cmd_file_execute (f);
                    return;
                }
            }
    }

    vfs_app = gnome_vfs_mime_get_default_application (f->info->mime_type);
    if (!vfs_app)
    {
        no_mime_app_found_error (f->info->mime_type);
        return;
    }

    app = gnome_cmd_app_new_from_vfs_app (vfs_app);
    gnome_vfs_mime_application_free (vfs_app);

    args = g_new0 (gpointer, 3);

    if (gnome_cmd_file_is_local (f))
    {
        args[0] = (gpointer) app;
        args[1] = (gpointer) g_strdup (gnome_cmd_file_get_real_path (f));
        args[2] = (gpointer) g_path_get_dirname ((gchar *) args[1]);            // set exec dir for local files
        do_mime_exec_single (args);
    }
    else
    {
        if (gnome_cmd_app_get_handles_uris (app) && gnome_cmd_data.honor_expect_uris)
        {
            args[0] = (gpointer) app;
            args[1] = (gpointer) g_strdup (gnome_cmd_file_get_uri_str (f));
            // args[2] is NULL here (don't set exec dir for remote files)
            do_mime_exec_single (args);
        }
        else
        {
            gchar *msg = g_strdup_printf (_("%s does not know how to open remote file. Do you want to download the file to a temporary location and then open it?"), gnome_cmd_app_get_name (app));
            GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (main_win), GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, msg);
            TmpDlData *dldata = g_new0 (TmpDlData, 1);
            args[0] = (gpointer) app;
            // args[2] is NULL here (don't set exec dir for temporarily downloaded files)
            dldata->f = f;
            dldata->dialog = dialog;
            dldata->args = args;

            gtk_signal_connect (GTK_OBJECT (dialog), "response", GTK_SIGNAL_FUNC (on_tmp_download_response), dldata);
            gtk_widget_show (dialog);
            g_free (msg);
        }
    }
}


static void do_mime_exec_multiple (gpointer *args)
{
    GnomeCmdApp *app = (GnomeCmdApp *) args[0];
    GList *files = (GList *) args[1];

    if (files)
    {
        string cmd = gnome_cmd_app_get_command (app);

        set<string> dirs;

        for (; files; files = files->next)
        {
            cmd += ' ';
            cmd += stringify (g_shell_quote ((gchar *) files->data));

            gchar *dpath = g_path_get_dirname ((gchar *) files->data);

            if (dpath)
                dirs.insert (stringify (dpath));
        }

        if (dirs.size()==1)
            run_command_indir (cmd.c_str(), dirs.begin()->c_str(), gnome_cmd_app_get_requires_terminal (app));
        else
            run_command (cmd.c_str(), gnome_cmd_app_get_requires_terminal (app));

        g_list_free (files);
    }

    gnome_cmd_app_free (app);
    g_free (args);
}


void mime_exec_multiple (GList *files, GnomeCmdApp *app)
{
    g_return_if_fail (files != NULL);
    g_return_if_fail (app != NULL);

    GList *src_uri_list = NULL;
    GList *dest_uri_list = NULL;
    GList *local_files = NULL;
    gboolean asked = FALSE;
    guint no_of_remote_files = 0;
    gint retid;

    for (; files; files = files->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) files->data;

        if (gnome_vfs_uri_is_local (gnome_cmd_file_get_uri (f)))
            local_files = g_list_append (local_files, g_strdup (gnome_cmd_file_get_real_path (f)));
        else
        {
            ++no_of_remote_files;
            if (gnome_cmd_app_get_handles_uris (app) && gnome_cmd_data.honor_expect_uris)
            {
                local_files = g_list_append (local_files,  g_strdup (gnome_cmd_file_get_uri_str (f)));
            }
            else
            {
                if (!asked)
                {
                    gchar *msg = g_strdup_printf (ngettext("%s does not know how to open remote file. Do you want to download the file to a temporary location and then open it?",
                                                           "%s does not know how to open remote files. Do you want to download the files to a temporary location and then open them?", no_of_remote_files),
                                                  gnome_cmd_app_get_name (app));
                    retid = run_simple_dialog (GTK_WIDGET (main_win), TRUE, GTK_MESSAGE_QUESTION, msg, "", -1, _("No"), _("Yes"), NULL);
                    asked = TRUE;
                }

                if (retid==1)
                {
                    gchar *path_str = get_temp_download_filepath (gnome_cmd_file_get_name (f));

                    if (!path_str) return;

                    GnomeVFSURI *src_uri = gnome_vfs_uri_dup (gnome_cmd_file_get_uri (f));
                    GnomeCmdPath *path = gnome_cmd_plain_path_new (path_str);
                    GnomeVFSURI *dest_uri = gnome_cmd_con_create_uri (get_home_con (), path);
                    gtk_object_destroy (GTK_OBJECT (path));

                    src_uri_list = g_list_append (src_uri_list, src_uri);
                    dest_uri_list = g_list_append (dest_uri_list, dest_uri);
                    local_files = g_list_append (local_files, path_str);
                }
            }
        }
    }

    g_list_free (files);

    gpointer *args = g_new0 (gpointer, 2);
    args[0] = app;
    args[1] = local_files;

    if (src_uri_list)
        gnome_cmd_xfer_tmp_download_multiple (src_uri_list,
                                              dest_uri_list,
                                              GNOME_VFS_XFER_FOLLOW_LINKS,
                                              GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
                                              GTK_SIGNAL_FUNC (do_mime_exec_multiple),
                                              args);
    else
        do_mime_exec_multiple (args);
}


void clear_event_key (GdkEventKey *event)
{
    g_return_if_fail (event != NULL);
    g_return_if_fail (event->string != NULL);

    event->keyval = GDK_VoidSymbol;
    event->string[0] = '\0';
}


/**
 * Transform a "\r\n" separated string into a GList with GnomeVFSURI's
 */
GList *strings_to_uris (gchar *data)
{
    GList *uri_list = NULL;
    gchar **filenames = g_strsplit (data, "\r\n", STRINGS_TO_URIS_CHUNK);

    for (gint i=0; filenames[i] != NULL; i++)
    {
        if (i == STRINGS_TO_URIS_CHUNK)
        {
            uri_list = g_list_concat (uri_list, strings_to_uris (filenames[i]));
            break;
        }

        gchar *fn = g_strdup (filenames[i]);
        GnomeVFSURI *uri = gnome_vfs_uri_new (fn);
        fix_uri (uri);
        if (uri)
            uri_list = g_list_append (uri_list, uri);
        g_free (fn);
    }

    g_strfreev (filenames);
    return uri_list;
}


GnomeVFSFileSize calc_tree_size (const GnomeVFSURI *dir_uri)
{
    if (!dir_uri)
        return -1;

    gchar *dir_uri_str;

    dir_uri_str = gnome_vfs_uri_to_string (dir_uri, GNOME_VFS_URI_HIDE_NONE);

    if (!dir_uri_str)
        return -1;

    GList *list = NULL;
    GnomeVFSFileSize size = 0;

    GnomeVFSResult result = gnome_vfs_directory_list_load (&list, dir_uri_str, GNOME_VFS_FILE_INFO_DEFAULT);

    if (result==GNOME_VFS_OK && list)
    {
        for (GList *tmp = list; tmp; tmp = tmp->next)
        {
            GnomeVFSFileInfo *info = (GnomeVFSFileInfo *) tmp->data;
            if (strcmp (info->name, ".") != 0 && strcmp (info->name, "..") != 0)
            {
                if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
                {
                    GnomeVFSURI *new_dir_uri = gnome_vfs_uri_append_file_name (dir_uri, info->name);
                    size += calc_tree_size (new_dir_uri);
                    gnome_vfs_uri_unref (new_dir_uri);
                }
                else
                    size += info->size;
            }
        }

        for (GList *tmp = list; tmp; tmp = tmp->next)
            gnome_vfs_file_info_unref ((GnomeVFSFileInfo *) tmp->data);

        g_list_free (list);
    }

    g_free (dir_uri_str);

    return size;
}


GList *string_history_add (GList *in, const gchar *value, guint maxsize)
{
    GList *tmp = g_list_find_custom (in, (gchar *) value, (GCompareFunc) strcmp);
    GList *out;

    // if the same value has been given before move it first in the list
    if (tmp != NULL)
    {
        out = g_list_remove_link (in, tmp);
        tmp->next = out;
        if (out)
            out->prev = tmp;
        out = tmp;
    }
    // or if its new just add it
    else
        out = g_list_prepend (in, g_strdup (value));

    // don't let the history get too long
    while (g_list_length (out) > maxsize)
    {
        tmp = g_list_last (out);
        g_free (tmp->data);
        out = g_list_remove_link (out, tmp);
    }

    return out;
}


const gchar *create_nice_size_str (GnomeVFSFileSize size)
{
    static gchar str1[64];
    const gchar *s1 = size2string (size, GNOME_CMD_SIZE_DISP_MODE_GROUPED);
    snprintf (str1, sizeof (str1), ngettext("%s byte","%s bytes",size), s1);

    if (size >= 1000)
    {
        static gchar str2[128];
        const gchar *s2 = size2string (size, GNOME_CMD_SIZE_DISP_MODE_POWERED);
        snprintf (str2, sizeof (str2), "%s (%s)", s2, str1);
        return str2;
    }

    return str1;
}


gchar *quote_if_needed (const gchar *in)
{
    //return strpbrk(in,";&$'\"?")==NULL ? g_strdup (in) : g_strdup_printf ("'%s'", in);
    return g_shell_quote (in);
}


gchar *unquote_if_needed (const gchar *in)
{

    g_return_val_if_fail (in != NULL, NULL);

    gint l = strlen (in);

    // Check if the first and last character is a quote
    if (l>1 && strchr("'\"",in[0])!=NULL && in[0]==in[l-1])
    {
        gchar *out = g_strdup (in+1);
        out[l-2] = '\0';
        return out;
    }

    return g_strdup (in);
}


GtkWidget *create_styled_button (const gchar *text)
{
    GtkWidget *w = text ? gtk_button_new_with_label (text) : gtk_button_new ();

    gtk_button_set_relief (GTK_BUTTON (w), gnome_cmd_data.button_relief);
    gtk_widget_ref (w);
    gtk_widget_show (w);

    return w;
}


GtkWidget *create_styled_pixmap_button (const gchar *text, GnomeCmdPixmap *pm)
{
    g_return_val_if_fail (text || pm, NULL);

    GtkWidget *btn = create_styled_button (NULL);
    GtkWidget *hbox = gtk_hbox_new (FALSE, 1);
    GtkWidget *label = NULL;
    GtkWidget *pixmap = NULL;

    gtk_object_set_data_full (GTK_OBJECT (btn), "hbox", hbox, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_ref (hbox);
    gtk_widget_show (hbox);

    if (pm)
    {
        pixmap = gtk_pixmap_new (pm->pixmap, pm->mask);
        if (pixmap)
        {
            gtk_widget_ref (pixmap);
            gtk_object_set_data_full (GTK_OBJECT (btn), "pixmap", pixmap,
                                      (GtkDestroyNotify) gtk_widget_unref);
            gtk_widget_show (pixmap);
        }
    }

    if (text)
    {
        label = gtk_label_new (text);
        gtk_widget_ref (label);
        gtk_object_set_data_full (GTK_OBJECT (btn), "label", label,
                                  (GtkDestroyNotify) gtk_widget_unref);
        gtk_widget_show (label);
    }

    if (pm && !text)
        gtk_container_add (GTK_CONTAINER (btn), pixmap);
    else
        if (!pm && text)
            gtk_container_add (GTK_CONTAINER (btn), label);
        else
        {
            gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, TRUE, 0);
            gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
            gtk_container_add (GTK_CONTAINER (btn), hbox);
        }

    return btn;
}


void set_cursor_busy_for_widget (GtkWidget *widget)
{
    if (!cursor_busy)
        cursor_busy = gdk_cursor_new (GDK_WATCH);

    gdk_window_set_cursor (widget->window, cursor_busy);

    while (gtk_events_pending ())
        gtk_main_iteration();
}


void set_cursor_busy ()
{
    set_cursor_busy_for_widget (GTK_WIDGET (main_win));
}


void set_cursor_default ()
{
    set_cursor_default_for_widget (GTK_WIDGET (main_win));
}


GList *app_get_linked_libs (GnomeCmdFile *f)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), NULL);

    gchar *s;
    gchar tmp[256];

    gchar *arg = g_shell_quote (gnome_cmd_file_get_real_path (f));
    gchar *cmd = g_strdup_printf ("ldd %s", arg);
    g_free (arg);
    FILE *fd = popen (cmd, "r");
    g_free (cmd);

    if (!fd) return NULL;

    GList *libs = NULL;

    while ((s = fgets (tmp, sizeof(tmp), fd)))
    {
        char **v = g_strsplit (s, " ", 1);
        if (v)
        {
            libs = g_list_append (libs, g_strdup (v[0]));
            g_strfreev (v);
        }
    }

    pclose (fd);

    return libs;
}


gboolean app_needs_terminal (GnomeCmdFile *f)
{
    gboolean need_term = TRUE;

    if (strcmp (f->info->mime_type, "application/x-executable") && strcmp (f->info->mime_type, "application/x-executable-binary"))
        return need_term;

    GList *libs = app_get_linked_libs (f);
    if  (!libs) return FALSE;

    for (GList *tmp = libs; tmp; tmp = tmp->next)
    {
        gchar *lib = (gchar *) tmp->data;
        lib = g_strstrip (lib);
        if (g_str_has_prefix (lib, "libX11"))
        {
            need_term = FALSE;
            break;
        }
    }

    g_list_foreach (libs, (GFunc) g_free, NULL);
    g_list_free (libs);

    return need_term;
}


gchar *get_temp_download_filepath (const gchar *fname)
{
    const gchar *tmp_dir = g_get_tmp_dir ();

    if (!tmp_file_dir)
    {
        gchar *tmp_file_dir_template = g_strdup_printf ("gcmd-%s-XXXXXX", g_get_user_name());
        chdir (tmp_dir);
        tmp_file_dir = mkdtemp (tmp_file_dir_template);
        if (!tmp_file_dir)
        {
            g_free (tmp_file_dir_template);

            create_error_dialog (
                _("Failed to create a directory in which to store temporary files.\nError message: %s\n"),
                strerror (errno));
            return NULL;
        }
    }

    return g_build_path (G_DIR_SEPARATOR_S, tmp_dir, tmp_file_dir, fname, NULL);
}


void remove_temp_download_dir ()
{
    const gchar *tmp_dir = g_get_tmp_dir ();

    if (tmp_file_dir)
    {
        gchar *path = g_build_path (G_DIR_SEPARATOR_S, tmp_dir, tmp_file_dir, NULL);
        gchar *command = g_strdup_printf ("rm -rf %s", path);
        g_free (path);
        system (command);
        g_free (command);
    }
}


inline GtkWidget *scale_pixbuf (GdkPixbuf *pixbuf, GtkIconSize icon_size)
{
    int width, height;

    gtk_icon_size_lookup (icon_size, &width, &height);

    double pix_x = gdk_pixbuf_get_width (pixbuf);
    double pix_y = gdk_pixbuf_get_height (pixbuf);

    // Only scale if the image doesn't match the required icon size
    if (pix_x > width || pix_y > height)
    {
        double greatest = pix_x > pix_y ? pix_x : pix_y;
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf,
                                                     (width/ greatest) * pix_x,
                                                     (height/ greatest) * pix_y,
                                                      GDK_INTERP_BILINEAR);
        GtkWidget *image = gtk_image_new_from_pixbuf (scaled);
        g_object_unref (G_OBJECT (scaled));

        return image;
    }

    return gtk_image_new_from_pixbuf (pixbuf);
}

// Creates a pixmap appropriate for items

GtkWidget *create_ui_pixmap (GtkWidget *unused, GnomeUIPixmapType pixmap_type, gconstpointer pixmap_info, GtkIconSize icon_size)
{
    GtkWidget *pixmap = NULL;
    char *name;

    switch (pixmap_type)
    {
        case GNOME_APP_PIXMAP_STOCK:
            pixmap = gtk_image_new_from_stock (static_cast<const gchar *>(pixmap_info), icon_size);
            break;

        case GNOME_APP_PIXMAP_DATA:
            if (pixmap_info)
            {
                GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) pixmap_info);
                if (pixbuf != NULL)
                {
                    pixmap = scale_pixbuf (pixbuf, icon_size);
                    g_object_unref (G_OBJECT (pixbuf));
                }
            }
            break;

        case GNOME_APP_PIXMAP_NONE:
            break;

        case GNOME_APP_PIXMAP_FILENAME:
            name = gnome_program_locate_file (gnome_program_get (), GNOME_FILE_DOMAIN_APP_PIXMAP,
                                              static_cast<const gchar *>(pixmap_info), TRUE, NULL);

            if (!name)
                g_warning ("Could not find GNOME pixmap file %s", (char *) pixmap_info);
            else
            {
                GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (name, NULL);
                pixmap = scale_pixbuf (pixbuf, icon_size);
                g_object_unref (G_OBJECT (pixbuf));
                g_free (name);
            }
            break;

        default:
            g_assert_not_reached ();
            g_warning("Invalid pixmap_type %d", (int) pixmap_type);
    }

    return pixmap;
}


inline void transform (gchar *s, gchar from, gchar to)
{
    gint len = strlen (s);

    for (gint i=0; i<len; i++)
        if (s[i] == from)
            s[i] = to;
}


gchar *unix_to_unc (const gchar *path)
{
    g_return_val_if_fail (path != NULL, NULL);
    g_return_val_if_fail (path[0] == '/', NULL);

    gchar *out = (gchar *) g_malloc (strlen(path)+2);
    out[0] = '\\';
    strcpy (out+1, path);
    transform (out+1, '/', '\\');

    return out;
}


gchar *unc_to_unix (const gchar *path)
{
    g_return_val_if_fail (path != NULL, NULL);
    g_return_val_if_fail (path[0] == '\\', NULL);
    g_return_val_if_fail (path[1] == '\\', NULL);

    gchar *out = (gchar *) g_malloc(strlen(path));
    strcpy (out, path+1);
    transform (out, '\\', '/');

    return out;
}


GdkColor *gdk_color_new (gushort r, gushort g, gushort b)
{
    GdkColor *c = g_new0 (GdkColor, 1);
    c->pixel = 0;
    c->red = r;
    c->green = g;
    c->blue = b;

    return c;
}


GList *file_list_to_uri_list (GList *files)
{
    GList *uris = NULL;

    for (; files; files = files->next)
    {
        GnomeCmdFile *f = GNOME_CMD_FILE (files->data);
        GnomeVFSURI *uri = gnome_cmd_file_get_uri (f);

        if (!uri)
            g_warning ("NULL uri!!!");
        else
            uris = g_list_append (uris, uri);
    }

    return uris;
}


GList *file_list_to_info_list (GList *files)
{
    GList *infos = NULL;

    for (; files; files = files->next)
    {
        GnomeCmdFile *f = GNOME_CMD_FILE (files->data);
        infos = g_list_append (infos, f->info);
    }

    return infos;
}


gboolean create_dir_if_needed (const gchar *dpath)
{
    g_return_val_if_fail (dpath, FALSE);

    DIR *dir = opendir (dpath);

    if (!dir)
    {
        if (errno == ENOENT)
        {
            g_print (_("Creating directory %s... "), dpath);
            if (mkdir (dpath, S_IRUSR|S_IWUSR|S_IXUSR) == 0)  return TRUE;  else
            {
                gchar *msg = g_strdup_printf (_("Failed to create the directory %s"), dpath);
                perror (msg);
                g_free (msg);
            }
        }
        else
            g_warning(_("Couldn't read from the directory %s: %s"), dpath, strerror (errno));

        return FALSE;
    }

    closedir (dir);
    return TRUE;
}


void edit_mimetypes (const gchar *mime_type, gboolean blocking)
{
    if (!mime_type)
        mime_type = "";

    gchar *arg = g_shell_quote (mime_type);
    gchar *cmd = g_strdup_printf ("gnome-file-types-properties %s\n", arg);
    g_free (arg);
    if (blocking)
        system (cmd);
    else
        run_command (cmd, FALSE);
    g_free (cmd);
}


void fix_uri (GnomeVFSURI *uri)
{
#ifdef FIX_PW_HACK
    gchar *p, *t;
    const gchar *hn, *pw;

    hn = gnome_vfs_uri_get_host_name (uri);
    if (!hn) return;

    pw = gnome_vfs_uri_get_password (uri);

    t = g_strdup (hn);
    p = g_strrstr (t, "@");
    if (p && p[1] != '\0')
    {
        gchar *hn2;
        gchar *pw2;
        *p = '\0';
        hn2 = g_strdup (p+1);
        pw2 = g_strdup_printf ("%s@%s", pw, t);
        gnome_vfs_uri_set_host_name (uri, hn2);
        gnome_vfs_uri_set_password (uri, pw2);
        g_free (hn2);
        g_free (pw2);
    }
    g_free (t);
#endif
}


GList *patlist_new (const gchar *pattern_string)
{
    g_return_val_if_fail (pattern_string != NULL, NULL);

    GList *patlist = NULL;
    gchar **ents = g_strsplit (pattern_string, ";", 0);

    for (gint i = 0; ents[i]; i++)
        patlist = g_list_append (patlist, ents[i]);

    g_free (ents);

    return patlist;
}


void patlist_free (GList *pattern_list)
{
    g_return_if_fail (pattern_list != NULL);

    g_list_foreach (pattern_list, (GFunc) g_free, NULL);
    g_list_free (pattern_list);
}


gboolean patlist_matches (GList *pattern_list, const gchar *s)
{
    for (GList *tmp = pattern_list; tmp; tmp = tmp->next)
#ifdef FNM_CASEFOLD
        if (fnmatch ((gchar *) tmp->data, s, FNM_NOESCAPE|FNM_CASEFOLD) == 0)
#else
        if (fnmatch ((gchar *) tmp->data, s, FNM_NOESCAPE) == 0)   // omit FNM_CASEFOLD as it is a GNU extension.
#endif
            return TRUE;

    return FALSE;
}


void gnome_cmd_toggle_file_name_selection (GtkWidget *entry)
{
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));
    const char *s = strrchr(text,G_DIR_SEPARATOR);                  // G_DIR_SEPARATOR is ASCII, g_utf8_strrchr() is not needed here
    glong base = s ? g_utf8_pointer_to_offset (text, ++s) : 0;

    gint beg;
    gint end;

    if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &beg, &end))
    {
        beg = base;
        end = -1;
    }
    else
    {
        glong text_len = g_utf8_strlen (text, -1);

        s = strrchr(text,'.');                                      // '.' is ASCII, g_utf8_strrchr() is not needed here
        glong ext = s ? g_utf8_pointer_to_offset (text, s) : -1;

        if (beg==0 && end==text_len)
        {
            beg = base;
            end = ext;
        }
        else
        {
            if (beg!=base)
                beg = beg>base ? base : 0;
            else
                if (end!=ext || end==text_len)
                    beg = 0;

            end = -1;
        }
    }

    gtk_editable_select_region (GTK_EDITABLE (entry), beg, end);
}


int split(const string &s, vector<string> &coll, const char *sep)
{
  coll.clear();

  if (s.empty())  return 0;

  if (!sep)  sep = "";
  int seplen = strlen(sep);

  if (!seplen)
  {
    for (string::const_iterator i=s.begin(); i!=s.end(); ++i)
      coll.push_back(string(1,*i));

    return s.length();
  }

  int n = 1;
  int start = 0;

  for (int end; (end=s.find(sep,start))!=string::npos; ++n)
  {
    coll.push_back(string(s,start,end-start));
    start = end + seplen;
  }

  coll.push_back(string(s,start));

  return n;
}
