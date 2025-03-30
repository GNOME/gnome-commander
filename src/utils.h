/**
 * @file utils.h
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
#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#include <stdio.h>

#include <config.h>

#include "gnome-cmd-file.h"
#include "gnome-cmd-types.h"

using namespace std;

#define GNOME_CMD_PERM_USER_READ  256 //r--------
#define GNOME_CMD_PERM_USER_WRITE 128 //-w-------
#define GNOME_CMD_PERM_USER_EXEC   64 //--x------
#define GNOME_CMD_PERM_GROUP_READ  32 //---r-----
#define GNOME_CMD_PERM_GROUP_WRITE 16 //----w----
#define GNOME_CMD_PERM_GROUP_EXEC   8 //-----x---
#define GNOME_CMD_PERM_OTHER_READ   4 //------r--
#define GNOME_CMD_PERM_OTHER_WRITE  2 //-------w-
#define GNOME_CMD_PERM_OTHER_EXEC   1 //--------x
#define GNOME_CMD_PERM_USER_ALL   448 //rwx------
#define GNOME_CMD_PERM_GROUP_ALL   56 //---rwx---
#define GNOME_CMD_PERM_OTHER_ALL    7 //------rwx

#define TRACE(s)  std::cout << __FILE__ "(" << __LINE__ << ") " << __PRETTY_FUNCTION__ << "\t" #s ": `" << (s) << "'" << std::endl

extern gchar *debug_flags;

inline gboolean DEBUG_ENABLED (gchar flag)
{
    return debug_flags ? strchr(debug_flags, flag) != 0 : FALSE;
}

void DEBUG (gchar flag, const gchar *fmt, ...);

void gnome_cmd_show_message (GtkWindow *parent, const gchar *message, const gchar *secondary_text=NULL);
void gnome_cmd_error_message (GtkWindow *parent, const gchar *message, GError *error);

const gchar *type2string (guint32 type, gchar *buf, guint max);
const gchar *perm2string (guint32 permissions, gchar *buf, guint max);
const gchar *perm2textstring (guint32 permissions, gchar *buf, guint max);
const gchar *perm2numstring (guint32 permissions, gchar *buf, guint max);
const gchar *size2string (guint64 size, GnomeCmdSizeDispMode size_disp_mode);
const gchar *time2string (GDateTime *gDateTime, const gchar *date_format);

inline GdkModifierType get_modifiers_state()
{
    GdkDisplay *display = gdk_display_get_default ();
    GdkSeat *seat = gdk_display_get_default_seat (display);
    GdkDevice *keyboard = gdk_seat_get_keyboard (seat);

    return gdk_device_get_modifier_state (keyboard);
}

inline gboolean state_is_blank (gint state)
{
    gboolean ret = (state & GDK_SHIFT_MASK) || (state & GDK_CONTROL_MASK) || (state & GDK_ALT_MASK);

    return !ret;
}

inline gboolean state_is_shift (gint state)
{
    return (state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && !(state & GDK_ALT_MASK);
}

inline gboolean state_is_ctrl (gint state)
{
    return !(state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && !(state & GDK_ALT_MASK);
}

inline gboolean state_is_alt (gint state)
{
    return !(state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && (state & GDK_ALT_MASK);
}

inline gboolean state_is_alt_shift (gint state)
{
    return (state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && (state & GDK_ALT_MASK);
}

inline gboolean state_is_ctrl_alt (gint state)
{
    return !(state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && (state & GDK_ALT_MASK);
}

inline gboolean state_is_ctrl_shift (gint state)
{
    return (state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && !(state & GDK_ALT_MASK);
}

inline gboolean state_is_ctrl_alt_shift (gint state)
{
    return (state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && (state & GDK_ALT_MASK);
}

GList *uri_strings_to_gfiles (gchar *data);

gchar *create_nice_size_str (guint64 size);

inline gchar *quote_if_needed (const gchar *in)
{
    return g_shell_quote (in);
}

gchar *unquote_if_needed (const gchar *in);

GList *string_history_add (GList *in, const gchar *value, guint maxsize);

GtkWidget *create_styled_button (const gchar *text);

void set_cursor_busy_for_widget (GtkWidget *widget);

void remove_temp_download_dir ();

gchar *unix_to_unc (const gchar *path);

gboolean is_dir_existing(const gchar *dpath);
gboolean create_dir (const gchar *dpath);

GList *patlist_new (const gchar *pattern_string);
void patlist_free (GList *pattern_list);
gboolean patlist_matches (GList *pattern_list, const gchar *s);

void gnome_cmd_toggle_file_name_selection (GtkWidget *entry);

void gnome_cmd_help_display (const gchar *file_name, const gchar *link_id=NULL);


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
std::string &stringify(std::string &s, const T &val)
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

gint get_string_pixel_size (const char *s, int len);

gchar* get_package_config_dir();

gchar *string_double_underscores (const gchar *string);

gboolean get_gfile_attribute_boolean(GFileInfo *gFileInfo, const char *attribute);

gboolean get_gfile_attribute_boolean(GFile *gFile, const char *attribute);

guint32 get_gfile_attribute_uint32(GFileInfo *gFileInfo, const char *attribute);

guint32 get_gfile_attribute_uint32(GFile *gFile, const char *attribute);

guint64 get_gfile_attribute_uint64(GFileInfo *gFileInfo, const char *attribute);

guint64 get_gfile_attribute_uint64(GFile *gFile, const char *attribute);

gchar *get_gfile_attribute_string(GFileInfo *gFileInfo, const char *attribute);

gchar *get_gfile_attribute_string(GFile *gFile, const char *attribute);
