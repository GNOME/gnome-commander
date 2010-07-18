/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak

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

#include <vector>
#include <string>

#include "gnome-cmd-app.h"
#include "gnome-cmd-types.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-regex.h"
#include "gnome-cmd-xml-config.h"
#include "filter.h"
#include "history.h"
#include "dict.h"

struct GnomeCmdConFtp;

struct GnomeCmdData
{
    enum LeftMouseButtonMode
    {
        LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK,
        LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK
    };

    enum RightMouseButtonMode
    {
        RIGHT_BUTTON_POPUPS_MENU,
        RIGHT_BUTTON_SELECTS
    };

    enum {SEARCH_HISTORY_SIZE=10, ADVRENAME_HISTORY_SIZE=10, INTVIEWER_HISTORY_SIZE=16};

    struct Selection
    {
        std::string name;
        std::string filename_pattern;
        Filter::Type syntax;
        gboolean recursive;
        std::string text_pattern;
        gboolean match_case;

        void reset();

        Selection(): syntax(Filter::TYPE_REGEX),
                     recursive(TRUE),
                     match_case(FALSE)                          {}

        friend XML::xstream &operator << (XML::xstream &xml, Selection &cfg);
    };

    struct SearchConfig
    {
        gint width, height;

        Selection default_profile;

        History name_patterns;
        History directories;
        History content_patterns;

        SearchConfig(): width(600), height(400),
                        name_patterns(SEARCH_HISTORY_SIZE),
                        directories(SEARCH_HISTORY_SIZE),
                        content_patterns(SEARCH_HISTORY_SIZE)   {}

        friend XML::xstream &operator << (XML::xstream &xml, SearchConfig &cfg);
    };

    struct AdvrenameConfig
    {
        struct Profile
        {
            std::string name;
            std::string template_string;
            guint counter_start;
            guint counter_width;
            gint counter_step;

            std::vector<GnomeCmd::ReplacePattern> regexes;

            guint case_conversion;
            guint trim_blanks;

            void reset();

            Profile(): template_string("$N"),
                       counter_start(1), counter_width(1), counter_step(1),
                       case_conversion(0), trim_blanks(3)                     {}
        };

        gint width, height;

        Profile default_profile;
        std::vector<Profile> profiles;

        History templates;

        AdvrenameConfig(): width(600), height(400), templates(ADVRENAME_HISTORY_SIZE)   {}
        ~AdvrenameConfig()                                                              {}

        friend XML::xstream &operator << (XML::xstream &xml, AdvrenameConfig &cfg);
    };

    struct IntViewerConfig
    {
        History text_patterns;
        History hex_patterns;
        gboolean case_sensitive;
        guint search_mode;

        IntViewerConfig(): text_patterns(INTVIEWER_HISTORY_SIZE),
                           hex_patterns(INTVIEWER_HISTORY_SIZE),
                           case_sensitive(FALSE), search_mode(0)    {}
    };

    struct BookmarksConfig
    {
        gint width, height;

        BookmarksConfig(): width(400), height(250)   {}

        friend XML::xstream &operator << (XML::xstream &xml, BookmarksConfig &cfg);
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

    gchar *viewer;
    gchar *editor;
    gchar *differ;
    gchar *term;

    void load_auto_load_plugins();
    void load_cmdline_history();
    void load_local_bookmarks();
    void load_rename_history();
    void load_search_defaults();
    void load_intviewer_defaults();
    void load_smb_bookmarks();
    void save_auto_load_plugins();
    void save_cmdline_history();
    void save_intviewer_defaults();

  public:

    gboolean                     XML_cfg_has_bookmarks;

    gboolean                     confirm_delete;
    GnomeCmdConfirmOverwriteMode confirm_copy_overwrite;
    GnomeCmdConfirmOverwriteMode confirm_move_overwrite;
    LeftMouseButtonMode          left_mouse_button_mode;
    gboolean                     left_mouse_button_unselects;
    RightMouseButtonMode         right_mouse_button_mode;
    GnomeCmdColorMode            color_mode;
    GnomeCmdSizeDispMode         size_disp_mode;
    GnomeCmdPermDispMode         perm_disp_mode;
    gboolean                     alt_quick_search;

    Filter::Type                 filter_type;
    FilterSettings               filter_settings;

    std::vector<Selection>       selections;

    SearchConfig                 search_defaults;
    AdvrenameConfig              advrename_defaults;
    IntViewerConfig              intviewer_defaults;
    BookmarksConfig              bookmarks_defaults;

    gboolean                     case_sens_sort;
    GnomeCmdLayout               layout;
    GnomeCmdExtDispMode          ext_disp_mode;
    gboolean                     list_orientation;

    gboolean                     toolbar_visibility;
    gboolean                     conbuttons_visibility;
    gboolean                     concombo_visibility;
    gboolean                     cmdline_visibility;
    gboolean                     buttonbar_visibility;

    gboolean                     use_ls_colors;

    guint                        icon_size;
    guint                        dev_icon_size;
    gboolean                     device_only_icon;
    gint                         list_row_height;
    guint                        fs_col_width[GnomeCmdFileList::NUM_COLUMNS];
    GdkInterpType                icon_scale_quality;
    guint                        gui_update_rate;
    GtkReliefStyle               button_relief;

    GList                       *cmdline_history;
    gint                         cmdline_history_length;

    gboolean                     save_tabs_on_exit;

    gboolean                     allow_multiple_instances;
    gboolean                     use_internal_viewer;
    gboolean                     use_gcmd_block;
    gboolean                     use_gnome_auth_manager;

    gboolean                     honor_expect_uris;
    gboolean                     skip_mounting;

    gint                         main_win_width;
    gint                         main_win_height;
    GdkWindowState               main_win_state;

    GnomeCmdData();

    void free();                // FIXME: free() -> ~GnomeCmdData()

    void load();
    void load_more();
    void save();

    gboolean hide_type(GnomeVFSFileType type) const {  return filter_settings.file_types[type];  }
    GnomeCmdConFtp *get_quick_connect() const       {  return quick_connect;                     }

    GnomeCmdFileList::ColumnID get_sort_col(FileSelectorID id) const;
    GtkSortType get_sort_direction(FileSelectorID id) const;

    const gchar *get_viewer() const                 {  return viewer;                            }
    const gchar *get_editor() const                 {  return editor;                            }
    const gchar *get_differ() const                 {  return differ;                            }
    const gchar *get_term() const                   {  return term;                              }

    void set_viewer(const gchar *command);
    void set_editor(const gchar *command);
    void set_differ(const gchar *command);
    void set_term(const gchar *command);
};

gpointer gnome_cmd_data_get_con_list ();

void gnome_cmd_data_add_fav_app (GnomeCmdApp *app);
void gnome_cmd_data_remove_fav_app (GnomeCmdApp *app);
GList *gnome_cmd_data_get_fav_apps ();
void gnome_cmd_data_set_fav_apps (GList *apps);

const gchar *gnome_cmd_data_get_ftp_anonymous_password ();
void gnome_cmd_data_set_ftp_anonymous_password (const gchar *pw);

GnomeCmdDateFormat gnome_cmd_data_get_date_format ();
void gnome_cmd_data_set_date_format (GnomeCmdDateFormat format);

GnomeCmdColorTheme *gnome_cmd_data_get_custom_color_theme ();
GnomeCmdColorTheme *gnome_cmd_data_get_current_color_theme ();

GnomeCmdLsColorsPalette *gnome_cmd_data_get_ls_colors_palette ();

inline void GnomeCmdData::set_viewer(const gchar *command)
{
    g_free (viewer);
    viewer = g_strdup (command);
}

inline void GnomeCmdData::set_editor(const gchar *command)
{
    g_free (editor);
    editor = g_strdup (command);
}

inline void GnomeCmdData::set_differ(const gchar *command)
{
    g_free (differ);
    differ = g_strdup (command);
}

inline void GnomeCmdData::set_term(const gchar *command)
{
    g_free (term);
    term = g_strdup (command);
}

const gchar *gnome_cmd_data_get_list_font ();
void gnome_cmd_data_set_list_font (const gchar *list_font);

const gchar *gnome_cmd_data_get_theme_icon_dir ();
void gnome_cmd_data_set_theme_icon_dir (const gchar *dir);

const gchar *gnome_cmd_data_get_document_icon_dir ();
void gnome_cmd_data_set_document_icon_dir (const gchar *dir);

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
