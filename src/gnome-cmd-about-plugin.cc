/* Ripped from libgnomeui and adjusted to my needs. Original comments left intact below.

    Copyright (C) 2003-2006 Marcus Bjurman <marbj499@student.liu.se>
    Copyright (C) 2007-2009 Piotr Eljasiak
*/
/* gnome-about.c - An about box widget for gnome.

   Copyright (C) 2001 CodeFactory AB
   Copyright (C) 2001, 2002 Anders Carlsson

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Anders Carlsson <andersca@gnu.org>
*/

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-about-plugin.h"

using namespace std;


#define CALL_PARENT(parent_class_cast, parent, name, args)               \
        ((parent_class_cast(parent##_parent_class)->name != NULL) ?      \
                 parent_class_cast(parent##_parent_class)->name args : (void)0)


struct GnomeCmdAboutPluginPrivate
{
    gchar *name;
    gchar *version;
    gchar *copyright;
    gchar *comments;
    gchar *translator_credits;
    gchar *webpage;

    GSList *authors;
    GSList *documenters;

    GtkWidget *name_label;
    GtkWidget *comments_label;
    GtkWidget *copyright_label;

    GtkWidget *credits_dialog;
    GtkWidget *web_button;
};

enum
{
    PROP_0,
    PROP_NAME,
    PROP_VERSION,
    PROP_COPYRIGHT,
    PROP_COMMENTS,
    PROP_AUTHORS,
    PROP_DOCUMENTERS,
    PROP_TRANSLATOR_CREDITS,
    PROP_WEBPAGE
};

#define GNOME_RESPONSE_CREDITS 1

static void gnome_cmd_about_plugin_finalize (GObject *object);
static void gnome_cmd_about_plugin_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gnome_cmd_about_plugin_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

//GNOME_CLASS_BOILERPLATE (GnomeCmdAboutPlugin, gnome_cmd_about_plugin, GtkDialog, (GTypeFlags) GTK_TYPE_DIALOG)
G_DEFINE_TYPE (GnomeCmdAboutPlugin, gnome_cmd_about_plugin, GTK_TYPE_DIALOG)

static void gnome_cmd_about_plugin_update_authors_label (GnomeCmdAboutPlugin *about, GtkWidget *label)
{
    if (about->priv->authors == NULL)
    {
        gtk_widget_hide (label);
        return;
    }
    else
        gtk_widget_show (label);

    GString *string = g_string_new (NULL);

    for (GSList *list = about->priv->authors; list; list = list->next)
    {
        gchar *tmp = g_markup_escape_text ((gchar *) list->data, -1);
        g_string_append (string, tmp);

        if (list->next)
            g_string_append_c (string, '\n');
        g_free (tmp);
    }

    gtk_label_set_markup (GTK_LABEL (label), string->str);
    g_string_free (string, TRUE);
}


static void gnome_cmd_about_plugin_update_documenters_label (GnomeCmdAboutPlugin *about, GtkWidget *label)
{

    if (about->priv->documenters == NULL)
    {
        gtk_widget_hide (label);
        return;
    }

    gtk_widget_show (label);

    GString *string = g_string_new (NULL);

    for (GSList *list = about->priv->documenters; list; list = list->next)
    {
        gchar *tmp = g_markup_escape_text ((gchar *) list->data, -1);
        g_string_append (string, tmp);

        if (list->next)
            g_string_append (string, "\n");
        g_free (tmp);
    }

    gtk_label_set_markup (GTK_LABEL (label), string->str);
    g_string_free (string, TRUE);
}


static void gnome_cmd_about_plugin_update_translation_information_label (GnomeCmdAboutPlugin *about, GtkWidget *label)
{
    if (about->priv->translator_credits == NULL)
    {
        gtk_widget_hide (label);
        return;
    }

    gtk_widget_show (label);

    GString *string = g_string_new (NULL);

    gchar *tmp = g_markup_escape_text (about->priv->translator_credits, -1);
    g_string_append (string, tmp);
    g_free (tmp);

    gtk_label_set_markup (GTK_LABEL (label), string->str);
    g_string_free (string, TRUE);
}


inline GtkWidget *_create_label ()
{
    GtkWidget *label = gtk_label_new ("");

    gtk_label_set_selectable (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
    gtk_misc_set_padding (GTK_MISC (label), 8, 8);

    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    return label;
}


static void gnome_cmd_about_plugin_display_credits_dialog (GnomeCmdAboutPlugin *about)
{
    GtkWidget *dialog, *label, *notebook, *sw;

    if (about->priv->credits_dialog != NULL)
    {
        gtk_window_present (GTK_WINDOW (about->priv->credits_dialog));
        return;
    }

    dialog = gtk_dialog_new_with_buttons (_("Credits"),
                          GTK_WINDOW (about),
                          GTK_DIALOG_DESTROY_WITH_PARENT,
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                          NULL);
    about->priv->credits_dialog = dialog;
    gtk_window_set_default_size (GTK_WINDOW (dialog), 360, 260);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
    g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
    g_signal_connect (dialog, "destroy", G_CALLBACK (gtk_widget_destroyed), &(about->priv->credits_dialog));

    notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (notebook), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), notebook, TRUE, TRUE, 0);

    if (about->priv->authors != NULL)
    {
        label = _create_label ();

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), label);
        gtk_viewport_set_shadow_type (GTK_VIEWPORT (GTK_BIN (sw)->child), GTK_SHADOW_NONE);

        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, gtk_label_new (_("Written by")));
        gnome_cmd_about_plugin_update_authors_label (about, label);
    }

    if (about->priv->documenters != NULL)
    {
        label = _create_label ();

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), label);
        gtk_viewport_set_shadow_type (GTK_VIEWPORT (GTK_BIN (sw)->child), GTK_SHADOW_NONE);

        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, gtk_label_new (_("Documented by")));
        gnome_cmd_about_plugin_update_documenters_label (about, label);
    }

    if (about->priv->translator_credits != NULL)
    {
        label = _create_label ();

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), label);
        gtk_viewport_set_shadow_type (GTK_VIEWPORT (GTK_BIN (sw)->child), GTK_SHADOW_NONE);

        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, gtk_label_new (_("Translated by")));
        gnome_cmd_about_plugin_update_translation_information_label (about, label);
    }

    gtk_widget_show_all (dialog);
}


static void gnome_cmd_about_plugin_init (GnomeCmdAboutPlugin *about)
{
    GtkWidget *vbox, *hbox, *image, *label, *alignment, *button;

    // Data
    GnomeCmdAboutPluginPrivate *priv = g_new0 (GnomeCmdAboutPluginPrivate, 1);
    about->priv = priv;

    // priv->name = NULL;
    // priv->version = NULL;
    // priv->copyright = NULL;
    // priv->comments = NULL;
    // priv->translator_credits = NULL;
    // priv->authors = NULL;
    // priv->documenters = NULL;
    // priv->webpage = NULL;

    gtk_dialog_set_has_separator (GTK_DIALOG (about), FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (about), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (about)->vbox), 5);

    // Widgets
    vbox = gtk_vbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about)->vbox), vbox, TRUE, TRUE, 0);

    priv->name_label = gtk_label_new (NULL);
    gtk_widget_show (priv->name_label);
    gtk_label_set_selectable (GTK_LABEL (priv->name_label), TRUE);
    gtk_label_set_justify (GTK_LABEL (priv->name_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (vbox), priv->name_label, FALSE, FALSE, 0);

    priv->comments_label = gtk_label_new (NULL);
    gtk_widget_show (priv->comments_label);
    gtk_label_set_selectable (GTK_LABEL (priv->comments_label), TRUE);
    gtk_label_set_justify (GTK_LABEL (priv->comments_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap (GTK_LABEL (priv->comments_label), TRUE);
    gtk_box_pack_start (GTK_BOX (vbox), priv->comments_label, FALSE, FALSE, 0);

    priv->copyright_label = gtk_label_new (NULL);
    gtk_widget_show (priv->copyright_label);
    gtk_label_set_selectable (GTK_LABEL (priv->copyright_label), TRUE);
    gtk_label_set_justify (GTK_LABEL (priv->copyright_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (vbox), priv->copyright_label, FALSE, FALSE, 0);

    priv->web_button = gnome_href_new ("", _("Plugin Webpage"));
    gtk_box_pack_start (GTK_BOX (vbox), priv->web_button, FALSE, FALSE, 0);

    gtk_widget_show (vbox);

    // Add the close button
    gtk_dialog_add_button (GTK_DIALOG (about), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
    gtk_dialog_set_default_response (GTK_DIALOG (about), GTK_RESPONSE_CLOSE);

    // Add the credits button
    image = gtk_image_new_from_stock (GNOME_STOCK_ABOUT, GTK_ICON_SIZE_BUTTON);

    label = gtk_label_new_with_mnemonic (_("C_redits"));

    hbox = gtk_hbox_new (FALSE, 2);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
    gtk_container_add (GTK_CONTAINER (alignment), hbox);

    button = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (button), alignment);
    gtk_widget_show_all (button);

    gtk_dialog_add_action_widget (GTK_DIALOG (about), button, GNOME_RESPONSE_CREDITS);
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (about)->action_area), button, TRUE);

    gtk_window_set_resizable (GTK_WINDOW (about), FALSE);

    priv->credits_dialog = NULL;
}


static void gnome_cmd_about_plugin_response (GtkDialog *dialog, gint response)
{
    switch (response)
    {
        case GNOME_RESPONSE_CREDITS:
            gnome_cmd_about_plugin_display_credits_dialog (GNOME_CMD_ABOUT_PLUGIN (dialog));
            break;

        default:
            gtk_widget_destroy (GTK_WIDGET (dialog));
            break;
    }
}


static void gnome_cmd_about_plugin_class_init (GnomeCmdAboutPluginClass *klass)
{
    GObjectClass *object_class = (GObjectClass *) klass;
    GtkDialogClass *dialog_class = (GtkDialogClass *) klass;

    object_class->set_property = gnome_cmd_about_plugin_set_property;
    object_class->get_property = gnome_cmd_about_plugin_get_property;

    object_class->finalize = gnome_cmd_about_plugin_finalize;

    dialog_class->response = gnome_cmd_about_plugin_response;

    g_object_class_install_property (object_class,
                     PROP_NAME,
                     g_param_spec_string ("name",
                                  "Program name",
                                  "The name of the program",
                                  NULL,
                                  (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                     PROP_VERSION,
                     g_param_spec_string ("version",
                                  "Program version",
                                  "The version of the program",
                                  NULL,
                                  (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                     PROP_COPYRIGHT,
                     g_param_spec_string ("copyright",
                                  "Copyright string",
                                  "Copyright information for the program",
                                  NULL,
                                  (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                     PROP_COMMENTS,
                     g_param_spec_string ("comments",
                                  "Comments string",
                                  "Comments about the program",
                                  NULL,
                                  (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                     PROP_AUTHORS,
                     g_param_spec_value_array ("authors",
                                   "Authors",
                                   "List of authors of the programs",
                                   g_param_spec_string ("author-entry",
                                            "Author entry",
                                            "A single author entry",
                                            NULL,
                                            (GParamFlags) G_PARAM_READWRITE),
                                   G_PARAM_WRITABLE));
    g_object_class_install_property (object_class,
                     PROP_DOCUMENTERS,
                     g_param_spec_value_array ("documenters",
                                   "Documenters",
                                   "List of people documenting the program",
                                   g_param_spec_string ("documenter-entry",
                                            "Documenter entry",
                                            "A single documenter entry",
                                            NULL,
                                            (GParamFlags) G_PARAM_READWRITE),
                                   G_PARAM_WRITABLE));

    g_object_class_install_property (object_class,
                     PROP_TRANSLATOR_CREDITS,
                     g_param_spec_string ("translator_credits",
                                  "Translator credits",
                                  "Credits to the translators. This string should be marked as translatable",
                                  NULL,
                                  (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                     PROP_WEBPAGE,
                     g_param_spec_string ("webpage",
                                  "Webpage for the plugin",
                                  "Webpage",
                                  NULL,
                                  (GParamFlags) G_PARAM_READWRITE));

}


inline void gnome_cmd_about_plugin_set_comments (GnomeCmdAboutPlugin *about, const gchar *comments)
{
    g_free (about->priv->comments);
    about->priv->comments = g_strdup (comments);

    gtk_label_set_text (GTK_LABEL (about->priv->comments_label), about->priv->comments);
}


inline void gnome_cmd_about_plugin_set_translator_credits (GnomeCmdAboutPlugin *about, const gchar *translator_credits)
{
    g_free (about->priv->translator_credits);

    about->priv->translator_credits = g_strdup (translator_credits);
}


inline void gnome_cmd_about_plugin_set_webpage (GnomeCmdAboutPlugin *about, const gchar *webpage)
{
    if (!webpage) return;

    g_free (about->priv->webpage);

    about->priv->webpage = g_strdup (webpage);
    gnome_href_set_url (GNOME_HREF (about->priv->web_button), webpage);
    gtk_widget_show (about->priv->web_button);
}


static void gnome_cmd_about_plugin_set_copyright (GnomeCmdAboutPlugin *about, const gchar *copyright)
{
    char *copyright_string = NULL;

    g_free (about->priv->copyright);
    about->priv->copyright = g_strdup (copyright);

    if (about->priv->copyright)
    {
        char *tmp = g_markup_escape_text (about->priv->copyright, -1);
        copyright_string = g_strdup_printf ("<span size=\"small\">%s</span>", tmp);
        g_free (tmp);
    }

    gtk_label_set_markup (GTK_LABEL (about->priv->copyright_label), copyright_string);

    g_free (copyright_string);
}


static void gnome_cmd_about_plugin_set_version (GnomeCmdAboutPlugin *about, const gchar *version)
{
    g_free (about->priv->version);
    about->priv->version = g_strdup (version);

    gchar *name_string;
    gchar *tmp_name = g_markup_escape_text (about->priv->name, -1);

    if (about->priv->version)
    {
        gchar *tmp_version = g_markup_escape_text (about->priv->version, -1);
        name_string = g_strdup_printf ("<span size=\"xx-large\" weight=\"bold\">%s %s</span>", tmp_name, tmp_version);
        g_free (tmp_version);
    }
    else
        name_string = g_strdup_printf ("<span size=\"xx-large\" weight=\"bold\">%s</span>", tmp_name);

    gtk_label_set_markup (GTK_LABEL (about->priv->name_label), name_string);
    g_free (name_string);
    g_free (tmp_name);
}


static void gnome_cmd_about_plugin_set_name (GnomeCmdAboutPlugin *about, const gchar *name)
{
    gchar *title_string;
    gchar *name_string;
    gchar *tmp_name, *tmp_version;

    g_free (about->priv->name);
    about->priv->name = g_strdup (name ? name : "");

    title_string = g_strdup_printf (_("About %s"), name);
    gtk_window_set_title (GTK_WINDOW (about), title_string);
    g_free (title_string);

    tmp_name = g_markup_escape_text (about->priv->name, -1);

    if (about->priv->version)
    {
        tmp_version = g_markup_escape_text (about->priv->version, -1);
        name_string = g_strdup_printf ("<span size=\"xx-large\" weight=\"bold\">%s %s</span>", tmp_name, tmp_version);
        g_free (tmp_version);
    }
    else
        name_string = g_strdup_printf ("<span size=\"xx-large\" weight=\"bold\">%s</span>", tmp_name);

    gtk_label_set_markup (GTK_LABEL (about->priv->name_label), name_string);
    g_free (name_string);
    g_free (tmp_name);
}


inline void gnome_cmd_about_plugin_free_person_list (GSList *list)
{
    if (list == NULL)
        return;

    g_slist_foreach (list, (GFunc) g_free, NULL);
    g_slist_free (list);
}


static void gnome_cmd_about_plugin_finalize (GObject *object)
{
    GnomeCmdAboutPlugin *about = GNOME_CMD_ABOUT_PLUGIN (object);

    g_free (about->priv->name);
    g_free (about->priv->version);
    g_free (about->priv->copyright);
    g_free (about->priv->comments);

    gnome_cmd_about_plugin_free_person_list (about->priv->authors);
    gnome_cmd_about_plugin_free_person_list (about->priv->documenters);

    g_free (about->priv->translator_credits);
    g_free (about->priv->webpage);

    g_free (about->priv);
    about->priv = NULL;

    CALL_PARENT (G_OBJECT_CLASS, gnome_cmd_about_plugin, finalize, (object));
}


static void gnome_cmd_about_plugin_set_persons (GnomeCmdAboutPlugin *about, guint prop_id, const GValue *persons)
{
    GSList *list = NULL;

    // Free the old list
    switch (prop_id)
    {
        case PROP_AUTHORS:
            list = about->priv->authors;
            break;
        case PROP_DOCUMENTERS:
            list = about->priv->documenters;
            break;
        default:
            g_assert_not_reached ();
    }

    gnome_cmd_about_plugin_free_person_list (list);
    list = NULL;

    GValueArray *value_array = (GValueArray *) g_value_get_boxed (persons);

    if (!value_array)
        return;

    for (guint i = 0; i < value_array->n_values; i++)
        list = g_slist_prepend (list, g_value_dup_string (&value_array->values[i]));

    list = g_slist_reverse (list);

    switch (prop_id)
    {
        case PROP_AUTHORS:
            about->priv->authors = list;
            break;
        case PROP_DOCUMENTERS:
            about->priv->documenters = list;
            break;

        default:
            g_assert_not_reached ();
    }
}


static void set_value_array_from_list (GValue *value, GSList *list)
{
    gint length = g_slist_length (list);
    GValueArray *array = g_value_array_new (length);

    GValue tmp_value = { 0 };

    for (GSList *tmp = list; tmp; tmp = tmp->next)
    {
        char *str = (char *) tmp->data;

        g_value_init (&tmp_value, G_TYPE_STRING);
        g_value_set_string (&tmp_value, str);
        g_value_array_append (array, &tmp_value);
    }

    g_value_set_boxed (value, array);
    g_value_array_free (array);
}


static void gnome_cmd_about_plugin_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    switch (prop_id)
    {
        case PROP_NAME:
            gnome_cmd_about_plugin_set_name (GNOME_CMD_ABOUT_PLUGIN (object), g_value_get_string (value));
            break;
        case PROP_VERSION:
            gnome_cmd_about_plugin_set_version (GNOME_CMD_ABOUT_PLUGIN (object), g_value_get_string (value));
            break;
        case PROP_COMMENTS:
            gnome_cmd_about_plugin_set_comments (GNOME_CMD_ABOUT_PLUGIN (object), g_value_get_string (value));
            break;
        case PROP_COPYRIGHT:
            gnome_cmd_about_plugin_set_copyright (GNOME_CMD_ABOUT_PLUGIN (object), g_value_get_string (value));
            break;
        case PROP_AUTHORS:
        case PROP_DOCUMENTERS:
            gnome_cmd_about_plugin_set_persons (GNOME_CMD_ABOUT_PLUGIN (object), prop_id, value);
            break;
        case PROP_TRANSLATOR_CREDITS:
            gnome_cmd_about_plugin_set_translator_credits (GNOME_CMD_ABOUT_PLUGIN (object), g_value_get_string (value));
            break;
        case PROP_WEBPAGE:
            gnome_cmd_about_plugin_set_webpage (GNOME_CMD_ABOUT_PLUGIN (object), g_value_get_string (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


static void gnome_cmd_about_plugin_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GnomeCmdAboutPlugin *about = GNOME_CMD_ABOUT_PLUGIN (object);

    switch (prop_id)
    {
        case PROP_NAME:
            g_value_set_string (value, about->priv->name);
            break;
        case PROP_VERSION:
            g_value_set_string (value, about->priv->version);
            break;
        case PROP_COPYRIGHT:
            g_value_set_string (value, about->priv->copyright);
            break;
        case PROP_COMMENTS:
            g_value_set_string (value, about->priv->comments);
            break;
        case PROP_TRANSLATOR_CREDITS:
            g_value_set_string (value, about->priv->translator_credits);
            break;
        case PROP_WEBPAGE:
            g_value_set_string (value, about->priv->webpage);
            break;
        case PROP_AUTHORS:
            set_value_array_from_list (value, about->priv->authors);
            break;
        case PROP_DOCUMENTERS:
            set_value_array_from_list (value, about->priv->documenters);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


/**
 * gnome_cmd_about_plugin_construct:
 * @about: An existing #GnomeCmdAboutPlugin instance.
 * @name: The name of the application.
 * @version: The version string of the application.
 * @copyright: The application's copyright statement.
 * @comments: A short miscellaneous string.
 * @authors: An %NULL terminated array of the application authors.
 * @documenters: An array of the application documenters.
 * @translator_credits: The translator for the current locale.
 * @webpage: The plugins's webpage.
 *
 * Similar to gnome_cmd_about_plugin_new() except that the pre-existing @about widget is
 * used. Note that in this version of the function, @authors is not checked to
 * be non-%NULL, so callers must be careful, since bad things will happen if
 * this condition is not met.
 */
void gnome_cmd_about_plugin_construct (GnomeCmdAboutPlugin *about,
                                       const gchar  *name,
                                       const gchar  *version,
                                       const gchar  *copyright,
                                       const gchar  *comments,
                                       const gchar **authors,
                                       const gchar **documenters,
                                       const gchar  *translator_credits,
                                       const gchar  *webpage)
{
    GValueArray *authors_array = g_value_array_new (0);
    GValueArray *documenters_array;

    for (gint i = 0; authors[i] != NULL; i++)
    {
        GValue value = {0, };

        g_value_init (&value, G_TYPE_STRING);
        g_value_set_static_string (&value, authors[i]);
        authors_array = g_value_array_append (authors_array, &value);
    }

    if (documenters)
    {
        documenters_array = g_value_array_new (0);

        for (gint i = 0; documenters[i] != NULL; i++)
        {
            GValue value = {0, };

            g_value_init (&value, G_TYPE_STRING);
            g_value_set_static_string (&value, documenters[i]);
            documenters_array = g_value_array_append (documenters_array, &value);
        }

    }
    else
        documenters_array = NULL;

    g_object_set (G_OBJECT (about),
                  "name", name,
                  "version", version,
                  "copyright", copyright,
                  "comments", comments,
                  "authors", authors_array,
                  "documenters", documenters_array,
                  "translator_credits", translator_credits,
                  "webpage", webpage,
                  NULL);

    if (authors_array)
        g_value_array_free (authors_array);

    if (documenters_array)
        g_value_array_free (documenters_array);
}


/**
 * gnome_cmd_about_plugin_new:
 * @name: The name of the application.
 * @version: The version string of the application.
 * @copyright: The application's copyright statement.
 * @comments: A short miscellaneous string.
 * @authors: An %NULL terminated array of the application authors.
 * @documenters: An array of the application documenters.
 * @translator_credits: The translator for the current locale.
 *
 * Construct an application's credits box. The @authors array cannot be empty
 * and the @translator_credits should be marked as a translatable string (so
 * that only the translator for the currently active locale is displayed).
 *
 * Returns: A new "About" dialog.
 */
GtkWidget *gnome_cmd_about_plugin_new (PluginInfo *info)
{
    g_return_val_if_fail (info != NULL, NULL);

    GnomeCmdAboutPlugin *about = (GnomeCmdAboutPlugin *) g_object_new (GNOME_CMD_TYPE_ABOUT_PLUGIN, NULL);

    gnome_cmd_about_plugin_construct(about,
                                     info->name, info->version, info->copyright, info->comments,
                                     (const gchar**) info->authors,
                                     (const gchar**) info->documenters,
                                     info->translator, info->webpage);

    return GTK_WIDGET(about);
}
