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
#include "gnome-cmd-xml-config.h"
#include "filter.h"
#include "utils.h"

using namespace std;


#define MAX_GUI_UPDATE_RATE 1000
#define MIN_GUI_UPDATE_RATE 10
#define DEFAULT_GUI_UPDATE_RATE 100

GnomeCmdData gnome_cmd_data;
GnomeVFSVolumeMonitor *monitor = NULL;

struct GnomeCmdData::Private
{
    GnomeCmdConList      *con_list;
    GList                *fav_apps;
    GnomeCmdDateFormat   date_format;           // NOTE: internally stored as locale (which not always defaults to UTF8), needs converting from/to UTF8 for editing and cfg load/save
    GnomeCmdColorTheme   color_themes[GNOME_CMD_NUM_COLOR_MODES];
    gchar                *list_font;
    gchar                *theme_icon_dir;
    gchar                *document_icon_dir;
    guint                bookmark_dialog_col_width[BOOKMARK_DIALOG_NUM_COLUMNS];
    gchar                *start_dirs[2];
    gchar                *last_pattern;
    GList                *auto_load_plugins;
    gint                 sort_column[2];
    gboolean             sort_direction[2];
    gint                 main_win_pos[2];
    gchar                *backup_pattern;
    GList                *backup_pattern_list;
    gchar                *symlink_prefix;

    gchar                *ftp_anonymous_password;
};


DICT<guint> gdk_key_names(GDK_VoidSymbol);
DICT<guint> gdk_modifiers_names;


void GnomeCmdData::AdvrenameConfig::Profile::reset()
{
    name.clear();
    template_string = "$N";
    regexes.clear();
    counter_start = counter_width = counter_step = 1;
    case_conversion = 0;
    trim_blanks = 3;
}


inline gint get_int (const gchar *path, int def)
{
    gboolean b = FALSE;
    gint value = gnome_config_get_int_with_default (path, &b);

    return b ? def : value;
}


inline void save_connections (const gchar *fname)
{
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir (), fname);
    FILE  *fd = fopen (path, "w");

    if (fd)
    {
        chmod (path, S_IRUSR|S_IWUSR);

        for (GList *tmp = gnome_cmd_con_list_get_all_ftp (gnome_cmd_data.priv->con_list); tmp; tmp = tmp->next)
        {
            GnomeCmdConFtp *server = GNOME_CMD_CON_FTP (tmp->data);
            GnomeCmdCon *con = GNOME_CMD_CON (server);

            if (server)
            {
                string alias;

                stringify (alias, gnome_vfs_escape_string (gnome_cmd_con_get_alias (con)));

                fprintf (fd, "U:\t%s\t%s\n", alias.c_str(), con->uri?con->uri:"");

                GnomeCmdBookmarkGroup *bookmark_group = gnome_cmd_con_get_bookmarks (con);

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


inline void save_devices (const gchar *fname)
{
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir (), fname);
    FILE *fd = fopen (path, "w");

    if (fd)
    {
        for (GList *tmp = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list); tmp; tmp = tmp->next)
        {
            GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (tmp->data);
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
        warn_print ("Failed to open the file %s for writing\n", path);

    g_free (path);
}


inline void save_fav_apps (const gchar *fname)
{
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir (), fname);
    FILE *fd = fopen (path, "w");

    if (fd)
    {
        for (GList *tmp = gnome_cmd_data.priv->fav_apps; tmp; tmp = tmp->next)
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


inline gboolean load_connections (const gchar *fname)
{
    guint prev_ftp_cons_no = g_list_length (gnome_cmd_con_list_get_all_ftp (gnome_cmd_data.priv->con_list));

    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir (), fname);
    FILE  *fd = fopen (path, "r");

    if (fd)
    {
        gchar line[1024];

        while (fgets (line, sizeof(line), fd) != NULL)
        {
            GnomeCmdConFtp *server = NULL;

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

                        if (gnome_cmd_con_list_has_alias (gnome_cmd_data.priv->con_list, alias))
                            g_warning ("%s: ignored duplicate entry: %s", path, alias);
                        else
                        {
                            const gchar *text_uri = a[2].c_str();

                            server = gnome_cmd_con_ftp_new (alias, text_uri);

                            if (!server)
                            {
                                g_warning ("Invalid URI in the '%s' file", path);
                                g_warning ("\t... %s", line);

                                //  ????
                            }
                            else
                                gnome_cmd_con_list_add_ftp (gnome_cmd_data.priv->con_list, server);
                        }

                        g_free (alias);
                    }
                    break;

                case 'S':               // format       S: alias host port user password
                    {
                        gchar alias[256], host[256], user[256], pw[256];
                        guint port2;

                        gint ret = sscanf (line, "S: %256s %256s %ud %256s %256s\n", alias, host, &port2, user, pw);

                        if (ret == 4 || ret == 5)
                        {
                            gchar *alias2 = gnome_vfs_unescape_string (alias, NULL);

                            if (gnome_cmd_con_list_has_alias (gnome_cmd_data.priv->con_list, alias2))
                                g_warning ("%s: ignored duplicate entry: %s", path, alias2);
                            else
                            {
                                gchar *host2  = gnome_vfs_unescape_string (host, NULL);
                                gchar *user2  = gnome_vfs_unescape_string (user, NULL);
                                gchar *pw2 = NULL;

                                if (ret == 5)
                                    pw2 = gnome_vfs_unescape_string (pw, NULL);

                                server = gnome_cmd_con_ftp_new (alias2, host2, port2, user2, pw2, NULL);
                                // server = gnome_cmd_con_ftp_new (alias2, gnome_cmd_con_make_ftp_uri (host2, port2, NULL, user2, pw2));

                                GnomeCmdCon *con = GNOME_CMD_CON (server);

                                con->gnome_auth = FALSE;

                                gnome_cmd_con_list_add_ftp (gnome_cmd_data.priv->con_list, server);

                                g_free (host2);
                                g_free (user2);
                                g_free (pw2);
                            }

                            g_free (alias2);
                        }
                    }
                    break;

                case 'C':               // format       C: method: alias host port remote-dir user password
                    {
                        gchar **a = g_strsplit_set(line, " \t\n", 9);
                        guint port2;

                        if (g_strv_length(a)==8             &&
                            strcmp(a[0], "C:")==0           &&
                            strcmp(a[1], "ftp:")==0         &&
                            sscanf(a[4], "%ud", &port2)==1)
                        {
                            gchar *alias2 = gnome_vfs_unescape_string (a[2], NULL);

                            if (gnome_cmd_con_list_has_alias (gnome_cmd_data.priv->con_list, alias2))
                                g_warning ("%s: ignored duplicate entry: %s", path, alias2);
                            else
                            {
                                // gchar *scheme2      = a[1];
                                gchar *host2        = gnome_vfs_unescape_string (a[3], NULL);
                                gchar *remote_dir2  = gnome_vfs_unescape_string (a[5], NULL);
                                gchar *user2        = gnome_vfs_unescape_string (a[6], NULL);
                                gchar *password2    = gnome_vfs_unescape_string (a[7], NULL);

                                server = gnome_cmd_con_ftp_new (alias2, host2, port2, user2, password2, remote_dir2);
                                // server = gnome_cmd_con_ftp_new (alias2, gnome_cmd_con_make_ftp_uri (host2, port2, remote_dir2, user2, password2));

                                GnomeCmdCon *con = GNOME_CMD_CON (server);

                                con->gnome_auth = FALSE;

                                g_free (host2);
                                g_free (remote_dir2);
                                g_free (user2);
                                g_free (password2);

                                gnome_cmd_con_list_add_ftp (gnome_cmd_data.priv->con_list, server);
                            }

                            g_free (alias2);

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

    if (!g_list_length (gnome_cmd_con_list_get_all_ftp (gnome_cmd_data.priv->con_list)))
    {
        GnomeCmdConFtp *server = gnome_cmd_con_ftp_new (_("GNOME Commander"), "ftp://anonymous@ftp.gnome.org/pub/GNOME/sources/gnome-commander/");
        gnome_cmd_con_list_add_ftp (gnome_cmd_data.priv->con_list, server);
    }

    return fd!=NULL && g_list_length (gnome_cmd_con_list_get_all_ftp (gnome_cmd_data.priv->con_list))>prev_ftp_cons_no;
}


inline gboolean vfs_is_uri_local (const char *uri)
{
    GnomeVFSURI *pURI = gnome_vfs_uri_new (uri);

    if (!pURI)
        return FALSE;

    gboolean b = gnome_vfs_uri_is_local (pURI);
    gnome_vfs_uri_unref (pURI);

    /* make sure this is actually a local path
           (gnome treats "burn://" as local, too and we don't want that)  */
    if (g_strncasecmp (uri,"file:/", 6)!=0)
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

    for (GList *tmp = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list); tmp; tmp = tmp->next)
    {
        GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (tmp->data);
        if (device && gnome_cmd_con_device_get_autovol (device))
        {
            gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
            const gchar *mountp = gnome_cmd_con_device_get_mountp (device);

            if ((strcmp(device_fn, path)==0) && (strcmp(mountp, localpath)==0))
            {
                DEBUG('m',"Remove Volume:\ndevice_fn = %s\tmountp = %s\n",
                device_fn,mountp);
                gnome_cmd_con_list_remove_device (gnome_cmd_data.priv->con_list, device);
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
        gnome_cmd_con_list_add_device (gnome_cmd_data.priv->con_list, ConDev);
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

    gnome_cmd_con_list_add_device (gnome_cmd_data.priv->con_list, ConDev);

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
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir (), fname);
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
                    gnome_cmd_data.priv->con_list,
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

    load_vfs_auto_devices ();

    g_free (path);
}


inline void load_fav_apps (const gchar *fname)
{
    gnome_cmd_data.priv->fav_apps = NULL;
    gchar *path = g_strdup_printf ("%s/.gnome-commander/%s", g_get_home_dir (), fname);
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

                gnome_cmd_data.priv->fav_apps = g_list_append (
                    gnome_cmd_data.priv->fav_apps,
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

    cmdline_history = gnome_cmd_cmdline_get_history (gnome_cmd_main_win_get_cmdline (main_win));

    gnome_cmd_data_set_string_history ("/cmdline-history/line%d", cmdline_history);
}


inline void GnomeCmdData::save_search_defaults()
{
    gnome_cmd_data_set_int ("/search-history/width", search_defaults.width);
    gnome_cmd_data_set_int ("/search-history/height", search_defaults.height);

    gnome_cmd_data_set_string_history ("/search-history/name_pattern%d", search_defaults.name_patterns.ents);
    gnome_cmd_data_set_string_history ("/search-history/content_pattern%d", search_defaults.content_patterns.ents);
    gnome_cmd_data_set_string_history ("/search-history/directory%d", search_defaults.directories.ents);

    gnome_cmd_data_set_bool ("/search-history/recursive", search_defaults.recursive);
    gnome_cmd_data_set_bool ("/search-history/case_sens", search_defaults.case_sens);
}


inline void GnomeCmdData::save_intviewer_defaults()
{
    gnome_cmd_data_set_string_history ("/internal_viewer/text_pattern%d", intviewer_defaults.text_patterns.ents);
    gnome_cmd_data_set_string_history ("/internal_viewer/hex_pattern%d", intviewer_defaults.hex_patterns.ents);
    gnome_cmd_data_set_bool ("/internal_viewer/case_sens", intviewer_defaults.case_sensitive);
    gnome_cmd_data_set_int ("/internal_viewer/last_mode", intviewer_defaults.search_mode);
}


inline void GnomeCmdData::save_local_bookmarks()
{
    GnomeCmdCon *con = gnome_cmd_con_list_get_home (priv->con_list);
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
    gnome_cmd_data_set_string_history ("/local_bookmarks/name%d", names);
    gnome_cmd_data_set_string_history ("/local_bookmarks/path%d", paths);
}


inline void GnomeCmdData::save_smb_bookmarks()
{
    GnomeCmdCon *con = gnome_cmd_con_list_get_smb (priv->con_list);
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
    gnome_cmd_data_set_string_history ("/smb_bookmarks/name%d", names);
    gnome_cmd_data_set_string_history ("/smb_bookmarks/path%d", paths);
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
    GList *list = NULL;

    list = load_string_history ("/search-history/name_pattern%d", -1);
    search_defaults.name_patterns.ents = list;
    search_defaults.name_patterns.pos = list;
    list = load_string_history ("/search-history/directory%d", -1);
    search_defaults.directories.ents = list;
    search_defaults.directories.pos = list;
    list = load_string_history ("/search-history/content_pattern%d", -1);
    search_defaults.content_patterns.ents = list;
    search_defaults.content_patterns.pos = list;

    search_defaults.width = gnome_cmd_data_get_int ("/search-history/width", 640);
    search_defaults.height = gnome_cmd_data_get_int ("/search-history/height", 400);
    search_defaults.recursive = gnome_cmd_data_get_bool ("/search-history/recursive", TRUE);
    search_defaults.case_sens = gnome_cmd_data_get_bool ("/search-history/case_sens", FALSE);
}


inline void GnomeCmdData::load_intviewer_defaults()
{
    GList *list = NULL;

    list = load_string_history ("/internal_viewer/text_pattern%d", -1);
    intviewer_defaults.text_patterns.ents = list;
    intviewer_defaults.text_patterns.pos = list;
    list = load_string_history ("/internal_viewer/hex_pattern%d", -1);
    intviewer_defaults.hex_patterns.ents = list;
    intviewer_defaults.hex_patterns.pos = list;

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
    GList *templates = load_string_history ("/template-history/template%d", size);

    advrename_defaults.templates.ents = templates;
    advrename_defaults.templates.pos = templates;

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

    GnomeCmdCon *con = gnome_cmd_con_list_get_home (priv->con_list);

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


inline void GnomeCmdData::load_smb_bookmarks()
{
    GList *bookmarks = NULL;

    gint size = gnome_cmd_data_get_int ("/smb_bookmarks/count", 0);
    GList *names = load_string_history ("/smb_bookmarks/name%d", size);
    GList *paths = load_string_history ("/smb_bookmarks/path%d", size);

    GnomeCmdCon *con = gnome_cmd_con_list_get_smb (priv->con_list);

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


inline void GnomeCmdData::load_auto_load_plugins()
{
    gint count = gnome_cmd_data_get_int ("/plugins/count", 0);

    priv->auto_load_plugins = load_string_history ("/plugins/auto_load%d", count);
}


GnomeCmdData::GnomeCmdData()
{
    quick_connect = NULL;

    confirm_delete = TRUE;
    confirm_copy_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_QUERY;
    confirm_move_overwrite = GNOME_CMD_CONFIRM_OVERWRITE_QUERY;
    left_mouse_button_mode = LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK;
    left_mouse_button_unselects = TRUE;
    right_mouse_button_mode = RIGHT_BUTTON_POPUPS_MENU;
    color_mode = GNOME_CMD_COLOR_DEEP_BLUE;
    size_disp_mode = GNOME_CMD_SIZE_DISP_MODE_POWERED;
    perm_disp_mode = GNOME_CMD_PERM_DISP_MODE_TEXT;
    alt_quick_search = FALSE;

    filter_type = Filter::TYPE_FNMATCH;
    memset(&filter_settings, 0, sizeof(filter_settings));
    filter_settings.hidden = TRUE;
    filter_settings.backup = TRUE;

    case_sens_sort = TRUE;
    layout = GNOME_CMD_LAYOUT_MIME_ICONS;
    ext_disp_mode = GNOME_CMD_EXT_DISP_BOTH;
    list_orientation = FALSE;

    toolbar_visibility = TRUE;
    conbuttons_visibility = TRUE;
    concombo_visibility = TRUE;
    cmdline_visibility = TRUE;
    buttonbar_visibility = TRUE;

    use_ls_colors = FALSE;

    icon_size = 16;
    dev_icon_size = 16;
    device_only_icon = FALSE;
    list_row_height = 16;
    memset(fs_col_width, 0, sizeof(fs_col_width));
    icon_scale_quality = GDK_INTERP_HYPER;
    gui_update_rate = DEFAULT_GUI_UPDATE_RATE;
    button_relief = GTK_RELIEF_NONE;

    cmdline_history = NULL;
    cmdline_history_length = 0;

    use_internal_viewer = TRUE;
    use_gcmd_block = FALSE;
    use_gnome_auth_manager = FALSE;

    honor_expect_uris = FALSE;
    skip_mounting = FALSE;

    main_win_width = 600;
    main_win_height = 400;

    main_win_state = GDK_WINDOW_STATE_MAXIMIZED;

    viewer = NULL;
    editor = NULL;
    differ = NULL;
    term = NULL;
}


void GnomeCmdData::free()
{
    // free the external programs strings
    g_free (viewer);
    g_free (editor);
    g_free (differ);
    g_free (term);

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

        // free the date_format string
        g_free (priv->date_format);

        // free the font name strings
        g_free (priv->list_font);

        g_free (priv);
    }
}


void GnomeCmdData::load()
{
    gchar *xml_cfg_path = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);

    gchar *document_icon_dir = g_strdup_printf ("%s/share/pixmaps/document-icons/", GNOME_PREFIX);
    gchar *theme_icon_dir    = g_strdup_printf ("%s/mime-icons", PIXMAPS_DIR);

    priv = g_new0 (Private, 1);

    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].respect_theme = FALSE;
    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg = gdk_color_new (0xffff,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg = gdk_color_new (0,0,0x4444);
    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg = gdk_color_new (0xffff,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg = gdk_color_new (0,0,0x4444);
    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg = gdk_color_new (0xffff,0,0);
    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg = gdk_color_new (0,0,0x4444);
    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg = gdk_color_new (0,0,0);
    priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    priv->color_themes[GNOME_CMD_COLOR_MODERN].respect_theme = FALSE;
    priv->color_themes[GNOME_CMD_COLOR_MODERN].norm_fg = gdk_color_new (0,0,0);
    priv->color_themes[GNOME_CMD_COLOR_MODERN].norm_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    priv->color_themes[GNOME_CMD_COLOR_MODERN].alt_fg = gdk_color_new (0,0,0);
    priv->color_themes[GNOME_CMD_COLOR_MODERN].alt_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    priv->color_themes[GNOME_CMD_COLOR_MODERN].sel_fg = gdk_color_new (0xffff,0,0);
    priv->color_themes[GNOME_CMD_COLOR_MODERN].sel_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    priv->color_themes[GNOME_CMD_COLOR_MODERN].curs_fg = gdk_color_new (0xffff,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_MODERN].curs_bg = gdk_color_new (0,0,0x4444);

    priv->color_themes[GNOME_CMD_COLOR_FUSION].respect_theme = FALSE;
    priv->color_themes[GNOME_CMD_COLOR_FUSION].norm_fg = gdk_color_new (0x8080,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_FUSION].norm_bg = gdk_color_new (0,0x4040,0x8080);
    priv->color_themes[GNOME_CMD_COLOR_FUSION].alt_fg = gdk_color_new (0x8080,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_FUSION].alt_bg = gdk_color_new (0,0x4040,0x8080);
    priv->color_themes[GNOME_CMD_COLOR_FUSION].sel_fg = gdk_color_new (0xffff,0xffff,0);
    priv->color_themes[GNOME_CMD_COLOR_FUSION].sel_bg = gdk_color_new (0,0x4040,0x8080);
    priv->color_themes[GNOME_CMD_COLOR_FUSION].curs_fg = gdk_color_new (0,0,0x8080);
    priv->color_themes[GNOME_CMD_COLOR_FUSION].curs_bg = gdk_color_new (0,0x8080,0x8080);

    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].respect_theme = FALSE;
    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].norm_fg = gdk_color_new (0xffff,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].norm_bg = gdk_color_new (0,0,0x4444);
    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].alt_fg = gdk_color_new (0xffff,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].alt_bg = gdk_color_new (0,0,0x4444);
    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].sel_fg = gdk_color_new (0xffff,0xffff,0);
    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].sel_bg = gdk_color_new (0,0,0x4444);
    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].curs_fg = gdk_color_new (0,0,0);
    priv->color_themes[GNOME_CMD_COLOR_CLASSIC].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].respect_theme = FALSE;
    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].norm_fg = gdk_color_new (0,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].norm_bg = gdk_color_new (0,0,0x8080);
    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].alt_fg = gdk_color_new (0,0xffff,0xffff);
    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].alt_bg = gdk_color_new (0,0,0x8080);
    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].sel_fg = gdk_color_new (0xffff,0xffff,0);
    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].sel_bg = gdk_color_new (0,0,0x8080);
    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].curs_fg = gdk_color_new (0,0,0);
    priv->color_themes[GNOME_CMD_COLOR_DEEP_BLUE].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].respect_theme = FALSE;
    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].norm_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].norm_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].alt_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].alt_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].sel_fg = gdk_color_new (0xffff,0xcfcf,0x3636);
    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].sel_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].curs_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    priv->color_themes[GNOME_CMD_COLOR_CAFEZINHO].curs_bg = gdk_color_new (0x4d4d,0x4d4d,0x4d4d);

    priv->color_themes[GNOME_CMD_COLOR_NONE].respect_theme = TRUE;
    priv->color_themes[GNOME_CMD_COLOR_NONE].norm_fg = NULL;
    priv->color_themes[GNOME_CMD_COLOR_NONE].norm_bg = NULL;
    priv->color_themes[GNOME_CMD_COLOR_NONE].alt_fg = NULL;
    priv->color_themes[GNOME_CMD_COLOR_NONE].alt_bg = NULL;
    priv->color_themes[GNOME_CMD_COLOR_NONE].sel_fg = NULL;
    priv->color_themes[GNOME_CMD_COLOR_NONE].sel_bg = NULL;
    priv->color_themes[GNOME_CMD_COLOR_NONE].curs_fg = NULL;
    priv->color_themes[GNOME_CMD_COLOR_NONE].curs_bg = NULL;

    size_disp_mode = (GnomeCmdSizeDispMode) gnome_cmd_data_get_int ("/options/size_disp_mode", GNOME_CMD_SIZE_DISP_MODE_POWERED);
    perm_disp_mode = (GnomeCmdPermDispMode) gnome_cmd_data_get_int ("/options/perm_disp_mode", GNOME_CMD_PERM_DISP_MODE_TEXT);

#ifdef HAVE_LOCALE_H
    gchar *utf8_date_format = gnome_cmd_data_get_string ("/options/date_disp_mode", "%x %R");
#else
    gchar *utf8_date_format = gnome_cmd_data_get_string ("/options/date_disp_mode", "%D %R");
#endif
    priv->date_format = g_locale_from_utf8 (utf8_date_format, -1, NULL, NULL, NULL);
    g_free (utf8_date_format);

    layout = (GnomeCmdLayout) gnome_cmd_data_get_int ("/options/layout", GNOME_CMD_LAYOUT_MIME_ICONS);

    list_row_height = gnome_cmd_data_get_int ("/options/list_row_height", 16);

    confirm_delete = gnome_cmd_data_get_bool ("/confirm/delete", TRUE);
    confirm_copy_overwrite = (GnomeCmdConfirmOverwriteMode) gnome_cmd_data_get_int ("/confirm/copy_overwrite", GNOME_CMD_CONFIRM_OVERWRITE_QUERY);
    confirm_move_overwrite = (GnomeCmdConfirmOverwriteMode) gnome_cmd_data_get_int ("/confirm/move_overwrite", GNOME_CMD_CONFIRM_OVERWRITE_QUERY);

    filter_settings.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN] = gnome_cmd_data_get_bool ("/options/show_unknown", FALSE);
    filter_settings.file_types[GNOME_VFS_FILE_TYPE_REGULAR] = gnome_cmd_data_get_bool ("/options/show_regular", FALSE);
    filter_settings.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY] = gnome_cmd_data_get_bool ("/options/show_directory", FALSE);
    filter_settings.file_types[GNOME_VFS_FILE_TYPE_FIFO] = gnome_cmd_data_get_bool ("/options/show_fifo", FALSE);
    filter_settings.file_types[GNOME_VFS_FILE_TYPE_SOCKET] = gnome_cmd_data_get_bool ("/options/show_socket", FALSE);
    filter_settings.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE] = gnome_cmd_data_get_bool ("/options/show_char_device", FALSE);
    filter_settings.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE] = gnome_cmd_data_get_bool ("/options/show_block_device", FALSE);
    filter_settings.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK] = gnome_cmd_data_get_bool ("/options/show_symbolic_link", FALSE);
    filter_settings.hidden = gnome_cmd_data_get_bool ("/options/hidden_filter", TRUE);
    filter_settings.backup = gnome_cmd_data_get_bool ("/options/backup_filter", TRUE);

    case_sens_sort = gnome_cmd_data_get_bool ("/sort/case_sensitive", TRUE);

    main_win_width = get_int ("/gnome-commander-size/main_win/width", 600);
    main_win_height = get_int ("/gnome-commander-size/main_win/height", 400);

    for (gint i=0; i<GnomeCmdFileList::NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/fs_col_width%d", i);
        fs_col_width[i] = get_int (tmp, GnomeCmdFileList::get_column_default_width((GnomeCmdFileList::ColumnID) i));
        g_free (tmp);
    }

    for (gint i=0; i<BOOKMARK_DIALOG_NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/bookmark_dialog_col_width%d", i);
        priv->bookmark_dialog_col_width[i] = get_int (tmp, bookmark_dialog_default_column_width[i]);
        g_free (tmp);
    }

    color_mode = (GnomeCmdColorMode) gnome_cmd_data_get_int ("/colors/mode", GNOME_CMD_COLOR_DEEP_BLUE);

    gnome_cmd_data_get_color ("/colors/norm_fg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg);
    gnome_cmd_data_get_color ("/colors/norm_bg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg);
    gnome_cmd_data_get_color ("/colors/alt_fg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg);
    gnome_cmd_data_get_color ("/colors/alt_bg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg);
    gnome_cmd_data_get_color ("/colors/sel_fg",  priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg);
    gnome_cmd_data_get_color ("/colors/sel_bg",  priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg);
    gnome_cmd_data_get_color ("/colors/curs_fg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg);
    gnome_cmd_data_get_color ("/colors/curs_bg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg);

    priv->list_font = gnome_cmd_data_get_string ("/options/list_font", "-misc-fixed-medium-r-normal-*-10-*-*-*-c-*-iso8859-1");

    ext_disp_mode = (GnomeCmdExtDispMode) gnome_cmd_data_get_int ("/options/ext_disp_mode", GNOME_CMD_EXT_DISP_BOTH);
    left_mouse_button_mode = (LeftMouseButtonMode) gnome_cmd_data_get_int ("/options/left_mouse_button_mode", LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK);
    left_mouse_button_unselects = gnome_cmd_data_get_bool ("/options/left_mouse_button_unselects", TRUE);
    right_mouse_button_mode = (RightMouseButtonMode) gnome_cmd_data_get_int ("/options/right_mouse_button_mode", RIGHT_BUTTON_POPUPS_MENU);
    icon_size = gnome_cmd_data_get_int ("/options/icon_size", 16);
    dev_icon_size = gnome_cmd_data_get_int ("/options/dev_icon_size", 16);
    icon_scale_quality = (GdkInterpType) gnome_cmd_data_get_int ("/options/icon_scale_quality", GDK_INTERP_HYPER);
    priv->theme_icon_dir = gnome_cmd_data_get_string ("/options/theme_icon_dir", theme_icon_dir);
    g_free (theme_icon_dir);
    priv->document_icon_dir = gnome_cmd_data_get_string ("/options/document_icon_dir", document_icon_dir);
    g_free (document_icon_dir);
    cmdline_history_length = gnome_cmd_data_get_int ("/options/cmdline_history_length", 16);
    button_relief = (GtkReliefStyle) gnome_cmd_data_get_int ("/options/btn_relief", GTK_RELIEF_NONE);
    filter_type = (Filter::Type) gnome_cmd_data_get_int ("/options/filter_type", Filter::TYPE_FNMATCH);
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

    honor_expect_uris = gnome_cmd_data_get_bool ("/programs/honor_expect_uris", FALSE);
    use_internal_viewer = gnome_cmd_data_get_bool ("/programs/use_internal_viewer", TRUE);
    alt_quick_search = gnome_cmd_data_get_bool ("/programs/alt_quick_search", FALSE);
    skip_mounting = gnome_cmd_data_get_bool ("/programs/skip_mounting", FALSE);

    priv->symlink_prefix = gnome_cmd_data_get_string ("/options/symlink_prefix", _("link to %s"));
    if (!*priv->symlink_prefix || strcmp(priv->symlink_prefix, _("link to %s"))==0)
    {
        g_free (priv->symlink_prefix);
        priv->symlink_prefix = NULL;
    }

    priv->sort_column[LEFT] = gnome_cmd_data_get_int ("/options/sort_column_left", GnomeCmdFileList::COLUMN_NAME);
    priv->sort_direction[LEFT] = gnome_cmd_data_get_bool ("/options/sort_direction_left", GTK_SORT_ASCENDING);
    priv->sort_column[RIGHT] = gnome_cmd_data_get_int ("/options/sort_column_right", GnomeCmdFileList::COLUMN_NAME);
    priv->sort_direction[RIGHT] = gnome_cmd_data_get_bool ("/options/sort_direction_right", GTK_SORT_ASCENDING);

    viewer = gnome_cmd_data_get_string ("/programs/viewer", "gedit %s");
    editor = gnome_cmd_data_get_string ("/programs/editor", "gedit %s");
    differ = gnome_cmd_data_get_string ("/programs/differ", "meld %s");
    term   = gnome_cmd_data_get_string ("/programs/terminal", "xterm -hold -e %s");

    use_gcmd_block = gnome_cmd_data_get_bool ("/programs/use_gcmd_block", FALSE);

    device_only_icon = gnome_cmd_data_get_bool ("/devices/only_icon", FALSE);
    use_ls_colors = gnome_cmd_data_get_bool ("/colors/use_ls_colors", FALSE);

    priv->start_dirs[LEFT] = gnome_cmd_data_get_string ("/options/start_dir_left", g_get_home_dir ());
    priv->start_dirs[RIGHT] = gnome_cmd_data_get_string ("/options/start_dir_right", g_get_home_dir ());

    priv->last_pattern = gnome_cmd_data_get_string ("/defaults/last_pattern", "");
    priv->backup_pattern = gnome_cmd_data_get_string ("/defaults/backup_pattern", "*~;*.bak");
    priv->backup_pattern_list = patlist_new (priv->backup_pattern);

    main_win_state = (GdkWindowState) gnome_cmd_data_get_int ("/options/main_win_state", (gint) GDK_WINDOW_STATE_MAXIMIZED);

    use_gnome_auth_manager = gnome_cmd_data_get_bool ("/network/use_gnome_auth_manager", FALSE);
    priv->ftp_anonymous_password = gnome_cmd_data_get_string ("/network/ftp_anonymous_password", "you@provider.com");

    if (strcmp (priv->ftp_anonymous_password, "you@provider.com")==0)   // if '/network/ftp_anonymous_password' entry undefined, try to read '/ftp/anonymous_password'
    {
        g_free (priv->ftp_anonymous_password);
        priv->ftp_anonymous_password = gnome_cmd_data_get_string ("/ftp/anonymous_password", "you@provider.com");
    }

    // "/quick-connect/uri" must be read AFTER retrieving anonymous password

    gchar * quick_connect_uri = gnome_cmd_data_get_string ("/quick-connect/uri", "ftp://anonymous@ftp.gnome.org/pub/GNOME/");
    quick_connect = gnome_cmd_con_ftp_new (NULL, quick_connect_uri);
    g_free (quick_connect_uri);

    load_cmdline_history();
    //load_dir_history ();
    load_search_defaults();

    if (!gnome_cmd_xml_config_load (xml_cfg_path, *this))
    {
        load_rename_history();

        // add a few default templates here - for new users
#if GLIB_CHECK_VERSION (2, 14, 0)
        {
            AdvrenameConfig::Profile p;

            p.name = _("CamelCase");
            p.template_string = "$N";
            p.regexes.push_back(GnomeCmd::ReplacePattern("\\s*\\b(\\w)(\\w*)\\b", "\\u\\1\\L\\2\\E", FALSE));
            p.regexes.push_back(GnomeCmd::ReplacePattern("\\.(.+)$", ".\\L\\1", FALSE));

            advrename_defaults.profiles.push_back(p);
        }
#endif
    }

    load_intviewer_defaults();
    load_auto_load_plugins();

    set_vfs_volume_monitor ();

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

    g_free (xml_cfg_path);
}


void GnomeCmdData::load_more()
{
    priv->con_list = gnome_cmd_con_list_new ();
    gnome_cmd_con_list_begin_update (priv->con_list);
    load_devices ("devices");
    load_connections ("connections") || load_connections ("ftp-servers");
    gnome_cmd_con_list_end_update (priv->con_list);

    load_fav_apps ("fav-apps");
    load_local_bookmarks();
    load_smb_bookmarks();
}


void GnomeCmdData::save()
{
    gchar *xml_cfg_path = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);

    for (gint i=0; i<BOOKMARK_DIALOG_NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/bookmark_dialog_col_width%d", i);
        gnome_config_set_int (tmp, priv->bookmark_dialog_col_width[i]);
        g_free (tmp);
    }

    gnome_cmd_data_set_int    ("/options/size_disp_mode", size_disp_mode);
    gnome_cmd_data_set_int    ("/options/perm_disp_mode", perm_disp_mode);
    gnome_cmd_data_set_int    ("/options/layout", layout);
    gnome_cmd_data_set_int    ("/options/list_row_height", list_row_height);

    gchar *utf8_date_format = g_locale_to_utf8 (priv->date_format, -1, NULL, NULL, NULL);
    gnome_cmd_data_set_string ("/options/date_disp_mode", utf8_date_format);
    g_free (utf8_date_format);

    gnome_cmd_data_set_bool   ("/confirm/delete", confirm_delete);
    gnome_cmd_data_set_int    ("/confirm/copy_overwrite", confirm_copy_overwrite);
    gnome_cmd_data_set_int    ("/confirm/move_overwrite", confirm_move_overwrite);

    gnome_cmd_data_set_bool   ("/options/show_unknown", filter_settings.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN]);
    gnome_cmd_data_set_bool   ("/options/show_regular", filter_settings.file_types[GNOME_VFS_FILE_TYPE_REGULAR]);
    gnome_cmd_data_set_bool   ("/options/show_directory", filter_settings.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY]);
    gnome_cmd_data_set_bool   ("/options/show_fifo", filter_settings.file_types[GNOME_VFS_FILE_TYPE_FIFO]);
    gnome_cmd_data_set_bool   ("/options/show_socket", filter_settings.file_types[GNOME_VFS_FILE_TYPE_SOCKET]);
    gnome_cmd_data_set_bool   ("/options/show_char_device", filter_settings.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE]);
    gnome_cmd_data_set_bool   ("/options/show_block_device", filter_settings.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE]);
    gnome_cmd_data_set_bool   ("/options/show_symbolic_link", filter_settings.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK]);

    gnome_cmd_data_set_bool   ("/options/hidden_filter", filter_settings.hidden);
    gnome_cmd_data_set_bool   ("/options/backup_filter", filter_settings.backup);

    gnome_cmd_data_set_bool   ("/sort/case_sensitive", case_sens_sort);

    gnome_cmd_data_set_int    ("/colors/mode", color_mode);

    gnome_cmd_data_set_color  ("/colors/norm_fg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg);
    gnome_cmd_data_set_color  ("/colors/norm_bg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg);
    gnome_cmd_data_set_color  ("/colors/alt_fg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg);
    gnome_cmd_data_set_color  ("/colors/alt_bg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg);
    gnome_cmd_data_set_color  ("/colors/sel_fg",  priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg);
    gnome_cmd_data_set_color  ("/colors/sel_bg",  priv->color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg);
    gnome_cmd_data_set_color  ("/colors/curs_fg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg);
    gnome_cmd_data_set_color  ("/colors/curs_bg", priv->color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg);

    gnome_cmd_data_set_string ("/options/list_font", priv->list_font);

    gnome_cmd_data_set_int    ("/options/ext_disp_mode", ext_disp_mode);
    gnome_cmd_data_set_int    ("/options/left_mouse_button_mode", left_mouse_button_mode);
    gnome_cmd_data_set_bool   ("/options/left_mouse_button_unselects", left_mouse_button_unselects);
    gnome_cmd_data_set_int    ("/options/right_mouse_button_mode", right_mouse_button_mode);
    gnome_cmd_data_set_int    ("/options/icon_size", icon_size);
    gnome_cmd_data_set_int    ("/options/dev_icon_size", dev_icon_size);
    gnome_cmd_data_set_int    ("/options/icon_scale_quality", icon_scale_quality);
    gnome_cmd_data_set_string ("/options/theme_icon_dir", priv->theme_icon_dir);
    gnome_cmd_data_set_string ("/options/document_icon_dir", priv->document_icon_dir);
    gnome_cmd_data_set_int    ("/options/cmdline_history_length", cmdline_history_length);
    gnome_cmd_data_set_int    ("/options/btn_relief", button_relief);
    gnome_cmd_data_set_int    ("/options/filter_type", filter_type);
    gnome_cmd_data_set_bool   ("/options/list_orientation", list_orientation);
    gnome_cmd_data_set_int    ("/options/gui_update_rate", gui_update_rate);

    gnome_cmd_data_set_bool   ("/programs/honor_expect_uris", honor_expect_uris);
    gnome_cmd_data_set_bool   ("/programs/use_internal_viewer", use_internal_viewer);
    gnome_cmd_data_set_bool   ("/programs/alt_quick_search", alt_quick_search);
    gnome_cmd_data_set_bool   ("/programs/skip_mounting", skip_mounting);

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

    gnome_cmd_data_set_int    ("/options/sort_column_left", priv->sort_column[LEFT]);
    gnome_cmd_data_set_bool   ("/options/sort_direction_left", priv->sort_direction[LEFT]);
    gnome_cmd_data_set_int    ("/options/sort_column_right", priv->sort_column[RIGHT]);
    gnome_cmd_data_set_bool   ("/options/sort_direction_right", priv->sort_direction[RIGHT]);

    gnome_cmd_data_set_string ("/programs/viewer", viewer);
    gnome_cmd_data_set_string ("/programs/editor", editor);
    gnome_cmd_data_set_string ("/programs/differ", differ);
    gnome_cmd_data_set_string ("/programs/terminal", term);

    gnome_cmd_data_set_bool   ("/programs/use_gcmd_block", use_gcmd_block);

    gnome_cmd_data_set_bool   ("/devices/only_icon", device_only_icon);
    gnome_cmd_data_set_bool   ("/colors/use_ls_colors", use_ls_colors);

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

    gnome_cmd_data_set_string ("/options/start_dir_left", priv->start_dirs[LEFT]);
    gnome_cmd_data_set_string ("/options/start_dir_right", priv->start_dirs[RIGHT]);
    gnome_cmd_data_set_string ("/defaults/last_pattern", priv->last_pattern);
    gnome_cmd_data_set_string ("/defaults/backup_pattern", priv->backup_pattern);

    gnome_cmd_data_set_int ("/options/main_win_state", (gint) main_win_state);

    gnome_cmd_data_set_bool ("/network/use_gnome_auth_manager", use_gnome_auth_manager);
    gnome_cmd_data_set_string ("/network/ftp_anonymous_password", priv->ftp_anonymous_password);
    gnome_config_clean_section (G_DIR_SEPARATOR_S PACKAGE "/ftp");

    save_cmdline_history();
    //write_dir_history ();

    save_connections ("connections");
    save_devices ("devices");
    save_fav_apps ("fav-apps");
    save_search_defaults();
    save_intviewer_defaults();
    gnome_cmd_xml_config_save (xml_cfg_path, *this);
    save_local_bookmarks();
    save_smb_bookmarks();
    save_auto_load_plugins();

    gnome_config_sync ();

    g_free (xml_cfg_path);
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
    if (gnome_cmd_data.priv->ftp_anonymous_password)
        g_free (gnome_cmd_data.priv->ftp_anonymous_password);

    gnome_cmd_data.priv->ftp_anonymous_password = g_strdup (pw);
}


void gnome_cmd_data_add_fav_app (GnomeCmdApp *app)
{
    g_return_if_fail (app != NULL);

    gnome_cmd_data.priv->fav_apps = g_list_append (gnome_cmd_data.priv->fav_apps, app);
}


void gnome_cmd_data_remove_fav_app (GnomeCmdApp *app)
{
    g_return_if_fail (app != NULL);

    gnome_cmd_data.priv->fav_apps = g_list_remove (gnome_cmd_data.priv->fav_apps, app);
}


GList *gnome_cmd_data_get_fav_apps ()
{
    return gnome_cmd_data.priv->fav_apps;
}


void gnome_cmd_data_set_fav_apps (GList *apps)
{
    gnome_cmd_data.priv->fav_apps = apps;
}


GnomeCmdDateFormat gnome_cmd_data_get_date_format ()
{
    return gnome_cmd_data.priv->date_format;
}


void gnome_cmd_data_set_date_format (GnomeCmdDateFormat format)
{
    if (gnome_cmd_data.priv->date_format)
        g_free (gnome_cmd_data.priv->date_format);

    gnome_cmd_data.priv->date_format = g_strdup (format);
}


GnomeCmdColorTheme *gnome_cmd_data_get_current_color_theme ()
{
    return &gnome_cmd_data.priv->color_themes[gnome_cmd_data.color_mode];
}


GnomeCmdColorTheme *gnome_cmd_data_get_custom_color_theme ()
{
    return &gnome_cmd_data.priv->color_themes[GNOME_CMD_COLOR_CUSTOM];
}


const gchar *gnome_cmd_data_get_list_font ()
{
    return gnome_cmd_data.priv->list_font;
}


void gnome_cmd_data_set_list_font (const gchar *list_font)
{
    g_free (gnome_cmd_data.priv->list_font);
    gnome_cmd_data.priv->list_font = g_strdup (list_font);
}


const gchar *gnome_cmd_data_get_theme_icon_dir ()
{
    return gnome_cmd_data.priv->theme_icon_dir;
}


void gnome_cmd_data_set_theme_icon_dir (const gchar *dir)
{
    g_free (gnome_cmd_data.priv->theme_icon_dir);

    gnome_cmd_data.priv->theme_icon_dir = g_strdup (dir);
}


const gchar *gnome_cmd_data_get_document_icon_dir ()
{
    return gnome_cmd_data.priv->document_icon_dir;
}


void gnome_cmd_data_set_document_icon_dir (const gchar *dir)
{
    g_free (gnome_cmd_data.priv->document_icon_dir);

    gnome_cmd_data.priv->document_icon_dir = g_strdup (dir);
}


void gnome_cmd_data_set_bookmark_dialog_col_width (guint column, gint width)
{
    gnome_cmd_data.priv->bookmark_dialog_col_width[column] = width;
}


gint gnome_cmd_data_get_bookmark_dialog_col_width (guint column)
{
    return gnome_cmd_data.priv->bookmark_dialog_col_width[column];
}


GnomeCmdBookmarkGroup *gnome_cmd_data_get_local_bookmarks ()
{
    return gnome_cmd_con_get_bookmarks (gnome_cmd_con_list_get_home (gnome_cmd_data.priv->con_list));
}


void gnome_cmd_data_set_start_dir (gboolean fs, const gchar *start_dir)
{
    if (gnome_cmd_data.priv->start_dirs[fs])
        g_free (gnome_cmd_data.priv->start_dirs[fs]);

    gnome_cmd_data.priv->start_dirs[fs] = g_strdup (start_dir);
}


const gchar *gnome_cmd_data_get_start_dir (gboolean fs)
{
    return gnome_cmd_data.priv->start_dirs[fs];
}


void gnome_cmd_data_set_last_pattern (const gchar *value)
{
    gnome_cmd_data.priv->last_pattern = g_strdup (value);
}


const gchar *gnome_cmd_data_get_last_pattern ()
{
    return gnome_cmd_data.priv->last_pattern;
}


GList *gnome_cmd_data_get_auto_load_plugins ()
{
    return gnome_cmd_data.priv->auto_load_plugins;
}


void gnome_cmd_data_set_auto_load_plugins (GList *plugins)
{
    gnome_cmd_data.priv->auto_load_plugins = plugins;
}


void gnome_cmd_data_get_sort_params (GnomeCmdFileList *fl, gint &col, gboolean &direction)
{
    if (!gnome_cmd_main_win_get_fs (main_win, LEFT) || gnome_cmd_main_win_get_fs (main_win, LEFT)->file_list() == fl)
    {
        col = gnome_cmd_data.priv->sort_column[LEFT];
        direction = gnome_cmd_data.priv->sort_direction[LEFT];
    }
    else
        if (!gnome_cmd_main_win_get_fs (main_win, RIGHT) || gnome_cmd_main_win_get_fs (main_win, RIGHT)->file_list() == fl)
        {
            col = gnome_cmd_data.priv->sort_column[RIGHT];
            direction = gnome_cmd_data.priv->sort_direction[RIGHT];
        }
}


void gnome_cmd_data_set_sort_params (GnomeCmdFileList *fl, gint col, gboolean direction)
{
    if (gnome_cmd_main_win_get_fs (main_win, LEFT)->file_list() == fl)
    {
        gnome_cmd_data.priv->sort_column[LEFT] = col;
        gnome_cmd_data.priv->sort_direction[LEFT] = direction;
    }
    else
        if (gnome_cmd_main_win_get_fs (main_win, RIGHT)->file_list() == fl)
        {
            gnome_cmd_data.priv->sort_column[RIGHT] = col;
            gnome_cmd_data.priv->sort_direction[RIGHT] = direction;
        }
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


void gnome_cmd_data_set_backup_pattern (const gchar *value)
{
    g_free (gnome_cmd_data.priv->backup_pattern);

    gnome_cmd_data.priv->backup_pattern = g_strdup (value);

    if (gnome_cmd_data.priv->backup_pattern_list)
        patlist_free (gnome_cmd_data.priv->backup_pattern_list);

    gnome_cmd_data.priv->backup_pattern_list = patlist_new (gnome_cmd_data.priv->backup_pattern);
}


const gchar *gnome_cmd_data_get_backup_pattern ()
{
    return gnome_cmd_data.priv->backup_pattern;
}


GList *gnome_cmd_data_get_backup_pattern_list ()
{
    return gnome_cmd_data.priv->backup_pattern_list;
}


const gchar *gnome_cmd_data_get_symlink_prefix ()
{
    return gnome_cmd_data.priv->symlink_prefix ? gnome_cmd_data.priv->symlink_prefix : _("link to %s");
}


void gnome_cmd_data_set_symlink_prefix (const gchar *value)
{
    gnome_cmd_data.priv->symlink_prefix = g_strdup (value);
}
