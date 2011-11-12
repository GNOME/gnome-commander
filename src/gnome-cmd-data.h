/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

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
#include "tuple.h"

struct GnomeCmdConRemote;

struct GnomeCmdData
{
    enum LeftMouseButtonMode
    {
        LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK,
        LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK
    };

    enum MiddleMouseButtonMode
    {
        MIDDLE_BUTTON_GOES_UP_DIR,
        MIDDLE_BUTTON_OPENS_NEW_TAB
    };

    enum RightMouseButtonMode
    {
        RIGHT_BUTTON_POPUPS_MENU,
        RIGHT_BUTTON_SELECTS
    };

    enum TabLockIndicator
    {
        TAB_LOCK_ICON,
        TAB_LOCK_ASTERISK,
        TAB_LOCK_STYLED_TEXT
    };

    enum {SEARCH_HISTORY_SIZE=10, ADVRENAME_HISTORY_SIZE=10, INTVIEWER_HISTORY_SIZE=16};

    struct FilterSettings
    {
        gboolean file_types[8];

        gboolean hidden;
        gboolean backup;

        FilterSettings()
        {
            memset(file_types, 0, sizeof(file_types));
            hidden = TRUE;
            backup = TRUE;
        }
    };

    struct Options
    {
        //  General
        LeftMouseButtonMode          left_mouse_button_mode;
        gboolean                     left_mouse_button_unselects;
        MiddleMouseButtonMode        middle_mouse_button_mode;
        RightMouseButtonMode         right_mouse_button_mode;
        gboolean                     case_sens_sort;
        gboolean                     alt_quick_search;
        gboolean                     quick_search_exact_match_begin;
        gboolean                     quick_search_exact_match_end;
        gboolean                     allow_multiple_instances;
        gboolean                     save_dirs_on_exit;
        gboolean                     save_tabs_on_exit;
        gboolean                     save_dir_history_on_exit;
        // Format
        GnomeCmdSizeDispMode         size_disp_mode;
        GnomeCmdPermDispMode         perm_disp_mode;
        GnomeCmdDateFormat           date_format;           // NOTE: internally stored as locale (which not always defaults to UTF8), needs converting from/to UTF8 for editing and cfg load/save
        //  Layout
        gchar                       *list_font;
        gint                         list_row_height;
        GnomeCmdExtDispMode          ext_disp_mode;
        GnomeCmdLayout               layout;
        GnomeCmdColorMode            color_mode;
        gboolean                     use_ls_colors;
        GnomeCmdLsColorsPalette      ls_colors_palette;
        guint                        icon_size;
        GdkInterpType                icon_scale_quality;
        gchar                       *theme_icon_dir;
        gchar                       *document_icon_dir;
        //  Tabs
        gboolean                     always_show_tabs;
        int                          tab_lock_indicator;
        //  Confirmation
        gboolean                     confirm_delete;
        GnomeCmdConfirmOverwriteMode confirm_copy_overwrite;
        GnomeCmdConfirmOverwriteMode confirm_move_overwrite;
        gboolean                     confirm_mouse_dnd;
        //  Filters
        FilterSettings               filter;
        gchar                       *backup_pattern;
        GList                       *backup_pattern_list;
        //  Programs
        gboolean                     honor_expect_uris;
        gchar                       *viewer;
        gboolean                     use_internal_viewer;
        gchar                       *editor;
        gchar                       *differ;
        gchar                       *term;
        GList                       *fav_apps;
        //  Devices
        gboolean                     device_only_icon;
        gboolean                     skip_mounting;

        Options(): left_mouse_button_mode(LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK),
                   left_mouse_button_unselects(TRUE),
                   middle_mouse_button_mode(MIDDLE_BUTTON_GOES_UP_DIR),
                   right_mouse_button_mode(RIGHT_BUTTON_POPUPS_MENU),
                   case_sens_sort(TRUE),
                   alt_quick_search(FALSE),
                   quick_search_exact_match_begin(TRUE),
                   quick_search_exact_match_end(FALSE),
                   allow_multiple_instances(FALSE),
                   save_dirs_on_exit(FALSE),
                   save_tabs_on_exit(TRUE),
                   save_dir_history_on_exit(TRUE),
                   size_disp_mode(GNOME_CMD_SIZE_DISP_MODE_POWERED),
                   perm_disp_mode(GNOME_CMD_PERM_DISP_MODE_TEXT),
                   date_format(NULL),
                   list_font(NULL),
                   list_row_height(16),
                   ext_disp_mode(GNOME_CMD_EXT_DISP_BOTH),
                   layout(GNOME_CMD_LAYOUT_MIME_ICONS),
                   color_mode(GNOME_CMD_COLOR_DEEP_BLUE),
                   use_ls_colors(FALSE),
                   icon_size(16),
                   icon_scale_quality(GDK_INTERP_HYPER),
                   theme_icon_dir(NULL),
                   document_icon_dir(NULL),
                   always_show_tabs(FALSE),
                   tab_lock_indicator(TAB_LOCK_ICON),
                   confirm_delete(TRUE),
                   confirm_copy_overwrite(GNOME_CMD_CONFIRM_OVERWRITE_QUERY),
                   confirm_move_overwrite(GNOME_CMD_CONFIRM_OVERWRITE_QUERY),
                   confirm_mouse_dnd(TRUE),
                   backup_pattern(NULL),
                   backup_pattern_list(NULL),
                   honor_expect_uris(FALSE),
                   viewer(NULL),
                   use_internal_viewer(TRUE),
                   editor(NULL),
                   differ(NULL),
                   term(NULL),
                   fav_apps(NULL),
                   device_only_icon(FALSE),
                   skip_mounting(FALSE)
        {
            memset(&ls_colors_palette, 0, sizeof(ls_colors_palette));
        }

        ~Options()
        {
            g_free (date_format);
            g_free (list_font);
            g_free (backup_pattern);
            patlist_free (backup_pattern_list);
            g_free (viewer);
            g_free (editor);
            g_free (differ);
            g_free (term);
        }

        void set_date_format(const GnomeCmdDateFormat format)
        {
            g_free (date_format);
            date_format = g_strdup (format);
        }

        void set_list_font(const gchar *font)
        {
            g_free (list_font);
            list_font = g_strdup (font);
        }

        void set_theme_icon_dir(const gchar *dir)
        {
            g_free (theme_icon_dir);
            theme_icon_dir = g_strdup (dir);
        }

        void set_document_icon_dir(const gchar *dir)
        {
            g_free (document_icon_dir);
            document_icon_dir = g_strdup (dir);
        }

        void set_backup_pattern(const gchar *value)
        {
            g_free (backup_pattern);
            patlist_free (backup_pattern_list);

            backup_pattern = g_strdup (value);
            backup_pattern_list = patlist_new (backup_pattern);
        }

        void set_viewer(const gchar *command)
        {
            g_free (viewer);
            viewer = g_strdup (command);
        }

        void set_editor(const gchar *command)
        {
            g_free (editor);
            editor = g_strdup (command);
        }

        void set_differ(const gchar *command)
        {
            g_free (differ);
            differ = g_strdup (command);
        }

        void set_term(const gchar *command)
        {
            g_free (term);
            term = g_strdup (command);
        }

        void add_fav_app(GnomeCmdApp *app)
        {
            g_return_if_fail (app != NULL);
            fav_apps = g_list_append (fav_apps, app);
        }

        void remove_fav_app(GnomeCmdApp *app)
        {
            g_return_if_fail (app != NULL);
            fav_apps = g_list_remove (fav_apps, app);
        }

        void set_fav_apps(GList *apps)
        {
            // FIXME:   free fav_apps
            fav_apps = apps;
        }
    };

    struct Selection
    {
        std::string name;
        std::string filename_pattern;
        Filter::Type syntax;
        int max_depth;
        std::string text_pattern;
        gboolean content_search;
        gboolean match_case;

        Selection(): syntax(Filter::TYPE_REGEX), max_depth(-1), content_search(FALSE), match_case(FALSE)       {}

        const std::string &description() const    {  return filename_pattern;  }
        void reset();

        friend XML::xstream &operator << (XML::xstream &xml, Selection &cfg);
    };

    struct SearchConfig
    {
        gint width, height;

        Selection default_profile;

        History name_patterns;
        History content_patterns;

        std::vector<Selection> &profiles;

        SearchConfig(std::vector<Selection> &selections): width(600), height(400), name_patterns(SEARCH_HISTORY_SIZE), content_patterns(SEARCH_HISTORY_SIZE), profiles(selections)   {  default_profile.name = "Default";  }

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

            Profile(): template_string("$N"),
                       counter_start(1), counter_width(1), counter_step(1),
                       case_conversion(0), trim_blanks(3)                     {}

            const std::string &description() const {  return template_string;  }
            void reset();
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

    typedef std::pair<std::string,triple<GnomeCmdFileList::ColumnID,GtkSortType,gboolean> > Tab;

    struct Private;

    Private *priv;

  private:

    GnomeCmdConRemote           *quick_connect;

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

    gboolean                     XML_cfg_has_connections;
    gboolean                     XML_cfg_has_bookmarks;

    Options                      options;

    std::vector<Selection>       selections;

    SearchConfig                 search_defaults;
    AdvrenameConfig              advrename_defaults;
    IntViewerConfig              intviewer_defaults;
    BookmarksConfig              bookmarks_defaults;

    gboolean                     list_orientation;

    gboolean                     toolbar_visibility;
    gboolean                     conbuttons_visibility;
    gboolean                     concombo_visibility;
    gboolean                     cmdline_visibility;
    gboolean                     buttonbar_visibility;

    guint                        dev_icon_size;
    guint                        fs_col_width[GnomeCmdFileList::NUM_COLUMNS];
    guint                        gui_update_rate;
    GtkReliefStyle               button_relief;

    GList                       *cmdline_history;
    gint                         cmdline_history_length;

    gboolean                     use_gcmd_block;

    gint                         main_win_width;
    gint                         main_win_height;
    GdkWindowState               main_win_state;

    std::map<guint,std::vector<Tab> > tabs;

    mode_t                       umask;

    GnomeCmdData();

    void free();                // FIXME: free() -> ~GnomeCmdData()

    void load();
    void load_more();
    void save();

    GnomeCmdConRemote *get_quick_connect() const       {  return quick_connect;                     }

    GnomeCmdFileList::ColumnID get_sort_col(FileSelectorID id) const;
    GtkSortType get_sort_direction(FileSelectorID id) const;
};

gpointer gnome_cmd_data_get_con_list ();

const gchar *gnome_cmd_data_get_ftp_anonymous_password ();
void gnome_cmd_data_set_ftp_anonymous_password (const gchar *pw);

GnomeCmdColorTheme *gnome_cmd_data_get_custom_color_theme ();
GnomeCmdColorTheme *gnome_cmd_data_get_current_color_theme ();

GList *gnome_cmd_data_get_auto_load_plugins ();
void gnome_cmd_data_set_auto_load_plugins (GList *plugins);

void gnome_cmd_data_get_main_win_pos (gint *x, gint *y);
void gnome_cmd_data_set_main_win_pos (gint x, gint y);

const gchar *gnome_cmd_data_get_symlink_prefix ();
void gnome_cmd_data_set_symlink_prefix (const gchar *value);


extern GnomeCmdData gnome_cmd_data;

extern gchar *start_dir_left;
extern gchar *start_dir_right;
extern gchar *config_dir;

extern DICT<guint> gdk_key_names;
extern DICT<guint> gdk_modifiers_names;

#endif // __GNOME_CMD_DATA_H__
