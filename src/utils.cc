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
#include "text-utils.h"

using namespace std;


#define FIX_PW_HACK
#define STRINGS_TO_URIS_CHUNK 1024

static gchar *tmp_file_dir = NULL;


/**
 * The already reserved debug flags:
 * --------------------------------
 * a: set all debug flags\n
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
    if (!dateString)
        dateString = g_date_time_format (localGDateTime, "%c");

    if (dateString)
        strncpy (buf, dateString, sizeof(buf)-1);
    else
        buf[0] = '\0';

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


void set_cursor_busy_for_widget (GtkWidget *widget)
{
    static GdkCursor *cursor_busy = NULL;

    if (!cursor_busy)
        cursor_busy = gdk_cursor_new_from_name ("wait", nullptr);

    gtk_widget_set_cursor (widget, cursor_busy);
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


inline void transform (gchar *s, gchar from, gchar to)
{
    gint len = strlen (s);

    for (gint i=0; i<len; i++)
        if (s[i] == from)
            s[i] = to;
}


void gnome_cmd_toggle_file_name_selection (GtkWidget *entry)
{
    const gchar *text = gtk_editable_get_text (GTK_EDITABLE (entry));
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

    gtk_show_uri (NULL, help_uri, GDK_CURRENT_TIME);

    if (error != NULL)
        gnome_cmd_error_message (nullptr, _("There was an error displaying help."), error);
}


static GtkWidget *create_error_message_dialog (GtkWindow *parent, const gchar *message, const gchar *secondary_text)
{
    GtkWidget *dlg = gtk_message_dialog_new (parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", message);
    if (secondary_text)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg), "%s", secondary_text);
    g_signal_connect (dlg, "response", G_CALLBACK (gtk_window_destroy), dlg);
    return dlg;
}


void gnome_cmd_show_message (GtkWindow *parent, const gchar *message, const gchar *secondary_text)
{
    GtkWidget *dlg = create_error_message_dialog (parent, message, secondary_text);
    gtk_window_present (GTK_WINDOW (dlg));
}

void gnome_cmd_error_message (GtkWindow *parent, const gchar *message, GError *error)
{
    GtkWidget *dlg = create_error_message_dialog (parent, message, error->message);
    g_error_free (error);
    gtk_window_present (GTK_WINDOW (dlg));
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
