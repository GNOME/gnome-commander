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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "gnome-cmd-includes.h"
#include "gnome-cmd-smb-path.h"
#include "gnome-cmd-smb-net.h"
#include "utils.h"

struct _GnomeCmdSmbPathPrivate
{
    gchar *workgroup;
    gchar *resource;
    gchar *resource_path;
    gchar *path;
    gchar *display_path;
};

static GnomeCmdPathClass *parent_class = NULL;


static const gchar *
smb_path_get_path (GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_SMB_PATH (path), NULL);

    return GNOME_CMD_SMB_PATH (path)->priv->path;
}


static const gchar *
smb_path_get_display_path (GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_SMB_PATH (path), NULL);

    return GNOME_CMD_SMB_PATH (path)->priv->display_path;
}


static GnomeCmdPath *
smb_path_get_parent (GnomeCmdPath *path)
{
    GnomeCmdSmbPath *smb_path;
    gchar *a = NULL,
          *b = NULL,
          *c = NULL;

    g_return_val_if_fail (GNOME_CMD_IS_SMB_PATH (path), NULL);

    smb_path = GNOME_CMD_SMB_PATH (path);

    if (!smb_path->priv->workgroup)
        return NULL;

    if (smb_path->priv->resource) {
        if (smb_path->priv->resource_path) {
            GnomeVFSURI *u1, *t;

            t = gnome_vfs_uri_new ("/");
            u1 = gnome_vfs_uri_append_path (t, smb_path->priv->resource_path);
            gnome_vfs_uri_unref (t);
            if (u1 && gnome_vfs_uri_has_parent (u1)) {
                gchar *s;
                GnomeVFSURI *u2;

                u2 = gnome_vfs_uri_get_parent (u1);
                g_return_val_if_fail (u2 != NULL, NULL);

                s = gnome_vfs_uri_to_string (u2, 0);
                gnome_vfs_uri_unref (u2);

                c = gnome_vfs_get_local_path_from_uri (s);
                g_free (s);
            }

            b = smb_path->priv->resource;
            gnome_vfs_uri_unref (u1);
        }

        a = smb_path->priv->workgroup;
    }

    return gnome_cmd_smb_path_new (a, b, c);
}


static GnomeCmdPath *
smb_path_get_child (GnomeCmdPath *path, const gchar *child)
{
    gchar *a = NULL, *b = NULL, *c = NULL;
    GnomeVFSURI *u1, *u2;
    GnomeCmdSmbPath *smb_path;
    GnomeCmdPath *out;

    g_return_val_if_fail (GNOME_CMD_IS_SMB_PATH (path), NULL);
    g_return_val_if_fail (child != NULL, NULL);
    smb_path = GNOME_CMD_SMB_PATH (path);
    g_return_val_if_fail (child[0] != '/', NULL);

    if (smb_path->priv->workgroup) {
        if (smb_path->priv->resource) {
            if (smb_path->priv->resource_path) {
                GnomeVFSURI *t = gnome_vfs_uri_new ("/");
                u1 = gnome_vfs_uri_append_path (t, smb_path->priv->resource_path);
                gnome_vfs_uri_unref (t);
                if (!strchr (child, '/'))
                    u2 = gnome_vfs_uri_append_file_name (u1, child);
                else
                    u2 = gnome_vfs_uri_append_path (u1, child);
                gnome_vfs_uri_unref (u1);
                g_return_val_if_fail (u2 != NULL, NULL);

                c = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (u2), 0);
                gnome_vfs_uri_unref (u2);
            }
            else
                c = g_strdup_printf ("/%s", child);

            b = g_strdup (smb_path->priv->resource);
        }
        else
            b = g_strdup (child);

        a = g_strdup (smb_path->priv->workgroup);
    }
    else
        a = g_strdup (child);

    out = gnome_cmd_smb_path_new (a, b, c);
    g_free (a);
    g_free (b);
    g_free (c);

    return out;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdSmbPath *path = GNOME_CMD_SMB_PATH (object);

    g_free (path->priv->workgroup);
    g_free (path->priv->resource);
    g_free (path->priv->resource_path);
    g_free (path->priv->path);
    g_free (path->priv->display_path);
    g_free (path->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdSmbPathClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GnomeCmdPathClass *path_class = GNOME_CMD_PATH_CLASS (klass);

    parent_class = gtk_type_class (gnome_cmd_path_get_type ());

    object_class->destroy = destroy;

    path_class->get_path = smb_path_get_path;
    path_class->get_display_path = smb_path_get_display_path;
    path_class->get_parent = smb_path_get_parent;
    path_class->get_child = smb_path_get_child;
}


static void
init (GnomeCmdSmbPath *path)
{
    path->priv = g_new0 (GnomeCmdSmbPathPrivate, 1);
}


/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_smb_path_get_type         (void)
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdSmbPath",
            sizeof (GnomeCmdSmbPath),
            sizeof (GnomeCmdSmbPathClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gnome_cmd_path_get_type (), &info);
    }
    return type;
}


GnomeCmdPath *
gnome_cmd_smb_path_new (const gchar *workgroup,
                        const gchar *resource,
                        const gchar *resource_path)
{
    GnomeCmdSmbPath *smb_path = gtk_type_new (gnome_cmd_smb_path_get_type ());

    if (workgroup) {
        smb_path->priv->workgroup = g_strdup (workgroup);

        if (resource) {
            smb_path->priv->resource = g_strdup (resource);

            if (resource_path) {
                smb_path->priv->resource_path = g_strdup (resource_path);
                smb_path->priv->path = g_strdup_printf (
                    "/%s%s", resource, resource_path);
            }
            else
                smb_path->priv->path = g_strdup_printf (
                    "/%s", resource);
        }
        else
            smb_path->priv->path = g_strdup_printf (
                "/%s", workgroup);
    }
    else
        smb_path->priv->path = g_strdup ("/");

    smb_path->priv->display_path = unix_to_unc (smb_path->priv->path);

    return GNOME_CMD_PATH (smb_path);
}


GnomeCmdPath *
gnome_cmd_smb_path_new_from_str (const gchar *path_str)
{
    gint i;
    gchar *s, *t;
    gchar **v;
    gchar *a = NULL, *b = NULL, *c = NULL;
    SmbEntity *ent;
    GnomeCmdPath *out = NULL;

    g_return_val_if_fail (path_str != NULL, NULL);
    DEBUG('s', "Creating smb-path for %s\n", path_str);

    t = g_strdup (path_str);

    /* Replace \ with / */
    for ( i=0 ; i<strlen(t) ; i++ )
        if (t[i] == '\\')
            t[i] = '/';
    s = g_strdup (t);

    /* Eat up all leading slashes */
    while (*s == '/') {
        if (!strlen (s))
            return NULL;
        s++;
    }

    v = g_strsplit (s, "/", 0);
    g_free (t);
    if (v[0] != NULL) {
        a = g_strdup (v[0]);
        if (v[1] != NULL) {
            b = g_strdup (v[1]);
            if (v[2] != NULL) {
                c = g_strdup_printf ("/%s", v[2]);
                if (v[3] != NULL) {
                    gchar *t1 = c;
                    gchar *t2 = g_strjoinv ("/", &v[3]);
                    c = g_strjoin ("/", t1, t2, NULL);
                    g_free (t1);
                    g_free (t2);
                }
            }
        }

        ent = gnome_cmd_smb_net_get_entity (a);
        if (ent) {
            if (ent->type == SMB_WORKGROUP)
                out = gnome_cmd_smb_path_new (a, b, c);
            else {
                if (c) {
                    gchar *t = b;
                    b = g_strdup_printf ("/%s%s", b, c);
                    g_free (t);
                    g_free (c);
                }
                else {
                    gchar *t = b;
                    b = g_strdup_printf ("/%s", b);
                    g_free (t);
                }
                out = gnome_cmd_smb_path_new (ent->workgroup_name, a, b);
            }
        }
        else
            create_error_dialog (_("Can't find a host or workgroup named %s\n"), a);
    }
    else
        out = gnome_cmd_smb_path_new (NULL, NULL, NULL);

    return out;
}
