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

#ifndef __UTILS_H__
#define __UTILS_H__

#include "gnome-cmd-file.h"
#include "gnome-cmd-types.h"
#include "gnome-cmd-pixmap.h"
#include "gnome-cmd-app.h"

gboolean DEBUG_ENABLED (gchar flag);
void DEBUG (gchar flag, const gchar *fmt, ...);
void warn_print (const gchar *fmt, ...);

void run_command (const gchar *command, gboolean term);
void run_command_indir (const gchar *command, const gchar *dir, gboolean term);

const char **convert_varargs_to_name_array (va_list args);
gint run_simple_dialog (GtkWidget *parent, gboolean ignore_close_box,
                        GtkMessageType msg_type,
                        const char *text, const char *title, gint def_response, ...);



gboolean string2int (const gchar *s, gint *i);
gboolean string2uint (const gchar *s, guint *i);
gboolean string2short (const gchar *s, gshort *sh);
gboolean string2ushort (const gchar *s, gushort *sh);
gboolean string2char (const gchar *s, gchar *c);
gboolean string2uchar (const gchar *s, guchar *c);
gboolean string2float (const gchar *s, gfloat *f);

gchar *int2string (gint i);

gchar *str_uri_basename (const gchar *uri);


void type2string (GnomeVFSFileType type, gchar *buf, guint max);
void name2string (gchar *filename, gchar *buf, guint max);
void perm2string (GnomeVFSFilePermissions p, gchar *buf, guint max);
void perm2textstring (GnomeVFSFilePermissions p, gchar *buf, guint max);
void perm2numstring (GnomeVFSFilePermissions p, gchar *buf, guint max);
const gchar *size2string (GnomeVFSFileSize size, GnomeCmdSizeDispMode size_disp_mode);
const gchar *time2string (time_t t, const gchar *date_format);

void mime_exec_single (GnomeCmdFile *finfo);
void mime_exec_multiple (GList *files, GnomeCmdApp *app);

void clear_event_key (GdkEventKey *event);
gboolean state_is_blank (gint state);
gboolean state_is_shift (gint state);
gboolean state_is_ctrl (gint state);
gboolean state_is_alt (gint state);
gboolean state_is_alt_shift (gint state);
gboolean state_is_ctrl_alt (gint state);
gboolean state_is_ctrl_shift (gint state);
gboolean state_is_ctrl_alt_shift (gint state);

GList *strings_to_uris (gchar *data);

GnomeVFSFileSize calc_tree_size (const GnomeVFSURI *dir_uri);
const gchar *create_nice_size_str (GnomeVFSFileSize size);
gchar* quote_if_needed (const gchar *in);
gchar* unquote_if_needed (const gchar *in);
void stop_kp (GtkObject *obj);

GList *string_history_add (GList *in, const gchar *value, gint maxsize);

GtkWidget *create_styled_button (const gchar *text);
GtkWidget *create_styled_pixmap_button (const gchar *text, GnomeCmdPixmap *pixmap);
void set_cursor_busy_for_widget (GtkWidget *widget);
void set_cursor_default_for_widget (GtkWidget *widget);
void set_cursor_busy (void);
void set_cursor_default (void);

GList *app_get_linked_libs (GnomeCmdFile *finfo);
gboolean app_needs_terminal (GnomeCmdFile *finfo);

gchar *get_temp_download_filepath (const gchar *fname);
void remove_temp_download_dir (void);

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

GList *patlist_new (const gchar *pattern_string);
void patlist_free (GList *pattern_list);
gboolean patlist_matches (GList *pattern_list, const gchar *s);

void gnome_cmd_error_message(const gchar *title, GError *error);
void gnome_cmd_help_display(const gchar *file_name, const gchar *link_id);

#endif //__UTILS_H__
