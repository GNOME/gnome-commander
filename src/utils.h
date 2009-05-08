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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#include <stdio.h>

#include "gnome-cmd-file.h"
#include "gnome-cmd-types.h"
#include "gnome-cmd-pixmap.h"
#include "gnome-cmd-app.h"

#define TRACE(s)  std::cout << __FILE__ "(" << __LINE__ << ") " << __PRETTY_FUNCTION__ << "\t" #s ": `" << (s) << "'" << std::endl

gboolean DEBUG_ENABLED (gchar flag);
void DEBUG (gchar flag, const gchar *fmt, ...);
void warn_print (const gchar *fmt, ...);

void run_command_indir (const gchar *command, const gchar *dir, gboolean term);

inline void run_command (const gchar *command, gboolean term)
{
    run_command_indir (command, NULL, term);
}

const char **convert_varargs_to_name_array (va_list args);
gint run_simple_dialog (GtkWidget *parent, gboolean ignore_close_box,
                        GtkMessageType msg_type,
                        const char *text, const char *title, gint def_response, ...);

inline gboolean string2int (const gchar *s, gint &i)
{
    return sscanf (s, "%d", &i) == 1;
}

inline gboolean string2uint (const gchar *s, guint &i)
{
    return sscanf (s, "%ud", &i) == 1;
}

inline gboolean string2short (const gchar *s, gshort &sh)
{
    int i;
    int ret = sscanf (s, "%d", &i);
    sh = i;
    return ret == 1;
}

inline gboolean string2ushort (const gchar *s, gushort &sh)
{
    int i;
    int ret = sscanf (s, "%d", &i);
    sh = i;
    return ret == 1;
}

inline gboolean string2char (const gchar *s, gchar &c)
{
    int i;
    int ret = sscanf (s, "%d", &i);
    c = i;
    return ret == 1;
}

inline gboolean string2uchar (const gchar *s, guchar &c)
{
    int i;
    int ret = sscanf (s, "%d", &i);
    c = i;
    return ret == 1;
}

inline gboolean string2float (const gchar *s, gfloat &f)
{
    return sscanf (s, "%f", &f) == 1;
}

inline char *int2string (gint i)
{
    return g_strdup_printf ("%d", i);
}

gchar *str_uri_basename (const gchar *uri);

const gchar *type2string (GnomeVFSFileType type, gchar *buf, guint max);
const gchar *name2string (gchar *filename, gchar *buf, guint max);
const gchar *perm2string (GnomeVFSFilePermissions p, gchar *buf, guint max);
const gchar *perm2textstring (GnomeVFSFilePermissions p, gchar *buf, guint max);
const gchar *perm2numstring (GnomeVFSFilePermissions p, gchar *buf, guint max);
const gchar *size2string (GnomeVFSFileSize size, GnomeCmdSizeDispMode size_disp_mode);
const gchar *time2string (time_t t, const gchar *date_format);

void mime_exec_single (GnomeCmdFile *f);
void mime_exec_multiple (GList *files, GnomeCmdApp *app);

void clear_event_key (GdkEventKey *event);

inline gboolean state_is_blank (gint state)
{
    gboolean ret = (state & GDK_SHIFT_MASK) || (state & GDK_CONTROL_MASK) || (state & GDK_MOD1_MASK);

    return !ret;
}

inline gboolean state_is_shift (gint state)
{
    return (state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && !(state & GDK_MOD1_MASK);
}

inline gboolean state_is_ctrl (gint state)
{
    return !(state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && !(state & GDK_MOD1_MASK);
}

inline gboolean state_is_alt (gint state)
{
    return !(state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);
}

inline gboolean state_is_alt_shift (gint state)
{
    return (state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);
}

inline gboolean state_is_ctrl_alt (gint state)
{
    return !(state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);
}

inline gboolean state_is_ctrl_shift (gint state)
{
    return (state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && !(state & GDK_MOD1_MASK);
}

inline gboolean state_is_ctrl_alt_shift (gint state)
{
    return (state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);
}

GList *strings_to_uris (gchar *data);

GnomeVFSFileSize calc_tree_size (const GnomeVFSURI *dir_uri);
const gchar *create_nice_size_str (GnomeVFSFileSize size);
gchar *quote_if_needed (const gchar *in);
gchar *unquote_if_needed (const gchar *in);

inline void stop_kp (GtkObject *obj)
{
    gtk_signal_emit_stop_by_name (obj, "key-press-event");
}

GList *string_history_add (GList *in, const gchar *value, guint maxsize);

GtkWidget *create_styled_button (const gchar *text);
GtkWidget *create_styled_pixmap_button (const gchar *text, GnomeCmdPixmap *pixmap);

void set_cursor_busy_for_widget (GtkWidget *widget);

inline void set_cursor_default_for_widget (GtkWidget *widget)
{
    if (widget->window)
        gdk_window_set_cursor (widget->window, NULL);
}

void set_cursor_busy ();
void set_cursor_default ();

GList *app_get_linked_libs (GnomeCmdFile *f);
gboolean app_needs_terminal (GnomeCmdFile *f);

gchar *get_temp_download_filepath (const gchar *fname);
void remove_temp_download_dir ();

GtkWidget *create_ui_pixmap (GtkWidget *window,
                             GnomeUIPixmapType pixmap_type,
                             gconstpointer pixmap_info,
                             GtkIconSize size);

gchar *unix_to_unc (const gchar *path);
gchar *unc_to_unix (const gchar *path);
GdkColor *gdk_color_new (gushort r, gushort g, gushort b);
GList *file_list_to_uri_list (GList *files);
GList *file_list_to_info_list (GList *files);

gboolean create_dir_if_needed (const gchar *dpath);
void edit_mimetypes (const gchar *mime_type, gboolean blocking);
void fix_uri (GnomeVFSURI *uri);

inline gboolean uri_is_valid (const gchar *uri)
{
    GnomeVFSURI *vfs_uri = gnome_vfs_uri_new (uri);         //      gnome_vfs_uri_new() returns NULL for !uri

    if (!vfs_uri)
        return FALSE;

    gnome_vfs_uri_unref (vfs_uri);

    return TRUE;
}

inline gboolean uri_is_valid (const std::string &uri)
{
    return uri_is_valid (uri.c_str());
}

GList *patlist_new (const gchar *pattern_string);
void patlist_free (GList *pattern_list);
gboolean patlist_matches (GList *pattern_list, const gchar *s);

void gnome_cmd_toggle_file_name_selection (GtkWidget *entry);

inline void gnome_cmd_show_message (GtkWindow *parent, std::string message, const gchar *secondary_text=NULL)
{
    GtkWidget *dlg = gtk_message_dialog_new (parent,
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             message.c_str());

    if (secondary_text)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg), secondary_text);

    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
}


void gnome_cmd_error_message (const gchar *title, GError *error);


inline void gnome_cmd_help_display (const gchar *file_name, const gchar *link_id=NULL)
{
    GError *error = NULL;

    gnome_help_display (file_name, link_id, &error);

    if (error != NULL)
        gnome_cmd_error_message (_("There was an error displaying help."), error);
}


inline void gnome_cmd_error_message (const gchar *title, GError *error)
{
    gnome_cmd_show_message (NULL, title, error->message);
    g_error_free (error);
}


// Insert an item with an inline xpm icon and a user data pointer
#define GNOMEUIINFO_ITEM_FILENAME(label, tooltip, callback, filename) \
    { GNOME_APP_UI_ITEM, label, tooltip, (gpointer) callback, NULL, NULL, \
        GNOME_APP_PIXMAP_FILENAME, filename, 0, (GdkModifierType) 0, NULL }


inline std::string truncate(const std::string &s, guint n=100)
{
    if (s.size()<=n)
        return  s;

    return s.substr(0,n) + "...";
}


inline std::string &stringify(std::string &s, gchar *val)
{
    if (!val)  s.erase();  else
    {
        s = val;
        g_free (val);
    }

    return s;
}

template <typename T>
inline std::string &stringify(std::string &s, const T &val)
{
   std::ostringstream os;

   os << val;
   s = os.str();

   return s;
}

inline std::string stringify(gchar *val)
{
    std::string s;

    return val ? stringify(s, val) : s;
}

template <typename T>
inline std::string stringify(const T &val)
{
    std::string s;

    return stringify(s,val);
}

int split(const std::string &s, std::vector<std::string> &coll, const char *sep);

template <typename Iterator>
inline std::ostream &join(std::ostream &os, Iterator beg, Iterator end, const std::string sep=" ")
{
    if (beg==end)  return os;

    os << *beg++;

    while (beg!=end)
        os << sep << *beg++;

    return os;
}

// template <typename T, typename Allocator, template <typename, typename> class COLL>
// inline std::ostream &join(std::ostream &os, const COLL<T, Allocator> &coll, const std::string sep=" ")
// {
    // return join(os,coll.begin(),coll.end(),sep);
// }

template <typename T, typename Compare, typename Allocator, template <typename, typename, typename> class COLL>
inline std::ostream &join(std::ostream &os, const COLL<T, Compare, Allocator> &coll, const std::string sep=" ")
{
    return join(os,coll.begin(),coll.end(),sep);
}

template <typename Iterator>
inline std::string &join(std::string &s, Iterator beg, Iterator end, const std::string sep=" ")
{
    s.clear();

    if (beg==end)
        return s;

    s = *beg++;

    while (beg!=end)
    {
        s += sep;
        s += *beg++;
    }

    return s;
}

// template <typename T, typename Allocator, template <typename, typename> class COLL>
// inline std::string &join(std::string &s, const COLL<T, Allocator> &coll, const std::string sep=" ")
// {
    // return join(s,coll.begin(),coll.end(),sep);
// }

template <typename T, typename Compare, typename Allocator, template <typename, typename, typename> class COLL>
inline std::string &join(std::string &s, const COLL<T, Compare, Allocator> &coll, const std::string sep=" ")
{
    return join(s,coll.begin(),coll.end(),sep);
}

template <typename Iterator>
inline std::string join(Iterator beg, Iterator end, const std::string sep=" ")
{
    std::string s;

    return join(s,beg,end,sep);
}

// template <typename T, typename Allocator, template <typename, typename> class COLL>
// inline std::string join(const COLL<T, Allocator> &coll, const std::string sep=" ")
// {
    // std::string s;

    // return join(s,coll.begin(),coll.end(),sep);
// }

template <typename T, typename Compare, typename Allocator, template <typename, typename, typename> class COLL>
inline std::string join(const COLL<T, Compare, Allocator> &coll, const std::string sep=" ")
{
    std::string s;

    return join(s,coll.begin(),coll.end(),sep);
}

#endif // __UTILS_H__
