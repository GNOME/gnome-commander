/**
 * @file gnome-cmd-about-plugin.cc
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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-about-plugin.h"

using namespace std;


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

    GWeakRef credits_dialog;
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

G_DEFINE_TYPE (GnomeCmdAboutPlugin, gnome_cmd_about_plugin, GTK_TYPE_DIALOG)

static void gnome_cmd_about_plugin_update_authors_label (GnomeCmdAboutPlugin *about, GtkWidget *label)
{
    if (about->priv->authors == nullptr)
    {
        gtk_widget_hide (label);
        return;
    }
    else
        gtk_widget_show (label);

    GString *string = g_string_new (nullptr);

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

    if (about->priv->documenters == nullptr)
    {
        gtk_widget_hide (label);
        return;
    }

    gtk_widget_show (label);

    GString *string = g_string_new (nullptr);

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
    if (about->priv->translator_credits == nullptr)
    {
        gtk_widget_hide (label);
        return;
    }

    gtk_widget_show (label);

    GString *string = g_string_new (nullptr);

    gchar *tmp = g_markup_escape_text (about->priv->translator_credits, -1);
    g_string_append (string, tmp);
    g_free (tmp);

    g_string_append_c (string, '\n');

    gtk_label_set_markup (GTK_LABEL (label), string->str);
    g_string_free (string, TRUE);
}


inline GtkWidget *_create_label ()
{
    GtkWidget *label = gtk_label_new ("");

    gtk_label_set_selectable (GTK_LABEL (label), TRUE);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_START);
    gtk_widget_set_margin_start (label, 8);
    gtk_widget_set_margin_end (label, 8);
    gtk_widget_set_margin_top (label, 8);
    gtk_widget_set_margin_bottom (label, 8);

    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    return label;
}


static void gnome_cmd_about_plugin_display_credits_dialog (GnomeCmdAboutPlugin *about)
{
    GtkWidget *dialog, *label, *notebook, *sw;

    dialog = GTK_WIDGET (g_weak_ref_get (&about->priv->credits_dialog));
    if (dialog != nullptr)
    {
        gtk_window_present (GTK_WINDOW (dialog));
        return;
    }

    dialog = gtk_dialog_new_with_buttons (_("Credits"),
                          GTK_WINDOW (about),
                          GTK_DIALOG_DESTROY_WITH_PARENT,
                          _("_Close"), GTK_RESPONSE_CLOSE,
                          nullptr);
    g_weak_ref_set (&about->priv->credits_dialog, dialog);
    gtk_window_set_default_size (GTK_WINDOW (dialog), 360, 260);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_widget_set_margin_top (content_area, 10);
    gtk_widget_set_margin_bottom (content_area, 10);
    gtk_widget_set_margin_start (content_area, 10);
    gtk_widget_set_margin_end (content_area, 10);
    gtk_box_set_spacing (GTK_BOX (content_area), 6);

    g_signal_connect (dialog, "response", G_CALLBACK (gtk_window_destroy), dialog);

    notebook = gtk_notebook_new ();
    gtk_widget_set_vexpand (notebook, TRUE);
    gtk_box_append (GTK_BOX (content_area), notebook);

    if (about->priv->authors != nullptr)
    {
        label = _create_label ();

        sw = gtk_scrolled_window_new ();
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), label);

        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, gtk_label_new (_("Written by")));
        gnome_cmd_about_plugin_update_authors_label (about, label);
    }

    if (about->priv->documenters != nullptr)
    {
        label = _create_label ();

        sw = gtk_scrolled_window_new ();
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), label);

        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, gtk_label_new (_("Documented by")));
        gnome_cmd_about_plugin_update_documenters_label (about, label);
    }

    if (about->priv->translator_credits != nullptr)
    {
        label = _create_label ();

        sw = gtk_scrolled_window_new ();
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), label);

        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, gtk_label_new (_("Translated by")));
        gnome_cmd_about_plugin_update_translation_information_label (about, label);
    }
}


static void link_button_clicked_callback (GtkWidget *widget, gpointer data)
{
    const gchar *link;

    link = gtk_link_button_get_uri (GTK_LINK_BUTTON (widget));
    gtk_show_uri (nullptr, link, GDK_CURRENT_TIME);
}


static void gnome_cmd_about_plugin_init (GnomeCmdAboutPlugin *about)
{
    GtkWidget *button;

    // Data
    GnomeCmdAboutPluginPrivate *priv = g_new0 (GnomeCmdAboutPluginPrivate, 1);
    about->priv = priv;

    // priv->name = nullptr;
    // priv->version = nullptr;
    // priv->copyright = nullptr;
    // priv->comments = nullptr;
    // priv->translator_credits = nullptr;
    // priv->authors = nullptr;
    // priv->documenters = nullptr;
    // priv->webpage = nullptr;

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (about));

    gtk_widget_set_margin_top (content_area, 10);
    // gtk_widget_set_margin_bottom (content_area, 10);
    gtk_widget_set_margin_start (content_area, 10);
    gtk_widget_set_margin_end (content_area, 10);
    gtk_box_set_spacing (GTK_BOX (content_area), 8);

    // Widgets
    priv->name_label = gtk_label_new (nullptr);
    gtk_widget_show (priv->name_label);
    gtk_label_set_selectable (GTK_LABEL (priv->name_label), TRUE);
    gtk_label_set_justify (GTK_LABEL (priv->name_label), GTK_JUSTIFY_CENTER);
    gtk_box_append (GTK_BOX (content_area), priv->name_label);

    priv->comments_label = gtk_label_new (nullptr);
    gtk_widget_show (priv->comments_label);
    gtk_label_set_selectable (GTK_LABEL (priv->comments_label), TRUE);
    gtk_label_set_justify (GTK_LABEL (priv->comments_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap (GTK_LABEL (priv->comments_label), TRUE);
    gtk_box_append (GTK_BOX (content_area), priv->comments_label);

    priv->copyright_label = gtk_label_new (nullptr);
    gtk_widget_show (priv->copyright_label);
    gtk_label_set_selectable (GTK_LABEL (priv->copyright_label), TRUE);
    gtk_label_set_justify (GTK_LABEL (priv->copyright_label), GTK_JUSTIFY_CENTER);
    gtk_box_append (GTK_BOX (content_area), priv->copyright_label);

    priv->web_button = gtk_link_button_new_with_label ("", _("Plugin Webpage"));
    g_signal_connect (priv->web_button, "clicked", G_CALLBACK (link_button_clicked_callback), NULL);

    gtk_box_append (GTK_BOX (content_area), priv->web_button);

    GtkWidget *bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    GtkSizeGroup *size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

    // Add the credits button
    button = gtk_button_new_with_mnemonic (_("C_redits"));
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (gnome_cmd_about_plugin_display_credits_dialog), about);
    gtk_size_group_add_widget (size_group, button);
    gtk_widget_set_hexpand (button, TRUE);
    gtk_widget_set_halign (button, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX (bbox), button);

    // Add the close button
    button = gtk_button_new_with_mnemonic (_("_Close"));
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_window_destroy), about);
    gtk_size_group_add_widget (size_group, button);
    gtk_box_append (GTK_BOX (bbox), button);

    gtk_box_append (GTK_BOX (content_area), bbox);

    gtk_window_set_resizable (GTK_WINDOW (about), FALSE);

    priv->credits_dialog = { { nullptr } };
}


static void gnome_cmd_about_plugin_class_init (GnomeCmdAboutPluginClass *klass)
{
    GObjectClass *object_class = (GObjectClass *) klass;

    object_class->set_property = gnome_cmd_about_plugin_set_property;
    object_class->get_property = gnome_cmd_about_plugin_get_property;

    object_class->finalize = gnome_cmd_about_plugin_finalize;

    g_object_class_install_property (object_class,
                     PROP_NAME,
                     g_param_spec_string ("name",
                                  "Program name",
                                  "The name of the program",
                                  nullptr,
                                  (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                     PROP_VERSION,
                     g_param_spec_string ("version",
                                  "Program version",
                                  "The version of the program",
                                  nullptr,
                                  (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                     PROP_COPYRIGHT,
                     g_param_spec_string ("copyright",
                                  "Copyright string",
                                  "Copyright information for the program",
                                  nullptr,
                                  (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                     PROP_COMMENTS,
                     g_param_spec_string ("comments",
                                  "Comments string",
                                  "Comments about the program",
                                  nullptr,
                                  (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                     PROP_AUTHORS,
                     g_param_spec_value_array ("authors",
                                   "Authors",
                                   "List of authors of the programs",
                                   g_param_spec_string ("author-entry",
                                            "Author entry",
                                            "A single author entry",
                                            nullptr,
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
                                            nullptr,
                                            (GParamFlags) G_PARAM_READWRITE),
                                   G_PARAM_WRITABLE));

    g_object_class_install_property (object_class,
                     PROP_TRANSLATOR_CREDITS,
                     g_param_spec_string ("translator_credits",
                                  "Translator credits",
                                  "Credits to the translators. This string should be marked as translatable",
                                  nullptr,
                                  (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                     PROP_WEBPAGE,
                     g_param_spec_string ("webpage",
                                  "Webpage for the plugin",
                                  "Webpage",
                                  nullptr,
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
    gtk_link_button_set_uri(GTK_LINK_BUTTON (about->priv->web_button), webpage);
    gtk_widget_show (about->priv->web_button);
}

static void gnome_cmd_about_plugin_set_copyright (GnomeCmdAboutPlugin *about, const gchar *copyright)
{
    char *copyright_string = nullptr;

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
    gchar *tmp_name;

    g_free (about->priv->name);
    about->priv->name = g_strdup (name ? name : "");

    title_string = g_strdup_printf (_("About %s"), name);
    gtk_window_set_title (GTK_WINDOW (about), title_string);
    g_free (title_string);

    tmp_name = g_markup_escape_text (about->priv->name, -1);

    if (about->priv->version)
    {
        gchar *tmp_version;
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


static void gnome_cmd_about_plugin_finalize (GObject *object)
{
    GnomeCmdAboutPlugin *about = GNOME_CMD_ABOUT_PLUGIN (object);

    g_free (about->priv->name);
    g_free (about->priv->version);
    g_free (about->priv->copyright);
    g_free (about->priv->comments);

    g_clear_slist (&about->priv->authors, g_free);
    g_clear_slist (&about->priv->documenters, g_free);

    g_free (about->priv->translator_credits);
    g_free (about->priv->webpage);

    g_free (about->priv);
    about->priv = nullptr;

    G_OBJECT_CLASS (gnome_cmd_about_plugin_parent_class)->finalize (object);
}


static void gnome_cmd_about_plugin_set_persons (GnomeCmdAboutPlugin *about, guint prop_id, const GValue *persons)
{
    // Free the old list
    switch (prop_id)
    {
        case PROP_AUTHORS:
            g_clear_slist (&about->priv->authors, g_free);
            break;
        case PROP_DOCUMENTERS:
            g_clear_slist (&about->priv->documenters, g_free);
            break;
        default:
            g_assert_not_reached ();
    }

    GValueArray *value_array = (GValueArray *) g_value_get_boxed (persons);

    if (!value_array)
        return;

    GSList *list = nullptr;
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
    GArray *array = g_array_sized_new (FALSE, TRUE, sizeof(char*), length);

    for (GSList *i = list; i; i = i->next)
    {
        g_array_append_val (array, i->data);
    }

    g_value_set_boxed (value, array);
    g_array_free (array, TRUE);
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
 * Similar to gnome_cmd_about_plugin_new() except that the pre-existing
 * GnomeCmdAboutPlugin widget is used. Note that in this version of the
 * function, @c authors is not checked to be non-NULL, so callers must
 * be careful, since bad things will happen if this condition is not
 * met.
 *
 * @param about An existing GnomeCmdAboutPlugin instance.
 * @param name The name of the application.
 * @param version The version string of the application.
 * @param copyright The application's copyright statement.
 * @param comments A short miscellaneous string.
 * @param authors An %NULL terminated array of the application authors.
 * @param documenters An array of the application documenters.
 * @param translator_credits The translator for the current locale.
 * @param webpage The plugins's webpage.
 */
static void gnome_cmd_about_plugin_construct (GnomeCmdAboutPlugin *about,
                                              const gchar  *name,
                                              const gchar  *version,
                                              const gchar  *copyright,
                                              const gchar  *comments,
                                              const gchar **authors,
                                              const gchar **documenters,
                                              const gchar  *translator_credits,
                                              const gchar  *webpage)
{
    GArray *authors_array = g_array_new (FALSE, FALSE, sizeof(char*));
    GArray *documenters_array = nullptr;

    for (gint i = 0; authors[i] != nullptr; i++)
    {
        authors_array = g_array_append_val (authors_array, authors[i]);
    }

    if (documenters)
    {
        documenters_array = g_array_new (FALSE, FALSE, sizeof(char*));

        for (gint i = 0; documenters[i] != nullptr; i++)
        {
            documenters_array = g_array_append_val (documenters_array, documenters[i]);
        }

    }

    g_object_set (G_OBJECT (about),
                  "name", name,
                  "version", version,
                  "copyright", copyright,
                  "comments", comments,
//                  "authors", authors_array,
//                  "documenters", documenters_array,
                  "translator_credits", translator_credits,
                  "webpage", webpage,
                  nullptr);

    if (authors_array)
        g_array_free (authors_array, TRUE);

    if (documenters_array)
        g_array_free (documenters_array, TRUE);
}


/**
 * Construct an application's credits box. The authors array cannot be empty
 * and the translator_credits should be marked as a translatable string (so
 * that only the translator for the currently active locale is displayed).
 *
 * @returns A new "About" dialog.
 */
GtkWidget *gnome_cmd_about_plugin_new (PluginInfo *info)
{
    g_return_val_if_fail (info != nullptr, nullptr);

    auto about = static_cast<GnomeCmdAboutPlugin*> (g_object_new (GNOME_CMD_TYPE_ABOUT_PLUGIN, nullptr));

    gnome_cmd_about_plugin_construct(about,
                                     info->name, info->version, info->copyright, info->comments,
                                     (const gchar**) info->authors,
                                     (const gchar**) info->documenters,
                                     info->translator, info->webpage);

    return GTK_WIDGET(about);
}
