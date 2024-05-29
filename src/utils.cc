/**
 * @file utils.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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
#include <errno.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdlib.h>

#include <set>

#include "gnome-cmd-includes.h"
#include "utils.h"
#include "gnome-cmd-data.h"
#include "imageloader.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-user-actions.h"

using namespace std;


#define FIX_PW_HACK
#define STRINGS_TO_URIS_CHUNK 1024

static GdkCursor *cursor_busy = NULL;
static gchar *tmp_file_dir = NULL;


/**
 * The already reserved debug flags:
 * --------------------------------
 * a: set all debug flags\n
 * c: file and directory counting\n
 * d: directory ref-counting\n
 * f: file ref-counting\n
 * g: run_command debugging\n
 * i: imageloader\n
 * k: directory pool\n
 * l: directory listings\n
 * m: connection debugging\n
 * n: directory monitoring\n
 * s: smb network browser\n
 * t: metadata tags\n
 * u: user actions debugging\n
 * v: internal viewer\n
 * w: widget_lookup\n
 * y: brief mime-based imageload\n
 * z: detailed mime-based imageload\n
 * x: xfer
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


/**
 * This function executes a command in the active directory in a
 * terminal window, if desired.
 * \param in_command Command to be executed.
 * \param dpath Directory in which the command should be executed.
 * \param term If TRUE, the command is executed in a terminal window.
 * \sa GnomeCmdData::Options::termexec
 */
gboolean run_command_indir (const gchar *in_command, const gchar *dpath, gboolean term)
{
    g_return_val_if_fail (in_command != NULL, FALSE);
    g_return_val_if_fail (strlen(in_command) != 0, FALSE);

    gchar *command;

    if (term)
    {
        gchar *arg;

        if (gnome_cmd_data.use_gcmd_block)
        {
            gchar *s = g_strdup_printf ("bash -c \"%s; %s/bin/gcmd-block\"", in_command, PREFIX);
            arg = g_shell_quote (s);
            g_free (s);
        }
        else
            arg = g_shell_quote (in_command);

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
        command = g_strdup_printf (gnome_cmd_data.options.termexec, arg);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

        g_free (arg);
    }
    else
        command = g_strdup (in_command);

    DEBUG ('g', "running%s: %s\n", (term?" in terminal":""), command);

    gint argc;
    gchar **argv;
    GError *error = NULL;

    // check if command includes % and replace
    string cmd;
    cmd.reserve(2000);
    if (parse_command(&cmd, (const gchar*) command) == 0)
    {
        DEBUG ('g', "run_command_indir: command is not valid.\n");
        gnome_cmd_show_message (*main_win, _("No valid command given."));
        return FALSE;
    }

    //g_shell_parse_argv (command, &argc, &argv, NULL);
    g_shell_parse_argv (cmd.c_str(), &argc, &argv, NULL); // include parse_command
    if (!g_spawn_async (dpath, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error))
    {
        gnome_cmd_error_message (_("Unable to execute command."), error);
        g_strfreev (argv);
        g_free (command);
        return FALSE;
    }
    g_strfreev (argv);
    g_free (command);
    return TRUE;
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

    g_signal_stop_emission_by_name (data, "delete-event");

    return TRUE;
}


static gboolean on_run_dialog_keypress (GtkWidget *dialog, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_KEY_Escape)
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

    dialog = gtk_message_dialog_new (*main_win, GTK_DIALOG_MODAL, msg_type, GTK_BUTTONS_NONE, NULL);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), text);

    if (title)
        gtk_window_set_title (GTK_WINDOW (dialog), title);

    for (int i=0; button_titles[i]; i++)
        gtk_dialog_add_button (GTK_DIALOG (dialog), button_titles[i], i);

    g_free (button_titles);

    if (def_response>=0)
        gtk_dialog_set_default_response (GTK_DIALOG (dialog), def_response);

    // Allow close.
    if (ignore_close_box)
        g_signal_connect (dialog, "delete-event", G_CALLBACK (delete_event_callback), NULL);
    else
        g_signal_connect (dialog, "key-press-event", G_CALLBACK (on_run_dialog_keypress), dialog);

    gtk_window_set_wmclass (GTK_WINDOW (dialog), "dialog", "Eel");

    // Run it.
    do
    {
        gtk_widget_show (dialog);
        result = gtk_dialog_run (GTK_DIALOG (dialog));
    }
    while (ignore_close_box && result == GTK_RESPONSE_DELETE_EVENT);

    gtk_window_destroy (GTK_WINDOW (dialog));

    return result;
}

const gchar *type2string (guint32 type, gchar *buf, guint max)
{
    const char *s;

    switch (type)
    {
        case G_FILE_TYPE_UNKNOWN:
            s = "?";
            break;
        case G_FILE_TYPE_REGULAR:
            s = " ";
            break;
        case G_FILE_TYPE_DIRECTORY:
            s = G_DIR_SEPARATOR_S;
            break;
        case G_FILE_TYPE_SYMBOLIC_LINK:
            s = "@";
            break;
        case G_FILE_TYPE_SPECIAL:
            s = "S";
            break;
        case G_FILE_TYPE_SHORTCUT:
            s = "K"; // "Keyboard shortcut"
            break;
        case G_FILE_TYPE_MOUNTABLE:
            s = "M";
            break;

        default:
             s = "?";
    }

    g_snprintf (buf, max, "%s", s);

    return buf;
}


const gchar *perm2string (guint32 permissions, gchar *buf, guint max)
{
    switch (gnome_cmd_data.options.perm_disp_mode)
    {
        case GNOME_CMD_PERM_DISP_MODE_TEXT:
            return perm2textstring (permissions, buf, max);

        case GNOME_CMD_PERM_DISP_MODE_NUMBER:
            return perm2numstring (permissions, buf, max);

        default:
            return perm2textstring (permissions, buf, max);
    }
}


const gchar *perm2textstring (guint32 permissions, gchar *buf, guint max)
{
    g_snprintf (buf, max, "%s%s%s%s%s%s%s%s%s",
                (permissions & GNOME_CMD_PERM_USER_READ)   ? "r" : "-",
                (permissions & GNOME_CMD_PERM_USER_WRITE)  ? "w" : "-",
                (permissions & GNOME_CMD_PERM_USER_EXEC)   ? "x" : "-",
                (permissions & GNOME_CMD_PERM_GROUP_READ)  ? "r" : "-",
                (permissions & GNOME_CMD_PERM_GROUP_WRITE) ? "w" : "-",
                (permissions & GNOME_CMD_PERM_GROUP_EXEC)  ? "x" : "-",
                (permissions & GNOME_CMD_PERM_OTHER_READ)  ? "r" : "-",
                (permissions & GNOME_CMD_PERM_OTHER_WRITE) ? "w" : "-",
                (permissions & GNOME_CMD_PERM_OTHER_EXEC)  ? "x" : "-");

    return buf;
}


const gchar *perm2numstring (guint32 permissions, gchar *buf, guint max)
{
    gint i = 0;

    if (permissions & GNOME_CMD_PERM_USER_READ)   i += 400;
    if (permissions & GNOME_CMD_PERM_USER_WRITE)  i += 200;
    if (permissions & GNOME_CMD_PERM_USER_EXEC)   i += 100;
    if (permissions & GNOME_CMD_PERM_GROUP_READ)  i += 40;
    if (permissions & GNOME_CMD_PERM_GROUP_WRITE) i += 20;
    if (permissions & GNOME_CMD_PERM_GROUP_EXEC)  i += 10;
    if (permissions & GNOME_CMD_PERM_OTHER_READ)  i += 4;
    if (permissions & GNOME_CMD_PERM_OTHER_WRITE) i += 2;
    if (permissions & GNOME_CMD_PERM_OTHER_EXEC)  i += 1;

    g_snprintf (buf, max, "%i", i);

    return buf;
}


const gchar *size2string (guint64 size, GnomeCmdSizeDispMode size_disp_mode)
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
                    g_snprintf (buf0, sizeof(buf0), "%" G_GUINT64_FORMAT " %s ", size, prefixes[0]);
            }
            break;

        case GNOME_CMD_SIZE_DISP_MODE_GROUPED:
            {
                gint len = g_snprintf (buf0, sizeof(buf0), "%" G_GUINT64_FORMAT " ", size);

                if (len < 5)
                    return buf0;

                gchar *sep = (gchar*) " ";

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
            g_snprintf (buf0, sizeof(buf0), "%'" G_GUINT64_FORMAT " ", size);
            break;

        case GNOME_CMD_SIZE_DISP_MODE_PLAIN:
            g_snprintf (buf0, sizeof(buf0), "%" G_GUINT64_FORMAT " ", size);
            break;

        default:
            break;
    }

    return buf0;
}


const gchar *time2string (GDateTime *gDateTime, const gchar *dateFormat)
{
    // NOTE: date_format is passed in current locale format

    g_return_val_if_fail (gDateTime != nullptr, nullptr);
    g_return_val_if_fail (dateFormat != nullptr, nullptr);

    static gchar buf[64];

    auto localGDateTime = g_date_time_to_local (gDateTime);

    auto dateString = g_date_time_format (localGDateTime, dateFormat);

    strncpy (buf, dateString, sizeof(buf)-1);

    g_date_time_unref(localGDateTime);
    g_free (dateString);

    return buf;
}


/**
 * Transform a "\r\n" separated string into a GList with GFiles's
 */
GList *uri_strings_to_gfiles (gchar *data)
{
    GList *gFileGList = NULL;
    gchar **filenames = g_strsplit (data, "\r\n", STRINGS_TO_URIS_CHUNK);

    for (gint i=0; filenames[i] != NULL; i++)
    {
        if (i == STRINGS_TO_URIS_CHUNK)
        {
            gFileGList = g_list_concat (gFileGList, uri_strings_to_gfiles (filenames[i]));
            break;
        }

        gchar *fn = g_strdup (filenames[i]);
        if (!fn || strcmp(fn, "") == 0)
        {
            g_free(fn);
            continue;
        }
        auto gFile = g_file_new_for_uri(fn);
            gFileGList = g_list_append (gFileGList, gFile);
        g_free (fn);
    }

    g_strfreev (filenames);
    return gFileGList;
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


gchar *create_nice_size_str (guint64 size)
{
    GString *s = g_string_sized_new (64);

    if (gnome_cmd_data.options.size_disp_mode==GNOME_CMD_SIZE_DISP_MODE_POWERED && size>=1000)
    {
        g_string_printf (s, "%s", size2string (size, GNOME_CMD_SIZE_DISP_MODE_POWERED));
        g_string_append_printf (s, ngettext("(%sbyte)","(%sbytes)",size), size2string (size, GNOME_CMD_SIZE_DISP_MODE_GROUPED));
    }
    else
        g_string_printf (s, ngettext("%sbyte","%sbytes",size), size2string (size, GNOME_CMD_SIZE_DISP_MODE_GROUPED));

    return g_string_free (s, FALSE);
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

    gtk_button_set_relief (GTK_BUTTON (w), GTK_RELIEF_NONE);
    g_object_ref (w);
    gtk_widget_show (w);

    return w;
}


GtkWidget *create_styled_pixbuf_button (const gchar *text, GdkPixbuf *pb)
{
    g_return_val_if_fail (text || pb, NULL);

    GtkWidget *btn = create_styled_button (NULL);
    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);
    GtkWidget *label = NULL;
    GtkWidget *image = NULL;

    g_object_set_data_full (G_OBJECT (btn), "hbox", hbox, g_object_unref);
    g_object_ref (hbox);
    gtk_widget_show (hbox);

    if (pb)
    {
        image = gtk_image_new_from_pixbuf (pb);
        if (image)
        {
            g_object_ref (image);
            g_object_set_data_full (G_OBJECT (btn), "image", image, g_object_unref);
            gtk_widget_show (image);
        }
    }

    if (text)
    {
        label = gtk_label_new (text);
        g_object_ref (label);
        g_object_set_data_full (G_OBJECT (btn), "label", label, g_object_unref);
        gtk_widget_show (label);
    }

    if (pb && !text)
        gtk_container_add (GTK_CONTAINER (btn), image);
    else
        if (!pb && text)
            gtk_container_add (GTK_CONTAINER (btn), label);
        else
        {
            gtk_box_append (GTK_BOX (hbox), image);
            gtk_box_append (GTK_BOX (hbox), label);
            gtk_container_add (GTK_CONTAINER (btn), hbox);
        }

    return btn;
}


void set_cursor_busy_for_widget (GtkWidget *widget)
{
    if (!cursor_busy)
        cursor_busy = gdk_cursor_new (GDK_WATCH);

    gdk_window_set_cursor (gtk_widget_get_window (widget), cursor_busy);

    while (gtk_events_pending ())
        gtk_main_iteration();
}


void set_cursor_busy ()
{
    set_cursor_busy_for_widget (*main_win);
}


void set_cursor_default ()
{
    set_cursor_default_for_widget (*main_win);
}


GList *app_get_linked_libs (GnomeCmdFile *f)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), NULL);

    gchar *s;
    gchar tmp[256];

    gchar *arg = g_shell_quote (f->get_real_path());
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
    gboolean needTerminal;

    auto contentType = f->GetGfileAttributeString(G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
    if (strcmp (contentType, "application/x-executable") && strcmp (contentType, "application/x-executable-binary"))
    {
        needTerminal = TRUE;
    }
    else
    {
        GList *libs = app_get_linked_libs (f);
        if  (!libs)
        {
            needTerminal = FALSE;
        }
        else
        {
            for (GList *i = libs; i; i = i->next)
            {
                gchar *lib = (gchar *) i->data;
                lib = g_strstrip (lib);
                if (g_str_has_prefix (lib, "libX11"))
                {
                    needTerminal = FALSE;
                    break;
                }
            }
        }
        g_list_foreach (libs, (GFunc) g_free, NULL);
        g_list_free (libs);
    }

    g_free(contentType);

    return needTerminal;
}

// ToDo: This function should be reworked using GIO / GFile
gchar *get_temp_download_filepath (const gchar *fname)
{
    const gchar *tmp_dir = g_get_tmp_dir ();

    if (!tmp_file_dir)
    {
        if (chdir (tmp_dir))
        {
            gnome_cmd_show_message (NULL, _("Failed to change working directory to a temporary directory."), strerror (errno));

            return NULL;
        }
        gchar *tmp_file_dir_template = g_strdup_printf ("gcmd-%s-XXXXXX", g_get_user_name());
        tmp_file_dir = mkdtemp (tmp_file_dir_template);
        if (!tmp_file_dir)
        {
            g_free (tmp_file_dir_template);

            gnome_cmd_show_message (NULL, _("Failed to create a directory in which to store temporary files."), strerror (errno));

            return NULL;
        }
    }

    return g_build_filename (tmp_dir, tmp_file_dir, fname, NULL);
}


void remove_temp_download_dir ()
{
    if (tmp_file_dir)
    {
        gchar *path = g_build_filename (g_get_tmp_dir (), tmp_file_dir, NULL);
        gchar *command = g_strdup_printf ("rm -rf %s", path);
        g_free (path);
        int status;
        if ((status = system (command)))
        {
            g_warning ("executing \"%s\" failed with exit status %d\n", command, status);
        }
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
        g_object_unref (scaled);

        return image;
    }

    return gtk_image_new_from_pixbuf (pixbuf);
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


GdkRGBA *gdk_color_new (gushort r, gushort g, gushort b)
{
    GdkRGBA *c = g_new0 (GdkRGBA, 1);
    // c->pixel = 0;
    c->red = r;
    c->green = g;
    c->blue = b;

    return c;
}


GList *gnome_cmd_file_list_to_gfile_list (GList *files)
{
    GList *gFiles = nullptr;

    for (; files; files = files->next)
    {
        GnomeCmdFile *f = GNOME_CMD_FILE (files->data);
        auto gFile = f->get_gfile();

        if (!gFile)
            g_warning ("gnome_cmd_file_list_to_gfile_list: no gFile!!!");
        else
            gFiles = g_list_append (gFiles, gFile);
    }

    return gFiles;
}


gboolean is_dir_existing(const gchar *dpath)
{
    g_return_val_if_fail (dpath, FALSE);
    auto gFile = g_file_new_for_path(dpath);

    auto returnValue = false;

    if (g_file_query_exists(gFile, nullptr))
    {
        returnValue = true;
    }
    g_object_unref(gFile);
    return returnValue;
}


gboolean create_dir (const gchar *dpath)
{
    g_return_val_if_fail (dpath, FALSE);

    g_print (_("Creating directory %s… "), dpath);
    GError *error = nullptr;
    auto gFile = g_file_new_for_path(dpath);
    if (g_file_make_directory (gFile, nullptr, &error))
    {
        g_object_unref(gFile);
        return TRUE;
    }
    else
    {
        g_object_unref(gFile);
        g_warning (_("Failed to create the directory %s: %s"), dpath, error->message);
        g_error_free (error);
        return FALSE;
    }
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
    if (!pattern_list)  return;

    g_list_foreach (pattern_list, (GFunc) g_free, NULL);
    g_list_free (pattern_list);
}


gboolean patlist_matches (GList *pattern_list, const gchar *s)
{
    for (GList *i=pattern_list; i; i=i->next)
#ifdef FNM_CASEFOLD
        if (fnmatch ((gchar *) i->data, s, FNM_NOESCAPE|FNM_CASEFOLD) == 0)
#else
        if (fnmatch ((gchar *) i->data, s, FNM_NOESCAPE) == 0)   // omit FNM_CASEFOLD as it is a GNU extension.
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

        s = strrchr(s ? s : text,'.');                                      // '.' is ASCII, g_utf8_strrchr() is not needed here
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


void gnome_cmd_help_display (const gchar *file_name, const gchar *link_id)
{
    GError *error = NULL;
    gchar help_uri[256] = "help:";

    strcat(help_uri, PACKAGE_NAME);

    if (link_id != NULL)
    {
        strcat(help_uri, "/");
        strcat(help_uri, link_id);
    }

    gtk_show_uri (NULL, help_uri,  gtk_get_current_event_time (), &error);

    if (error != NULL)
        gnome_cmd_error_message (_("There was an error displaying help."), error);
}


void gnome_cmd_error_message (const gchar *title, GError *error)
{
    gnome_cmd_prompt_message (NULL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, title, error->message);
    g_error_free (error);
}


gboolean gnome_cmd_prepend_su_to_vector (int &argc, char **&argv)
{
    // sanity
    if(!argv)
        argc = 0;

    char *su = NULL;
    gboolean need_c = FALSE;

    if ((su = g_find_program_in_path ("gksudo")))
       goto without_c_param;
    if ((su = g_find_program_in_path ("xdg-su")))
       goto without_c_param;
    if ((su = g_find_program_in_path ("gksu")))
       goto without_c_param;
    if ((su = g_find_program_in_path ("gnomesu")))
       goto with_c_param;
    if ((su = g_find_program_in_path ("beesu")))
       goto without_c_param;
    if ((su = g_find_program_in_path ("kdesu")))
       goto without_c_param;

    return FALSE;

 with_c_param:

   need_c = TRUE;

 without_c_param:

    char **su_argv = g_new0 (char *, 3);
    int su_argc = 0;

    su_argv[su_argc++] = su;
    if (need_c)
        su_argv[su_argc++] = g_strdup("-c");

    // compute size if not given
    if (argc < 0)
        for (argc=0; argv[argc]; ++argc);

    int real_argc = su_argc + argc;
    char **real_argv = g_new0 (char *, real_argc+1);

    memmove (real_argv, su_argv, su_argc*sizeof(char *));
    memmove (real_argv+su_argc, argv, argc*sizeof(char *));

    g_free (argv);

    argv = real_argv;
    argc = real_argc;

    return TRUE;
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

  for (int end; (size_t)(end=s.find(sep,start))!=string::npos; ++n)
  {
    coll.push_back(string(s,start,end-start));
    start = end + seplen;
  }

  coll.push_back(string(s,start));

  return n;
}


gint get_string_pixel_size (const char *s, int len)
{
    // find the size, in pixels, of the given string
    gint xSize, ySize;

    gchar *buf = g_strndup(s, len);
    gchar *utf8buf = get_utf8 (buf);

    GtkLabel *label = GTK_LABEL (gtk_label_new (utf8buf));
    gchar *ms = get_mono_text (utf8buf);
    gtk_label_set_markup (label, ms);
    g_free (ms);
    g_object_ref_sink(G_OBJECT(label));

    PangoLayout *layout = gtk_label_get_layout (label);
    pango_layout_get_pixel_size (layout, &xSize, &ySize);

    g_object_unref (label);
    g_free (utf8buf);
    g_free (buf);

    return xSize;
}


gchar* get_package_config_dir()
{
    return g_build_filename (g_get_user_config_dir(), PACKAGE, NULL);
}


gchar *string_double_underscores (const gchar *string)
{
    if (!string)
        return nullptr;

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

guint32 get_gfile_attribute_uint32(GFileInfo *gFileInfo, const char *attribute)
{
    g_return_val_if_fail(gFileInfo != nullptr, 0);
    g_return_val_if_fail(G_IS_FILE_INFO(gFileInfo), 0);

    return g_file_info_get_attribute_uint32 (gFileInfo, attribute);
}

guint32 get_gfile_attribute_uint32(GFile *gFile, const char *attribute)
{
    GError *error;
    error = nullptr;
    guint32 gFileAttributeUInt32 = 0;

    auto gcmdFileInfo = g_file_query_info(gFile,
                                   attribute,
                                   G_FILE_QUERY_INFO_NONE,
                                   nullptr,
                                   &error);
    if (error)
    {
        auto gFileUri = g_file_get_uri(gFile);
        g_message ("get_gfile_attribute_uint32: retrieving file info for %s failed: %s",
                    gFileUri, error->message);
        g_free(gFileUri);
        g_error_free (error);
    }
    else
    {
        gFileAttributeUInt32 = get_gfile_attribute_uint32 (gcmdFileInfo, attribute);
        g_object_unref(gcmdFileInfo);
    }

    return gFileAttributeUInt32;
}

guint64 get_gfile_attribute_uint64(GFileInfo *gFileInfo, const char *attribute)
{
    g_return_val_if_fail(gFileInfo != nullptr, 0);
    g_return_val_if_fail(G_IS_FILE_INFO(gFileInfo), 0);

    return g_file_info_get_attribute_uint64 (gFileInfo, attribute);
}

guint64 get_gfile_attribute_uint64(GFile *gFile, const char *attribute)
{
    GError *error;
    error = nullptr;
    guint64 gFileAttributeUInt64 = 0;

    auto gcmdFileInfo = g_file_query_info(gFile,
                                   attribute,
                                   G_FILE_QUERY_INFO_NONE,
                                   nullptr,
                                   &error);
    if (error)
    {
        auto gFileUri = g_file_get_uri(gFile);
        g_message ("get_gfile_attribute_uint64: retrieving file info for %s failed: %s",
                    gFileUri, error->message);
        g_free(gFileUri);
        g_error_free (error);
    }
    else
    {
        gFileAttributeUInt64 = get_gfile_attribute_uint64 (gcmdFileInfo, attribute);
        g_object_unref(gcmdFileInfo);
    }

    return gFileAttributeUInt64;
}


gboolean get_gfile_attribute_boolean(GFileInfo *gFileInfo, const char *attribute)
{
    g_return_val_if_fail(gFileInfo != nullptr, FALSE);
    g_return_val_if_fail(G_IS_FILE_INFO(gFileInfo), FALSE);

    return g_file_info_get_attribute_boolean (gFileInfo, attribute);
}

gboolean get_gfile_attribute_boolean(GFile *gFile, const char *attribute)
{
    GError *error;
    error = nullptr;
    gboolean gFileAttributeBoolean = 0;

    auto gcmdFileInfo = g_file_query_info(gFile,
                                   attribute,
                                   G_FILE_QUERY_INFO_NONE,
                                   nullptr,
                                   &error);
    if (error)
    {
        auto gFileUri = g_file_get_uri(gFile);
        g_message ("get_gfile_attribute_boolean: retrieving file info for %s failed: %s",
                    gFileUri, error->message);
        g_free(gFileUri);
        g_error_free (error);
    }
    else
    {
        gFileAttributeBoolean = get_gfile_attribute_boolean (gcmdFileInfo, attribute);
        g_object_unref(gcmdFileInfo);
    }

    return gFileAttributeBoolean;
}

gchar *get_gfile_attribute_string(GFileInfo *gFileInfo, const char *attribute)
{
    g_return_val_if_fail(gFileInfo != nullptr, nullptr);
    g_return_val_if_fail(G_IS_FILE_INFO(gFileInfo), nullptr);

    return g_strdup(g_file_info_get_attribute_string (gFileInfo, attribute));
}

gchar *get_gfile_attribute_string(GFile *gFile, const char *attribute)
{
    GError *error;
    error = nullptr;
    auto gcmdFileInfo = g_file_query_info(gFile,
                                   attribute,
                                   G_FILE_QUERY_INFO_NONE,
                                   nullptr,
                                   &error);
    if (gcmdFileInfo && error)
    {
        auto gFileUri = g_file_get_uri(gFile);
        g_message ("get_gfile_attribute_string: retrieving file info for %s failed: %s",
                    gFileUri, error->message);
        g_free(gFileUri);
        g_error_free (error);
        return nullptr;
    }

    auto gFileAttributeString = get_gfile_attribute_string (gcmdFileInfo, attribute);
    g_object_unref(gcmdFileInfo);

    return gFileAttributeString;
}
