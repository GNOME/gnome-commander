/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
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
#include <string.h>
#include <libgnome/libgnome.h>

#include "libgviewer.h"
#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"

using namespace std;


// HEX history doesn't work yet
#undef HEX_HISTORY

static GtkDialogClass *parent_class = NULL;

void entry_changed(GtkEntry *entry, gpointer  user_data);
static void search_dlg_destroy (GtkObject *object);
static void search_dlg_action_response(GtkDialog *dlg, gint arg1, GViewerSearchDlg *sdlg);

struct GViewerSearchDlgPrivate
{
    GtkWidget  *table;
    GtkWidget  *label;
    GtkWidget  *entry;
    GtkWidget  *text_mode, *hex_mode;
    GtkWidget  *case_sensitive_checkbox;

    SEARCHMODE searchmode;

    gchar      *search_text_string;
    guint8     *search_hex_buffer;
    guint      search_hex_buflen;
};


guint8 *gviewer_search_dlg_get_search_hex_buffer (GViewerSearchDlg *sdlg, /*out*/ guint *buflen)
{
    g_return_val_if_fail (sdlg!=NULL, NULL);
    g_return_val_if_fail (sdlg->priv!=NULL, NULL);
    g_return_val_if_fail (buflen!=NULL, NULL);
    g_return_val_if_fail (sdlg->priv->search_hex_buffer!=NULL, NULL);
    g_return_val_if_fail (sdlg->priv->search_hex_buflen>0, NULL);

    guint8 *result = g_new0 (guint8, sdlg->priv->search_hex_buflen);
    memcpy (result, sdlg->priv->search_hex_buffer, sdlg->priv->search_hex_buflen);

    *buflen = sdlg->priv->search_hex_buflen;

    return result;
}


gchar *gviewer_search_dlg_get_search_text_string (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (sdlg!=NULL, NULL);
    g_return_val_if_fail (sdlg->priv!=NULL, NULL);
    g_return_val_if_fail (sdlg->priv->search_text_string!=NULL, NULL);

    return g_strdup (sdlg->priv->search_text_string);
}


SEARCHMODE gviewer_search_dlg_get_search_mode (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (sdlg!=NULL, SEARCH_MODE_TEXT);
    g_return_val_if_fail (sdlg->priv!=NULL, SEARCH_MODE_TEXT);

    return sdlg->priv->searchmode;
}


gboolean gviewer_search_dlg_get_case_sensitive (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (sdlg!=NULL, TRUE);
    g_return_val_if_fail (sdlg->priv!=NULL, TRUE);

    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sdlg->priv->case_sensitive_checkbox));
}


inline void set_text_history (GViewerSearchDlg *sdlg)
{
    for (GList *i=gnome_cmd_data.intviewer_defaults.text_patterns.ents; i; i=i->next)
        if (i->data)
            gtk_combo_box_append_text (GTK_COMBO_BOX (sdlg->priv->entry), (gchar *) i->data);
}


inline void set_text_mode (GViewerSearchDlg *sdlg)
{
    gtk_widget_grab_focus (sdlg->priv->entry);
    sdlg->priv->searchmode = SEARCH_MODE_TEXT;
    gtk_widget_set_sensitive(sdlg->priv->case_sensitive_checkbox, TRUE);

    GtkEntry *w = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (sdlg->priv->entry)));
    entry_changed (w, (gpointer) sdlg);
}


static void set_hex_mode (GViewerSearchDlg *sdlg)
{
#if HEX_HISTORY
    for (GList *i=gnome_cmd_data.intviewer_defaults.hex_patterns.ents; i; i=i->next)
        if (i->data)
            gtk_combo_box_prepend_text (GTK_COMBO_BOX (sdlg->priv->entry), (gchar *) i->data);
#endif
    gtk_widget_grab_focus (sdlg->priv->entry);

    sdlg->priv->searchmode = SEARCH_MODE_HEX;

    gtk_widget_set_sensitive(sdlg->priv->case_sensitive_checkbox, FALSE);

    // Check if the text in the GtkEntryBox has hex value, otherwise disable the "Find" button
    GtkEntry *w = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (sdlg->priv->entry)));
    entry_changed (w, (gpointer) sdlg);
}


static void search_mode_text (GtkToggleButton *btn, GViewerSearchDlg *sdlg)
{
    g_return_if_fail (btn!=NULL);
    g_return_if_fail (sdlg!=NULL);

    if (!gtk_toggle_button_get_active(btn))
        return;

    set_text_mode (sdlg);
}


static void search_mode_hex (GtkToggleButton *btn, GViewerSearchDlg *sdlg)
{
    g_return_if_fail (btn!=NULL);
    g_return_if_fail (sdlg!=NULL);

    if (gtk_toggle_button_get_active(btn))
        set_hex_mode(sdlg);
}


static void search_dlg_action_response (GtkDialog *dlg, gint arg1, GViewerSearchDlg *sdlg)
{
    g_return_if_fail (sdlg!=NULL);
    g_return_if_fail (sdlg->priv!=NULL);

    if (arg1 != GTK_RESPONSE_OK)
        return;

    g_return_if_fail (sdlg->priv->search_text_string==NULL);
    g_return_if_fail (sdlg->priv->search_hex_buffer==NULL);

    gchar *pattern = gtk_combo_box_get_active_text (GTK_COMBO_BOX (sdlg->priv->entry));

    sdlg->priv->search_text_string = g_strdup (pattern);

    if (sdlg->priv->searchmode==SEARCH_MODE_TEXT)   // text mode search
    {
        gnome_cmd_data.intviewer_defaults.text_patterns.add(pattern);

        gnome_cmd_data.intviewer_defaults.case_sensitive =
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sdlg->priv->case_sensitive_checkbox));
    }
    else    // hex mode search
    {
        sdlg->priv->search_hex_buffer = text2hex(pattern, &sdlg->priv->search_hex_buflen);
        g_return_if_fail (sdlg->priv->search_hex_buffer!=NULL);

        gnome_cmd_data.intviewer_defaults.hex_patterns.add(pattern);
    }
}


static void class_init (GViewerSearchDlgClass *klass)
{
    GtkObjectClass *object_class = (GtkObjectClass *) klass;

    parent_class = (GtkDialogClass *) gtk_type_class (gtk_dialog_get_type ());
    object_class->destroy = search_dlg_destroy;
}


void entry_changed (GtkEntry *entry, gpointer  user_data)
{
    g_return_if_fail (IS_GVIEWER_SEARCH_DLG(user_data));

    gboolean enable = FALSE;

    GViewerSearchDlg *sdlg = GVIEWER_SEARCH_DLG(user_data);
    g_return_if_fail (sdlg->priv!=NULL);

    if (sdlg->priv->searchmode==SEARCH_MODE_HEX)
    {
        // Only if the use entered a valid hex string, enable the "find" button
        guint len;
        guint8 *buf = text2hex (gtk_entry_get_text (entry), &len);

        enable = (buf!=NULL) && (len>0);
        g_free (buf);
    }
    else
    {
        // SEARCH_MODE_TEXT
        enable = strlen (gtk_entry_get_text (entry))>0;
    }
    gtk_dialog_set_response_sensitive (GTK_DIALOG (user_data), GTK_RESPONSE_OK, enable);
}


static void search_dlg_init (GViewerSearchDlg *sdlg)
{
    GtkDialog *dlg = GTK_DIALOG (sdlg);

    GtkTable *table;
    GtkWidget *entry;

    sdlg->priv = g_new0(GViewerSearchDlgPrivate, 1);

    sdlg->priv->searchmode = (SEARCHMODE) gnome_cmd_data.intviewer_defaults.search_mode;

    gtk_window_set_title(GTK_WINDOW (dlg), _("Find"));
    gtk_window_set_modal(GTK_WINDOW (dlg), TRUE);
    gtk_dialog_add_button(dlg, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(dlg, GTK_STOCK_FIND, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(dlg, GTK_RESPONSE_OK);

    g_signal_connect_swapped(GTK_WIDGET (dlg), "response", G_CALLBACK (search_dlg_action_response), sdlg);

    // 2x4 Table
    table = GTK_TABLE (gtk_table_new (2, 2, FALSE));
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 6);
    gtk_box_pack_start (GTK_BOX (dlg->vbox), GTK_WIDGET (table), FALSE, TRUE, 0);
    sdlg->priv->table = GTK_WIDGET (table);

    // Label
    sdlg->priv->label = gtk_label_new(NULL);
    gtk_label_set_markup_with_mnemonic (GTK_LABEL (sdlg->priv->label), "_Search for:");
    gtk_table_attach(table, sdlg->priv->label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    // Entry Box
    sdlg->priv->entry = gtk_combo_box_entry_new_text();
    entry = gtk_bin_get_child (GTK_BIN (sdlg->priv->entry));
    g_object_set(entry, "activates-default", TRUE, NULL);
    g_signal_connect (entry, "changed", G_CALLBACK (entry_changed), sdlg);
    gtk_table_attach(table, sdlg->priv->entry, 1, 3, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    set_text_history (sdlg);

    // Search mode radio buttons
    sdlg->priv->text_mode = gtk_radio_button_new_with_mnemonic(NULL, _("_Text"));
    sdlg->priv->hex_mode = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(sdlg->priv->text_mode), _("_Hexadecimal"));

    g_signal_connect(sdlg->priv->text_mode, "toggled", G_CALLBACK (search_mode_text), sdlg);
    g_signal_connect(sdlg->priv->hex_mode, "toggled", G_CALLBACK (search_mode_hex), sdlg);

    gtk_table_attach(table, sdlg->priv->text_mode, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(table, sdlg->priv->hex_mode, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);

    // Case-Sensitive Checkbox
    sdlg->priv->case_sensitive_checkbox = gtk_check_button_new_with_mnemonic(_("_Match case"));
    gtk_table_attach(table, sdlg->priv->case_sensitive_checkbox, 2, 3, 1, 2, GtkAttachOptions(GTK_EXPAND|GTK_FILL), GTK_FILL, 0, 0);

    gtk_widget_show_all(sdlg->priv->table);
    gtk_widget_show (GTK_WIDGET (dlg));

    // Restore the previously saved state (loaded with "load_search_dlg_state")
    if (sdlg->priv->searchmode==SEARCH_MODE_HEX)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sdlg->priv->hex_mode), TRUE);
        set_hex_mode (sdlg);
    }
    else
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sdlg->priv->text_mode), TRUE);
        set_text_mode (sdlg);
    }

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sdlg->priv->case_sensitive_checkbox), gnome_cmd_data.intviewer_defaults.case_sensitive);

    if (!gnome_cmd_data.intviewer_defaults.text_patterns.empty())
        gtk_entry_set_text(GTK_ENTRY(entry), gnome_cmd_data.intviewer_defaults.text_patterns.front());

    gtk_widget_grab_focus (sdlg->priv->entry);
}


static void search_dlg_destroy (GtkObject *object)
{
    g_return_if_fail (object != NULL);
    g_return_if_fail (IS_GVIEWER_SEARCH_DLG(object));

    GViewerSearchDlg *w = GVIEWER_SEARCH_DLG(object);

    if (w->priv)
    {
        gnome_cmd_data.intviewer_defaults.search_mode = w->priv->searchmode;

        g_free (w->priv->search_text_string);
        w->priv->search_text_string = NULL;

        g_free (w->priv);
        w->priv = NULL;
    }

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


GType gviewer_search_dlg_get_type ()
{
  static GType ttt_type = 0;
  if (!ttt_type)
    {
      static const GTypeInfo ttt_info =
      {
        sizeof (GViewerSearchDlgClass),
        NULL, // base_init
        NULL, // base_finalize
        (GClassInitFunc) class_init,
        NULL, // class_finalize
        NULL, // class_data
        sizeof (GViewerSearchDlg),
        0, // n_preallocs
        (GInstanceInitFunc) search_dlg_init,
      };

      ttt_type = g_type_register_static (GTK_TYPE_DIALOG, "GViewerSearchDlg", &ttt_info, (GTypeFlags) 0);
    }

  return ttt_type;
}


GtkWidget* gviewer_search_dlg_new (GtkWindow *parent)
{
    GViewerSearchDlg *dlg = (GViewerSearchDlg *) gtk_type_new (gviewer_search_dlg_get_type());

    return GTK_WIDGET (dlg);
}


void gviewer_show_search_dlg (GtkWindow *parent)
{
    GtkWidget *w = gviewer_search_dlg_new (parent);
    GViewerSearchDlg *dlg = GVIEWER_SEARCH_DLG (w);

    gtk_dialog_run (GTK_DIALOG (dlg));

    g_warning ("Search mode = %d", (int) gviewer_search_dlg_get_search_mode (dlg));

    gchar *text = gviewer_search_dlg_get_search_text_string (dlg);
    g_warning ("Search text string = \"%s\"", text);
    g_free (text);

    gtk_widget_destroy (GTK_WIDGET (dlg));
}
