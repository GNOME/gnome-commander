/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2008 Piotr Eljasiak

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

#ifndef __GNOME_CMD_DATA_H__
#define __GNOME_CMD_DATA_H__

#include "gnome-cmd-app.h"
#include "gnome-cmd-types.h"
#include "gnome-cmd-file-list.h"
#include "filter.h"
#include "history.h"
#include "dict.h"

typedef struct _GnomeCmdConFtp GnomeCmdConFtp;

struct GnomeCmdData
{
    enum RightMouseButtonMode
    {
        RIGHT_BUTTON_POPUPS_MENU,
        RIGHT_BUTTON_SELECTS
    };

    enum {PATTERN_HISTORY_SIZE=10};

    struct SearchConfig
    {
        History name_patterns;
        History directories;
        History content_patterns;
        gboolean recursive;
        gboolean case_sens;
        gint width, height;

        SearchConfig(): name_patterns(PATTERN_HISTORY_SIZE),
                        directories(PATTERN_HISTORY_SIZE),
                        content_patterns(PATTERN_HISTORY_SIZE),
                        recursive(TRUE), case_sens(FALSE),
                        width(600), height(400)                 {}
    };

    struct AdvrenameConfig
    {
        History templates;
        GtkTreeModel *regexes;

        guint counter_start;
        gint counter_increment;
        guint counter_precision;
        gint width, height;

        AdvrenameConfig(): templates(10), regexes(NULL),
                           counter_start(1), counter_increment(1), counter_precision(1),          //  defaults for
                           width(600), height(400)                                            {}  //  advrename settings
        ~AdvrenameConfig()                         {  if (regexes)  g_object_unref (regexes);  }
    };

    struct FilterSettings
    {
        gboolean file_types[8];

        gboolean hidden;
        gboolean backup;
        gboolean other;
        gchar *other_value;
    };

    struct Private;

    Private *priv;

  private:

    GnomeCmdConFtp              *quick_connect;

    void load_auto_load_plugins();
    void load_cmdline_history();
    void load_local_bookmarks();
    void load_rename_history();
    void load_search_defaults();
    void load_smb_bookmarks();
    void save_auto_load_plugins();
    void save_cmdline_history();
    void save_local_bookmarks();
    void save_rename_history();
    void save_search_defaults();
    void save_smb_bookmarks();

  public:

    gboolean                     confirm_delete;
    GnomeCmdConfirmOverwriteMode confirm_copy_overwrite;
    GnomeCmdConfirmOverwriteMode confirm_move_overwrite;
    RightMouseButtonMode         right_mouse_button_mode;
    GnomeCmdColorMode            color_mode;
    gboolean                     alt_quick_search;

    Filter::Type                 filter_type;
    FilterSettings               filter_settings;

    SearchConfig                 search_defaults;
    AdvrenameConfig              advrename_defaults;

    gboolean                     case_sens_sort;
    GnomeCmdExtDispMode          ext_disp_mode;
    gboolean                     list_orientation;

    gboolean                     toolbar_visibility;
    gboolean                     conbuttons_visibility;
    gboolean                     concombo_visibility;
    gboolean                     cmdline_visibility;
    gboolean                     buttonbar_visibility;

    guint                        icon_size;
    guint                        dev_icon_size;
    gboolean                     device_only_icon;
    gint                         list_row_height;
    guint                        fs_col_width[GnomeCmdFileList::NUM_COLUMNS];
    guint                        gui_update_rate;
    GtkReliefStyle               button_relief;

    GList                       *cmdline_history;
    gint                         cmdline_history_length;

    gboolean                     use_gcmd_block;

    gboolean                     use_gnome_auth_manager;

    gint                         main_win_width;
    gint                         main_win_height;
    GdkWindowState               main_win_state;

    GnomeCmdData();

    void free();                // FIXME: free() -> ~GnomeCmdData()

    void load();
    void load_more();
    void save();

    gboolean hide_type(GnomeVFSFileType type)     {  return filter_settings.file_types[type];  }
    GnomeCmdConFtp *get_quick_connect()           {  return quick_connect;                     }
};

gpointer gnome_cmd_data_get_con_list ();

void gnome_cmd_data_add_fav_app (GnomeCmdApp *app);
void gnome_cmd_data_remove_fav_app (GnomeCmdApp *app);
GList *gnome_cmd_data_get_fav_apps ();
void gnome_cmd_data_set_fav_apps (GList *apps);

const gchar *gnome_cmd_data_get_ftp_anonymous_password ();
void gnome_cmd_data_set_ftp_anonymous_password (const gchar *pw);

GnomeCmdSizeDispMode gnome_cmd_data_get_size_disp_mode ();
void gnome_cmd_data_set_size_disp_mode (GnomeCmdSizeDispMode mode);

GnomeCmdPermDispMode gnome_cmd_data_get_perm_disp_mode ();
void gnome_cmd_data_set_perm_disp_mode (GnomeCmdPermDispMode mode);

GnomeCmdDateFormat gnome_cmd_data_get_date_format ();
void gnome_cmd_data_set_date_format (GnomeCmdDateFormat format);

GnomeCmdLayout gnome_cmd_data_get_layout ();
void gnome_cmd_data_set_layout (GnomeCmdLayout layout);

GnomeCmdColorTheme *gnome_cmd_data_get_custom_color_theme ();
GnomeCmdColorTheme *gnome_cmd_data_get_current_color_theme ();

void gnome_cmd_data_get_sort_params (GnomeCmdFileList *fl, gint &col, gboolean &direction);
void gnome_cmd_data_set_sort_params (GnomeCmdFileList *fl, gint col, gboolean direction);

const gchar *gnome_cmd_data_get_viewer ();
const gchar *gnome_cmd_data_get_editor ();
const gchar *gnome_cmd_data_get_differ ();

void gnome_cmd_data_set_viewer (const gchar *command);
void gnome_cmd_data_set_editor (const gchar *command);
void gnome_cmd_data_set_differ (const gchar *command);

const gchar *gnome_cmd_data_get_list_font ();
void gnome_cmd_data_set_list_font (const gchar *list_font);

const gchar *gnome_cmd_data_get_term ();
void gnome_cmd_data_set_term (const gchar *shell);

GdkInterpType gnome_cmd_data_get_icon_scale_quality ();
void gnome_cmd_data_set_icon_scale_quality (GdkInterpType quality);

const gchar *gnome_cmd_data_get_theme_icon_dir ();
void gnome_cmd_data_set_theme_icon_dir (const gchar *dir);

const gchar *gnome_cmd_data_get_document_icon_dir ();
void gnome_cmd_data_set_document_icon_dir (const gchar *dir);

gint gnome_cmd_data_get_bookmark_dialog_col_width (guint column);
void gnome_cmd_data_set_bookmark_dialog_col_width (guint column, gint width);

gint gnome_cmd_data_get_dir_cache_size ();
void gnome_cmd_data_set_dir_cache_size (gint size);

gboolean gnome_cmd_data_get_use_ls_colors ();
void gnome_cmd_data_set_use_ls_colors (gboolean value);

GnomeCmdBookmarkGroup *gnome_cmd_data_get_local_bookmarks ();
GList *gnome_cmd_data_get_bookmark_groups ();

gboolean gnome_cmd_data_get_honor_expect_uris ();
void gnome_cmd_data_set_honor_expect_uris (gboolean value);

gboolean gnome_cmd_data_get_use_internal_viewer ();
void gnome_cmd_data_set_use_internal_viewer (gboolean value);

gboolean gnome_cmd_data_get_skip_mounting ();
void gnome_cmd_data_set_skip_mounting (gboolean value);

const gchar *gnome_cmd_data_get_start_dir (gboolean fs);
void gnome_cmd_data_set_start_dir (gboolean fs, const gchar *start_dir);

const gchar *gnome_cmd_data_get_last_pattern ();
void gnome_cmd_data_set_last_pattern (const gchar *value);

GList *gnome_cmd_data_get_auto_load_plugins ();
void gnome_cmd_data_set_auto_load_plugins (GList *plugins);

void gnome_cmd_data_get_main_win_pos (gint *x, gint *y);
void gnome_cmd_data_set_main_win_pos (gint x, gint y);

const gchar *gnome_cmd_data_get_backup_pattern ();
void gnome_cmd_data_set_backup_pattern (const gchar *value);

GList *gnome_cmd_data_get_backup_pattern_list ();

const gchar *gnome_cmd_data_get_symlink_prefix ();
void gnome_cmd_data_set_symlink_prefix (const gchar *value);


extern GnomeCmdData gnome_cmd_data;

extern DICT<guint> gdk_key_names;
extern DICT<guint> gdk_modifiers_names;

#endif // __GNOME_CMD_DATA_H__
