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

/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimphintbox.c
 * Copyright (C) 2006 Sven Neumann <sven@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gtk/gtk.h>

#include "gnome-cmd-hintbox.h"

using namespace std;


typedef GtkHBoxClass  GnomeCmdHintBoxClass;

typedef struct
{
  GtkHBox    parent_instance;

  gchar     *stock_id;
  gchar     *hint;
} GnomeCmdHintBox;

#define GNOME_CMD_HINT_BOX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_HINT_BOX, GnomeCmdHintBox))


enum
{
  PROP_0,
  PROP_STOCK_ID,
  PROP_HINT
};

static GObject *gnome_cmd_hint_box_constructor  (GType                  type,
                                                 guint                  n_params,
                                                 GObjectConstructParam *params);
static void     gnome_cmd_hint_box_finalize     (GObject               *object);
static void     gnome_cmd_hint_box_set_property (GObject               *object,
                                                 guint                  property_id,
                                                 const GValue          *value,
                                                 GParamSpec            *pspec);
static void     gnome_cmd_hint_box_get_property (GObject               *object,
                                                 guint                  property_id,
                                                 GValue                *value,
                                                 GParamSpec            *pspec);

G_DEFINE_TYPE (GnomeCmdHintBox, gnome_cmd_hint_box, GTK_TYPE_HBOX)

#define parent_class gnome_cmd_hint_box_parent_class


static void
gnome_cmd_label_set_attributes (GtkLabel *label, ...);


static void
gnome_cmd_hint_box_class_init (GnomeCmdHintBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor   = gnome_cmd_hint_box_constructor;
  object_class->finalize      = gnome_cmd_hint_box_finalize;
  object_class->set_property  = gnome_cmd_hint_box_set_property;
  object_class->get_property  = gnome_cmd_hint_box_get_property;

  g_object_class_install_property (object_class, PROP_STOCK_ID,
                                   g_param_spec_string ("stock-id", NULL, NULL,
                                                        GTK_STOCK_DIALOG_INFO,
                                                        GParamFlags (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE)));
  g_object_class_install_property (object_class, PROP_HINT,
                                   g_param_spec_string ("hint", NULL, NULL,
                                                        NULL,
                                                        GParamFlags(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE)));
}

static void
gnome_cmd_hint_box_init (GnomeCmdHintBox *box)
{
  box->stock_id = NULL;
  box->hint     = NULL;
}

static GObject *
gnome_cmd_hint_box_constructor (GType                  type,
                                guint                  n_params,
                                GObjectConstructParam *params)
{
  GObject     *object;
  GnomeCmdHintBox *box;
  GtkWidget   *label;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  box = GNOME_CMD_HINT_BOX (object);

  gtk_box_set_spacing (GTK_BOX (box), 12);

  if (box->stock_id)
    {
      GtkWidget *image = gtk_image_new_from_stock (box->stock_id,
                                                   GTK_ICON_SIZE_DIALOG);

      gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
      gtk_widget_show (image);
    }

  label = (GtkWidget *) g_object_new (GTK_TYPE_LABEL,
                                      "label",   box->hint,
                                      "wrap",    TRUE,
                                      "justify", GTK_JUSTIFY_LEFT,
                                      "xalign",  0.0,
                                      "yalign",  0.5,
                                      NULL);

  gnome_cmd_label_set_attributes (GTK_LABEL (label),
                                  PANGO_ATTR_STYLE, PANGO_STYLE_ITALIC,
                                  -1);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  return object;
}

static void
gnome_cmd_hint_box_finalize (GObject *object)
{
  GnomeCmdHintBox *box = GNOME_CMD_HINT_BOX (object);

  if (box->stock_id)
    {
      g_free (box->stock_id);
      box->stock_id = NULL;
    }

  if (box->hint)
    {
      g_free (box->hint);
      box->hint = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_cmd_hint_box_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GnomeCmdHintBox *box = GNOME_CMD_HINT_BOX (object);

  switch (property_id)
    {
    case PROP_STOCK_ID:
      box->stock_id = g_value_dup_string (value);
      break;

    case PROP_HINT:
      box->hint = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gnome_cmd_hint_box_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GnomeCmdHintBox *box = GNOME_CMD_HINT_BOX (object);

  switch (property_id)
    {
    case PROP_STOCK_ID:
      g_value_set_string (value, box->stock_id);
      break;

    case PROP_HINT:
      g_value_set_string (value, box->hint);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/**
 * gnome_cmd_hint_box_new:
 * @hint: text to display as a user hint
 *
 * Creates a new widget that shows a text label showing @hint,
 * decorated with a GTK_STOCK_DIALOG_INFO icon.
 *
 * Return value: a new widget
 **/
GtkWidget *
gnome_cmd_hint_box_new (const gchar *hint)
{
  g_return_val_if_fail (hint != NULL, NULL);

  return (GtkWidget *) g_object_new (GNOME_CMD_TYPE_HINT_BOX,
                                     "hint", hint,
                                     NULL);
}

/**
 * gnome_cmd_label_set_attributes:
 * @label: a #GtkLabel
 * @...:   a list of PangoAttrType and value pairs terminated by -1.
 *
 * Sets Pango attributes on a #GtkLabel in a more convenient way than
 * gtk_label_set_attributes().
 *
 * This function is useful if you want to change the font attributes
 * of a #GtkLabel. This is an alternative to using PangoMarkup which
 * is slow to parse and akward to handle in an i18n-friendly way.
 *
 * The attributes are set on the complete label, from start to end. If
 * you need to set attributes on part of the label, you will have to
 * use the PangoAttributes API directly.
 **/
static void
gnome_cmd_label_set_attributes (GtkLabel *label, ...)
{
  PangoAttribute *attr  = NULL;
  PangoAttrList  *attrs;
  va_list         args;

  g_return_if_fail (GTK_IS_LABEL (label));

  attrs = pango_attr_list_new ();

  va_start (args, label);

  do
    {
      PangoAttrType   attr_type = ( PangoAttrType) va_arg (args, gint);

      switch (attr_type)
        {
        case PANGO_ATTR_LANGUAGE:
          attr = pango_attr_language_new (va_arg (args, PangoLanguage *));
          break;

        case PANGO_ATTR_FAMILY:
          attr = pango_attr_family_new (va_arg (args, const gchar *));
          break;

        case PANGO_ATTR_STYLE:
          attr = pango_attr_style_new ((PangoStyle) va_arg (args, gint));
          break;

        case PANGO_ATTR_WEIGHT:
          attr = pango_attr_weight_new ((PangoWeight) va_arg (args, gint));
          break;

        case PANGO_ATTR_VARIANT:
          attr = pango_attr_variant_new ((PangoVariant) va_arg (args, gint));
          break;

        case PANGO_ATTR_STRETCH:
          attr = pango_attr_stretch_new ((PangoStretch) va_arg (args, gint));
          break;

        case PANGO_ATTR_SIZE:
          attr = pango_attr_size_new (va_arg (args, gint));
          break;

        case PANGO_ATTR_FONT_DESC:
          attr = pango_attr_font_desc_new (va_arg (args,
                                                   const PangoFontDescription *));
          break;

        case PANGO_ATTR_FOREGROUND:
          {
            const PangoColor *color = va_arg (args, const PangoColor *);

            attr = pango_attr_foreground_new (color->red,
                                              color->green,
                                              color->blue);
          }
          break;

        case PANGO_ATTR_BACKGROUND:
          {
            const PangoColor *color = va_arg (args, const PangoColor *);

            attr = pango_attr_background_new (color->red,
                                              color->green,
                                              color->blue);
          }
          break;

        case PANGO_ATTR_UNDERLINE:
          attr = pango_attr_underline_new ((PangoUnderline) va_arg (args, gint));
          break;

        case PANGO_ATTR_STRIKETHROUGH:
          attr = pango_attr_strikethrough_new (va_arg (args, gboolean));
          break;

        case PANGO_ATTR_RISE:
          attr = pango_attr_rise_new (va_arg (args, gint));
          break;

        case PANGO_ATTR_SCALE:
          attr = pango_attr_scale_new (va_arg (args, gdouble));
          break;

        default:
          g_warning ("%s: invalid PangoAttribute type %d",
                     G_STRFUNC, attr_type);
        case -1:
        case PANGO_ATTR_INVALID:
          attr = NULL;
          break;
        }

      if (attr)
        {
          attr->start_index = 0;
          attr->end_index   = -1;
          pango_attr_list_insert (attrs, attr);
        }
    }
  while (attr);

  va_end (args);

  gtk_label_set_attributes (label, attrs);
  pango_attr_list_unref (attrs);
}
