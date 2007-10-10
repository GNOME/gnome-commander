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

#include <config.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <libgnomevfs/gnome-vfs-volume.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-con-device.h"
#include "gnome-cmd-con-ftp.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-bookmark-dialog.h"
#include "filter.h"
#include "utils.h"

using namespace std;


#define MAX_GUI_UPDATE_RATE 1000
#define MIN_GUI_UPDATE_RATE 10
#define DEFAULT_GUI_UPDATE_RATE 100


GnomeCmdData *data = NULL;
GnomeVFSVolumeMonitor *monitor = NULL;

struct _GnomeCmdDataPrivate
{
    GnomeCmdConList      *con_list;
    gchar                *ftp_anonymous_password;
    GList                *fav_apps;
    GnomeCmdSizeDispMode size_disp_mode;
    GnomeCmdPermDispMode perm_disp_mode;
    GnomeCmdDateFormat   date_format;
    GnomeCmdLayout       layout;
    GnomeCmdColorTheme   color_themes[GNOME_CMD_NUM_COLOR_MODES];
    GnomeCmdColorMode    color_mode;
    GnomeCmdExtDispMode  ext_disp_mode;
    FilterSettings       filter_settings;
    gint                 main_win_width, main_win_height;
    gboolean             case_sens_sort;
    gboolean             confirm_delete;
    GnomeCmdConfirmOverwriteMode confirm_copy_overwrite;
    GnomeCmdConfirmOverwriteMode confirm_move_overwrite;
    gint                 list_row_height;
    gchar                *list_font;
    GnomeCmdRightMouseButtonMode right_mouse_button_mode;
    gboolean             show_toolbar;
    guint                icon_size;
    guint                dev_icon_size;
    GdkInterpType        icon_scale_quality;
    gchar                *theme_icon_dir;
    gchar                *document_icon_dir;
    guint                fs_col_width[FILE_LIST_NUM_COLUMNS];
    guint                bookmark_dialog_col_width[BOOKMARK_DIALOG_NUM_COLUMNS];
    gint                 cmdline_history_length;
    GList                *cmdline_history;
    GtkReliefStyle       btn_relief;
    FilterType           filter_type;
    gboolean             device_only_icon;
    gint                 dir_cache_size;
    gboolean             use_ls_colors;
    gchar                *quick_connect_host;
    gchar                *quick_connect_user;
    gint                 quick_connect_port;
    gboolean             honor_expect_uris;
    gboolean             use_internal_viewer;
    gboolean             alt_quick_search;
    gboolean             skip_mounting;
    gboolean             toolbar_visibility;
    gboolean             buttonbar_visibility;
    SearchDefaults       *search_defaults;
    AdvrenameDefaults    *advrename_defaults;
    gboolean             list_orientation;
    gboolean             conbuttons_visibility;
    gboolean             cmdline_visibility;
    gchar                *start_dirs[2];
    gchar                *last_pattern;
    GList                *auto_load_plugins;
    guint                gui_update_rate;
    gint                 sort_column[2];
    gboolean             sort_direction[2];
    gint                 main_win_pos[2];
    gchar                *backup_pattern;
    GList                *backup_pattern_list;
    GdkWindowState       main_win_state;
    gchar                *symlink_prefix;

    gchar *viewer;
    gchar *editor;
    gchar *differ;
    gchar *term;
};


DICT<guint> gdk_key_names(GDK_VoidSymbol);


inline gint get_int (const gchar *path, int def)
{
    gboolean b = FALSE;
    gint value = gnome_config_get_int_with_default (path, &b);

    return b ? def : value;
}


inline void write_ftp_servers (const gchar *fname)
{
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir(), fname);
    FILE  *fd = fopen (path, "w");

    if (fd)
    {
        chmod (path, S_IRUSR|S_IWUSR);

        for (GList *tmp = gnome_cmd_con_list_get_all_ftp (data->priv->con_list); tmp; tmp = tmp->next)
        {
            GnomeCmdConFtp *server = GNOME_CMD_CON_FTP (tmp->data);

            if (server)
            {
                string uri_str;
                string alias;
                string remote_dir;

                stringify (alias, gnome_vfs_escape_string (gnome_cmd_con_ftp_get_alias (server)));
                stringify (remote_dir, gnome_vfs_escape_string (gnome_cmd_con_ftp_get_remote_dir (server)));

                gchar *hname = gnome_vfs_escape_string (gnome_cmd_con_ftp_get_host_name (server));
                gushort port = gnome_cmd_con_ftp_get_host_port (server);
                gchar *uname = gnome_vfs_escape_string (gnome_cmd_con_ftp_get_user_name (server));
                gchar *pw    = gnome_vfs_escape_string (gnome_cmd_con_ftp_get_pw (server));
                GnomeCmdBookmarkGroup *bookmark_group = gnome_cmd_con_get_bookmarks (GNOME_CMD_CON (server));

                fprintf (fd, "C: %s %s %s %d %s %s %s\n", "ftp:", alias.c_str(), hname, port, remote_dir.c_str(), uname, pw?pw:"");

                g_free (hname);
                g_free (uname);
                g_free (pw);

                GnomeCmdPath *path = gnome_cmd_con_create_path (GNOME_CMD_CON (server), remote_dir.c_str());
                GnomeVFSURI *uri = gnome_cmd_con_create_uri (GNOME_CMD_CON (server), path);
                stringify (uri_str, gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD));
                // stringify (uri_str, gnome_vfs_format_uri_for_display (uri_str.c_str());
                gtk_object_destroy (GTK_OBJECT (path));

                fprintf (fd, "U: %s\t%s\n", alias.c_str(), uri_str.c_str());

                gnome_vfs_uri_unref (uri);

                for (GList *bookmarks = bookmark_group->bookmarks; bookmarks; bookmarks = bookmarks->next)
                {
                    GnomeCmdBookmark *bookmark = (GnomeCmdBookmark *) bookmarks->data;
                    gchar *name = gnome_vfs_escape_string (bookmark->name);
                    gchar *path = gnome_vfs_escape_string (bookmark->path);

                    fprintf (fd, "B: %s %s\n", name, path);

                    g_free (name);
                    g_free (path);
                }
            }
        }

        fclose (fd);
    }
    else
        warn_print ("Failed to open the file %s for writing\n", path);

    g_free (path);
}


inline void write_devices (const gchar *fname)
{
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir(), fname);
    FILE *fd = fopen (path, "w");

    if (fd)
    {
        for (GList *tmp = gnome_cmd_con_list_get_all_dev (data->priv->con_list); tmp; tmp = tmp->next)
        {
            GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (tmp->data);
            if (device && !gnome_cmd_con_device_get_autovol(device))
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
        warn_print ("Failed to open the file %s for writing\n", path);

    g_free (path);
}


inline void write_fav_apps (const gchar *fname)
{
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir(), fname);
    FILE *fd = fopen (path, "w");

    if (fd)
    {
        for (GList *tmp = data->priv->fav_apps; tmp; tmp = tmp->next)
        {
            GnomeCmdApp *app = (GnomeCmdApp *) tmp->data;
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

                fprintf (fd, "%s %s %s %d %s %d %d %d\n",
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
        warn_print ("Failed to open the file %s for writing\n", path);

    g_free (path);
}


inline gboolean load_ftp_servers (const gchar *fname)
{
    guint prev_ftp_cons_no = g_list_length (gnome_cmd_con_list_get_all_ftp (data->priv->con_list));

    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir(), fname);
    FILE  *fd = fopen (path, "r");

    if (fd)
    {
        gchar line[1024];

        while (fgets (line, sizeof(line), fd) != NULL)
        {
            GnomeCmdConFtp *server = NULL;

            switch (line[0])
            {
                case '\0':             // do not warn about empty lines
                case '#' :             // do not warn about empty comments
                    break;

                case 'U':
                    break;

                case 'S':
                    {
                        gchar alias[256], host[256], user[256], pw[256];
                        gchar *alias2, *host2, *user2, *pw2=NULL;
                        gint port;

                        gint ret = sscanf (line, "S: %256s %256s %d %256s %256s\n", alias, host, &port, user, pw);

                        if (ret == 4 || ret == 5)
                        {
                            alias2 = gnome_vfs_unescape_string (alias, NULL);
                            host2  = gnome_vfs_unescape_string (host, NULL);
                            user2  = gnome_vfs_unescape_string (user, NULL);
                            if (ret == 5)
                                pw2 = gnome_vfs_unescape_string (pw, NULL);

                            server = gnome_cmd_con_ftp_new (alias2, host2, (gshort)port, user2, pw2, "");

                            gnome_cmd_con_list_add_ftp (data->priv->con_list, server);

                            g_free (alias2);
                            g_free (host2);
                            g_free (user2);
                            g_free (pw2);
                        }
                    }
                    break;

                case 'C':
                    {
                        gchar **a = g_strsplit_set(line, " \t\n", 9);
                        gint port2;

                        if (g_strv_length(a)==9             &&
                            strcmp(a[0], "C:")==0           &&
                            strcmp(a[1], "ftp:")==0         &&
                            sscanf(a[4], "%ud", &port2)==1)
                        {
                            gchar *alias2       = gnome_vfs_unescape_string (a[2], NULL);
                            gchar *host2        = gnome_vfs_unescape_string (a[3], NULL);
                            gchar *remote_dir2  = gnome_vfs_unescape_string (a[5], NULL);
                            gchar *user2        = gnome_vfs_unescape_string (a[6], NULL);
                            gchar *pw2          = gnome_vfs_unescape_string (a[7], NULL);

                            GnomeCmdConFtp *server = gnome_cmd_con_ftp_new (alias2, host2, (gshort)port2, user2, pw2, remote_dir2);

                            g_free (alias2);
                            g_free (host2);
                            g_free (remote_dir2);
                            g_free (user2);
                            g_free (pw2);

                            gnome_cmd_con_list_add_ftp (data->priv->con_list, server);
                        }
                        else
                        {
                            g_warning ("Invalid line in the '%s' file - skipping it...", path);
                            g_warning ("\t... %s", line);
                        }

                        g_strfreev(a);
                    }
                    break;

                case 'B':
                    if (server)
                    {
                        gchar name[256], path[256];
                        gint ret = sscanf (line, "B: %256s %256s\n", name, path);

                        if (ret == 2)
                        {
                            GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (GNOME_CMD_CON (server));
                            GnomeCmdBookmark *bookmark = g_new0 (GnomeCmdBookmark, 1);
                            bookmark->name = gnome_vfs_unescape_string (name, NULL);
                            bookmark->path = gnome_vfs_unescape_string (path, NULL);
                            bookmark->group = group;

                            group->bookmarks = g_list_append (group->bookmarks, bookmark);
                        }
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
            warn_print ("Failed to open the file %s for reading\n", path);

    g_free (path);

    return fd!=NULL && g_list_length(gnome_cmd_con_list_get_all_ftp(data->priv->con_list))>prev_ftp_cons_no;
}


inline gboolean vfs_is_uri_local (const char *uri)
{
    GnomeVFSURI *pURI = gnome_vfs_uri_new(uri);

    if (!pURI)
        return FALSE;

    gboolean b = gnome_vfs_uri_is_local(pURI);
    gnome_vfs_uri_unref(pURI);

    /* make sure this is actually a local path
           (gnome treats "burn://" as local, too and we don't want that)  */
    if (g_strncasecmp(uri,"file:/", 6)!=0)
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
    if (!vfs_is_uri_local(uri))
    {
        g_free(uri);
        return;
    }

    path = gnome_vfs_volume_get_device_path (volume);
    localpath = gnome_vfs_get_local_path_from_uri(uri);

    for (GList *tmp = gnome_cmd_con_list_get_all_dev (data->priv->con_list); tmp; tmp = tmp->next)
    {
        GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (tmp->data);
        if (device && gnome_cmd_con_device_get_autovol(device))
        {
            gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
            const gchar *mountp = gnome_cmd_con_device_get_mountp (device);

            if ((strcmp(device_fn, path)==0) && (strcmp(mountp,localpath)==0))
            {
                DEBUG('m',"Remove Volume:\ndevice_fn = %s\tmountp = %s\n",
                device_fn,mountp);
                gnome_cmd_con_list_remove_device(data->priv->con_list, device);
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
        if (device && !gnome_cmd_con_device_get_autovol(device))
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

    if (!vfs_is_uri_local(uri))
    {
        g_free(uri);
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
    if (!device_mount_point_exists(data->priv->con_list, localpath))
    {
        GnomeCmdConDevice *ConDev = gnome_cmd_con_device_new (name, path?path:NULL, localpath, iconpath);
        gnome_cmd_con_device_set_autovol(ConDev, TRUE);
        gnome_cmd_con_device_set_vfs_volume(ConDev, volume);
        gnome_cmd_con_list_add_device (data->priv->con_list,ConDev);
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
    if (!gnome_vfs_drive_is_user_visible(drive))
        return;

    char *uri = gnome_vfs_drive_get_activation_uri (drive);

    if (!vfs_is_uri_local(uri))
    {
        g_free(uri);
        return;
    }

    char *path = gnome_vfs_drive_get_device_path (drive);
    char *icon = gnome_vfs_drive_get_icon (drive);
    char *name = gnome_vfs_drive_get_display_name (drive);
    GnomeVFSVolume *volume = gnome_vfs_drive_get_mounted_volume (drive);

    char *localpath = gnome_vfs_get_local_path_from_uri(uri);

    DEBUG('m',"name = %s\tpath = %s\turi = %s\tlocal = %s\n",name,path,uri,localpath);

    GnomeCmdConDevice *ConDev = gnome_cmd_con_device_new (name,path,localpath, icon);

    gnome_cmd_con_device_set_autovol(ConDev, TRUE);

    gnome_cmd_con_list_add_device (data->priv->con_list,ConDev);

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
    add_vfs_volume(volume);
}


static void volume_unmounted (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSVolume *volume)
{
    remove_vfs_volume(volume);
}

#if 0
static void drive_connected (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSDrive *drive)
{
    add_vfs_drive(drive);
}

static void drive_disconnected (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSDrive *drive)
{
    // TODO: Remove from Drives combobox
}
#endif

inline void set_vfs_volume_monitor()
{
    monitor = gnome_vfs_get_volume_monitor ();

    g_signal_connect (monitor, "volume_mounted", G_CALLBACK (volume_mounted), NULL);
    g_signal_connect (monitor, "volume_unmounted", G_CALLBACK (volume_unmounted), NULL);
#if 0
    g_signal_connect (monitor, "drive_connected", G_CALLBACK (drive_connected), NULL);
    g_signal_connect (monitor, "drive_disconnected", G_CALLBACK (drive_disconnected), NULL);
#endif
}


inline void load_vfs_auto_devices()
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
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir(), fname);
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

                gnome_cmd_con_list_add_device (
                    data->priv->con_list,
                    gnome_cmd_con_device_new (alias2, device_fn2, mountp2, icon_path2));

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
            warn_print ("Failed to open the file %s for reading\n", path);

    load_vfs_auto_devices();

    g_free (path);
}


inline void load_fav_apps (const gchar *fname)
{
    data->priv->fav_apps = NULL;
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir(), fname);
    FILE *fd = fopen (path, "r");
    if (fd)
    {
        int ret;
        gchar name[256], cmd[256], icon_path[256], pattern_string[256];
        gint target, handles_uris, handles_multiple, requires_terminal;

        do
        {
            ret = fscanf (fd, "%s %s %s %d %s %d %d %d\n",
                          name, cmd, icon_path,
                          &target, pattern_string,
                          &handles_uris, &handles_multiple, &requires_terminal);

            if (ret == 8)
            {
                gchar *name2      = gnome_vfs_unescape_string (name, NULL);
                gchar *cmd2       = gnome_vfs_unescape_string (cmd, NULL);
                gchar *icon_path2 = gnome_vfs_unescape_string (icon_path, NULL);
                gchar *pattern_string2 = gnome_vfs_unescape_string (pattern_string, NULL);

                data->priv->fav_apps = g_list_append (
                    data->priv->fav_apps,
                    gnome_cmd_app_new_with_values (
                        name2, cmd2, icon_path2,
                        (AppTarget) target, pattern_string2,
                        handles_uris, handles_multiple, requires_terminal));

                g_free (name2);
                g_free (cmd2);
                g_free (icon_path2);
                g_free (pattern_string2);
            }
        }
        while (ret == 8);

        fclose (fd);
    }
    else
        if (errno != ENOENT)
            warn_print ("Failed to open the file %s for reading\n", path);

    g_free (path);
}


inline void write_string_history (gchar *format, GList *strings)
{
    gchar key[128];

    for (gint i=0; strings; strings=strings->next, ++i)
    {
        snprintf (key, sizeof (key), format, i);
        gnome_cmd_data_set_string (key, (gchar *) strings->data);
    }
}


inline void write_uint_array (const gchar *format, guint *array, gint length)
{
    for (gint i=0; i<length; i++)
    {
        gchar *name = g_strdup_printf (format, i);
        gnome_cmd_data_set_int (name, array[i]);
        g_free (name);
    }
}


inline void write_cmdline_history ()
{
    if (!data->priv->cmdline_visibility)
        return;

    data->priv->cmdline_history = gnome_cmd_cmdline_get_history (gnome_cmd_main_win_get_cmdline (main_win));

    write_string_history ("/cmdline-history/line%d", data->priv->cmdline_history);
}


inline void write_search_defaults ()
{
    gnome_cmd_data_set_int ("/search-history/width",data->priv->search_defaults->width);
    gnome_cmd_data_set_int ("/search-history/height",data->priv->search_defaults->height);

    write_string_history ("/search-history/name_pattern%d",data->priv->search_defaults->name_patterns);
    write_string_history ("/search-history/content_pattern%d",data->priv->search_defaults->content_patterns);
    write_string_history ("/search-history/directory%d",data->priv->search_defaults->directories);

    gnome_cmd_data_set_bool ("/search-history/recursive",data->priv->search_defaults->recursive);
    gnome_cmd_data_set_bool ("/search-history/case_sens",data->priv->search_defaults->case_sens);
}


inline void write_rename_history ()
{
    GList *from=NULL;
    GList *to=NULL;
    GList *csens=NULL;

    for (GList *tmp = data->priv->advrename_defaults->patterns; tmp; tmp = tmp->next)
    {
        PatternEntry *entry = (PatternEntry *) tmp->data;
        from = g_list_append (from, entry->from);
        to = g_list_append (to, entry->to);
        csens = g_list_append (csens, (gpointer) (entry->case_sens ? "T" : "F"));
    }

    gnome_cmd_data_set_int ("/options/template-auto-update", data->priv->advrename_defaults->auto_update);
    gnome_cmd_data_set_int ("/advrename/width", data->priv->advrename_defaults->width);
    gnome_cmd_data_set_int ("/advrename/height", data->priv->advrename_defaults->height);

    write_uint_array ("/advrename/pat_col_widths%d", advrename_dialog_default_pat_column_width, ADVRENAME_DIALOG_PAT_NUM_COLUMNS);
    write_uint_array ("/advrename/res_col_widths%d", advrename_dialog_default_res_column_width, ADVRENAME_DIALOG_RES_NUM_COLUMNS);

    gnome_cmd_data_set_int ("/advrename/sep_value", data->priv->advrename_defaults->sep_value);

    gnome_cmd_data_set_int ("/options/template-history-size", g_list_length (data->priv->advrename_defaults->templates->ents));
    write_string_history ("/template-history/template%d", data->priv->advrename_defaults->templates->ents);

    gnome_cmd_data_set_int ("/options/counter_start", data->priv->advrename_defaults->counter_start);
    gnome_cmd_data_set_int ("/options/counter_precision", data->priv->advrename_defaults->counter_precision);
    gnome_cmd_data_set_int ("/options/counter_increment", data->priv->advrename_defaults->counter_increment);

    gnome_cmd_data_set_int ("/options/rename-history-size",g_list_length (data->priv->advrename_defaults->patterns));
    write_string_history ("/rename-history-from/from%d", from);
    write_string_history ("/rename-history-to/to%d", to);
    write_string_history ("/rename-history-csens/csens%d", csens);
}


inline void write_local_bookmarks ()
{
    GnomeCmdCon *con = gnome_cmd_con_list_get_home (data->priv->con_list);
    GList *tmp, *bookmarks;
    GList *names = NULL;
    GList *paths = NULL;

    for (tmp = bookmarks = gnome_cmd_con_get_bookmarks (con)->bookmarks; tmp; tmp = tmp->next)
    {
        GnomeCmdBookmark *bookmark = (GnomeCmdBookmark *) tmp->data;
        names = g_list_append (names, bookmark->name);
        paths = g_list_append (paths, bookmark->path);
    }

    gnome_cmd_data_set_int ("/local_bookmarks/count", g_list_length (bookmarks));
    write_string_history ("/local_bookmarks/name%d", names);
    write_string_history ("/local_bookmarks/path%d", paths);
}


inline void write_smb_bookmarks ()
{
    GnomeCmdCon *con = gnome_cmd_con_list_get_smb (data->priv->con_list);
    GList *tmp, *bookmarks;
    GList *names = NULL;
    GList *paths = NULL;

    for (tmp = bookmarks = gnome_cmd_con_get_bookmarks (con)->bookmarks; tmp; tmp = tmp->next)
    {
        GnomeCmdBookmark *bookmark = (GnomeCmdBookmark *) tmp->data;
        names = g_list_append (names, bookmark->name);
        paths = g_list_append (paths, bookmark->path);
    }

    gnome_cmd_data_set_int ("/smb_bookmarks/count", g_list_length (bookmarks));
    write_string_history ("/smb_bookmarks/name%d", names);
    write_string_history ("/smb_bookmarks/path%d", paths);
}


inline void write_auto_load_plugins ()
{
    gnome_cmd_data_set_int ("/plugins/count",g_list_length (data->priv->auto_load_plugins));
    write_string_history ("/plugins/auto_load%d",data->priv->auto_load_plugins);
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


inline GList *load_string_history (gchar *format, gint size)
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


inline void load_cmdline_history ()
{
    data->priv->cmdline_history = load_string_history ("/cmdline-history/line%d", -1);
}


inline void load_search_defaults ()
{
    data->priv->search_defaults = g_new (SearchDefaults, 1);

    data->priv->search_defaults->width = gnome_cmd_data_get_int ("/search-history/width", 450);
    data->priv->search_defaults->height = gnome_cmd_data_get_int ("/search-history/height", 400);

    data->priv->search_defaults->name_patterns = load_string_history ("/search-history/name_pattern%d", -1);
    data->priv->search_defaults->content_patterns = load_string_history ("/search-history/content_pattern%d", -1);
    data->priv->search_defaults->directories = load_string_history ("/search-history/directory%d", -1);
    data->priv->search_defaults->recursive = gnome_cmd_data_get_bool ("/search-history/recursive", TRUE);
    data->priv->search_defaults->case_sens = gnome_cmd_data_get_bool ("/search-history/case_sens", FALSE);
}


inline void load_rename_history ()
{
    gint size;
    GList *from=NULL, *to=NULL, *csens=NULL;
    GList *tmp_from, *tmp_to, *tmp_csens;
    GList *templates;

    data->priv->advrename_defaults = g_new (AdvrenameDefaults, 1);

    data->priv->advrename_defaults->auto_update = gnome_cmd_data_get_int ("/advrename/template-auto-update", TRUE);
    data->priv->advrename_defaults->width = gnome_cmd_data_get_int ("/advrename/width", 450);
    data->priv->advrename_defaults->height = gnome_cmd_data_get_int ("/advrename/height", 400);

    load_uint_array ("/advrename/pat_col_widths%d",
                     advrename_dialog_default_pat_column_width,
                     ADVRENAME_DIALOG_PAT_NUM_COLUMNS);
    load_uint_array ("/advrename/res_col_widths%d",
                     advrename_dialog_default_res_column_width,
                     ADVRENAME_DIALOG_RES_NUM_COLUMNS);

    data->priv->advrename_defaults->sep_value = gnome_cmd_data_get_int ("/advrename/sep_value", 150);

    size = gnome_cmd_data_get_int ("/advrename/template-history-size", 0);
    templates = load_string_history ("/template-history/template%d", size);

    data->priv->advrename_defaults->templates = history_new (10);
    data->priv->advrename_defaults->templates->ents = templates;
    data->priv->advrename_defaults->templates->pos = templates;

    data->priv->advrename_defaults->counter_start = gnome_cmd_data_get_int ("/advrename/counter_start", 1);
    data->priv->advrename_defaults->counter_precision = gnome_cmd_data_get_int ("/advrename/counter_precision", 1);
    data->priv->advrename_defaults->counter_increment = gnome_cmd_data_get_int ("/advrename/counter_increment", 1);

    data->priv->advrename_defaults->patterns = NULL;
    size = gnome_cmd_data_get_int ("/advrename/rename-history-size", 0);

    tmp_from = from = load_string_history ("/rename-history-from/from%d", size);
    tmp_to = to = load_string_history ("/rename-history-to/to%d", size);
    tmp_csens = csens = load_string_history ("/rename-history-csens/csens%d", size);

    while (tmp_from && size > 0)
    {
        PatternEntry *entry = g_new0 (PatternEntry, 1);
        entry->from = (gchar *) tmp_from->data;
        entry->to = (gchar *) tmp_to->data;
        entry->case_sens = ((gchar *) tmp_csens->data)[0] == 'T';

        tmp_from = tmp_from->next;
        tmp_to = tmp_to->next;
        tmp_csens = tmp_csens->next;

        data->priv->advrename_defaults->patterns = g_list_append (
            data->priv->advrename_defaults->patterns, entry);
        size--;
    }

    g_list_free (from);
    g_list_free (to);
    g_list_free (csens);
}


inline void load_local_bookmarks ()
{
    gint size = gnome_cmd_data_get_int ("/local_bookmarks/count", 0);
    GList *names = load_string_history ("/local_bookmarks/name%d", size);
    GList *paths = load_string_history ("/local_bookmarks/path%d", size);

    GnomeCmdCon *con = gnome_cmd_con_list_get_home (data->priv->con_list);

    GList *bookmarks = NULL;

    for (gint i=0; i<size; i++)
    {
        GnomeCmdBookmark *bookmark = g_new (GnomeCmdBookmark, 1);
        bookmark->name = (gchar *) g_list_nth_data (names, i);
        bookmark->path = (gchar *) g_list_nth_data (paths, i);
        bookmark->group = gnome_cmd_con_get_bookmarks (con);
        bookmarks = g_list_append (bookmarks, bookmark);
    }

    gnome_cmd_con_get_bookmarks (con)->bookmarks = bookmarks;
}


inline void load_smb_bookmarks ()
{
    GList *bookmarks = NULL;

    gint size = gnome_cmd_data_get_int ("/smb_bookmarks/count", 0);
    GList *names = load_string_history ("/smb_bookmarks/name%d", size);
    GList *paths = load_string_history ("/smb_bookmarks/path%d", size);

    GnomeCmdCon *con = gnome_cmd_con_list_get_smb (data->priv->con_list);

    for (gint i=0; i<size; i++)
    {
        GnomeCmdBookmark *bookmark = g_new (GnomeCmdBookmark, 1);
        bookmark->name = (gchar *) g_list_nth_data (names, i);
        bookmark->path = (gchar *) g_list_nth_data (paths, i);
        bookmark->group = gnome_cmd_con_get_bookmarks (con);
        bookmarks = g_list_append (bookmarks, bookmark);
    }

    gnome_cmd_con_get_bookmarks (con)->bookmarks = bookmarks;
}


inline void load_auto_load_plugins ()
{
    gint count = gnome_cmd_data_get_int ("/plugins/count", 0);

    data->priv->auto_load_plugins = load_string_history ("/plugins/auto_load%d", count);
}


void gnome_cmd_data_free (void)
{
    if (data)
    {
        if (data->priv)
        {
            // free the connections
            gtk_object_unref (GTK_OBJECT (data->priv->con_list));

            // free the anonymous password string
            g_free (data->priv->ftp_anonymous_password);

            // free the dateformat string
            g_free (data->priv->date_format);

            // free the font name strings
            g_free (data->priv->list_font);

            // free the external programs strings
            g_free (data->priv->viewer);
            g_free (data->priv->editor);
            g_free (data->priv->differ);
            g_free (data->priv->term);

            g_free (data->priv);
        }

        g_free (data);
    }
}


void gnome_cmd_data_save (void)
{
    for (gint i=0; i<BOOKMARK_DIALOG_NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/bookmark_dialog_col_width%d", i);
        gnome_config_set_int (tmp, data->priv->bookmark_dialog_col_width[i]);
        g_free (tmp);
    }

    gnome_cmd_data_set_string ("/ftp/anonymous_password", data->priv->ftp_anonymous_password);
    gnome_cmd_data_set_int    ("/options/size_disp_mode", data->priv->size_disp_mode);
    gnome_cmd_data_set_int    ("/options/perm_disp_mode", data->priv->perm_disp_mode);
    gnome_cmd_data_set_string ("/options/date_disp_mode", data->priv->date_format);
    gnome_cmd_data_set_int    ("/options/layout", data->priv->layout);
    gnome_cmd_data_set_int    ("/options/list_row_height", data->priv->list_row_height);

    gnome_cmd_data_set_bool   ("/confirm/delete", data->priv->confirm_delete);
    gnome_cmd_data_set_int    ("/confirm/copy_overwrite", data->priv->confirm_copy_overwrite);
    gnome_cmd_data_set_int    ("/confirm/move_overwrite", data->priv->confirm_move_overwrite);

    gnome_cmd_data_set_bool   ("/options/show_unknown", data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN]);
    gnome_cmd_data_set_bool   ("/options/show_regular", data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_REGULAR]);
    gnome_cmd_data_set_bool   ("/options/show_directory", data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY]);
    gnome_cmd_data_set_bool   ("/options/show_fifo", data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_FIFO]);
    gnome_cmd_data_set_bool   ("/options/show_socket", data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_SOCKET]);
    gnome_cmd_data_set_bool   ("/options/show_char_device", data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE]);
    gnome_cmd_data_set_bool   ("/options/show_block_device", data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE]);
    gnome_cmd_data_set_bool   ("/options/show_symbolic_link", data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK]);

    gnome_cmd_data_set_bool   ("/options/hidden_filter", data->priv->filter_settings.hidden);
    gnome_cmd_data_set_bool   ("/options/backup_filter", data->priv->filter_settings.backup);

    gnome_cmd_data_set_bool   ("/sort/case_sensitive", data->priv->case_sens_sort);

    gnome_cmd_data_set_int    ("/colors/mode", data->priv->color_mode);

    gnome_cmd_data_set_color  ("/colors/norm_fg", data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg);
    gnome_cmd_data_set_color  ("/colors/norm_bg", data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg);
    gnome_cmd_data_set_color  ("/colors/sel_fg",  data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg);
    gnome_cmd_data_set_color  ("/colors/sel_bg",  data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg);
    gnome_cmd_data_set_color  ("/colors/curs_fg", data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg);
    gnome_cmd_data_set_color  ("/colors/curs_bg", data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg);

    gnome_cmd_data_set_string ("/options/list_font", data->priv->list_font);

    gnome_cmd_data_set_int    ("/options/ext_disp_mode", data->priv->ext_disp_mode);
    gnome_cmd_data_set_int    ("/options/right_mouse_button_mode", data->priv->right_mouse_button_mode);
    gnome_cmd_data_set_bool   ("/options/show_toolbar", data->priv->show_toolbar);
    gnome_cmd_data_set_int    ("/options/icon_size", data->priv->icon_size);
    gnome_cmd_data_set_int    ("/options/dev_icon_size", data->priv->dev_icon_size);
    gnome_cmd_data_set_int    ("/options/icon_scale_quality", data->priv->icon_scale_quality);
    gnome_cmd_data_set_string ("/options/theme_icon_dir", data->priv->theme_icon_dir);
    gnome_cmd_data_set_string ("/options/document_icon_dir", data->priv->document_icon_dir);
    gnome_cmd_data_set_int    ("/options/cmdline_history_length", data->priv->cmdline_history_length);
    gnome_cmd_data_set_int    ("/options/btn_relief", data->priv->btn_relief);
    gnome_cmd_data_set_int    ("/options/filter_type", data->priv->filter_type);
    gnome_cmd_data_set_bool   ("/options/list_orientation", data->priv->list_orientation);
    gnome_cmd_data_set_bool   ("/options/conbuttons_visibility", data->priv->conbuttons_visibility);
    gnome_cmd_data_set_bool   ("/options/cmdline_visibility", data->priv->cmdline_visibility);

    gnome_cmd_data_set_bool   ("/programs/honor_expect_uris", data->priv->honor_expect_uris);
    gnome_cmd_data_set_bool   ("/programs/use_internal_viewer", data->priv->use_internal_viewer);
    gnome_cmd_data_set_bool   ("/programs/alt_quick_search", data->priv->alt_quick_search);
    gnome_cmd_data_set_bool   ("/programs/skip_mounting", data->priv->skip_mounting);
    gnome_cmd_data_set_bool   ("/programs/toolbar_visibility", data->priv->toolbar_visibility);
    gnome_cmd_data_set_bool   ("/programs/buttonbar_visibility", data->priv->buttonbar_visibility);

    if (data->priv->symlink_prefix && *data->priv->symlink_prefix && strcmp(data->priv->symlink_prefix,_("link to %s"))!=0)
        gnome_cmd_data_set_string ("/options/symlink_prefix", data->priv->symlink_prefix);
    else
        gnome_cmd_data_set_string ("/options/symlink_prefix", "");

    gnome_cmd_data_set_int    ("/options/main_win_pos_x", data->priv->main_win_pos[0]);
    gnome_cmd_data_set_int    ("/options/main_win_pos_y", data->priv->main_win_pos[1]);

    gnome_cmd_data_set_int    ("/options/sort_column_left", data->priv->sort_column[LEFT]);
    gnome_cmd_data_set_bool   ("/options/sort_direction_left", data->priv->sort_direction[LEFT]);
    gnome_cmd_data_set_int    ("/options/sort_column_right", data->priv->sort_column[RIGHT]);
    gnome_cmd_data_set_bool   ("/options/sort_direction_right", data->priv->sort_direction[RIGHT]);

    gnome_cmd_data_set_string ("/programs/viewer",data->priv->viewer);
    gnome_cmd_data_set_string ("/programs/editor",data->priv->editor);
    gnome_cmd_data_set_string ("/programs/differ",data->priv->differ);
    gnome_cmd_data_set_string ("/programs/term",data->priv->term);

    gnome_cmd_data_set_bool   ("/devices/only_icon",data->priv->device_only_icon);
    gnome_cmd_data_set_int    ("/options/dir_cache_size",data->priv->dir_cache_size);
    gnome_cmd_data_set_bool   ("/colors/use_ls_colors",data->priv->use_ls_colors);

    gnome_cmd_data_set_string ("/quick-connect/host",data->priv->quick_connect_host);
    gnome_cmd_data_set_int    ("/quick-connect/port",data->priv->quick_connect_port);
    gnome_cmd_data_set_string ("/quick-connect/user",data->priv->quick_connect_user);

    gnome_config_set_int ("/gnome-commander-size/main_win/width", data->priv->main_win_width);
    gnome_config_set_int ("/gnome-commander-size/main_win/height", data->priv->main_win_height);

    for (gint i=0; i<FILE_LIST_NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/fs_col_width%d", i);
        gnome_config_set_int (tmp, data->priv->fs_col_width[i]);
        g_free (tmp);
    }

    gnome_cmd_data_set_string ("/options/start_dir_left", data->priv->start_dirs[LEFT]);
    gnome_cmd_data_set_string ("/options/start_dir_right", data->priv->start_dirs[RIGHT]);
    gnome_cmd_data_set_string ("/defaults/last_pattern", data->priv->last_pattern);
    gnome_cmd_data_set_string ("/defaults/backup_pattern", data->priv->backup_pattern);

    gnome_cmd_data_set_int ("/options/main_win_state",(gint) data->priv->main_win_state);

    write_cmdline_history ();
    //write_dir_history ();

    write_ftp_servers ("connections");
    write_devices ("devices");
    write_fav_apps ("fav-apps");
    write_search_defaults ();
    write_rename_history ();
    write_local_bookmarks ();
    write_smb_bookmarks ();
    write_auto_load_plugins ();

    gnome_config_sync ();
}


void gnome_cmd_data_load (void)
{
    gchar *document_icon_dir = g_strdup_printf ("%s/share/pixmaps/document-icons/", GNOME_PREFIX);
    gchar *theme_icon_dir    = g_strdup_printf ("%s/mime-icons", PIXMAPS_DIR);

    data = g_new0 (GnomeCmdData, 1);
    data->priv = g_new0 (GnomeCmdDataPrivate, 1);

    data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].respect_theme = FALSE;
    data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg = gdk_color_new (0xffff,0xffff,0xffff);
    data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg = gdk_color_new (0,0,0x4444);
    data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg = gdk_color_new (0xffff,0,0);
    data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg = gdk_color_new (0,0,0x4444);
    data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg = gdk_color_new (0,0,0);
    data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    data->priv->color_themes[GNOME_CMD_COLOR_MODERN].respect_theme = FALSE;
    data->priv->color_themes[GNOME_CMD_COLOR_MODERN].norm_fg = gdk_color_new (0,0,0);
    data->priv->color_themes[GNOME_CMD_COLOR_MODERN].norm_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    data->priv->color_themes[GNOME_CMD_COLOR_MODERN].sel_fg = gdk_color_new (0xffff,0,0);
    data->priv->color_themes[GNOME_CMD_COLOR_MODERN].sel_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    data->priv->color_themes[GNOME_CMD_COLOR_MODERN].curs_fg = gdk_color_new (0xffff,0xffff,0xffff);
    data->priv->color_themes[GNOME_CMD_COLOR_MODERN].curs_bg = gdk_color_new (0,0,0x4444);

    data->priv->color_themes[GNOME_CMD_COLOR_FUSION].respect_theme = FALSE;
    data->priv->color_themes[GNOME_CMD_COLOR_FUSION].norm_fg = gdk_color_new (0x8080,0xffff,0xffff);
    data->priv->color_themes[GNOME_CMD_COLOR_FUSION].norm_bg = gdk_color_new (0,0x4040,0x8080);
    data->priv->color_themes[GNOME_CMD_COLOR_FUSION].sel_fg = gdk_color_new (0xffff,0xffff,0);
    data->priv->color_themes[GNOME_CMD_COLOR_FUSION].sel_bg = gdk_color_new (0,0x4040,0x8080);
    data->priv->color_themes[GNOME_CMD_COLOR_FUSION].curs_fg = gdk_color_new (0,0,0x8080);
    data->priv->color_themes[GNOME_CMD_COLOR_FUSION].curs_bg = gdk_color_new (0,0x8080,0x8080);

    data->priv->color_themes[GNOME_CMD_COLOR_CLASSIC].respect_theme = FALSE;
    data->priv->color_themes[GNOME_CMD_COLOR_CLASSIC].norm_fg = gdk_color_new (0xffff,0xffff,0xffff);
    data->priv->color_themes[GNOME_CMD_COLOR_CLASSIC].norm_bg = gdk_color_new (0,0,0x4444);
    data->priv->color_themes[GNOME_CMD_COLOR_CLASSIC].sel_fg = gdk_color_new (0xffff,0xffff,0);
    data->priv->color_themes[GNOME_CMD_COLOR_CLASSIC].sel_bg = gdk_color_new (0,0,0x4444);
    data->priv->color_themes[GNOME_CMD_COLOR_CLASSIC].curs_fg = gdk_color_new (0,0,0);
    data->priv->color_themes[GNOME_CMD_COLOR_CLASSIC].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    data->priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].respect_theme = FALSE;
    data->priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].norm_fg = gdk_color_new (0,0xffff,0xffff);
    data->priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].norm_bg = gdk_color_new (0,0,0x8080);
    data->priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].sel_fg = gdk_color_new (0xffff,0xffff,0);
    data->priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].sel_bg = gdk_color_new (0x8080,0x8080,0x8080);
    data->priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].curs_fg = gdk_color_new (0,0,0);
    data->priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    data->priv->color_themes[GNOME_CMD_COLOR_NONE].respect_theme = TRUE;
    data->priv->color_themes[GNOME_CMD_COLOR_NONE].norm_fg = NULL;
    data->priv->color_themes[GNOME_CMD_COLOR_NONE].norm_bg = NULL;
    data->priv->color_themes[GNOME_CMD_COLOR_NONE].sel_fg = NULL;
    data->priv->color_themes[GNOME_CMD_COLOR_NONE].sel_bg = NULL;
    data->priv->color_themes[GNOME_CMD_COLOR_NONE].curs_fg = NULL;
    data->priv->color_themes[GNOME_CMD_COLOR_NONE].curs_bg = NULL;

    data->priv->ftp_anonymous_password = gnome_cmd_data_get_string ("/ftp/anonymous_password", "you@provider.com");

    data->priv->size_disp_mode = (GnomeCmdSizeDispMode) gnome_cmd_data_get_int ("/options/size_disp_mode", GNOME_CMD_SIZE_DISP_MODE_POWERED);
    data->priv->perm_disp_mode = (GnomeCmdPermDispMode) gnome_cmd_data_get_int ("/options/perm_disp_mode", GNOME_CMD_PERM_DISP_MODE_TEXT);

#ifdef HAVE_LOCALE_H
    data->priv->date_format = gnome_cmd_data_get_string ("/options/date_disp_mode", "%x %R");
#else
    data->priv->date_format = gnome_cmd_data_get_string ("/options/date_disp_mode", "%D %R");
#endif

    data->priv->layout = (GnomeCmdLayout) gnome_cmd_data_get_int ("/options/layout", GNOME_CMD_LAYOUT_MIME_ICONS);

    data->priv->list_row_height = gnome_cmd_data_get_int ("/options/list_row_height", 16);

    data->priv->confirm_delete = gnome_cmd_data_get_bool ("/confirm/delete", TRUE);
    data->priv->confirm_copy_overwrite = (GnomeCmdConfirmOverwriteMode) gnome_cmd_data_get_int ("/confirm/copy_overwrite", GNOME_CMD_CONFIRM_OVERWRITE_QUERY);
    data->priv->confirm_move_overwrite = (GnomeCmdConfirmOverwriteMode) gnome_cmd_data_get_int ("/confirm/move_overwrite", GNOME_CMD_CONFIRM_OVERWRITE_QUERY);

    data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN] =
        gnome_cmd_data_get_bool ("/options/show_unknown", FALSE);

    data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_REGULAR] =
        gnome_cmd_data_get_bool ("/options/show_regular", FALSE);

    data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY] =
        gnome_cmd_data_get_bool ("/options/show_directory", FALSE);

    data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_FIFO] =
        gnome_cmd_data_get_bool ("/options/show_fifo", FALSE);

    data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_SOCKET] =
        gnome_cmd_data_get_bool ("/options/show_socket", FALSE);

    data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE] =
        gnome_cmd_data_get_bool ("/options/show_char_device", FALSE);

    data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE] =
        gnome_cmd_data_get_bool ("/options/show_block_device", FALSE);

    data->priv->filter_settings.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK] =
        gnome_cmd_data_get_bool ("/options/show_symbolic_link", FALSE);

    data->priv->filter_settings.hidden = gnome_cmd_data_get_bool ("/options/hidden_filter", TRUE);
    data->priv->filter_settings.backup = gnome_cmd_data_get_bool ("/options/backup_filter", TRUE);

    data->priv->case_sens_sort = gnome_cmd_data_get_bool ("/sort/case_sensitive", TRUE);

    data->priv->main_win_width = get_int ("/gnome-commander-size/main_win/width", 600);
    data->priv->main_win_height = get_int ("/gnome-commander-size/main_win/height", 400);

    for (gint i=0; i<FILE_LIST_NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/fs_col_width%d", i);
        data->priv->fs_col_width[i] = get_int (tmp, file_list_column[i].default_width);
        g_free (tmp);
    }

    for (gint i=0; i<BOOKMARK_DIALOG_NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/bookmark_dialog_col_width%d", i);
        data->priv->bookmark_dialog_col_width[i] = get_int (tmp, bookmark_dialog_default_column_width[i]);
        g_free (tmp);
    }

    data->priv->color_mode = (GnomeCmdColorMode) gnome_cmd_data_get_int ("/colors/mode", GNOME_CMD_COLOR_DEEP_BLUE);

    gnome_cmd_data_get_color ("/colors/norm_fg", data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg);
    gnome_cmd_data_get_color ("/colors/norm_bg", data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg);
    gnome_cmd_data_get_color ("/colors/sel_fg",  data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg);
    gnome_cmd_data_get_color ("/colors/sel_bg",  data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg);
    gnome_cmd_data_get_color ("/colors/curs_fg", data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg);
    gnome_cmd_data_get_color ("/colors/curs_bg", data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg);

    data->priv->list_font = gnome_cmd_data_get_string ("/options/list_font", "-misc-fixed-medium-r-normal-*-10-*-*-*-c-*-iso8859-1");

    data->priv->ext_disp_mode = (GnomeCmdExtDispMode) gnome_cmd_data_get_int ("/options/ext_disp_mode", GNOME_CMD_EXT_DISP_BOTH);
    data->priv->right_mouse_button_mode = (GnomeCmdRightMouseButtonMode) gnome_cmd_data_get_int ("/options/right_mouse_button_mode",RIGHT_BUTTON_POPUPS_MENU);
    data->priv->show_toolbar = gnome_cmd_data_get_bool ("/options/show_toolbar", TRUE);
    data->priv->icon_size = gnome_cmd_data_get_int ("/options/icon_size", 16);
    data->priv->dev_icon_size = gnome_cmd_data_get_int ("/options/dev_icon_size", 16);
    data->priv->icon_scale_quality = (GdkInterpType) gnome_cmd_data_get_int ("/options/icon_scale_quality", GDK_INTERP_HYPER);
    data->priv->theme_icon_dir = gnome_cmd_data_get_string ("/options/theme_icon_dir", theme_icon_dir);
    g_free (theme_icon_dir);
    data->priv->document_icon_dir = gnome_cmd_data_get_string ("/options/document_icon_dir", document_icon_dir);
    g_free (document_icon_dir);
    data->priv->cmdline_history_length = gnome_cmd_data_get_int ("/options/cmdline_history_length", 16);
    data->priv->btn_relief = (GtkReliefStyle) gnome_cmd_data_get_int ("/options/btn_relief", GTK_RELIEF_NONE);
    data->priv->filter_type = (FilterType) gnome_cmd_data_get_int ("/options/filter_type", FILTER_TYPE_FNMATCH);
    data->priv->list_orientation = gnome_cmd_data_get_bool ("/options/list_orientation", FALSE);
    data->priv->conbuttons_visibility = gnome_cmd_data_get_bool ("/options/conbuttons_visibility", TRUE);
    data->priv->cmdline_visibility = gnome_cmd_data_get_bool ("/options/cmdline_visibility", TRUE);
    data->priv->gui_update_rate = gnome_cmd_data_get_int ("/options/gui_update_rate", DEFAULT_GUI_UPDATE_RATE);
    data->priv->main_win_pos[0] = gnome_cmd_data_get_int ("/options/main_win_pos_x", -1);
    data->priv->main_win_pos[1] = gnome_cmd_data_get_int ("/options/main_win_pos_y", -1);

    if (data->priv->gui_update_rate < MIN_GUI_UPDATE_RATE)
        data->priv->gui_update_rate = MIN_GUI_UPDATE_RATE;
    if (data->priv->gui_update_rate > MAX_GUI_UPDATE_RATE)
        data->priv->gui_update_rate = MAX_GUI_UPDATE_RATE;

    data->priv->honor_expect_uris = gnome_cmd_data_get_bool ("/programs/honor_expect_uris", FALSE);
    data->priv->use_internal_viewer = gnome_cmd_data_get_bool ("/programs/use_internal_viewer", TRUE);
    data->priv->alt_quick_search = gnome_cmd_data_get_bool ("/programs/alt_quick_search", FALSE);
    data->priv->skip_mounting = gnome_cmd_data_get_bool ("/programs/skip_mounting", FALSE);
    data->priv->toolbar_visibility = gnome_cmd_data_get_bool ("/programs/toolbar_visibility", TRUE);
    data->priv->buttonbar_visibility = gnome_cmd_data_get_bool ("/programs/buttonbar_visibility", TRUE);

    data->priv->symlink_prefix = gnome_cmd_data_get_string ("/options/symlink_prefix", _("link to %s"));
    if (!*data->priv->symlink_prefix || strcmp(data->priv->symlink_prefix, _("link to %s"))==0)
    {
        g_free(data->priv->symlink_prefix);
        data->priv->symlink_prefix = NULL;
    }

    data->priv->sort_column[LEFT] = gnome_cmd_data_get_int ("/options/sort_column_left", FILE_LIST_COLUMN_NAME);
    data->priv->sort_direction[LEFT] = gnome_cmd_data_get_bool ("/options/sort_direction_left", FILE_LIST_SORT_ASCENDING);
    data->priv->sort_column[RIGHT] = gnome_cmd_data_get_int ("/options/sort_column_right", FILE_LIST_COLUMN_NAME);
    data->priv->sort_direction[RIGHT] = gnome_cmd_data_get_bool ("/options/sort_direction_right", FILE_LIST_SORT_ASCENDING);

    data->priv->viewer = gnome_cmd_data_get_string ("/programs/viewer", "gedit %s");
    data->priv->editor = gnome_cmd_data_get_string ("/programs/editor", "gedit %s");
    data->priv->differ = gnome_cmd_data_get_string ("/programs/differ", "meld %s");
    data->priv->term   = gnome_cmd_data_get_string ("/programs/term", "gnome-terminal -e %s");

    data->priv->device_only_icon = gnome_cmd_data_get_bool ("/devices/only_icon", FALSE);
    data->priv->dir_cache_size = gnome_cmd_data_get_int ("/options/dir_cache_size", 10);
    data->priv->use_ls_colors = gnome_cmd_data_get_bool ("/colors/use_ls_colors", FALSE);

    data->priv->start_dirs[LEFT] = gnome_cmd_data_get_string ("/options/start_dir_left", g_get_home_dir ());
    data->priv->start_dirs[RIGHT] = gnome_cmd_data_get_string ("/options/start_dir_right", g_get_home_dir ());

    data->priv->last_pattern = gnome_cmd_data_get_string ("/defaults/last_pattern", "");
    data->priv->backup_pattern = gnome_cmd_data_get_string ("/defaults/backup_pattern", "*~;*.bak");
    data->priv->backup_pattern_list = patlist_new (data->priv->backup_pattern);

    data->priv->quick_connect_host = gnome_cmd_data_get_string ("/quick-connect/host", "ftp.gnome.org");
    data->priv->quick_connect_port = gnome_cmd_data_get_int    ("/quick-connect/port", 21);
    data->priv->quick_connect_user = gnome_cmd_data_get_string ("/quick-connect/user", "anonymous");

    data->priv->main_win_state = (GdkWindowState) gnome_cmd_data_get_int ("/options/main_win_state", (gint) GDK_WINDOW_STATE_MAXIMIZED);

    load_cmdline_history ();
    //load_dir_history ();
    load_search_defaults ();
    load_rename_history ();
    load_auto_load_plugins ();

    set_vfs_volume_monitor();

    static struct
    {
        guint code;
        gchar *name;
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
}


void gnome_cmd_data_load_more (void)
{
    data->priv->con_list = gnome_cmd_con_list_new ();
    gnome_cmd_con_list_begin_update (data->priv->con_list);
    load_devices ("devices");
    load_ftp_servers ("connections") || load_ftp_servers ("ftp-servers");
    gnome_cmd_con_list_end_update (data->priv->con_list);

    load_fav_apps ("fav-apps");
    load_local_bookmarks ();
    load_smb_bookmarks ();
}


gpointer gnome_cmd_data_get_con_list (void)
{
    return data->priv->con_list;
}


const gchar *gnome_cmd_data_get_ftp_anonymous_password (void)
{
    return data->priv->ftp_anonymous_password;
}


void gnome_cmd_data_set_ftp_anonymous_password (const gchar *pw)
{
    if (data->priv->ftp_anonymous_password)
        g_free (data->priv->ftp_anonymous_password);

    data->priv->ftp_anonymous_password = g_strdup (pw);
}


void gnome_cmd_data_add_fav_app (GnomeCmdApp *app)
{
    g_return_if_fail (app != NULL);

    data->priv->fav_apps = g_list_append (data->priv->fav_apps, app);
}


void gnome_cmd_data_remove_fav_app (GnomeCmdApp *app)
{
    g_return_if_fail (app != NULL);

    data->priv->fav_apps = g_list_remove (data->priv->fav_apps, app);
}


GList *gnome_cmd_data_get_fav_apps (void)
{
    return data->priv->fav_apps;
}


void gnome_cmd_data_set_fav_apps (GList *apps)
{
    data->priv->fav_apps = apps;
}


GnomeCmdSizeDispMode gnome_cmd_data_get_size_disp_mode (void)
{
    return data->priv->size_disp_mode;
}


void
gnome_cmd_data_set_size_disp_mode (GnomeCmdSizeDispMode mode)
{
    data->priv->size_disp_mode = mode;
}


GnomeCmdPermDispMode gnome_cmd_data_get_perm_disp_mode (void)
{
    return data->priv->perm_disp_mode;
}

void gnome_cmd_data_set_perm_disp_mode (GnomeCmdPermDispMode mode)
{
    data->priv->perm_disp_mode = mode;
}


GnomeCmdDateFormat gnome_cmd_data_get_date_format (void)
{
    return data->priv->date_format;
}


void gnome_cmd_data_set_date_format (GnomeCmdDateFormat format)
{
    if (data->priv->date_format)
        g_free (data->priv->date_format);

    data->priv->date_format = g_strdup (format);
}


GnomeCmdLayout gnome_cmd_data_get_layout (void)
{
    return data->priv->layout;
}


void gnome_cmd_data_set_layout (GnomeCmdLayout layout)
{
    data->priv->layout = layout;
}


GnomeCmdColorMode gnome_cmd_data_get_color_mode (void)
{
    return data->priv->color_mode;
}


void gnome_cmd_data_set_color_mode (GnomeCmdColorMode mode)
{
    data->priv->color_mode = mode;
}


GnomeCmdColorTheme *gnome_cmd_data_get_current_color_theme (void)
{
    return &data->priv->color_themes[data->priv->color_mode];
}


GnomeCmdColorTheme *gnome_cmd_data_get_custom_color_theme (void)
{
    return &data->priv->color_themes[GNOME_CMD_COLOR_CUSTOM];
}


gint gnome_cmd_data_get_list_row_height (void)
{
    return data->priv->list_row_height;
}


void gnome_cmd_data_set_list_row_height (gint height)
{
    data->priv->list_row_height = height;
}


void gnome_cmd_data_set_ext_disp_mode (GnomeCmdExtDispMode mode)
{
    data->priv->ext_disp_mode = mode;
}


GnomeCmdExtDispMode gnome_cmd_data_get_ext_disp_mode (void)
{
    return data->priv->ext_disp_mode;
}


void gnome_cmd_data_set_main_win_size (gint width, gint height)
{
    data->priv->main_win_width = width;
    data->priv->main_win_height = height;
}


void gnome_cmd_data_get_main_win_size (gint *width, gint *height)
{
    *width = data->priv->main_win_width;
    *height = data->priv->main_win_height;
}


void gnome_cmd_data_set_viewer (const gchar *command)
{
    g_free (data->priv->viewer);
    data->priv->viewer = g_strdup (command);
}


void gnome_cmd_data_set_editor (const gchar *command)
{
    g_free (data->priv->editor);
    data->priv->editor = g_strdup (command);
}


void gnome_cmd_data_set_differ (const gchar *command)
{
    g_free (data->priv->differ);
    data->priv->differ = g_strdup (command);
}


void gnome_cmd_data_set_term (const gchar *term)
{
    g_free (data->priv->term);
    data->priv->term = g_strdup (term);
}


const gchar *gnome_cmd_data_get_viewer (void)
{
    return data->priv->viewer;
}


const gchar *gnome_cmd_data_get_editor (void)
{
    return data->priv->editor;
}


const gchar *gnome_cmd_data_get_differ (void)
{
    return data->priv->differ;
}


const gchar *gnome_cmd_data_get_term (void)
{
    return data->priv->term;
}


gboolean gnome_cmd_data_get_case_sens_sort (void)
{
    return data->priv->case_sens_sort;
}


void gnome_cmd_data_set_case_sens_sort (gboolean value)
{
    data->priv->case_sens_sort = value;
}


gboolean gnome_cmd_data_get_confirm_delete (void)
{
    return data->priv->confirm_delete;
}


void gnome_cmd_data_set_confirm_delete (gboolean value)
{
    data->priv->confirm_delete = value;
}


GnomeCmdConfirmOverwriteMode gnome_cmd_data_get_confirm_overwrite_copy (void)
{
    return data->priv->confirm_copy_overwrite;
}


void gnome_cmd_data_set_confirm_overwrite_copy (GnomeCmdConfirmOverwriteMode value)
{
    data->priv->confirm_copy_overwrite = value;
}


GnomeCmdConfirmOverwriteMode gnome_cmd_data_get_confirm_overwrite_move (void)
{
    return data->priv->confirm_move_overwrite;
}


void gnome_cmd_data_set_confirm_overwrite_move (GnomeCmdConfirmOverwriteMode value)
{
    data->priv->confirm_move_overwrite = value;
}


const gchar *gnome_cmd_data_get_list_font (void)
{
    return data->priv->list_font;
}


void gnome_cmd_data_set_list_font (const gchar *list_font)
{
    g_free (data->priv->list_font);
    data->priv->list_font = g_strdup (list_font);
}


void gnome_cmd_data_set_right_mouse_button_mode (GnomeCmdRightMouseButtonMode mode)
{
    data->priv->right_mouse_button_mode = mode;
}


GnomeCmdRightMouseButtonMode gnome_cmd_data_get_right_mouse_button_mode (void)
{
    return data->priv->right_mouse_button_mode;
}


void gnome_cmd_data_set_show_toolbar (gboolean value)
{
    data->priv->show_toolbar = value;
}


gboolean
gnome_cmd_data_get_show_toolbar (void)
{
    return data->priv->show_toolbar;
}


guint gnome_cmd_data_get_icon_size (void)
{
    return data->priv->icon_size;
}


void gnome_cmd_data_set_icon_size (guint size)
{
    data->priv->icon_size = size;
}


guint gnome_cmd_data_get_dev_icon_size (void)
{
    return data->priv->dev_icon_size;
}


GdkInterpType gnome_cmd_data_get_icon_scale_quality (void)
{
    return data->priv->icon_scale_quality;
}


void
gnome_cmd_data_set_icon_scale_quality (GdkInterpType quality)
{
    data->priv->icon_scale_quality = quality;
}


const gchar *gnome_cmd_data_get_theme_icon_dir (void)
{
    return data->priv->theme_icon_dir;
}


void gnome_cmd_data_set_theme_icon_dir (const gchar *dir)
{
    g_free (data->priv->theme_icon_dir);

    data->priv->theme_icon_dir = g_strdup (dir);
}


const gchar *gnome_cmd_data_get_document_icon_dir (void)
{
    return data->priv->document_icon_dir;
}


void gnome_cmd_data_set_document_icon_dir (const gchar *dir)
{
    g_free (data->priv->document_icon_dir);

    data->priv->document_icon_dir = g_strdup (dir);
}


void gnome_cmd_data_set_fs_col_width (guint column, gint width)
{
    if (column > FILE_LIST_NUM_COLUMNS)
        return;

    data->priv->fs_col_width[column] = width;
}


gint gnome_cmd_data_get_fs_col_width (guint column)
{
    if (column > FILE_LIST_NUM_COLUMNS)
        return 0;

    return data->priv->fs_col_width[column];
}


void gnome_cmd_data_set_bookmark_dialog_col_width (guint column, gint width)
{
    data->priv->bookmark_dialog_col_width[column] = width;
}


gint gnome_cmd_data_get_bookmark_dialog_col_width (guint column)
{
    return data->priv->bookmark_dialog_col_width[column];
}


gint gnome_cmd_data_get_cmdline_history_length (void)
{
    return data->priv->cmdline_history_length;
}


void gnome_cmd_data_set_cmdline_history_length (gint length)
{
    data->priv->cmdline_history_length = length;
}


GList *gnome_cmd_data_get_cmdline_history (void)
{
    return data->priv->cmdline_history;
}


void gnome_cmd_data_set_button_relief (GtkReliefStyle relief)
{
    data->priv->btn_relief = relief;
}


GtkReliefStyle gnome_cmd_data_get_button_relief (void)
{
    return data->priv->btn_relief;
}


void gnome_cmd_data_set_filter_type (FilterType type)
{
    data->priv->filter_type = type;
}


FilterType gnome_cmd_data_get_filter_type (void)
{
    return data->priv->filter_type;
}


FilterSettings *gnome_cmd_data_get_filter_settings (void)
{
    return &data->priv->filter_settings;
}


gboolean gnome_cmd_data_get_type_filter (GnomeVFSFileType type)
{
    return data->priv->filter_settings.file_types[type];
}


void gnome_cmd_data_set_hidden_filter (gboolean hide)
{
    data->priv->filter_settings.hidden = hide;
}


gboolean gnome_cmd_data_get_hidden_filter (void)
{
    return data->priv->filter_settings.hidden;
}


gboolean gnome_cmd_data_get_backup_filter (void)
{
    return data->priv->filter_settings.backup;
}


gboolean gnome_cmd_data_get_other_filter (void)
{
    return FALSE;
}


void gnome_cmd_data_set_device_only_icon (gboolean value)
{
    data->priv->device_only_icon = value;
}


gboolean gnome_cmd_data_get_device_only_icon (void)
{
    return data->priv->device_only_icon;
}


gint gnome_cmd_data_get_dir_cache_size (void)
{
    return data->priv->dir_cache_size;
}


void gnome_cmd_data_set_dir_cache_size (gint size)
{
    data->priv->dir_cache_size = size;
}


gboolean gnome_cmd_data_get_use_ls_colors (void)
{
    return data->priv->use_ls_colors;
}


void gnome_cmd_data_set_use_ls_colors (gboolean value)
{
    data->priv->use_ls_colors = value;
}


SearchDefaults *gnome_cmd_data_get_search_defaults (void)
{
    return data->priv->search_defaults;
}


const gchar *gnome_cmd_data_get_quick_connect_host (void)
{
    return data->priv->quick_connect_host;
}


const gchar *gnome_cmd_data_get_quick_connect_user (void)
{
    return data->priv->quick_connect_user;
}


gint gnome_cmd_data_get_quick_connect_port (void)
{
    return data->priv->quick_connect_port;
}


void gnome_cmd_data_set_quick_connect_host (const gchar *value)
{
    g_free (data->priv->quick_connect_host);
    data->priv->quick_connect_host = g_strdup (value);
}


void gnome_cmd_data_set_quick_connect_user (const gchar *value)
{
    g_free (data->priv->quick_connect_user);
    data->priv->quick_connect_user = g_strdup (value);
}


void gnome_cmd_data_set_quick_connect_port (gint value)
{
    data->priv->quick_connect_port = value;
}


GnomeCmdBookmarkGroup *gnome_cmd_data_get_local_bookmarks (void)
{
    return gnome_cmd_con_get_bookmarks (gnome_cmd_con_list_get_home (data->priv->con_list));
}


gboolean gnome_cmd_data_get_honor_expect_uris (void)
{
    return data->priv->honor_expect_uris;
}


void gnome_cmd_data_set_honor_expect_uris (gboolean value)
{
    data->priv->honor_expect_uris = value;
}


gboolean gnome_cmd_data_get_use_internal_viewer (void)
{
    return data->priv->use_internal_viewer;
}


void gnome_cmd_data_set_use_internal_viewer (gboolean value)
{
    data->priv->use_internal_viewer = value;
}


gboolean gnome_cmd_data_get_alt_quick_search (void)
{
    return data->priv->alt_quick_search;
}


void gnome_cmd_data_set_alt_quick_search (gboolean value)
{
    data->priv->alt_quick_search = value;
}


gboolean gnome_cmd_data_get_skip_mounting (void)
{
    return data->priv->skip_mounting;
}


void gnome_cmd_data_set_skip_mounting (gboolean value)
{
    data->priv->skip_mounting = value;
}


gboolean gnome_cmd_data_get_toolbar_visibility (void)
{
    return data->priv->toolbar_visibility;
}


gboolean gnome_cmd_data_get_buttonbar_visibility (void)
{
    return data->priv->buttonbar_visibility;
}


void gnome_cmd_data_set_toolbar_visibility (gboolean value)
{
    data->priv->toolbar_visibility = value;
}


void gnome_cmd_data_set_buttonbar_visibility (gboolean value)
{
    data->priv->buttonbar_visibility = value;
}


AdvrenameDefaults *gnome_cmd_data_get_advrename_defaults (void)
{
    return data->priv->advrename_defaults;
}


void gnome_cmd_data_set_list_orientation (gboolean vertical)
{
    data->priv->list_orientation = vertical;
}


gboolean gnome_cmd_data_get_list_orientation (void)
{
    return data->priv->list_orientation;
}


gboolean gnome_cmd_data_get_conbuttons_visibility (void)
{
    return data->priv->conbuttons_visibility;
}


void gnome_cmd_data_set_conbuttons_visibility (gboolean value)
{
    data->priv->conbuttons_visibility = value;
}


gboolean gnome_cmd_data_get_cmdline_visibility (void)
{
    return data->priv->cmdline_visibility;
}


void gnome_cmd_data_set_cmdline_visibility (gboolean value)
{
    data->priv->cmdline_visibility = value;
}


void gnome_cmd_data_set_start_dir (gboolean fs, const gchar *start_dir)
{
    if (data->priv->start_dirs[fs])
        g_free (data->priv->start_dirs[fs]);

    data->priv->start_dirs[fs] = g_strdup (start_dir);
}


const gchar *gnome_cmd_data_get_start_dir (gboolean fs)
{
    return data->priv->start_dirs[fs];
}


void gnome_cmd_data_set_last_pattern (const gchar *value)
{
    data->priv->last_pattern = g_strdup (value);
}


const gchar *gnome_cmd_data_get_last_pattern (void)
{
    return data->priv->last_pattern;
}


GList *gnome_cmd_data_get_auto_load_plugins ()
{
    return data->priv->auto_load_plugins;
}


void gnome_cmd_data_set_auto_load_plugins (GList *plugins)
{
    data->priv->auto_load_plugins = plugins;
}


guint gnome_cmd_data_get_gui_update_rate (void)
{
    return data->priv->gui_update_rate;
}


void gnome_cmd_data_get_sort_params (GnomeCmdFileList *fl, gint *col, gboolean *direction)
{
    if (!gnome_cmd_main_win_get_fs (main_win, LEFT) || gnome_cmd_main_win_get_fs (main_win, LEFT)->list == fl)
    {
        *col = data->priv->sort_column[LEFT];
        *direction = data->priv->sort_direction[LEFT];
    }
    else
        if (!gnome_cmd_main_win_get_fs (main_win, RIGHT) || gnome_cmd_main_win_get_fs (main_win, RIGHT)->list == fl)
        {
            *col = data->priv->sort_column[RIGHT];
            *direction = data->priv->sort_direction[RIGHT];
        }
}


void gnome_cmd_data_set_sort_params (GnomeCmdFileList *fl, gint col, gboolean direction)
{
    if (gnome_cmd_main_win_get_fs (main_win, LEFT)->list == fl)
    {
        data->priv->sort_column[LEFT] = col;
        data->priv->sort_direction[LEFT] = direction;
    }
    else
        if (gnome_cmd_main_win_get_fs (main_win, RIGHT)->list == fl)
        {
            data->priv->sort_column[RIGHT] = col;
            data->priv->sort_direction[RIGHT] = direction;
        }
}


void gnome_cmd_data_set_main_win_pos (gint x, gint y)
{
    data->priv->main_win_pos[0] = x;
    data->priv->main_win_pos[1] = y;
}


void gnome_cmd_data_get_main_win_pos (gint *x, gint *y)
{
    *x = data->priv->main_win_pos[0];
    *y = data->priv->main_win_pos[1];
}


void gnome_cmd_data_set_backup_pattern (const gchar *value)
{
    g_free (data->priv->backup_pattern);

    data->priv->backup_pattern = g_strdup (value);

    if (data->priv->backup_pattern_list)
        patlist_free (data->priv->backup_pattern_list);

    data->priv->backup_pattern_list = patlist_new (data->priv->backup_pattern);
}


const gchar *gnome_cmd_data_get_backup_pattern (void)
{
    return data->priv->backup_pattern;
}


GList *gnome_cmd_data_get_backup_pattern_list (void)
{
    return data->priv->backup_pattern_list;
}


GdkWindowState gnome_cmd_data_get_main_win_state (void)
{
    return data->priv->main_win_state;
}


void gnome_cmd_data_set_main_win_state (GdkWindowState state)
{
    data->priv->main_win_state = state;
//    data->priv->main_win_state = gdk_window_get_state (GTK_WIDGET (main_win)->window);
}


const gchar *gnome_cmd_data_get_symlink_prefix (void)
{
    return data->priv->symlink_prefix ? data->priv->symlink_prefix : _("link to %s");
}


void gnome_cmd_data_set_symlink_prefix (const gchar *value)
{
    data->priv->symlink_prefix = g_strdup (value);
}
