/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2015 Uwe Scholz

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __EGG_CELL_RENDERER_KEYS_H__
#define __EGG_CELL_RENDERER_KEYS_H__

#include <gtk/gtkcellrenderertext.h>

#define EGG_TYPE_CELL_RENDERER_KEYS              (egg_cell_renderer_keys_get_type ())
#define EGG_CELL_RENDERER_KEYS(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), EGG_TYPE_CELL_RENDERER_KEYS, EggCellRendererKeys))
#define EGG_CELL_RENDERER_KEYS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EGG_TYPE_CELL_RENDERER_KEYS, EggCellRendererKeysClass))
#define EGG_IS_CELL_RENDERER_KEYS(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), EGG_TYPE_CELL_RENDERER_KEYS))
#define EGG_IS_CELL_RENDERER_KEYS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_CELL_RENDERER_KEYS))
#define EGG_CELL_RENDERER_KEYS_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), EGG_TYPE_CELL_RENDERER_KEYS, EggCellRendererKeysClass))


struct EggCellRendererKeys
{
    GtkCellRendererText parent;
    guint accel_key;
    GdkModifierType accel_mask;
    GtkWidget *edit_widget;
    GtkWidget *grab_widget;
    guint edit_key;
    GtkWidget *sizing_label;
    GtkCellRendererAccelMode accel_mode;
};

struct EggCellRendererKeysClass
{
  GtkCellRendererTextClass parent_class;

  void (* keys_edited) (EggCellRendererKeys    *keys,
                        const char             *path_string,
                        guint                   keyval,
                        GdkModifierType         mask,
                        guint                   hardware_keycode);
};

GType egg_cell_renderer_keys_get_type ();
GtkCellRenderer *egg_cell_renderer_keys_new ();

#if !GTK_CHECK_VERSION (2, 10, 0)
enum GtkCellRendererAccelMode
{
  GTK_CELL_RENDERER_ACCEL_MODE_GTK,
  GTK_CELL_RENDERER_ACCEL_MODE_OTHER
};
#endif

void  egg_cell_renderer_keys_set_accelerator (EggCellRendererKeys *keys, guint keyval, GdkModifierType mask);
void  egg_cell_renderer_keys_get_accelerator (EggCellRendererKeys *keys, guint *keyval, GdkModifierType *mask);
void  egg_cell_renderer_keys_set_accel_mode (EggCellRendererKeys *keys, GtkCellRendererAccelMode accel_mode);
gchar *egg_accelerator_get_label (guint accel_key, GdkModifierType accel_mods);

#endif // __GTK_CELL_RENDERER_KEYS_H__
