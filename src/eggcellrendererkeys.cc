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
#include <gdk/gdkx.h>
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
                                                              GdkRectangle             *background_area,
                                                              GdkRectangle             *cell_area,
                                                              GtkCellRendererState      flags);


static void egg_cell_renderer_keys_get_property (GObject         *object,
                                                 guint            param_id,
                                                 GValue          *value,
                                                 GParamSpec      *pspec);
static void egg_cell_renderer_keys_set_property (GObject         *object,
                                                 guint            param_id,
                                                 const GValue    *value,
                                                 GParamSpec      *pspec);
static void egg_cell_renderer_keys_get_size     (GtkCellRenderer *cell,
                                                 GtkWidget       *widget,
                                                 GdkRectangle    *cell_area,
                                                 gint            *x_offset,
                                                 gint            *y_offset,
                                                 gint            *width,
                                                 gint            *height);


enum
{
    PROP_0,
    PROP_ACCEL_KEY,
    PROP_ACCEL_MASK,
    PROP_ACCEL_MODE
};


static GtkCellRendererTextClass *parent_class = NULL;


GType egg_cell_renderer_keys_get_type ()
{
    static GType cell_keys_type = 0;

    if (!cell_keys_type)
    {
        static const GTypeInfo cell_keys_info =
        {
            sizeof(EggCellRendererKeysClass),
            NULL,                /* base_init */
            NULL,                /* base_finalize */
            (GClassInitFunc) egg_cell_renderer_keys_class_init,
            NULL,                /* class_finalize */
            NULL,                /* class_data */
            sizeof(EggCellRendererKeys),
            0,              /* n_preallocs */
            (GInstanceInitFunc) egg_cell_renderer_keys_init
        };

        cell_keys_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT, "EggCellRendererKeys", &cell_keys_info, GTypeFlags(0));
    }

    return cell_keys_type;
}


static void egg_cell_renderer_keys_init (EggCellRendererKeys *cell_keys)
{
    cell_keys->accel_mode = GTK_CELL_RENDERER_ACCEL_MODE_GTK;
}

static void
marshal_VOID__STRING_UINT_FLAGS_UINT (GClosure     *closure,
                                      GValue       *return_value,
                                      guint         n_param_values,
                                      const GValue *param_values,
                                      gpointer      invocation_hint,
                                      gpointer      marshal_data)
{
    g_return_if_fail (n_param_values == 5);

    typedef void (*GMarshalFunc_VOID__STRING_UINT_FLAGS_UINT) (gpointer     data1,
                                                               const char  *arg_1,
                                                               guint        arg_2,
                                                               int          arg_3,
                                                               guint        arg_4,
                                                               gpointer     data2);
    register GMarshalFunc_VOID__STRING_UINT_FLAGS_UINT callback;
    register GCClosure *cc = (GCClosure*) closure;
    register gpointer data1, data2;

    if (G_CCLOSURE_SWAP_DATA (closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }

    callback = (GMarshalFunc_VOID__STRING_UINT_FLAGS_UINT) (marshal_data ? marshal_data : cc->callback);

    callback (data1,
              g_value_get_string (param_values + 1),
              g_value_get_uint (param_values + 2),
              g_value_get_flags (param_values + 3),
              g_value_get_uint (param_values + 4),
              data2);
}


static void egg_cell_renderer_keys_class_init (EggCellRendererKeysClass *cell_keys_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (cell_keys_class);
    GtkCellRendererClass *cell_renderer_class = GTK_CELL_RENDERER_CLASS (cell_keys_class);

    parent_class = (GtkCellRendererTextClass *) g_type_class_peek_parent (object_class);

    GTK_CELL_RENDERER_CLASS (cell_keys_class)->start_editing = egg_cell_renderer_keys_start_editing;

    object_class->get_property = egg_cell_renderer_keys_get_property;
    object_class->set_property = egg_cell_renderer_keys_set_property;
    cell_renderer_class->get_size = egg_cell_renderer_keys_get_size;

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
                  marshal_VOID__STRING_UINT_FLAGS_UINT,
                  G_TYPE_NONE, 4,
                  G_TYPE_STRING,
                  G_TYPE_UINT,
                  GDK_TYPE_MODIFIER_TYPE,
                  G_TYPE_UINT);
}


GtkCellRenderer *egg_cell_renderer_keys_new ()
{
    return GTK_CELL_RENDERER (g_object_new (EGG_TYPE_CELL_RENDERER_KEYS, NULL));
}


static void egg_cell_renderer_keys_finalize (GObject *object)
{
    (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


gchar *egg_accelerator_get_label (guint accel_key, GdkModifierType accel_mods)
{
    if (accel_key == 0)
        return g_strdup (_("Disabled"));

    static const gchar text_shift[] = "Shift+";
    static const gchar text_control[] = "Ctrl+";
    static const gchar text_mod1[] = "Alt+";
    static const gchar text_mod2[] = "Mod2+";
    static const gchar text_mod3[] = "Mod3+";
#if GTK_CHECK_VERSION (2, 10, 0)
    static const gchar text_mod4[] = "Mod4+";
#else
    static const gchar text_mod4[] = "Super+";
#endif
    static const gchar text_mod5[] = "Mod5+";
#if GTK_CHECK_VERSION (2, 10, 0)
    static const gchar text_meta[] = "Meta+";
    static const gchar text_super[] = "Super+";
    static const gchar text_hyper[] = "Hyper+";
#endif

    gchar *keyval_name = gdk_keyval_name (gdk_keyval_to_upper (accel_key));
    if (!keyval_name)
        keyval_name = "";

    guint l = strlen (keyval_name);

    if (accel_mods & GDK_SHIFT_MASK)
        l += sizeof(text_shift)-1;
    if (accel_mods & GDK_CONTROL_MASK)
        l += sizeof(text_control)-1;
    if (accel_mods & GDK_MOD1_MASK)
        l += sizeof(text_mod1)-1;
    if (accel_mods & GDK_MOD2_MASK)
        l += sizeof(text_mod2)-1;
    if (accel_mods & GDK_MOD3_MASK)
        l += sizeof(text_mod3)-1;
    if (accel_mods & GDK_MOD4_MASK)
        l += sizeof(text_mod4)-1;
    if (accel_mods & GDK_MOD5_MASK)
        l += sizeof(text_mod5)-1;
#if GTK_CHECK_VERSION (2, 10, 0)
    if (accel_mods & GDK_META_MASK)
        l += sizeof(text_meta)-1;
    if (accel_mods & GDK_HYPER_MASK)
        l += sizeof(text_hyper)-1;
    if (accel_mods & GDK_SUPER_MASK)
        l += sizeof(text_super)-1;
#endif

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
    if (accel_mods & GDK_MOD1_MASK)
    {
      strcpy (s, text_mod1);
      s +=  sizeof(text_mod1)-1;
    }
    if (accel_mods & GDK_MOD2_MASK)
    {
      strcpy (s, text_mod2);
      s +=  sizeof(text_mod2)-1;
    }
    if (accel_mods & GDK_MOD3_MASK)
    {
      strcpy (s, text_mod3);
      s +=  sizeof(text_mod3)-1;
    }
    if (accel_mods & GDK_MOD4_MASK)
    {
      strcpy (s, text_mod4);
      s +=  sizeof(text_mod4)-1;
    }
    if (accel_mods & GDK_MOD5_MASK)
    {
      strcpy (s, text_mod5);
      s +=  sizeof(text_mod5)-1;
    }
#if GTK_CHECK_VERSION (2, 10, 0)
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
#endif

    strcpy (s, keyval_name);

    return accelerator;
}


static void egg_cell_renderer_keys_get_property  (GObject *object, guint param_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (object));

    EggCellRendererKeys *keys = EGG_CELL_RENDERER_KEYS (object);

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

    EggCellRendererKeys *keys = EGG_CELL_RENDERER_KEYS (object);

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


inline gboolean is_modifier (guint keycode)
{
    gboolean retval = FALSE;

    XModifierKeymap *mod_keymap = XGetModifierMapping (gdk_display);

    gint map_size = 8 * mod_keymap->max_keypermod;

    for (gint i=0; i<map_size;  ++i)
        if (keycode == mod_keymap->modifiermap[i])
        {
            retval = TRUE;
            break;
        }

    XFreeModifiermap (mod_keymap);

    return retval;
}


void egg_cell_renderer_keys_get_size (GtkCellRenderer *cell,
                                      GtkWidget       *widget,
                                      GdkRectangle    *cell_area,
                                      gint            *x_offset,
                                      gint            *y_offset,
                                      gint            *width,
                                      gint            *height)
{
    EggCellRendererKeys *keys = (EggCellRendererKeys *) cell;
    GtkRequisition requisition;

    if (keys->sizing_label == NULL)
        keys->sizing_label = gtk_label_new (_("New accelerator..."));

    gtk_widget_size_request (keys->sizing_label, &requisition);
    GTK_CELL_RENDERER_CLASS (parent_class)->get_size (cell, widget, cell_area, x_offset, y_offset, width, height);

    // FIXME: need to take the cell_area et al. into account
    if (width)
        *width = MAX (*width, requisition.width);
    if (height)
        *height = MAX (*height, requisition.height);
}


static gboolean grab_key_callback (GtkWidget *widget, GdkEventKey *event, EggCellRendererKeys *keys)
{
#if GTK_CHECK_VERSION (2, 10, 0)
    if (event->is_modifier)
        return TRUE;
#else
    if (is_modifier (event->hardware_keycode))
        return TRUE;
#endif

    switch (event->keyval)
    {
        case GDK_Super_L:
        case GDK_Super_R:
        // case GDK_Meta_L:
        // case GDK_Meta_R:
        // case GDK_Hyper_L:
        // case GDK_Hyper_R:
            return TRUE;
    }

    GdkDisplay *display = gtk_widget_get_display (widget);

    gboolean edited = FALSE;
    guint consumed_modifiers = 0;

    if (keys->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
        gdk_keymap_translate_keyboard_state (gdk_keymap_get_for_display (display),
                                             event->hardware_keycode,
                                             (GdkModifierType) event->state,
                                             event->group,
                                             NULL, NULL, NULL, (GdkModifierType *) &consumed_modifiers);

    guint accel_key = gdk_keyval_to_lower (event->keyval);
    guint accel_mods = 0;

    if (accel_key == GDK_ISO_Left_Tab)
        accel_key = GDK_Tab;

#if GTK_CHECK_VERSION (2, 10, 0)
    accel_mods = event->state & gtk_accelerator_get_default_mod_mask ();
#else
    accel_mods = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);
#endif

    // filter consumed modifiers
    if (keys->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
        accel_mods &= ~consumed_modifiers;

    // put shift back if it changed the case of the key, not otherwise.
    if (keys->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
        if (accel_key != event->keyval)
            accel_mods |= GDK_SHIFT_MASK;

    if (accel_mods == 0)
        switch (event->keyval)
        {
            case GDK_Escape:
                accel_key = 0;
                accel_mods = 0;

                goto out; // cancel

            default:
                break;
        }

    if (keys->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
        if (accel_key != GDK_Tab && !gtk_accelerator_valid (accel_key, (GdkModifierType) accel_mods))
        {
#if GTK_CHECK_VERSION (2, 12, 0)
            gtk_widget_error_bell (widget);
#endif
            return TRUE;
        }

    edited = TRUE;


  out:
    gdk_display_keyboard_ungrab (display, event->time);
    gdk_display_pointer_ungrab (display, event->time);

    char *path = g_strdup ((gchar *) g_object_get_data (G_OBJECT (keys->edit_widget), EGG_CELL_RENDERER_TEXT_PATH));

    gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (keys->edit_widget));
    gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (keys->edit_widget));
    keys->edit_widget = NULL;
    keys->grab_widget = NULL;

    if (edited)
        g_signal_emit_by_name (G_OBJECT (keys), "accel-edited", path, accel_key, accel_mods, event->hardware_keycode);

    g_free (path);

    return TRUE;
}


static void ungrab_stuff (GtkWidget *widget, EggCellRendererKeys *keys)
{
    GdkDisplay *display = gtk_widget_get_display (widget);

    gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
    gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);

    g_signal_handlers_disconnect_by_func (G_OBJECT (keys->grab_widget), (gpointer) G_CALLBACK (grab_key_callback), keys);
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
        sizeof(GtkEventBoxClass),
        NULL,               /* base_init */
        NULL,               /* base_finalize */
        NULL,
        NULL,               /* class_finalize */
        NULL,               /* class_data */
        sizeof(GtkEventBox),
        0,                  /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };

      static const GInterfaceInfo cell_editable_info = { (GInterfaceInitFunc) pointless_eventbox_cell_editable_init, NULL, NULL };

      eventbox_type = g_type_register_static (GTK_TYPE_EVENT_BOX, "EggCellEditableEventBox", &eventbox_info, GTypeFlags(0));

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
                                      GdkRectangle         *background_area,
                                      GdkRectangle         *cell_area,
                                      GtkCellRendererState  flags)
{
    g_return_val_if_fail (widget->window != NULL, NULL);

    GtkWidget *label;
    GtkWidget *eventbox;

    GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
    EggCellRendererKeys *keys = EGG_CELL_RENDERER_KEYS (cell);

    // If the cell isn't editable we return NULL
    if (celltext->editable == FALSE)
        return NULL;

    if (gdk_keyboard_grab (widget->window, FALSE, gdk_event_get_time (event)) != GDK_GRAB_SUCCESS)
        return NULL;

    if (gdk_pointer_grab (widget->window, FALSE,
                          GDK_BUTTON_PRESS_MASK,
                          NULL, NULL,
                          gdk_event_get_time (event)) != GDK_GRAB_SUCCESS)
    {
        gdk_keyboard_ungrab (gdk_event_get_time (event));
        return NULL;
    }

    keys->grab_widget = widget;

    g_signal_connect (G_OBJECT (widget), "key-press-event", G_CALLBACK (grab_key_callback), keys);

    eventbox = (GtkWidget *) g_object_new (pointless_eventbox_subclass_get_type (), NULL);
    keys->edit_widget = eventbox;
    g_object_add_weak_pointer (G_OBJECT (keys->edit_widget), (void**) &keys->edit_widget);

    label = gtk_label_new (NULL);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &widget->style->bg[GTK_STATE_SELECTED]);

    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &widget->style->fg[GTK_STATE_SELECTED]);

    gtk_label_set_text (GTK_LABEL (label), _("New accelerator..."));

    gtk_container_add (GTK_CONTAINER (eventbox), label);

    g_object_set_data_full (G_OBJECT (keys->edit_widget), EGG_CELL_RENDERER_TEXT_PATH, g_strdup (path), g_free);

    gtk_widget_show_all (keys->edit_widget);

    g_signal_connect (G_OBJECT (keys->edit_widget), "unrealize", G_CALLBACK (ungrab_stuff), keys);

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
