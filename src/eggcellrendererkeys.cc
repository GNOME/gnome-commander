/**
 * @file eggcellrendererkeys.cc
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

/*
 * This file is part of gnome-terminal.
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Gnome-terminal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "gnome-cmd-includes.h"
#include "eggcellrendererkeys.h"
#include "utils.h"

#define EGG_CELL_RENDERER_TEXT_PATH "egg-cell-renderer-text"

static void             egg_cell_renderer_keys_finalize      (GObject             *object);
static void             egg_cell_renderer_keys_init          (EggCellRendererKeys *cell_keys);
static void             egg_cell_renderer_keys_class_init    (EggCellRendererKeysClass *cell_keys_class);
static GtkCellEditable *egg_cell_renderer_keys_start_editing (GtkCellRenderer          *cell,
                                                              GdkEvent                 *event,
                                                              GtkWidget                *widget,
                                                              const gchar              *path,
                                                              const GdkRectangle       *background_area,
                                                              const GdkRectangle       *cell_area,
                                                              GtkCellRendererState      flags);


static void egg_cell_renderer_keys_get_property (GObject         *object,
                                                 guint            param_id,
                                                 GValue          *value,
                                                 GParamSpec      *pspec);
static void egg_cell_renderer_keys_set_property (GObject         *object,
                                                 guint            param_id,
                                                 const GValue    *value,
                                                 GParamSpec      *pspec);
static void egg_cell_renderer_get_preferred_width (GtkCellRenderer* cell,
                                                   GtkWidget* widget,
                                                   int* minimum_size,
                                                   int* natural_size);
static void egg_cell_renderer_get_preferred_height (GtkCellRenderer* cell,
                                                    GtkWidget* widget,
                                                    int* minimum_size,
                                                    int* natural_size);

enum
{
    PROP_0,
    PROP_ACCEL_KEY,
    PROP_ACCEL_MASK,
    PROP_ACCEL_MODE
};


G_DEFINE_TYPE (EggCellRendererKeys, egg_cell_renderer_keys, GTK_TYPE_CELL_RENDERER_TEXT)


static void egg_cell_renderer_keys_init (EggCellRendererKeys *cell_keys)
{
    cell_keys->accel_mode = GTK_CELL_RENDERER_ACCEL_MODE_GTK;
}


static void egg_cell_renderer_keys_class_init (EggCellRendererKeysClass *cell_keys_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (cell_keys_class);
    GtkCellRendererClass *cell_renderer_class = GTK_CELL_RENDERER_CLASS (cell_keys_class);

    cell_renderer_class->start_editing = egg_cell_renderer_keys_start_editing;
    cell_renderer_class->get_preferred_width = egg_cell_renderer_get_preferred_width;
    cell_renderer_class->get_preferred_height = egg_cell_renderer_get_preferred_height;
    object_class->get_property = egg_cell_renderer_keys_get_property;
    object_class->set_property = egg_cell_renderer_keys_set_property;
    object_class->finalize = egg_cell_renderer_keys_finalize;

    g_object_class_install_property (object_class,
                                     PROP_ACCEL_KEY,
                                     g_param_spec_uint ("accel-key",
                                                       _("Accelerator key"),
                                                       _("Accelerator key"),
                                                        0,
                                                        G_MAXINT,
                                                        0,
                                                        GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE)));

    g_object_class_install_property (object_class,
                                     PROP_ACCEL_MASK,
                                     g_param_spec_flags ("accel-mods",
                                                         _("Accelerator modifiers"),
                                                         _("Accelerator modifiers"),
                                                         GDK_TYPE_MODIFIER_TYPE,
                                                         0,
                                                         GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE)));

    g_object_class_install_property (object_class,
                                     PROP_ACCEL_MODE,
                                     g_param_spec_int ("accel-mode",
                                                       _("Accelerator Mode"),
                                                       _("The type of accelerator."),
                                                       0,
                                                       2,
                                                       0,
                                                       GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE)));

    g_signal_new ("accel-edited",
                  EGG_TYPE_CELL_RENDERER_KEYS,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EggCellRendererKeysClass, keys_edited),
                  NULL, NULL,
                  nullptr,
                  G_TYPE_NONE, 3,
                  G_TYPE_STRING,
                  G_TYPE_UINT,
                  GDK_TYPE_MODIFIER_TYPE);
}


GtkCellRenderer *egg_cell_renderer_keys_new ()
{
    return GTK_CELL_RENDERER (g_object_new (EGG_TYPE_CELL_RENDERER_KEYS, NULL));
}


static void egg_cell_renderer_keys_finalize (GObject *object)
{
    G_OBJECT_CLASS (egg_cell_renderer_keys_parent_class)->finalize (object);
}


gchar *egg_accelerator_get_label (guint accel_key, GdkModifierType accel_mods)
{
    if (accel_key == 0)
        return g_strdup (_("Disabled"));

    static const gchar text_shift[] = "Shift+";
    static const gchar text_control[] = "Ctrl+";
    static const gchar text_alt[] = "Alt+";
    static const gchar text_meta[] = "Meta+";
    static const gchar text_super[] = "Super+";
    static const gchar text_hyper[] = "Hyper+";

    const gchar *keyval_name = gdk_keyval_name (gdk_keyval_to_upper (accel_key));
    if (!keyval_name)
        keyval_name = "";

    guint l = strlen (keyval_name);

    if (accel_mods & GDK_SHIFT_MASK)
        l += sizeof(text_shift)-1;
    if (accel_mods & GDK_CONTROL_MASK)
        l += sizeof(text_control)-1;
    if (accel_mods & GDK_ALT_MASK)
        l += sizeof(text_alt)-1;
    if (accel_mods & GDK_META_MASK)
        l += sizeof(text_meta)-1;
    if (accel_mods & GDK_HYPER_MASK)
        l += sizeof(text_hyper)-1;
    if (accel_mods & GDK_SUPER_MASK)
        l += sizeof(text_super)-1;

    gchar *accelerator = g_new (gchar, l+1);
    gchar *s = accelerator;

    if (accel_mods & GDK_SHIFT_MASK)
    {
      strcpy (s, text_shift);
      s += sizeof(text_shift)-1;
    }
    if (accel_mods & GDK_CONTROL_MASK)
    {
      strcpy (s, text_control);
      s +=  sizeof(text_control)-1;
    }
    if (accel_mods & GDK_ALT_MASK)
    {
      strcpy (s, text_alt);
      s +=  sizeof(text_alt)-1;
    }
    if (accel_mods & GDK_META_MASK)
    {
      strcpy (s, text_meta);
      s +=  sizeof(text_meta)-1;
    }
    if (accel_mods & GDK_HYPER_MASK)
    {
      strcpy (s, text_hyper);
      s +=  sizeof(text_hyper)-1;
    }
    if (accel_mods & GDK_SUPER_MASK)
    {
      strcpy (s, text_super);
      s +=  sizeof(text_super)-1;
    }

    strcpy (s, keyval_name);

    return accelerator;
}


static void egg_cell_renderer_keys_get_property  (GObject *object, guint param_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (object));

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    EggCellRendererKeys *keys = EGG_CELL_RENDERER_KEYS (object);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    switch (param_id)
    {
        case PROP_ACCEL_KEY:
            g_value_set_uint (value, keys->accel_key);
            break;

        case PROP_ACCEL_MASK:
            g_value_set_flags (value, keys->accel_mask);
            break;

        case PROP_ACCEL_MODE:
            g_value_set_int (value, keys->accel_mode);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
    }
}


static void egg_cell_renderer_keys_set_property (GObject *object, guint param_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (object));

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    EggCellRendererKeys *keys = EGG_CELL_RENDERER_KEYS (object);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    switch (param_id)
    {
        case PROP_ACCEL_KEY:
            egg_cell_renderer_keys_set_accelerator (keys, g_value_get_uint (value), keys->accel_mask);
            break;

        case PROP_ACCEL_MASK:
            egg_cell_renderer_keys_set_accelerator (keys, keys->accel_key, (GdkModifierType) g_value_get_flags (value));
            break;

        case PROP_ACCEL_MODE:
            egg_cell_renderer_keys_set_accel_mode (keys, (GtkCellRendererAccelMode) g_value_get_int (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
    }
}


static void measure_text_size (EggCellRendererKeys *keys, GtkRequisition *minimum, GtkRequisition *natural)
{
    if (keys->sizing_label == nullptr)
        keys->sizing_label = gtk_label_new (_("New accelerator…"));
    gtk_widget_get_preferred_size (keys->sizing_label, minimum, natural);
}


static void egg_cell_renderer_get_preferred_width (GtkCellRenderer* cell,
                                                   GtkWidget* widget,
                                                   int* minimum_size,
                                                   int* natural_size)
{
    GTK_CELL_RENDERER_CLASS (egg_cell_renderer_keys_parent_class)->get_preferred_width (cell, widget, minimum_size, natural_size);

    auto keys = EGG_CELL_RENDERER_KEYS (cell);

    GtkRequisition label_minimum_size;
    GtkRequisition label_natural_size;
    measure_text_size (keys, &label_minimum_size, &label_natural_size);

    // FIXME: need to take the cell_area et al. into account
    *minimum_size = MAX (*minimum_size, label_minimum_size.width);
    *natural_size = MAX (*natural_size, label_natural_size.width);
}


static void egg_cell_renderer_get_preferred_height (GtkCellRenderer* cell,
                                                    GtkWidget* widget,
                                                    int* minimum_size,
                                                    int* natural_size)
{
    GTK_CELL_RENDERER_CLASS (egg_cell_renderer_keys_parent_class)->get_preferred_height (cell, widget, minimum_size, natural_size);

    auto keys = EGG_CELL_RENDERER_KEYS (cell);

    GtkRequisition label_minimum_size;
    GtkRequisition label_natural_size;
    measure_text_size (keys, &label_minimum_size, &label_natural_size);

    // FIXME: need to take the cell_area et al. into account
    *minimum_size = MAX (*minimum_size, label_minimum_size.height);
    *natural_size = MAX (*natural_size, label_natural_size.height);
}

static gboolean grab_key_callback (GtkWidget *widget, GdkKeyEvent *event, EggCellRendererKeys *keys)
{
    if (gdk_key_event_is_modifier (GDK_EVENT (event)))
        return TRUE;

    switch (gdk_key_event_get_keyval (GDK_EVENT (event)))
    {
        case GDK_KEY_Super_L:
        case GDK_KEY_Super_R:
        // case GDK_KEY_Meta_L:
        // case GDK_KEY_Meta_R:
        // case GDK_KEY_Hyper_L:
        // case GDK_KEY_Hyper_R:
            return TRUE;
        default:
            break;
    }

    gboolean edited = FALSE;

    guint accel_key = gdk_keyval_to_lower (gdk_key_event_get_keyval (GDK_EVENT (event)));
    guint accel_mods = 0;

    if (accel_key == GDK_KEY_ISO_Left_Tab)
        accel_key = GDK_KEY_Tab;

    accel_mods = gdk_event_get_modifier_state (GDK_EVENT (event)) & gtk_accelerator_get_default_mod_mask ();

    if (accel_mods == 0)
        switch (gdk_key_event_get_keyval (GDK_EVENT (event)))
        {
            case GDK_KEY_Escape:
                accel_key = 0;
                accel_mods = 0;

                goto out; // cancel

            default:
                break;
        }

    if (keys->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
        if (accel_key != GDK_KEY_Tab && !gtk_accelerator_valid (accel_key, (GdkModifierType) accel_mods))
        {
            gtk_widget_error_bell (widget);
            return TRUE;
        }

    edited = TRUE;


  out:
    char *path = g_strdup ((gchar *) g_object_get_data (G_OBJECT (keys->edit_widget), EGG_CELL_RENDERER_TEXT_PATH));

    gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (keys->edit_widget));
    gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (keys->edit_widget));
    keys->edit_widget = NULL;
    keys->grab_widget = NULL;

    if (edited)
        g_signal_emit_by_name (keys, "accel-edited", path, accel_key, accel_mods);

    g_free (path);

    return TRUE;
}


static void ungrab_stuff (GtkWidget *widget, EggCellRendererKeys *keys)
{
    g_signal_handlers_disconnect_by_func (keys->grab_widget, (gpointer) grab_key_callback, keys);
}


static void pointless_eventbox_start_editing (GtkCellEditable *cell_editable, GdkEvent *event)
{
    // do nothing, because we are pointless
}


static void pointless_eventbox_cell_editable_init (GtkCellEditableIface *iface)
{
    iface->start_editing = pointless_eventbox_start_editing;
}


static GType pointless_eventbox_subclass_get_type ()
{
  static GType eventbox_type = 0;

  if (!eventbox_type)
    {
      static const GTypeInfo eventbox_info =
      {
        sizeof(GtkWidgetClass),
        NULL,               /* base_init */
        NULL,               /* base_finalize */
        NULL,
        NULL,               /* class_finalize */
        NULL,               /* class_data */
        sizeof(GtkWidget),
        0,                  /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };

      static const GInterfaceInfo cell_editable_info = { (GInterfaceInitFunc) pointless_eventbox_cell_editable_init, NULL, NULL };

      eventbox_type = g_type_register_static (GTK_TYPE_WIDGET, "EggCellEditableEventBox", &eventbox_info, GTypeFlags(0));

      g_type_add_interface_static (eventbox_type,
                                   GTK_TYPE_CELL_EDITABLE,
                                   &cell_editable_info);
    }

  return eventbox_type;
}


static GtkCellEditable *
egg_cell_renderer_keys_start_editing (GtkCellRenderer      *cell,
                                      GdkEvent             *event,
                                      GtkWidget            *widget,
                                      const gchar          *path,
                                      const GdkRectangle   *background_area,
                                      const GdkRectangle   *cell_area,
                                      GtkCellRendererState  flags)
{
    g_return_val_if_fail (widget != NULL, NULL);

    GtkWidget *label;
    GtkWidget *eventbox;

    GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
    EggCellRendererKeys *keys = EGG_CELL_RENDERER_KEYS (cell);

    gboolean celltext_editable;
    g_object_get (celltext, "editable", &celltext_editable, NULL);

    // If the cell isn't editable we return NULL
    if (celltext_editable == FALSE)
        return NULL;

    keys->grab_widget = widget;

    g_signal_connect (widget, "key-press-event", G_CALLBACK (grab_key_callback), keys);

    eventbox = (GtkWidget *) g_object_new (pointless_eventbox_subclass_get_type (), NULL);
    keys->edit_widget = eventbox;
    g_object_add_weak_pointer (G_OBJECT (keys->edit_widget), (void**) &keys->edit_widget);

    label = gtk_label_new (NULL);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

/*
    auto style_context = gtk_widget_get_style_context (widget);
    GdkRGBA fg, bg;
    gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_SELECTED, &bg);
    gtk_style_context_get_color (style_context, GTK_STATE_FLAG_SELECTED, &fg);

    gtk_widget_override_background_color (eventbox, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_widget_override_color (label, GTK_STATE_FLAG_NORMAL, &fg);
*/
    gtk_label_set_text (GTK_LABEL (label), _("New accelerator…"));

    gtk_widget_set_parent (label, eventbox);

    g_object_set_data_full (G_OBJECT (keys->edit_widget), EGG_CELL_RENDERER_TEXT_PATH, g_strdup (path), g_free);

    g_signal_connect (keys->edit_widget, "unrealize", G_CALLBACK (ungrab_stuff), keys);

    keys->edit_key = keys->accel_key;

    return GTK_CELL_EDITABLE (keys->edit_widget);
}


void egg_cell_renderer_keys_set_accelerator (EggCellRendererKeys *keys, guint keyval, GdkModifierType mask)
{
    g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (keys));

    g_object_freeze_notify (G_OBJECT (keys));

    gboolean changed = FALSE;

    if (keyval != keys->accel_key)
    {
        keys->accel_key = keyval;
        g_object_notify (G_OBJECT (keys), "accel-key");
        changed = TRUE;
    }

    if (mask != keys->accel_mask)
    {
        keys->accel_mask = mask;
        g_object_notify (G_OBJECT (keys), "accel-mods");
        changed = TRUE;
    }

    g_object_thaw_notify (G_OBJECT (keys));

    if (changed)
    {
        // sync string to the key values
        // GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (keys);
        char *text = egg_accelerator_get_label (keys->accel_key, keys->accel_mask);
        g_object_set (keys, "text", text, NULL);
        g_free (text);
    }

}


void egg_cell_renderer_keys_get_accelerator (EggCellRendererKeys *keys, guint *keyval, GdkModifierType *mask)
{
    g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (keys));

    if (keyval)
        *keyval = keys->accel_key;

    if (mask)
        *mask = keys->accel_mask;
}


void egg_cell_renderer_keys_set_accel_mode (EggCellRendererKeys *keys, GtkCellRendererAccelMode accel_mode)
{
    g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (keys));

    keys->accel_mode = accel_mode;
    g_object_notify (G_OBJECT (keys), "accel-mode");
}
