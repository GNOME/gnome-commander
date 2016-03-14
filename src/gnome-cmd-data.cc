/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2016 Uwe Scholz

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
#include <libgnomevfs/gnome-vfs-volume.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>

#include <fstream>
#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-user-actions.h"
#include "utils.h"
#include "owner.h"
#include "dialogs/gnome-cmd-advrename-dialog.h"

using namespace std;


#define MAX_GUI_UPDATE_RATE 1000
#define MIN_GUI_UPDATE_RATE 10
#define DEFAULT_GUI_UPDATE_RATE 100

GnomeCmdData gnome_cmd_data;
GnomeVFSVolumeMonitor *monitor = NULL;

struct GnomeCmdData::Private
{
    GnomeCmdConList *con_list;
    GList           *auto_load_plugins;
    gint             sort_column[2];
    gboolean         sort_direction[2];
    gint             main_win_pos[2];
    gchar           *symlink_prefix;

    gchar           *ftp_anonymous_password;
};


DICT<guint> gdk_key_names(GDK_VoidSymbol);
DICT<guint> gdk_modifiers_names;


GnomeCmdData::Options::Options(const Options &cfg)
{
    copy (cfg.color_themes, cfg.color_themes+G_N_ELEMENTS(cfg.color_themes), color_themes);
    left_mouse_button_mode = cfg.left_mouse_button_mode;
    left_mouse_button_unselects = cfg.left_mouse_button_unselects;
    middle_mouse_button_mode = cfg.middle_mouse_button_mode;
    right_mouse_button_mode = cfg.right_mouse_button_mode;
    select_dirs = cfg.select_dirs;
    case_sens_sort = cfg.case_sens_sort;
    alt_quick_search = cfg.alt_quick_search;
    quick_search_exact_match_begin = cfg.quick_search_exact_match_begin;
    quick_search_exact_match_end = cfg.quick_search_exact_match_end;
    allow_multiple_instances = cfg.allow_multiple_instances;
    save_dirs_on_exit = cfg.save_dirs_on_exit;
    save_tabs_on_exit = cfg.save_tabs_on_exit;
    save_dir_history_on_exit = cfg.save_dir_history_on_exit;
    size_disp_mode = cfg.size_disp_mode;
    perm_disp_mode = cfg.perm_disp_mode;
    date_format = g_strdup (cfg.date_format);
    list_font = g_strdup (cfg.list_font);
    list_row_height = cfg.list_row_height;
    ext_disp_mode = cfg.ext_disp_mode;
    layout = cfg.layout;
    color_mode = cfg.color_mode;
    use_ls_colors = cfg.use_ls_colors;
    ls_colors_palette = cfg.ls_colors_palette;
    icon_size = cfg.icon_size;
    icon_scale_quality = cfg.icon_scale_quality;
    theme_icon_dir = cfg.theme_icon_dir;
    document_icon_dir = cfg.document_icon_dir;
    always_show_tabs = cfg.always_show_tabs;
    tab_lock_indicator = cfg.tab_lock_indicator;
    confirm_delete = cfg.confirm_delete;
    confirm_delete_default = cfg.confirm_delete_default;
    confirm_copy_overwrite = cfg.confirm_copy_overwrite;
    confirm_move_overwrite = cfg.confirm_move_overwrite;
    confirm_mouse_dnd = cfg.confirm_mouse_dnd;
    filter = cfg.filter;
    backup_pattern = g_strdup (cfg.backup_pattern);
    backup_pattern_list = patlist_new (cfg.backup_pattern);
    honor_expect_uris = cfg.honor_expect_uris;
    viewer = g_strdup (cfg.viewer);
    use_internal_viewer = cfg.use_internal_viewer;
    editor = g_strdup (cfg.editor);
    differ = g_strdup (cfg.differ);
    termopen = g_strdup (cfg.termopen);
    termexec = g_strdup (cfg.termexec);
    fav_apps = cfg.fav_apps;
    device_only_icon = cfg.device_only_icon;
    skip_mounting = cfg.skip_mounting;
}


GnomeCmdData::Options &GnomeCmdData::Options::operator = (const Options &cfg)
{
    if (this != &cfg)
    {
        this->~Options();       //  free allocated data

        copy (cfg.color_themes, cfg.color_themes+G_N_ELEMENTS(cfg.color_themes), color_themes);
        left_mouse_button_mode = cfg.left_mouse_button_mode;
        left_mouse_button_unselects = cfg.left_mouse_button_unselects;
        middle_mouse_button_mode = cfg.middle_mouse_button_mode;
        right_mouse_button_mode = cfg.right_mouse_button_mode;
        select_dirs = cfg.select_dirs;
        case_sens_sort = cfg.case_sens_sort;
        alt_quick_search = cfg.alt_quick_search;
        quick_search_exact_match_begin = cfg.quick_search_exact_match_begin;
        quick_search_exact_match_end = cfg.quick_search_exact_match_end;
        allow_multiple_instances = cfg.allow_multiple_instances;
        save_dirs_on_exit = cfg.save_dirs_on_exit;
        save_tabs_on_exit = cfg.save_tabs_on_exit;
        save_dir_history_on_exit = cfg.save_dir_history_on_exit;
        size_disp_mode = cfg.size_disp_mode;
        perm_disp_mode = cfg.perm_disp_mode;
        date_format = g_strdup (cfg.date_format);
        list_font = g_strdup (cfg.list_font);
        list_row_height = cfg.list_row_height;
        ext_disp_mode = cfg.ext_disp_mode;
        layout = cfg.layout;
        color_mode = cfg.color_mode;
        use_ls_colors = cfg.use_ls_colors;
        ls_colors_palette = cfg.ls_colors_palette;
        icon_size = cfg.icon_size;
        icon_scale_quality = cfg.icon_scale_quality;
        theme_icon_dir = cfg.theme_icon_dir;
        document_icon_dir = cfg.document_icon_dir;
        always_show_tabs = cfg.always_show_tabs;
        tab_lock_indicator = cfg.tab_lock_indicator;
        confirm_delete = cfg.confirm_delete;
        confirm_copy_overwrite = cfg.confirm_copy_overwrite;
        confirm_move_overwrite = cfg.confirm_move_overwrite;
        confirm_mouse_dnd = cfg.confirm_mouse_dnd;
        filter = cfg.filter;
        backup_pattern = g_strdup (cfg.backup_pattern);
        backup_pattern_list = patlist_new (cfg.backup_pattern);
        honor_expect_uris = cfg.honor_expect_uris;
        viewer = g_strdup (cfg.viewer);
        use_internal_viewer = cfg.use_internal_viewer;
        editor = g_strdup (cfg.editor);
        differ = g_strdup (cfg.differ);
        termopen = g_strdup (cfg.termopen);
        termexec = g_strdup (cfg.termexec);
        fav_apps = cfg.fav_apps;
        device_only_icon = cfg.device_only_icon;
        skip_mounting = cfg.skip_mounting;
    }

    return *this;
}


void GnomeCmdData::Selection::reset()
{
    name.clear();
    filename_pattern.clear();
    syntax = Filter::TYPE_REGEX;
    max_depth = -1;
    text_pattern.clear();
    content_search = FALSE;
    match_case = FALSE;
}


void GnomeCmdData::AdvrenameConfig::Profile::reset()
{
    name.clear();
    template_string = "$N";
    regexes.clear();
    counter_start = counter_step = 1;
    counter_width = 0;
    case_conversion = 0;
    trim_blanks = 3;
}


inline gint get_int (const gchar *path, int def)
{
    gboolean b = FALSE;
    gint value = gnome_config_get_int_with_default (path, &b);

    return b ? def : value;
}


inline XML::xstream &operator << (XML::xstream &xml, GnomeCmdBookmark &bookmark)
{
    xml << XML::tag("Bookmark") << XML::attr("name") << XML::escape(bookmark.name);
    xml << XML::attr("path") << XML::escape(bookmark.path) << XML::endtag();

    return xml;
}


inline void write(XML::xstream &xml, GnomeCmdCon *con, const gchar *name)
{
    if (!con)
        return;

    GList *bookmarks = gnome_cmd_con_get_bookmarks (con)->bookmarks;

    if (!bookmarks)
        return;

    xml << XML::tag("Group") << XML::attr("name") << name;

    if (GNOME_CMD_IS_CON_REMOTE (con))
        xml << XML::attr("remote") << TRUE;

    for (GList *i=bookmarks; i; i=i->next)
        xml << *(GnomeCmdBookmark *) i->data;

    xml << XML::endtag("Group");
}


inline void save_devices (const gchar *fname)
{
    gchar *path = config_dir ? g_build_filename (config_dir, fname, NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    FILE *fd = fopen (path, "w");

    if (fd)
    {
        for (GList *i = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list); i; i = i->next)
        {
            GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (i->data);
            if (device && !gnome_cmd_con_device_get_autovol (device))
            {
                gchar *alias = gnome_vfs_escape_string (gnome_cmd_con_device_get_alias (device));
                gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
                gchar *mountp = gnome_vfs_escape_string (gnome_cmd_con_device_get_mountp (device));
                gchar *icon_path = (gchar *) gnome_cmd_con_device_get_icon_path (device);

                if (device_fn && device_fn[0] != '\0')
                    device_fn = gnome_vfs_escape_string (device_fn);
                else
                    device_fn = g_strdup ("x");

                if (icon_path && icon_path[0] != '\0')
                    icon_path = gnome_vfs_escape_string (icon_path);
                else
                    icon_path = g_strdup ("x");

                fprintf (fd, "%s %s %s %s\n", alias, device_fn, mountp, icon_path);

                g_free (alias);
                g_free (device_fn);
                g_free (mountp);
                g_free (icon_path);
            }
        }

        fclose (fd);
    }
    else
        g_warning ("Failed to open the file %s for writing", path);

    g_free (path);
}


inline void save_fav_apps (const gchar *fname)
{
    gchar *path = config_dir ? g_build_filename (config_dir, fname, NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    FILE *fd = fopen (path, "w");

    if (fd)
    {
        for (GList *i = gnome_cmd_data.options.fav_apps; i; i = i->next)
        {
            GnomeCmdApp *app = (GnomeCmdApp *) i->data;
            if (app)
            {
                gchar *name = gnome_vfs_escape_string (gnome_cmd_app_get_name (app));
                gchar *cmd = gnome_vfs_escape_string (gnome_cmd_app_get_command (app));
                gchar *icon_path = gnome_vfs_escape_string (gnome_cmd_app_get_icon_path (app));
                gint target = gnome_cmd_app_get_target (app);
                gchar *pattern_string = gnome_vfs_escape_string (gnome_cmd_app_get_pattern_string (app));
                gint handles_uris = gnome_cmd_app_get_handles_uris (app);
                gint handles_multiple = gnome_cmd_app_get_handles_multiple (app);
                gint requires_terminal = gnome_cmd_app_get_requires_terminal (app);

                fprintf (fd, "%s\t%s\t%s\t%d\t%s\t%d\t%d\t%d\n",
                         name, cmd, icon_path,
                         target, pattern_string,
                         handles_uris, handles_multiple, requires_terminal);

                g_free (name);
                g_free (cmd);
                g_free (icon_path);
                g_free (pattern_string);
            }
        }

        fclose (fd);
    }
    else
        g_warning ("Failed to open the file %s for writing", path);

    g_free (path);
}


inline gboolean load_connections (const gchar *fname)
{
    guint prev_ftp_cons_no = g_list_length (gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list));

    gchar *path = config_dir ? g_build_filename (config_dir, fname, NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    FILE  *fd = fopen (path, "r");

    if (fd)
    {
        gchar line[1024];

        while (fgets (line, sizeof(line), fd) != NULL)
        {
            GnomeCmdConRemote *server = NULL;

            gchar *s = strchr (line, '\n');             // g_utf8_strchr (line, -1, '\n') ???

            if (s)
                *s = 0;

            switch (line[0])
            {
                case '\0':              // do not warn about empty lines
                case '#' :              // do not warn about comments
                    break;

                case 'U':               // format       U:<tab>alias<tab>uri
                    {
                        vector<string> a;

                        split(line, a, "\t");

                        if (a.size()!=3)
                        {
                            g_warning ("Invalid line in the '%s' file - skipping it...", path);
                            g_warning ("\t... %s", line);
                            break;
                        }

                        gchar *alias = gnome_vfs_unescape_string (a[1].c_str(), NULL);

                        if (gnome_cmd_data.priv->con_list->has_alias(alias))
                            g_warning ("%s: ignored duplicate entry: %s", path, alias);
                        else
                        {
                            server = gnome_cmd_con_remote_new (alias, a[2]);

                            if (!server)
                            {
                                g_warning ("Invalid URI in the '%s' file", path);
                                g_warning ("\t... %s", line);
                            }
                            else
                                gnome_cmd_data.priv->con_list->add(server);
                        }

                        g_free (alias);
                    }
                    break;

                case 'B':
                    if (server)
                    {
                        gchar name[256], path[256];
                        gint ret = sscanf (line, "B: %256s %256s\n", name, path);

                        if (ret == 2)
                            gnome_cmd_con_add_bookmark (GNOME_CMD_CON (server), gnome_vfs_unescape_string (name, NULL), gnome_vfs_unescape_string (path, NULL));
                    }
                    break;

                default:
                    g_warning ("Invalid line in the '%s' file - skipping it...", path);
                    g_warning ("\t... %s", line);
                    break;
            }
        }

        fclose (fd);
    }
    else
        if (errno != ENOENT)
            g_warning ("Failed to open the file %s for reading", path);

    g_free (path);

    if (!g_list_length (gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list)))
    {
        GnomeCmdConRemote *server = gnome_cmd_con_remote_new (_("GNOME Commander"), "ftp://anonymous@ftp.gnome.org/pub/GNOME/sources/gnome-commander/", GnomeCmdCon::NOT_REQUIRED);
        gnome_cmd_data.priv->con_list->add(server);
    }

    return fd!=NULL && g_list_length (gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list))>prev_ftp_cons_no;
}


inline gboolean vfs_is_uri_local (const char *uri)
{
    GnomeVFSURI *pURI = gnome_vfs_uri_new (uri);

    if (!pURI)
        return FALSE;

    gboolean b = gnome_vfs_uri_is_local (pURI);
    gnome_vfs_uri_unref (pURI);

    // make sure this is actually a local path (gnome treats "burn://" as local too, and we don't want that)
    if (g_ascii_strncasecmp (uri, "file:/", 6)!=0)
        b = FALSE;

    DEBUG('m',"uri (%s) is %slocal\n", uri, b?"":"NOT ");

    return b;
}


inline void remove_vfs_volume (GnomeVFSVolume *volume)
{
    char *path, *uri, *localpath;

    if (!gnome_vfs_volume_is_user_visible (volume))
        return;

    uri = gnome_vfs_volume_get_activation_uri (volume);
    if (!vfs_is_uri_local (uri))
    {
        g_free (uri);
        return;
    }

    path = gnome_vfs_volume_get_device_path (volume);
    localpath = gnome_vfs_get_local_path_from_uri (uri);

    for (GList *i = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list); i; i = i->next)
    {
        GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (i->data);
        if (device && gnome_cmd_con_device_get_autovol (device))
        {
            gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
            const gchar *mountp = gnome_cmd_con_device_get_mountp (device);

            if ((strcmp(device_fn, path)==0) && (strcmp(mountp, localpath)==0))
            {
                DEBUG('m',"Remove Volume:\ndevice_fn = %s\tmountp = %s\n",
                device_fn,mountp);
                gnome_cmd_data.priv->con_list->remove(device);
                break;
            }
        }
    }

    g_free (path);
    g_free (uri);
    g_free (localpath);
}


inline gboolean device_mount_point_exists (GnomeCmdConList *list, const gchar *mountpoint)
{
    gboolean rc = FALSE;

    for (GList *tmp = gnome_cmd_con_list_get_all_dev (list); tmp; tmp = tmp->next)
    {
        GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (tmp->data);
        if (device && !gnome_cmd_con_device_get_autovol (device))
        {
            gchar *mountp = gnome_vfs_escape_string (gnome_cmd_con_device_get_mountp (device));
            gchar *mountp2= gnome_vfs_unescape_string (mountp, NULL);

            rc = strcmp(mountp2, mountpoint)==0;

            g_free (mountp);
            g_free (mountp2);

            if (rc)
                break;
        }
    }

    return rc;
}


inline void add_vfs_volume (GnomeVFSVolume *volume)
{
    if (!gnome_vfs_volume_is_user_visible (volume))
        return;

    char *uri = gnome_vfs_volume_get_activation_uri (volume);

    if (!vfs_is_uri_local (uri))
    {
        g_free (uri);
        return;
    }

    char *path = gnome_vfs_volume_get_device_path (volume);
    char *icon = gnome_vfs_volume_get_icon (volume);
    char *name = gnome_vfs_volume_get_display_name (volume);
    GnomeVFSDrive *drive = gnome_vfs_volume_get_drive (volume);

    // Try to load the icon, using current theme
    const gchar *iconpath = NULL;
    GtkIconTheme *icontheme = gtk_icon_theme_get_default();
    if (icontheme)
    {
        GtkIconInfo *iconinfo = gtk_icon_theme_lookup_icon (icontheme, icon, 16, GTK_ICON_LOOKUP_USE_BUILTIN);
        // This returned string should not be free, see gtk documentation
        if (iconinfo)
            iconpath = gtk_icon_info_get_filename (iconinfo);
    }

    char *localpath = gnome_vfs_get_local_path_from_uri (uri);

    DEBUG('m',"name = %s\n", name);
    DEBUG('m',"path = %s\n", path);
    DEBUG('m',"uri = %s\n", uri);
    DEBUG('m',"local = %s\n", localpath);
    DEBUG('m',"icon = %s (full path = %s)\n", icon, iconpath);

    // Don't create a new device connect if one already exists. This can happen if the user manually added the same device in "Options|Devices" menu
    if (!device_mount_point_exists (gnome_cmd_data.priv->con_list, localpath))
    {
        GnomeCmdConDevice *ConDev = gnome_cmd_con_device_new (name, path?path:NULL, localpath, iconpath);
        gnome_cmd_con_device_set_autovol (ConDev, TRUE);
        gnome_cmd_con_device_set_vfs_volume (ConDev, volume);
        gnome_cmd_data.priv->con_list->add(ConDev);
    }
    else
        DEBUG('m', "Device for mountpoint(%s) already exists. AutoVolume not added\n", localpath);

    g_free (path);
    g_free (uri);
    g_free (icon);
    g_free (name);
    g_free (localpath);

    gnome_vfs_drive_unref (drive);
}

#if 0
inline void add_vfs_drive (GnomeVFSDrive *drive)
{
    if (!gnome_vfs_drive_is_user_visible (drive))
        return;

    char *uri = gnome_vfs_drive_get_activation_uri (drive);

    if (!vfs_is_uri_local (uri))
    {
        g_free (uri);
        return;
    }

    char *path = gnome_vfs_drive_get_device_path (drive);
    char *icon = gnome_vfs_drive_get_icon (drive);
    char *name = gnome_vfs_drive_get_display_name (drive);
    GnomeVFSVolume *volume = gnome_vfs_drive_get_mounted_volume (drive);

    char *localpath = gnome_vfs_get_local_path_from_uri (uri);

    DEBUG('m',"name = %s\tpath = %s\turi = %s\tlocal = %s\n",name,path,uri,localpath);

    GnomeCmdConDevice *ConDev = gnome_cmd_con_device_new (name, path, localpath, icon);

    gnome_cmd_con_device_set_autovol (ConDev, TRUE);

    gnome_cmd_data.priv->con_list->add(ConDev);

    g_free (path);
    g_free (uri);
    g_free (icon);
    g_free (name);
    g_free (localpath);

    gnome_vfs_volume_unref (volume);
}
#endif

static void volume_mounted (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSVolume *volume)
{
    add_vfs_volume (volume);
}


static void volume_unmounted (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSVolume *volume)
{
    remove_vfs_volume (volume);
}

#if 0
static void drive_connected (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSDrive *drive)
{
    add_vfs_drive (drive);
}

static void drive_disconnected (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSDrive *drive)
{
    // TODO: Remove from Drives combobox
}
#endif

inline void set_vfs_volume_monitor ()
{
    monitor = gnome_vfs_get_volume_monitor ();

    g_signal_connect (monitor, "volume-mounted", G_CALLBACK (volume_mounted), NULL);
    g_signal_connect (monitor, "volume-unmounted", G_CALLBACK (volume_unmounted), NULL);
#if 0
    g_signal_connect (monitor, "drive-connected", G_CALLBACK (drive_connected), NULL);
    g_signal_connect (monitor, "drive-disconnected", G_CALLBACK (drive_disconnected), NULL);
#endif
}


inline void load_vfs_auto_devices ()
{
    GnomeVFSVolumeMonitor *monitor = gnome_vfs_get_volume_monitor ();
    GList *volumes = gnome_vfs_volume_monitor_get_mounted_volumes (monitor);

    for (GList *l = volumes; l; l = l->next)
    {
        add_vfs_volume ((GnomeVFSVolume *) l->data);
        gnome_vfs_volume_unref ((GnomeVFSVolume *) l->data);
    }
    g_list_free (volumes);

#if 0
    GList *drives = gnome_vfs_volume_monitor_get_connected_drives (monitor);
    for (GList *l = drives; l; l = l->next)
    {
        add_vfs_drive (l->data);
        gnome_vfs_drive_unref (l->data);
    }
    g_list_free (drives);
#endif
}


inline void load_devices (const gchar *fname)
{
    gchar *path = config_dir ? g_build_filename (config_dir, fname, NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    FILE *fd = fopen (path, "r");

    if (fd)
    {
        int ret;
        gchar alias[256], device_fn[256], mountp[256], icon_path[256];

        do
        {
            ret = fscanf (fd, "%s %s %s %s\n", alias, device_fn, mountp, icon_path);

            if (ret == 4)
            {
                gchar *alias2      = gnome_vfs_unescape_string (alias, NULL);
                gchar *device_fn2  = NULL;
                gchar *mountp2     = gnome_vfs_unescape_string (mountp, NULL);
                gchar *icon_path2  = NULL;

                if (strcmp (device_fn, "x") != 0)
                    device_fn2  = gnome_vfs_unescape_string (device_fn, NULL);
                if (strcmp (icon_path, "x") != 0)
                    icon_path2  = gnome_vfs_unescape_string (icon_path, NULL);

                gnome_cmd_data.priv->con_list->add (gnome_cmd_con_device_new (alias2, device_fn2, mountp2, icon_path2));

                g_free (alias2);
                g_free (device_fn2);
                g_free (mountp2);
                g_free (icon_path2);
            }
        } while (ret == 4);

        fclose (fd);
    }
    else
        if (errno != ENOENT)
            g_warning ("Failed to open the file %s for reading", path);

    load_vfs_auto_devices ();

    g_free (path);
}


inline void load_fav_apps (const gchar *fname)
{
    gnome_cmd_data.options.fav_apps = NULL;
    gchar *path = config_dir ? g_build_filename (config_dir, fname, NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);

    ifstream f(path);
    string line;

    while (getline(f,line))
    {
        gchar **a = g_strsplit_set (line.c_str()," \t",-1);

        if (g_strv_length (a)==8)
        {
            guint target, handles_uris, handles_multiple, requires_terminal;

            if (string2uint (a[3], target) &&
                string2uint (a[5], handles_uris) &&
                string2uint (a[6], handles_multiple) &&
                string2uint (a[7], requires_terminal))
            {
                gchar *name      = gnome_vfs_unescape_string (a[0], NULL);
                gchar *cmd       = gnome_vfs_unescape_string (a[1], NULL);
                gchar *icon_path = gnome_vfs_unescape_string (a[2], NULL);
                gchar *pattern   = gnome_vfs_unescape_string (a[4], NULL);

                gnome_cmd_data.options.fav_apps = g_list_append (
                    gnome_cmd_data.options.fav_apps,
                    gnome_cmd_app_new_with_values (
                        name, cmd, icon_path, (AppTarget) target, pattern, handles_uris, handles_multiple, requires_terminal));

                g_free (name);
                g_free (cmd);
                g_free (icon_path);
                g_free (pattern);
            }
        }

        g_strfreev (a);
    }

    g_free (path);
}


inline void gnome_cmd_data_set_string_history (const gchar *format, GList *strings)
{
    gchar key[128];

    for (gint i=0; strings; strings=strings->next, ++i)
    {
        snprintf (key, sizeof (key), format, i);
        gnome_cmd_data_set_string (key, (gchar *) strings->data);
    }
}


inline void gnome_cmd_data_set_uint_array (const gchar *format, guint *array, gint length)
{
    for (gint i=0; i<length; i++)
    {
        gchar *name = g_strdup_printf (format, i);
        gnome_cmd_data_set_int (name, array[i]);
        g_free (name);
    }
}


inline void GnomeCmdData::save_cmdline_history()
{
    if (!cmdline_visibility)
        return;

    cmdline_history = gnome_cmd_cmdline_get_history (main_win->get_cmdline());

    gnome_cmd_data_set_string_history ("/cmdline-history/line%d", cmdline_history);
}


inline void GnomeCmdData::save_intviewer_defaults()
{
    gnome_cmd_data_set_string_history ("/internal_viewer/text_pattern%d", intviewer_defaults.text_patterns.ents);
    gnome_cmd_data_set_string_history ("/internal_viewer/hex_pattern%d", intviewer_defaults.hex_patterns.ents);
    gnome_cmd_data_set_bool ("/internal_viewer/case_sens", intviewer_defaults.case_sensitive);
    gnome_cmd_data_set_int ("/internal_viewer/last_mode", intviewer_defaults.search_mode);
}


inline void GnomeCmdData::save_auto_load_plugins()
{
    gnome_cmd_data_set_int ("/plugins/count", g_list_length (priv->auto_load_plugins));
    gnome_cmd_data_set_string_history ("/plugins/auto_load%d", priv->auto_load_plugins);
}


inline void load_uint_array (const gchar *format, guint *array, gint length)
{
    for (gint i=0; i<length; i++)
    {
        gchar *name = g_strdup_printf (format, i);
        array[i] = gnome_cmd_data_get_int (name, array[i]);
        g_free (name);
    }
}


inline GList *load_string_history (const gchar *format, gint size)
{
    GList *list = NULL;

    for (gint i=0; i<size || size==-1; ++i)
    {
        gchar *key = g_strdup_printf (format, i);
        gchar *value = gnome_cmd_data_get_string (key, NULL);
        g_free (key);
        if (!value)
            break;
        list = g_list_append (list, value);
    }

    return list;
}


inline void GnomeCmdData::load_cmdline_history()
{
    cmdline_history = load_string_history ("/cmdline-history/line%d", -1);
}


inline void GnomeCmdData::load_search_defaults()
{
    search_defaults.name_patterns = load_string_history ("/search-history/name_pattern%d", -1);
    search_defaults.content_patterns = load_string_history ("/search-history/content_pattern%d", -1);
    search_defaults.width = gnome_cmd_data_get_int ("/search-history/width", 640);
    search_defaults.height = gnome_cmd_data_get_int ("/search-history/height", 400);
    search_defaults.default_profile.max_depth = gnome_cmd_data_get_bool ("/search-history/recursive", TRUE) ? -1 : 0;
    search_defaults.default_profile.match_case = gnome_cmd_data_get_bool ("/search-history/case_sens", FALSE);
}


inline void GnomeCmdData::load_intviewer_defaults()
{
    intviewer_defaults.text_patterns = load_string_history ("/internal_viewer/text_pattern%d", -1);
    intviewer_defaults.hex_patterns.ents = load_string_history ("/internal_viewer/hex_pattern%d", -1);
    intviewer_defaults.case_sensitive = gnome_cmd_data_get_bool ("/internal_viewer/case_sens", FALSE);
    intviewer_defaults.search_mode = gnome_cmd_data_get_int ("/internal_viewer/last_mode", 0);
}


inline void GnomeCmdData::load_rename_history()
{
    gint size;
    GList *from=NULL, *to=NULL, *csens=NULL;

    advrename_defaults.width = gnome_cmd_data_get_int ("/advrename/width", 640);
    advrename_defaults.height = gnome_cmd_data_get_int ("/advrename/height", 400);

    size = gnome_cmd_data_get_int ("/template-history/size", 0);
    advrename_defaults.templates = load_string_history ("/template-history/template%d", size);

    advrename_defaults.default_profile.counter_start = gnome_cmd_data_get_int ("/advrename/counter_start", 1);
    advrename_defaults.default_profile.counter_width = gnome_cmd_data_get_int ("/advrename/counter_precision", 1);
    advrename_defaults.default_profile.counter_step = gnome_cmd_data_get_int ("/advrename/counter_increment", 1);

    size = gnome_cmd_data_get_int ("/rename-history/size", 0);

    GList *tmp_from = from = load_string_history ("/rename-history/from%d", size);
    GList *tmp_to = to = load_string_history ("/rename-history/to%d", size);
    GList *tmp_csens = csens = load_string_history ("/rename-history/csens%d", size);

    for (; tmp_from && size > 0; --size)
    {
        advrename_defaults.default_profile.regexes.push_back(GnomeCmd::ReplacePattern((gchar *) tmp_from->data,
                                                                                      (gchar *) tmp_to->data,
                                                                                      *((gchar *) tmp_csens->data)=='T'));
        tmp_from = tmp_from->next;
        tmp_to = tmp_to->next;
        tmp_csens = tmp_csens->next;
    }

    g_list_free (from);
    g_list_free (to);
    g_list_free (csens);
}


inline void GnomeCmdData::load_local_bookmarks()
{
    gint size = gnome_cmd_data_get_int ("/local_bookmarks/count", 0);
    GList *names = load_string_history ("/local_bookmarks/name%d", size);
    GList *paths = load_string_history ("/local_bookmarks/path%d", size);

    GnomeCmdCon *con = priv->con_list->get_home();

    for (gint i=0; i<size; i++)
        gnome_cmd_con_add_bookmark (con, (gchar *) g_list_nth_data (names, i), (gchar *) g_list_nth_data (paths, i));
}


inline void GnomeCmdData::load_smb_bookmarks()
{
    gint size = gnome_cmd_data_get_int ("/smb_bookmarks/count", 0);
    GList *names = load_string_history ("/smb_bookmarks/name%d", size);
    GList *paths = load_string_history ("/smb_bookmarks/path%d", size);

    GnomeCmdCon *con = priv->con_list->get_smb();

    for (gint i=0; i<size; i++)
        gnome_cmd_con_add_bookmark (con, (gchar *) g_list_nth_data (names, i), (gchar *) g_list_nth_data (paths, i));
}


inline void GnomeCmdData::load_auto_load_plugins()
{
    gint count = gnome_cmd_data_get_int ("/plugins/count", 0);

    priv->auto_load_plugins = load_string_history ("/plugins/auto_load%d", count);
}


GnomeCmdData::GnomeCmdData(): search_defaults(selections)
{
    quick_connect = NULL;

    XML_cfg_has_connections = FALSE;
    XML_cfg_has_bookmarks = FALSE;

    list_orientation = FALSE;

    toolbar_visibility = TRUE;
    conbuttons_visibility = TRUE;
    concombo_visibility = TRUE;
    cmdline_visibility = TRUE;
    buttonbar_visibility = TRUE;

    dev_icon_size = 16;
    memset(fs_col_width, 0, sizeof(fs_col_width));
    gui_update_rate = DEFAULT_GUI_UPDATE_RATE;
    button_relief = GTK_RELIEF_NONE;

    cmdline_history = NULL;
    cmdline_history_length = 0;

    use_gcmd_block = FALSE;

    main_win_width = 600;
    main_win_height = 400;

    main_win_state = GDK_WINDOW_STATE_MAXIMIZED;

    umask = ::umask(0);
    ::umask(umask);
}


void GnomeCmdData::free()
{
    if (priv)
    {
        // free the connections
        // g_object_unref (priv->con_list);

        // close quick connect
        if (quick_connect)
        {
            gnome_cmd_con_close (GNOME_CMD_CON (quick_connect));
            // gtk_object_destroy (GTK_OBJECT (quick_connect));
        }

        // free the anonymous password string
        g_free (priv->ftp_anonymous_password);

        g_free (priv);
    }
}


void GnomeCmdData::load()
{
    gchar *xml_cfg_path = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);

    gchar *document_icon_dir = g_strconcat (GNOME_PREFIX, "/share/pixmaps/document-icons/", NULL);
    gchar *theme_icon_dir    = g_strconcat (PIXMAPS_DIR, "/mime-icons", NULL);

    priv = g_new0 (Private, 1);

    options.color_themes[GNOME_CMD_COLOR_CUSTOM].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg = gdk_color_new (0xffff,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    options.color_themes[GNOME_CMD_COLOR_MODERN].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_MODERN].norm_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_MODERN].norm_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    options.color_themes[GNOME_CMD_COLOR_MODERN].alt_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_MODERN].alt_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    options.color_themes[GNOME_CMD_COLOR_MODERN].sel_fg = gdk_color_new (0xffff,0,0);
    options.color_themes[GNOME_CMD_COLOR_MODERN].sel_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    options.color_themes[GNOME_CMD_COLOR_MODERN].curs_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_MODERN].curs_bg = gdk_color_new (0,0,0x4444);

    options.color_themes[GNOME_CMD_COLOR_FUSION].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_FUSION].norm_fg = gdk_color_new (0x8080,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_FUSION].norm_bg = gdk_color_new (0,0x4040,0x8080);
    options.color_themes[GNOME_CMD_COLOR_FUSION].alt_fg = gdk_color_new (0x8080,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_FUSION].alt_bg = gdk_color_new (0,0x4040,0x8080);
    options.color_themes[GNOME_CMD_COLOR_FUSION].sel_fg = gdk_color_new (0xffff,0xffff,0);
    options.color_themes[GNOME_CMD_COLOR_FUSION].sel_bg = gdk_color_new (0,0x4040,0x8080);
    options.color_themes[GNOME_CMD_COLOR_FUSION].curs_fg = gdk_color_new (0,0,0x8080);
    options.color_themes[GNOME_CMD_COLOR_FUSION].curs_bg = gdk_color_new (0,0x8080,0x8080);

    options.color_themes[GNOME_CMD_COLOR_CLASSIC].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].norm_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].norm_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].alt_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].alt_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].sel_fg = gdk_color_new (0xffff,0xffff,0);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].sel_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].norm_fg = gdk_color_new (0,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].norm_bg = gdk_color_new (0,0,0x8080);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].alt_fg = gdk_color_new (0,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].alt_bg = gdk_color_new (0,0,0x8080);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].sel_fg = gdk_color_new (0xffff,0xffff,0);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].sel_bg = gdk_color_new (0,0,0x8080);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].norm_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].norm_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].alt_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].alt_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].sel_fg = gdk_color_new (0xffff,0xcfcf,0x3636);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].sel_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].curs_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].curs_bg = gdk_color_new (0x4d4d,0x4d4d,0x4d4d);

    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].norm_fg = gdk_color_new (0xffff,0xc644,0);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].norm_bg = gdk_color_new (0x1919,0x2e2e,0);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].alt_fg = gdk_color_new (0xffff,0xc6c6,0);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].alt_bg = gdk_color_new (0x1f1f,0x3939,0x101);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].sel_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].sel_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    options.color_themes[GNOME_CMD_COLOR_NONE].respect_theme = TRUE;
    options.color_themes[GNOME_CMD_COLOR_NONE].norm_fg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].norm_bg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].alt_fg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].alt_bg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].sel_fg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].sel_bg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].curs_fg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].curs_bg = NULL;

    options.size_disp_mode = (GnomeCmdSizeDispMode) gnome_cmd_data_get_int ("/options/size_disp_mode", GNOME_CMD_SIZE_DISP_MODE_POWERED);
    options.perm_disp_mode = (GnomeCmdPermDispMode) gnome_cmd_data_get_int ("/options/perm_disp_mode", GNOME_CMD_PERM_DISP_MODE_TEXT);

#ifdef HAVE_LOCALE_H
    gchar *utf8_date_format = gnome_cmd_data_get_string ("/options/date_disp_mode", "%x %R");
#else
    gchar *utf8_date_format = gnome_cmd_data_get_string ("/options/date_disp_mode", "%D %R");
#endif
    options.date_format = g_locale_from_utf8 (utf8_date_format, -1, NULL, NULL, NULL);
    g_free (utf8_date_format);

    options.layout = (GnomeCmdLayout) gnome_cmd_data_get_int ("/options/layout", GNOME_CMD_LAYOUT_MIME_ICONS);

    options.list_row_height = gnome_cmd_data_get_int ("/options/list_row_height", 16);

    options.confirm_delete = gnome_cmd_data_get_bool ("/confirm/delete", TRUE);
    options.confirm_delete_default = (GtkButtonsType) gnome_cmd_data_get_int ("/confirm/delete_default", GTK_BUTTONS_OK);
    options.confirm_copy_overwrite = (GnomeCmdConfirmOverwriteMode) gnome_cmd_data_get_int ("/confirm/copy_overwrite", GNOME_CMD_CONFIRM_OVERWRITE_QUERY);
    options.confirm_move_overwrite = (GnomeCmdConfirmOverwriteMode) gnome_cmd_data_get_int ("/confirm/move_overwrite", GNOME_CMD_CONFIRM_OVERWRITE_QUERY);
    options.confirm_mouse_dnd = gnome_cmd_data_get_bool ("/confirm/confirm_mouse_dnd", TRUE);

    options.filter.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN] = gnome_cmd_data_get_bool ("/options/show_unknown", FALSE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_REGULAR] = gnome_cmd_data_get_bool ("/options/show_regular", FALSE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY] = gnome_cmd_data_get_bool ("/options/show_directory", FALSE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_FIFO] = gnome_cmd_data_get_bool ("/options/show_fifo", FALSE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_SOCKET] = gnome_cmd_data_get_bool ("/options/show_socket", FALSE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE] = gnome_cmd_data_get_bool ("/options/show_char_device", FALSE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE] = gnome_cmd_data_get_bool ("/options/show_block_device", FALSE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK] = gnome_cmd_data_get_bool ("/options/show_symbolic_link", FALSE);
    options.filter.hidden = gnome_cmd_data_get_bool ("/options/hidden_filter", TRUE);
    options.filter.backup = gnome_cmd_data_get_bool ("/options/backup_filter", TRUE);

    options.select_dirs = gnome_cmd_data_get_bool ("/sort/select_dirs", TRUE);
    options.case_sens_sort = gnome_cmd_data_get_bool ("/sort/case_sensitive", TRUE);

    main_win_width = get_int ("/gnome-commander-size/main_win/width", 600);
    main_win_height = get_int ("/gnome-commander-size/main_win/height", 400);

    for (gint i=0; i<GnomeCmdFileList::NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/fs_col_width%d", i);
        fs_col_width[i] = get_int (tmp, GnomeCmdFileList::get_column_default_width((GnomeCmdFileList::ColumnID) i));
        g_free (tmp);
    }

    options.color_mode = (GnomeCmdColorMode) gnome_cmd_data_get_int ("/colors/mode", gcmd_owner.is_root() ? GNOME_CMD_COLOR_GREEN_TIGER :
                                                                                                            GNOME_CMD_COLOR_DEEP_BLUE);

    gnome_cmd_data_get_color ("/colors/norm_fg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg);
    gnome_cmd_data_get_color ("/colors/norm_bg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg);
    gnome_cmd_data_get_color ("/colors/alt_fg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg);
    gnome_cmd_data_get_color ("/colors/alt_bg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg);
    gnome_cmd_data_get_color ("/colors/sel_fg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg);
    gnome_cmd_data_get_color ("/colors/sel_bg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg);
    gnome_cmd_data_get_color ("/colors/curs_fg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg);
    gnome_cmd_data_get_color ("/colors/curs_bg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg);

    options.use_ls_colors = gnome_cmd_data_get_bool ("/colors/use_ls_colors", FALSE);

    options.ls_colors_palette.black_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.black_bg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.red_fg = gdk_color_new (0xffff, 0, 0);
    options.ls_colors_palette.red_bg = gdk_color_new (0xffff, 0, 0);
    options.ls_colors_palette.green_fg = gdk_color_new (0, 0xffff, 0);
    options.ls_colors_palette.green_bg = gdk_color_new (0, 0xffff, 0);
    options.ls_colors_palette.yellow_fg = gdk_color_new (0xffff, 0xffff, 0);
    options.ls_colors_palette.yellow_bg = gdk_color_new (0xffff, 0xffff, 0);
    options.ls_colors_palette.blue_fg = gdk_color_new (0, 0, 0xffff);
    options.ls_colors_palette.blue_bg = gdk_color_new (0, 0, 0xffff);
    options.ls_colors_palette.magenta_fg = gdk_color_new (0xffff, 0, 0xffff);
    options.ls_colors_palette.magenta_bg = gdk_color_new (0xffff, 0, 0xffff);
    options.ls_colors_palette.cyan_fg = gdk_color_new (0, 0xffff, 0xffff);
    options.ls_colors_palette.cyan_bg = gdk_color_new (0, 0xffff, 0xffff);
    options.ls_colors_palette.white_fg = gdk_color_new (0xffff, 0xffff, 0xffff);
    options.ls_colors_palette.white_bg = gdk_color_new (0xffff, 0xffff, 0xffff);

    options.list_font = gnome_cmd_data_get_string ("/options/list_font", "-misc-fixed-medium-r-normal-*-10-*-*-*-c-*-iso8859-1");

    options.ext_disp_mode = (GnomeCmdExtDispMode) gnome_cmd_data_get_int ("/options/ext_disp_mode", GNOME_CMD_EXT_DISP_BOTH);
    options.left_mouse_button_mode = (LeftMouseButtonMode) gnome_cmd_data_get_int ("/options/left_mouse_button_mode", LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK);
    options.left_mouse_button_unselects = gnome_cmd_data_get_bool ("/options/left_mouse_button_unselects", TRUE);
    options.middle_mouse_button_mode = (MiddleMouseButtonMode) gnome_cmd_data_get_int ("/options/middle_mouse_button_mode", MIDDLE_BUTTON_GOES_UP_DIR);
    options.right_mouse_button_mode = (RightMouseButtonMode) gnome_cmd_data_get_int ("/options/right_mouse_button_mode", RIGHT_BUTTON_POPUPS_MENU);
    options.icon_size = gnome_cmd_data_get_int ("/options/icon_size", 16);
    dev_icon_size = gnome_cmd_data_get_int ("/options/dev_icon_size", 16);
    options.icon_scale_quality = (GdkInterpType) gnome_cmd_data_get_int ("/options/icon_scale_quality", GDK_INTERP_HYPER);
    options.theme_icon_dir = gnome_cmd_data_get_string ("/options/theme_icon_dir", theme_icon_dir);
    g_free (theme_icon_dir);
    options.document_icon_dir = gnome_cmd_data_get_string ("/options/document_icon_dir", document_icon_dir);
    g_free (document_icon_dir);
    cmdline_history_length = gnome_cmd_data_get_int ("/options/cmdline_history_length", 16);
    button_relief = (GtkReliefStyle) gnome_cmd_data_get_int ("/options/btn_relief", GTK_RELIEF_NONE);
    list_orientation = gnome_cmd_data_get_bool ("/options/list_orientation", FALSE);
    gui_update_rate = gnome_cmd_data_get_int ("/options/gui_update_rate", DEFAULT_GUI_UPDATE_RATE);
    priv->main_win_pos[0] = gnome_cmd_data_get_int ("/options/main_win_pos_x", -1);
    priv->main_win_pos[1] = gnome_cmd_data_get_int ("/options/main_win_pos_y", -1);

    toolbar_visibility = gnome_cmd_data_get_bool ("/programs/toolbar_visibility", TRUE);
    conbuttons_visibility = gnome_cmd_data_get_bool ("/options/conbuttons_visibility", TRUE);
    concombo_visibility = gnome_cmd_data_get_bool ("/options/con_list_visibility", TRUE);
    cmdline_visibility = gnome_cmd_data_get_bool ("/options/cmdline_visibility", TRUE);
    buttonbar_visibility = gnome_cmd_data_get_bool ("/programs/buttonbar_visibility", TRUE);

    if (gui_update_rate < MIN_GUI_UPDATE_RATE)
        gui_update_rate = MIN_GUI_UPDATE_RATE;
    if (gui_update_rate > MAX_GUI_UPDATE_RATE)
        gui_update_rate = MAX_GUI_UPDATE_RATE;

    options.honor_expect_uris = gnome_cmd_data_get_bool ("/programs/honor_expect_uris", FALSE);
    options.allow_multiple_instances = gnome_cmd_data_get_bool ("/programs/allow_multiple_instances", FALSE);
    options.use_internal_viewer = gnome_cmd_data_get_bool ("/programs/use_internal_viewer", TRUE);
    options.alt_quick_search = gnome_cmd_data_get_bool ("/programs/alt_quick_search", FALSE);
    options.quick_search_exact_match_begin = gnome_cmd_data_get_bool ("/programs/quick_search_exact_match_begin", TRUE);
    options.quick_search_exact_match_end = gnome_cmd_data_get_bool ("/programs/quick_search_exact_match_end", FALSE);
    options.skip_mounting = gnome_cmd_data_get_bool ("/programs/skip_mounting", FALSE);

    priv->symlink_prefix = gnome_cmd_data_get_string ("/options/symlink_prefix", _("link to %s"));
    if (!*priv->symlink_prefix || strcmp(priv->symlink_prefix, _("link to %s"))==0)
    {
        g_free (priv->symlink_prefix);
        priv->symlink_prefix = NULL;
    }

    options.viewer = gnome_cmd_data_get_string ("/programs/viewer", "gedit %s");
    options.editor = gnome_cmd_data_get_string ("/programs/editor", "gedit %s");
    options.differ = gnome_cmd_data_get_string ("/programs/differ", "meld %s");
    options.termopen = gnome_cmd_data_get_string ("/programs/terminal_open", "xterm -hold");
    options.termexec = gnome_cmd_data_get_string ("/programs/terminal_exec", "xterm -hold -e %s");

    use_gcmd_block = gnome_cmd_data_get_bool ("/programs/use_gcmd_block", FALSE);

    options.device_only_icon = gnome_cmd_data_get_bool ("/devices/only_icon", FALSE);

    gnome_cmd_data_get_color ("/colors/ls_colors_black_fg", options.ls_colors_palette.black_fg);
    gnome_cmd_data_get_color ("/colors/ls_colors_black_bg", options.ls_colors_palette.black_bg);
    gnome_cmd_data_get_color ("/colors/ls_colors_red_fg", options.ls_colors_palette.red_fg);
    gnome_cmd_data_get_color ("/colors/ls_colors_red_bg", options.ls_colors_palette.red_bg);
    gnome_cmd_data_get_color ("/colors/ls_colors_green_fg", options.ls_colors_palette.green_fg);
    gnome_cmd_data_get_color ("/colors/ls_colors_green_bg", options.ls_colors_palette.green_bg);
    gnome_cmd_data_get_color ("/colors/ls_colors_yellow_fg", options.ls_colors_palette.yellow_fg);
    gnome_cmd_data_get_color ("/colors/ls_colors_yellow_bg", options.ls_colors_palette.yellow_bg);
    gnome_cmd_data_get_color ("/colors/ls_colors_blue_fg", options.ls_colors_palette.blue_fg);
    gnome_cmd_data_get_color ("/colors/ls_colors_blue_bg", options.ls_colors_palette.blue_bg);
    gnome_cmd_data_get_color ("/colors/ls_colors_magenta_fg", options.ls_colors_palette.magenta_fg);
    gnome_cmd_data_get_color ("/colors/ls_colors_magenta_bg", options.ls_colors_palette.magenta_bg);
    gnome_cmd_data_get_color ("/colors/ls_colors_cyan_fg", options.ls_colors_palette.cyan_fg);
    gnome_cmd_data_get_color ("/colors/ls_colors_cyan_bg", options.ls_colors_palette.cyan_bg);
    gnome_cmd_data_get_color ("/colors/ls_colors_white_fg", options.ls_colors_palette.white_fg);
    gnome_cmd_data_get_color ("/colors/ls_colors_white_bg", options.ls_colors_palette.white_bg);

    options.save_dirs_on_exit = gnome_cmd_data_get_bool ("/options/save_dirs_on_exit", TRUE);
    options.save_tabs_on_exit = gnome_cmd_data_get_bool ("/options/save_tabs_on_exit", TRUE);
    options.save_dir_history_on_exit = gnome_cmd_data_get_bool ("/options/save_dir_history_on_exit", TRUE);

    options.always_show_tabs = gnome_cmd_data_get_bool ("/options/always_show_tabs", FALSE);
    options.tab_lock_indicator = (TabLockIndicator) gnome_cmd_data_get_int ("/options/tab_lock_indicator", TAB_LOCK_ICON);

    options.backup_pattern = gnome_cmd_data_get_string ("/defaults/backup_pattern", "*~;*.bak");
    options.backup_pattern_list = patlist_new (options.backup_pattern);

    main_win_state = (GdkWindowState) gnome_cmd_data_get_int ("/options/main_win_state", (gint) GDK_WINDOW_STATE_MAXIMIZED);

    priv->ftp_anonymous_password = gnome_cmd_data_get_string ("/network/ftp_anonymous_password", "you@provider.com");

    if (strcmp (priv->ftp_anonymous_password, "you@provider.com")==0)   // if '/network/ftp_anonymous_password' entry undefined, try to read '/ftp/anonymous_password'
    {
        g_free (priv->ftp_anonymous_password);
        priv->ftp_anonymous_password = gnome_cmd_data_get_string ("/ftp/anonymous_password", "you@provider.com");
    }

    static struct
    {
        guint code;
        const gchar *name;
    }
    gdk_key_names_data[] = {
                            {GDK_ampersand, "ampersand"},
                            {GDK_apostrophe, "apostrophe"},
                            {GDK_asciicircum, "asciicircum"},
                            {GDK_asciitilde, "asciitilde"},
                            {GDK_asterisk, "asterisk"},
                            {GDK_at, "at"},
                            {GDK_backslash, "backslash"},
                            {GDK_bar, "bar"},
                            {GDK_braceleft, "braceleft"},
                            {GDK_braceright, "braceright"},
                            {GDK_bracketleft, "bracketleft"},
                            {GDK_bracketright, "bracketright"},
                            {GDK_colon, "colon"},
                            {GDK_comma, "comma"},
                            {GDK_dollar, "dollar"},
                            {GDK_equal, "equal"},
                            {GDK_exclam, "exclam"},
                            {GDK_greater, "greater"},
                            {GDK_grave, "grave"},
                            {GDK_less, "less"},
                            {GDK_minus, "minus"},
                            {GDK_numbersign, "numbersign"},
                            {GDK_parenleft, "parenleft"},
                            {GDK_parenright, "parenright"},
                            {GDK_percent, "percent"},
                            {GDK_period, "period"},
                            {GDK_plus, "plus"},
                            {GDK_question, "question"},
                            {GDK_quotedbl, "quotedbl"},
                            {GDK_quoteleft, "quoteleft"},
                            {GDK_quoteright, "quoteright"},
                            {GDK_semicolon, "semicolon"},
                            {GDK_slash, "slash"},
                            {GDK_space, "space"},
                            {GDK_underscore, "underscore"},

                            {GDK_F1, "f1"},
                            {GDK_F2, "f2"},
                            {GDK_F3, "f3"},
                            {GDK_F4, "f4"},
                            {GDK_F5, "f5"},
                            {GDK_F6, "f6"},
                            {GDK_F7, "f7"},
                            {GDK_F8, "f8"},
                            {GDK_F9, "f9"},
                            {GDK_F10, "f10"},
                            {GDK_F11, "f11"},
                            {GDK_F12, "f12"},
                            {GDK_F13, "f13"},
                            {GDK_F14, "f14"},
                            {GDK_F15, "f15"},
                            {GDK_F16, "f16"},
                            {GDK_F17, "f17"},
                            {GDK_F18, "f18"},
                            {GDK_F19, "f19"},
                            {GDK_F20, "f20"},
                            {GDK_F21, "f21"},
                            {GDK_F22, "f22"},
                            {GDK_F23, "f23"},
                            {GDK_F24, "f24"},
                            {GDK_F25, "f25"},
                            {GDK_F26, "f26"},
                            {GDK_F27, "f27"},
                            {GDK_F28, "f28"},
                            {GDK_F29, "f29"},
                            {GDK_F30, "f30"},
                            {GDK_F31, "f31"},
                            {GDK_F32, "f32"},
                            {GDK_F33, "f33"},
                            {GDK_F34, "f34"},
                            {GDK_F35, "f35"},

                            {GDK_KP_0, "kp.0"},
                            {GDK_KP_1, "kp.1"},
                            {GDK_KP_2, "kp.2"},
                            {GDK_KP_3, "kp.3"},
                            {GDK_KP_4, "kp.4"},
                            {GDK_KP_5, "kp.5"},
                            {GDK_KP_6, "kp.6"},
                            {GDK_KP_7, "kp.7"},
                            {GDK_KP_8, "kp.8"},
                            {GDK_KP_9, "kp.9"},
                            {GDK_KP_Add, "kp.add"},
                            {GDK_KP_Begin, "kp.begin"},
                            {GDK_KP_Decimal, "kp.decimal"},
                            {GDK_KP_Delete, "kp.delete"},
                            {GDK_KP_Divide, "kp.divide"},
                            {GDK_KP_Down, "kp.down"},
                            {GDK_KP_End, "kp.end"},
                            {GDK_KP_Enter, "kp.enter"},
                            {GDK_KP_Equal, "kp.equal"},
                            {GDK_KP_F1, "kp.f1"},
                            {GDK_KP_F2, "kp.f2"},
                            {GDK_KP_F3, "kp.f3"},
                            {GDK_KP_F4, "kp.f4"},
                            {GDK_KP_Home, "kp.home"},
                            {GDK_KP_Insert, "kp.insert"},
                            {GDK_KP_Left, "kp.left"},
                            {GDK_KP_Multiply, "kp.multiply"},
                            {GDK_KP_Next, "kp.next"},
                            {GDK_KP_Page_Down, "kp.page.down"},
                            {GDK_KP_Page_Up, "kp.page.up"},
                            {GDK_KP_Prior, "kp.prior"},
                            {GDK_KP_Right, "kp.right"},
                            {GDK_KP_Separator, "kp.separator"},
                            {GDK_KP_Space, "kp.space"},
                            {GDK_KP_Subtract, "kp.subtract"},
                            {GDK_KP_Tab, "kp.tab"},
                            {GDK_KP_Up, "kp.up"},

                            {GDK_Caps_Lock, "caps.lock"},
                            {GDK_Num_Lock, "num.lock"},
                            {GDK_Scroll_Lock, "scroll.lock"},
                            {GDK_Shift_Lock, "shift.lock"},

                            {GDK_BackSpace, "backspace"},
                            {GDK_Begin, "begin"},
                            {GDK_Break, "break"},
                            {GDK_Cancel, "cancel"},
                            {GDK_Clear, "clear"},
                            {GDK_Codeinput, "codeinput"},
                            {GDK_Delete, "delete"},
                            {GDK_Down, "down"},
                            {GDK_Eisu_Shift, "eisu.shift"},
                            {GDK_Eisu_toggle, "eisu.toggle"},
                            {GDK_End, "end"},
                            {GDK_Escape, "escape"},
                            {GDK_Execute, "execute"},
                            {GDK_Find, "find"},
                            {GDK_First_Virtual_Screen, "first.virtual.screen"},
                            {GDK_Help, "help"},
                            {GDK_Home, "home"},
                            {GDK_Hyper_L, "hyper.l"},
                            {GDK_Hyper_R, "hyper.r"},
                            {GDK_Insert, "insert"},
                            {GDK_Last_Virtual_Screen, "last.virtual.screen"},
                            {GDK_Left, "left"},
                            {GDK_Linefeed, "linefeed"},
                            {GDK_Menu, "menu"},
                            {GDK_Meta_L, "meta.l"},
                            {GDK_Meta_R, "meta.r"},
                            {GDK_Mode_switch, "mode.switch"},
                            {GDK_MultipleCandidate, "multiplecandidate"},
                            {GDK_Multi_key, "multi.key"},
                            {GDK_Next, "next"},
                            {GDK_Next_Virtual_Screen, "next.virtual.screen"},
                            {GDK_Page_Down, "page.down"},
                            {GDK_Page_Up, "page.up"},
                            {GDK_Pause, "pause"},
                            {GDK_PreviousCandidate, "previouscandidate"},
                            {GDK_Prev_Virtual_Screen, "prev.virtual.screen"},
                            {GDK_Print, "print"},
                            {GDK_Prior, "prior"},
                            {GDK_Redo, "redo"},
                            {GDK_Return, "return"},
                            {GDK_Right, "right"},
                            {GDK_script_switch, "script.switch"},
                            {GDK_Select, "select"},
                            {GDK_SingleCandidate, "singlecandidate"},
                            {GDK_Super_L, "super.l"},
                            {GDK_Super_R, "super.r"},
                            {GDK_Sys_Req, "sys.req"},
                            {GDK_Tab, "tab"},
                            {GDK_Terminate_Server, "terminate.server"},
                            {GDK_Undo, "undo"},
                            {GDK_Up, "up"}
                           };

    load_data (gdk_key_names, gdk_key_names_data, G_N_ELEMENTS(gdk_key_names_data));

    static struct
    {
        guint code;
        const gchar *name;
    }
    gdk_mod_names_data[] = {
                            {GDK_SHIFT_MASK, "<shift>"},
                            {GDK_CONTROL_MASK, "<control>"},
                            {GDK_MOD1_MASK, "<alt>"},
#if GTK_CHECK_VERSION (2, 10, 0)
                            {GDK_SUPER_MASK, "<super>"},
                            {GDK_SUPER_MASK, "<win>"},
                            {GDK_SUPER_MASK, "<mod4>"},
                            {GDK_HYPER_MASK, "<hyper>"},
                            {GDK_META_MASK, "<meta>"},
#endif
                            {GDK_MOD1_MASK, "<mod1>"},
                            {GDK_MOD4_MASK, "<super>"},
                            {GDK_MOD4_MASK, "<win>"},
                            {GDK_MOD4_MASK, "<mod4>"}
                           };

    load_data (gdk_modifiers_names, gdk_mod_names_data, G_N_ELEMENTS(gdk_mod_names_data));

    load_cmdline_history();
    //load_dir_history ();

    priv->con_list = gnome_cmd_con_list_new ();

    priv->con_list->lock();
    load_devices ("devices");

    if (!gnome_cmd_xml_config_load (xml_cfg_path, *this))
    {
        load_rename_history();

        // add a few default templates here - for new users
#if GLIB_CHECK_VERSION (2, 14, 0)
        {
            AdvrenameConfig::Profile p;

            p.name = _("Audio Files");
            p.template_string = "$T(Audio.AlbumArtist) - $T(Audio.Title).$e";
            p.regexes.push_back(GnomeCmd::ReplacePattern("[ _]+", " ", FALSE));
            p.regexes.push_back(GnomeCmd::ReplacePattern("[fF]eat\\.", "fr.", TRUE));

            advrename_defaults.profiles.push_back(p);

            p.reset();
            p.name = _("CamelCase");
            p.template_string = "$N";
            p.regexes.push_back(GnomeCmd::ReplacePattern("\\s*\\b(\\w)(\\w*)\\b", "\\u\\1\\L\\2\\E", FALSE));
            p.regexes.push_back(GnomeCmd::ReplacePattern("\\.(.+)$", ".\\L\\1", FALSE));

            advrename_defaults.profiles.push_back(p);
        }
#endif

        load_search_defaults();
    }

    if (!XML_cfg_has_connections)
        load_connections ("connections");

    priv->con_list->unlock();

    // "/quick-connect/uri" must be read AFTER retrieving anonymous password

    gchar *quick_connect_uri = gnome_cmd_data_get_string ("/quick-connect/uri", "ftp://anonymous@ftp.gnome.org/pub/GNOME/");
    quick_connect = gnome_cmd_con_remote_new (NULL, quick_connect_uri);
    g_free (quick_connect_uri);

    // if number of registered user actions does not exceed 10 (nothing has been read), try to read old cfg file
    if (gcmd_user_actions.size()<10)
        gcmd_user_actions.load("key-bindings");

    load_intviewer_defaults();
    load_auto_load_plugins();

    set_vfs_volume_monitor ();

    g_free (xml_cfg_path);
}


void GnomeCmdData::load_more()
{
    load_fav_apps ("fav-apps");

    if (!XML_cfg_has_bookmarks)
    {
        load_local_bookmarks();
        load_smb_bookmarks();
    }
}


void GnomeCmdData::save()
{
    gnome_cmd_data_set_int    ("/options/size_disp_mode", options.size_disp_mode);
    gnome_cmd_data_set_int    ("/options/perm_disp_mode", options.perm_disp_mode);
    gnome_cmd_data_set_int    ("/options/layout", options.layout);
    gnome_cmd_data_set_int    ("/options/list_row_height", options.list_row_height);

    gchar *utf8_date_format = g_locale_to_utf8 (options.date_format, -1, NULL, NULL, NULL);
    gnome_cmd_data_set_string ("/options/date_disp_mode", utf8_date_format);
    g_free (utf8_date_format);

    gnome_cmd_data_set_bool   ("/confirm/delete", options.confirm_delete);
    gnome_cmd_data_set_int    ("/confirm/delete_default", options.confirm_delete_default);
    gnome_cmd_data_set_int    ("/confirm/copy_overwrite", options.confirm_copy_overwrite);
    gnome_cmd_data_set_int    ("/confirm/move_overwrite", options.confirm_move_overwrite);
    gnome_cmd_data_set_bool   ("/confirm/confirm_mouse_dnd", options.confirm_mouse_dnd);

    gnome_cmd_data_set_bool   ("/options/show_unknown", options.filter.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN]);
    gnome_cmd_data_set_bool   ("/options/show_regular", options.filter.file_types[GNOME_VFS_FILE_TYPE_REGULAR]);
    gnome_cmd_data_set_bool   ("/options/show_directory", options.filter.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY]);
    gnome_cmd_data_set_bool   ("/options/show_fifo", options.filter.file_types[GNOME_VFS_FILE_TYPE_FIFO]);
    gnome_cmd_data_set_bool   ("/options/show_socket", options.filter.file_types[GNOME_VFS_FILE_TYPE_SOCKET]);
    gnome_cmd_data_set_bool   ("/options/show_char_device", options.filter.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE]);
    gnome_cmd_data_set_bool   ("/options/show_block_device", options.filter.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE]);
    gnome_cmd_data_set_bool   ("/options/show_symbolic_link", options.filter.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK]);

    gnome_cmd_data_set_bool   ("/options/hidden_filter", options.filter.hidden);
    gnome_cmd_data_set_bool   ("/options/backup_filter", options.filter.backup);

    gnome_cmd_data_set_bool   ("/sort/select_dirs", options.select_dirs);
    gnome_cmd_data_set_bool   ("/sort/case_sensitive", options.case_sens_sort);

    gnome_cmd_data_set_int    ("/colors/mode", options.color_mode);

    gnome_cmd_data_set_color  ("/colors/norm_fg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg);
    gnome_cmd_data_set_color  ("/colors/norm_bg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg);
    gnome_cmd_data_set_color  ("/colors/alt_fg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg);
    gnome_cmd_data_set_color  ("/colors/alt_bg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg);
    gnome_cmd_data_set_color  ("/colors/sel_fg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg);
    gnome_cmd_data_set_color  ("/colors/sel_bg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg);
    gnome_cmd_data_set_color  ("/colors/curs_fg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg);
    gnome_cmd_data_set_color  ("/colors/curs_bg", options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg);

    gnome_cmd_data_set_bool   ("/colors/use_ls_colors", options.use_ls_colors);

    gnome_cmd_data_set_color ("/colors/ls_colors_black_fg", options.ls_colors_palette.black_fg);
    gnome_cmd_data_set_color ("/colors/ls_colors_black_bg", options.ls_colors_palette.black_bg);
    gnome_cmd_data_set_color ("/colors/ls_colors_red_fg", options.ls_colors_palette.red_fg);
    gnome_cmd_data_set_color ("/colors/ls_colors_red_bg", options.ls_colors_palette.red_bg);
    gnome_cmd_data_set_color ("/colors/ls_colors_green_fg", options.ls_colors_palette.green_fg);
    gnome_cmd_data_set_color ("/colors/ls_colors_green_bg", options.ls_colors_palette.green_bg);
    gnome_cmd_data_set_color ("/colors/ls_colors_yellow_fg", options.ls_colors_palette.yellow_fg);
    gnome_cmd_data_set_color ("/colors/ls_colors_yellow_bg", options.ls_colors_palette.yellow_bg);
    gnome_cmd_data_set_color ("/colors/ls_colors_blue_fg", options.ls_colors_palette.blue_fg);
    gnome_cmd_data_set_color ("/colors/ls_colors_blue_bg", options.ls_colors_palette.blue_bg);
    gnome_cmd_data_set_color ("/colors/ls_colors_magenta_fg", options.ls_colors_palette.magenta_fg);
    gnome_cmd_data_set_color ("/colors/ls_colors_magenta_bg", options.ls_colors_palette.magenta_bg);
    gnome_cmd_data_set_color ("/colors/ls_colors_cyan_fg", options.ls_colors_palette.cyan_fg);
    gnome_cmd_data_set_color ("/colors/ls_colors_cyan_bg", options.ls_colors_palette.cyan_bg);
    gnome_cmd_data_set_color ("/colors/ls_colors_white_fg", options.ls_colors_palette.white_fg);
    gnome_cmd_data_set_color ("/colors/ls_colors_white_bg", options.ls_colors_palette.white_bg);

    gnome_cmd_data_set_string ("/options/list_font", options.list_font);

    gnome_cmd_data_set_int    ("/options/ext_disp_mode", options.ext_disp_mode);
    gnome_cmd_data_set_int    ("/options/left_mouse_button_mode", options.left_mouse_button_mode);
    gnome_cmd_data_set_bool   ("/options/left_mouse_button_unselects", options.left_mouse_button_unselects);
    gnome_cmd_data_set_int    ("/options/middle_mouse_button_mode", options.middle_mouse_button_mode);
    gnome_cmd_data_set_int    ("/options/right_mouse_button_mode", options.right_mouse_button_mode);
    gnome_cmd_data_set_int    ("/options/icon_size", options.icon_size);
    gnome_cmd_data_set_int    ("/options/dev_icon_size", dev_icon_size);
    gnome_cmd_data_set_int    ("/options/icon_scale_quality", options.icon_scale_quality);
    gnome_cmd_data_set_string ("/options/theme_icon_dir", options.theme_icon_dir);
    gnome_cmd_data_set_string ("/options/document_icon_dir", options.document_icon_dir);
    gnome_cmd_data_set_int    ("/options/cmdline_history_length", cmdline_history_length);
    gnome_cmd_data_set_int    ("/options/btn_relief", button_relief);
    gnome_cmd_data_set_bool   ("/options/list_orientation", list_orientation);
    gnome_cmd_data_set_int    ("/options/gui_update_rate", gui_update_rate);

    gnome_cmd_data_set_bool   ("/programs/honor_expect_uris", options.honor_expect_uris);
    gnome_cmd_data_set_bool   ("/programs/allow_multiple_instances", options.allow_multiple_instances);
    gnome_cmd_data_set_bool   ("/programs/use_internal_viewer", options.use_internal_viewer);
    gnome_cmd_data_set_bool   ("/programs/alt_quick_search", options.alt_quick_search);
    gnome_cmd_data_set_bool   ("/programs/quick_search_exact_match_begin", options.quick_search_exact_match_begin);
    gnome_cmd_data_set_bool   ("/programs/quick_search_exact_match_end", options.quick_search_exact_match_end);
    gnome_cmd_data_set_bool   ("/programs/skip_mounting", options.skip_mounting);

    gnome_cmd_data_set_bool   ("/programs/toolbar_visibility", toolbar_visibility);
    gnome_cmd_data_set_bool   ("/options/conbuttons_visibility", conbuttons_visibility);
    gnome_cmd_data_set_bool   ("/options/con_list_visibility", concombo_visibility);
    gnome_cmd_data_set_bool   ("/options/cmdline_visibility", cmdline_visibility);
    gnome_cmd_data_set_bool   ("/programs/buttonbar_visibility", buttonbar_visibility);

    if (priv->symlink_prefix && *priv->symlink_prefix && strcmp(priv->symlink_prefix, _("link to %s"))!=0)
        gnome_cmd_data_set_string ("/options/symlink_prefix", priv->symlink_prefix);
    else
        gnome_cmd_data_set_string ("/options/symlink_prefix", "");

    gnome_cmd_data_set_int    ("/options/main_win_pos_x", priv->main_win_pos[0]);
    gnome_cmd_data_set_int    ("/options/main_win_pos_y", priv->main_win_pos[1]);

    gnome_cmd_data_set_string ("/programs/viewer", options.viewer);
    gnome_cmd_data_set_string ("/programs/editor", options.editor);
    gnome_cmd_data_set_string ("/programs/differ", options.differ);
    gnome_cmd_data_set_string ("/programs/terminal_open", options.termopen);
    gnome_cmd_data_set_string ("/programs/terminal_exec", options.termexec);

    gnome_cmd_data_set_bool   ("/programs/use_gcmd_block", use_gcmd_block);

    gnome_cmd_data_set_bool   ("/devices/only_icon", options.device_only_icon);

    const gchar *quick_connect_uri = gnome_cmd_con_get_uri (GNOME_CMD_CON (quick_connect));

    if (quick_connect_uri)
        gnome_cmd_data_set_string ("/quick-connect/uri", quick_connect_uri);

    gnome_config_clean_key (G_DIR_SEPARATOR_S PACKAGE "/quick-connect/host");
    gnome_config_clean_key (G_DIR_SEPARATOR_S PACKAGE "/quick-connect/port");
    gnome_config_clean_key (G_DIR_SEPARATOR_S PACKAGE "/quick-connect/user");

    gnome_config_set_int ("/gnome-commander-size/main_win/width", main_win_width);
    gnome_config_set_int ("/gnome-commander-size/main_win/height", main_win_height);

    for (gint i=0; i<GnomeCmdFileList::NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/fs_col_width%d", i);
        gnome_config_set_int (tmp, fs_col_width[i]);
        g_free (tmp);
    }

    gnome_cmd_data_set_bool ("/options/save_dirs_on_exit", options.save_dirs_on_exit);
    gnome_cmd_data_set_bool ("/options/save_tabs_on_exit", options.save_tabs_on_exit);
    gnome_cmd_data_set_bool ("/options/save_dir_history_on_exit", options.save_dir_history_on_exit);

    gnome_cmd_data_set_bool ("/options/always_show_tabs", options.always_show_tabs);
    gnome_cmd_data_set_int ("/options/tab_lock_indicator", (int) options.tab_lock_indicator);

    gnome_cmd_data_set_string ("/defaults/backup_pattern", options.backup_pattern);

    gnome_cmd_data_set_int ("/options/main_win_state", (gint) main_win_state);

    gnome_cmd_data_set_string ("/network/ftp_anonymous_password", priv->ftp_anonymous_password);
    gnome_config_clean_section (G_DIR_SEPARATOR_S PACKAGE "/ftp");

    save_cmdline_history();
    //write_dir_history ();

    save_devices ("devices");
    save_fav_apps ("fav-apps");
    save_intviewer_defaults();

    {
        gchar *xml_cfg_path = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);

        ofstream f(xml_cfg_path);
        XML::xstream xml(f);

        xml << XML::comment("Created with GNOME Commander (http://gcmd.github.io/)");
        xml << XML::tag("GnomeCommander") << XML::attr("version") << VERSION;

        xml << *main_win;

        xml << XML::tag("History");

        if (options.save_dir_history_on_exit)
        {
            xml << XML::tag("Directories");

            for (GList *i=gnome_cmd_con_get_dir_history (priv->con_list->get_home())->ents; i; i=i->next)
                xml << XML::tag("Directory") << XML::attr("path") << XML::escape((const gchar *) i->data) << XML::endtag();

            xml << XML::endtag("Directories");
        }

        xml << XML::endtag("History");

        xml << advrename_defaults;
        xml << search_defaults;
        xml << bookmarks_defaults;

        xml << XML::tag("Connections");

        for (GList *i=gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list); i; i=i->next)
        {
            GnomeCmdCon *con = GNOME_CMD_CON (i->data);

            if (con)
                xml << *con;
        }

        xml << XML::endtag("Connections");

        xml << XML::tag("Bookmarks");

        write (xml, priv->con_list->get_home(), "Home");
        write (xml, priv->con_list->get_smb(), "SMB");

        for (GList *i=gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list); i; i=i->next)
        {
            GnomeCmdCon *con = GNOME_CMD_CON (i->data);
            write (xml, con, XML::escape(gnome_cmd_con_get_alias (con)));
        }

        xml << XML::endtag("Bookmarks");

        xml << XML::tag("Selections");
        for (vector<Selection>::iterator i=selections.begin(); i!=selections.end(); ++i)
            xml << *i;
        xml << XML::endtag("Selections");

        xml << gcmd_user_actions;

        xml << XML::endtag("GnomeCommander");

        g_free (xml_cfg_path);
    }

    save_auto_load_plugins();

    gnome_config_sync ();
}


GnomeCmdFileList::ColumnID GnomeCmdData::get_sort_col(FileSelectorID id) const
{
    return (GnomeCmdFileList::ColumnID) priv->sort_column[id];
}


GtkSortType GnomeCmdData::get_sort_direction(FileSelectorID id) const
{
    return (GtkSortType) priv->sort_direction[id];
}


gpointer gnome_cmd_data_get_con_list ()
{
    return gnome_cmd_data.priv->con_list;
}


const gchar *gnome_cmd_data_get_ftp_anonymous_password ()
{
    return gnome_cmd_data.priv->ftp_anonymous_password;
}


void gnome_cmd_data_set_ftp_anonymous_password (const gchar *pw)
{
    g_free (gnome_cmd_data.priv->ftp_anonymous_password);

    gnome_cmd_data.priv->ftp_anonymous_password = g_strdup (pw);
}


GList *gnome_cmd_data_get_auto_load_plugins ()
{
    return gnome_cmd_data.priv->auto_load_plugins;
}


void gnome_cmd_data_set_auto_load_plugins (GList *plugins)
{
    gnome_cmd_data.priv->auto_load_plugins = plugins;
}


void gnome_cmd_data_set_main_win_pos (gint x, gint y)
{
    gnome_cmd_data.priv->main_win_pos[0] = x;
    gnome_cmd_data.priv->main_win_pos[1] = y;
}


void gnome_cmd_data_get_main_win_pos (gint *x, gint *y)
{
    *x = gnome_cmd_data.priv->main_win_pos[0];
    *y = gnome_cmd_data.priv->main_win_pos[1];
}


const gchar *gnome_cmd_data_get_symlink_prefix ()
{
    return gnome_cmd_data.priv->symlink_prefix ? gnome_cmd_data.priv->symlink_prefix : _("link to %s");
}


void gnome_cmd_data_set_symlink_prefix (const gchar *value)
{
    gnome_cmd_data.priv->symlink_prefix = g_strdup (value);
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::AdvrenameConfig &cfg)
{
    xml << XML::tag("AdvancedRenameTool");

        xml << XML::tag("WindowSize") << XML::attr("width") << cfg.width << XML::attr("height") << cfg.height << XML::endtag();

        xml << XML::tag("Profile") << XML::attr("name") << "Default";

            xml << XML::tag("Template") << XML::chardata() << XML::escape(cfg.templates.empty()  ? "$N" : cfg.templates.front()) << XML::endtag();
            xml << XML::tag("Counter") << XML::attr("start") << cfg.default_profile.counter_start
                                       << XML::attr("step") << cfg.default_profile.counter_step
                                       << XML::attr("width") << cfg.default_profile.counter_width << XML::endtag();

            xml << XML::tag("Regexes");
            for (vector<GnomeCmd::ReplacePattern>::const_iterator r=cfg.default_profile.regexes.begin(); r!=cfg.default_profile.regexes.end(); ++r)
            {
                xml << XML::tag("Regex") << XML::attr("pattern") << XML::escape(r->pattern);
                xml << XML::attr("replace") << XML::escape(r->replacement) << XML::attr("match-case") << r->match_case << XML::endtag();
            }
            xml << XML::endtag();

            xml << XML::tag("CaseConversion") << XML::attr("use") << cfg.default_profile.case_conversion << XML::endtag();
            xml << XML::tag("TrimBlanks") << XML::attr("use") << cfg.default_profile.trim_blanks << XML::endtag();

        xml << XML::endtag();

        for (vector<GnomeCmdData::AdvrenameConfig::Profile>::const_iterator p=cfg.profiles.begin(); p!=cfg.profiles.end(); ++p)
        {
            xml << XML::tag("Profile") << XML::attr("name") << p->name;
                xml << XML::tag("Template") << XML::chardata() << XML::escape(p->template_string.empty() ? "$N" : p->template_string) << XML::endtag();
                xml << XML::tag("Counter") << XML::attr("start") << p->counter_start
                                           << XML::attr("step") << p->counter_step
                                           << XML::attr("width") << p->counter_width << XML::endtag();
                xml << XML::tag("Regexes");
                for (vector<GnomeCmd::ReplacePattern>::const_iterator r=p->regexes.begin(); r!=p->regexes.end(); ++r)
                {
                    xml << XML::tag("Regex") << XML::attr("pattern") << XML::escape(r->pattern);
                    xml << XML::attr("replace") << XML::escape(r->replacement) << XML::attr("match-case") << r->match_case << XML::endtag();
                }
                xml << XML::endtag();
                xml << XML::tag("CaseConversion") << XML::attr("use") << p->case_conversion << XML::endtag();
                xml << XML::tag("TrimBlanks") << XML::attr("use") << p->trim_blanks << XML::endtag();
            xml << XML::endtag();
        }

        xml << XML::tag("History");
        for (GList *i=cfg.templates.ents; i; i=i->next)
            xml << XML::tag("Template") << XML::chardata() << XML::escape((const gchar *) i->data) << XML::endtag();
        xml << XML::endtag();

    xml << XML::endtag();

    return xml;
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::Selection &cfg)
{
    xml << XML::tag("Profile") << XML::attr("name") << XML::escape(cfg.name);

        xml << XML::tag("Pattern") << XML::attr("syntax") << (cfg.syntax==Filter::TYPE_REGEX ? "regex" : "shell")
                                   << XML::attr("match-case") << 0 << XML::chardata() << XML::escape(cfg.filename_pattern) << XML::endtag();
        xml << XML::tag("Subdirectories") << XML::attr("max-depth") << cfg.max_depth << XML::endtag();
        xml << XML::tag("Text") << XML::attr("content-search") << cfg.content_search << XML::attr("match-case") << cfg.match_case << XML::chardata() << XML::escape(cfg.text_pattern) << XML::endtag();

    xml << XML::endtag();

    return xml;
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::SearchConfig &cfg)
{
    xml << XML::tag("SearchTool");

        xml << XML::tag("WindowSize") << XML::attr("width") << cfg.width << XML::attr("height") << cfg.height << XML::endtag();
        xml << cfg.default_profile;

        xml << XML::tag("History");

        for (GList *i=cfg.name_patterns.ents; i; i=i->next)
            xml << XML::tag("Pattern") << XML::chardata() << XML::escape((const gchar *) i->data) << XML::endtag();

        for (GList *i=cfg.content_patterns.ents; i; i=i->next)
            xml << XML::tag("Text") << XML::chardata() << XML::escape((const gchar *) i->data) << XML::endtag();

        xml << XML::endtag();

    xml << XML::endtag();

    return xml;
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::BookmarksConfig &cfg)
{
    xml << XML::tag("BookmarksTool");

        xml << XML::tag("WindowSize") << XML::attr("width") << cfg.width << XML::attr("height") << cfg.height << XML::endtag();

    xml << XML::endtag();

    return xml;
}
