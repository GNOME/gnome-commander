/*
 * Copyright (C) 2024-2025 Andrey Kutejko
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "gnome-cmd-state.h"

typedef struct _GnomeCmdStatePrivate
{
    GnomeCmdFileDescriptor *active_dir;
    GnomeCmdFileDescriptor *inactive_dir;
    GList *active_dir_files;
    GList *inactive_dir_files;
    GList *active_dir_selected_files;
    GList *inactive_dir_selected_files;
} GnomeCmdStatePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdState, gnome_cmd_state, G_TYPE_OBJECT)

static void dispose (GObject *object)
{
    GnomeCmdState *state = GNOME_CMD_STATE (object);
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);

    g_clear_object (&priv->active_dir);
    g_clear_object (&priv->inactive_dir);
    g_clear_list (&priv->active_dir_files, g_object_unref);
    g_clear_list (&priv->inactive_dir_files, g_object_unref);
    g_clear_list (&priv->active_dir_selected_files, g_object_unref);
    g_clear_list (&priv->inactive_dir_selected_files, g_object_unref);

    G_OBJECT_CLASS (gnome_cmd_state_parent_class)->dispose (object);
}

static void gnome_cmd_state_class_init (GnomeCmdStateClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = dispose;
}

static void gnome_cmd_state_init (GnomeCmdState *state)
{
}

GnomeCmdState *gnome_cmd_state_new()
{
    return g_object_new (GNOME_CMD_TYPE_STATE, NULL);
}

GnomeCmdFileDescriptor *gnome_cmd_state_get_active_dir(GnomeCmdState *state)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    return priv->active_dir;
}

void gnome_cmd_state_set_active_dir(GnomeCmdState *state, GnomeCmdFileDescriptor *active_dir)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    g_set_object (&priv->active_dir, active_dir);
}

GnomeCmdFileDescriptor *gnome_cmd_state_get_inactive_dir(GnomeCmdState *state)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    return priv->inactive_dir;
}

void gnome_cmd_state_set_inactive_dir(GnomeCmdState *state, GnomeCmdFileDescriptor *inactive_dir)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    g_set_object (&priv->inactive_dir, inactive_dir);
}

GList *gnome_cmd_state_get_active_dir_files(GnomeCmdState *state)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    return priv->active_dir_files;
}

void gnome_cmd_state_set_active_dir_files(GnomeCmdState *state, GList *active_dir_files)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    g_clear_list (&priv->active_dir_files, g_object_unref);
    priv->active_dir_files = g_list_copy_deep (active_dir_files, (GCopyFunc) g_object_ref, NULL);
}

GList *gnome_cmd_state_get_inactive_dir_files(GnomeCmdState *state)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    return priv->inactive_dir_files;
}

void gnome_cmd_state_set_inactive_dir_files(GnomeCmdState *state, GList *inactive_dir_files)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    g_clear_list (&priv->inactive_dir_files, g_object_unref);
    priv->inactive_dir_files = g_list_copy_deep (inactive_dir_files, (GCopyFunc) g_object_ref, NULL);
}

GList *gnome_cmd_state_get_active_dir_selected_files(GnomeCmdState *state)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    return priv->active_dir_selected_files;
}

void gnome_cmd_state_set_active_dir_selected_files(GnomeCmdState *state, GList *active_dir_selected_files)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    g_clear_list (&priv->active_dir_selected_files, g_object_unref);
    priv->active_dir_selected_files = g_list_copy_deep (active_dir_selected_files, (GCopyFunc) g_object_ref, NULL);
}

GList *gnome_cmd_state_get_inactive_dir_selected_files(GnomeCmdState *state)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    return priv->inactive_dir_selected_files;
}

void gnome_cmd_state_set_inactive_dir_selected_files(GnomeCmdState *state, GList *inactive_dir_selected_files)
{
    GnomeCmdStatePrivate *priv = (GnomeCmdStatePrivate *) gnome_cmd_state_get_instance_private (state);
    g_clear_list (&priv->inactive_dir_selected_files, g_object_unref);
    priv->inactive_dir_selected_files = g_list_copy_deep (inactive_dir_selected_files, (GCopyFunc) g_object_ref, NULL);
}
