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
#include <libgnome/libgnome.h>

#include "libgviewer.h"

using namespace std;


static GtkDialogClass *parent_class = NULL;

static void search_progress_dlg_destroy (GtkObject *object);
static void search_progress_dlg_action_response(GtkDialog *dlg, gint arg1, GViewerSearchProgressDlg *sdlg);

struct GViewerSearchProgressDlgPrivate
{
    GtkWidget *label, *progressbar;
    gdouble progress;

    gint *abort_indicator;
    gint *progress_value;
    gint *completed_indicator;
};


static void search_progress_dlg_action_response(GtkDialog *dlg, gint arg1, GViewerSearchProgressDlg *sdlg)
{
    g_return_if_fail (sdlg!=NULL);
    g_return_if_fail (sdlg->priv!=NULL);

    g_return_if_fail (sdlg->priv->abort_indicator!=NULL);

    g_atomic_int_add(sdlg->priv->abort_indicator, 1);
}


static void search_progress_dlg_class_init(GViewerSearchProgressDlgClass *klass)
{
    GtkObjectClass *object_class = (GtkObjectClass *) klass;

    parent_class = (GtkDialogClass *) gtk_type_class (gtk_dialog_get_type ());

    object_class->destroy = search_progress_dlg_destroy;
}


static void search_progress_dlg_init (GViewerSearchProgressDlg *sdlg)
{
    sdlg->priv = g_new0(GViewerSearchProgressDlgPrivate, 1);

    GtkDialog *dlg = GTK_DIALOG(sdlg);
    sdlg->priv->progress = 0;

    gtk_window_set_title(GTK_WINDOW (dlg), _("Searching..."));
    gtk_window_set_modal(GTK_WINDOW (dlg), TRUE);
    gtk_dialog_add_button(dlg, GTK_STOCK_STOP, 12);

    g_signal_connect_swapped(GTK_WIDGET (dlg), "response", G_CALLBACK (search_progress_dlg_action_response), sdlg);

    // Text Label
    sdlg->priv->label = gtk_label_new("");

    gtk_box_pack_start(GTK_BOX(dlg->vbox), sdlg->priv->label, FALSE, TRUE, 5);

    // Progress Bar
    sdlg->priv->progressbar = gtk_progress_bar_new();
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sdlg->priv->progressbar), "0.0");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sdlg->priv->progressbar), 0.0);

    gtk_box_pack_start(GTK_BOX(dlg->vbox), sdlg->priv->progressbar, TRUE, TRUE, 0);

    gtk_widget_show_all(dlg->vbox);

    gtk_widget_show (GTK_WIDGET (dlg));
}


static void search_progress_dlg_destroy (GtkObject *object)
{
    g_return_if_fail (object != NULL);
    g_return_if_fail (IS_GVIEWER_SEARCH_PROGRESS_DLG(object));

    GViewerSearchProgressDlg *w = GVIEWER_SEARCH_PROGRESS_DLG(object);

    g_free (w->priv);
    w->priv = NULL;

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


GType gviewer_search_progress_dlg_get_type ()
{
    static GType ttt_type = 0;
    if (!ttt_type)
    {
        static const GTypeInfo ttt_info =
        {
            sizeof (GViewerSearchProgressDlgClass),
            NULL, // base_init
            NULL, // base_finalize
            (GClassInitFunc) search_progress_dlg_class_init,
            NULL, // class_finalize
            NULL, // class_data
            sizeof (GViewerSearchProgressDlg),
            0, // n_preallocs
            (GInstanceInitFunc) search_progress_dlg_init,
        };

        ttt_type = g_type_register_static (GTK_TYPE_DIALOG, "GViewerSearchProgressDlg", &ttt_info, (GTypeFlags) 0);
    }

  return ttt_type;
}


GtkWidget *gviewer_search_progress_dlg_new (GtkWindow *parent)
{
    GViewerSearchProgressDlg *dlg = (GViewerSearchProgressDlg *) gtk_type_new (gviewer_search_progress_dlg_get_type());

    return GTK_WIDGET (dlg);
}


gboolean search_progress_dlg_timeout(gpointer data)
{
    g_return_val_if_fail (data != NULL, FALSE);
    g_return_val_if_fail (IS_GVIEWER_SEARCH_PROGRESS_DLG(data), FALSE);

    gdouble progress;
    gchar text[20];

    GViewerSearchProgressDlg *w = GVIEWER_SEARCH_PROGRESS_DLG(data);

    progress = g_atomic_int_get (w->priv->progress_value);

    g_snprintf(text, sizeof(text), "%3.1f%%", progress/10.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w->priv->progressbar), text);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->priv->progressbar), progress/1000.0);

    if (g_atomic_int_get (w->priv->completed_indicator)!=0)
    {
        gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_CANCEL);
        return FALSE;
    }

    return TRUE;
}


void gviewer_show_search_progress_dlg(GtkWindow *parent, const gchar *searching_text,
                                      gint *abort, gint *complete, gint *progress)
{
    g_return_if_fail (abort!=NULL);
    g_return_if_fail (complete!=NULL);
    g_return_if_fail (progress!=NULL);
    g_return_if_fail (searching_text!=NULL);

    gdouble dprogress;
    gchar text[20];

    GtkWidget *w = gviewer_search_progress_dlg_new(parent);
    GViewerSearchProgressDlg *dlg = GVIEWER_SEARCH_PROGRESS_DLG(w);

    gchar *str = g_strdup_printf(_("Searching for \"%s\""), searching_text);
    gtk_label_set_text(GTK_LABEL(dlg->priv->label), str);

    dlg->priv->abort_indicator = abort;
    dlg->priv->progress_value = progress;
    dlg->priv->completed_indicator = complete;

    gint timeout_source_id = g_timeout_add (300, search_progress_dlg_timeout, (gpointer) dlg);

    dprogress = g_atomic_int_get (dlg->priv->progress_value);
    g_snprintf(text, sizeof(text), "%3.1f%%", dprogress/10.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(dlg->priv->progressbar), text);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dlg->priv->progressbar), dprogress/1000.0);

    gtk_dialog_run(GTK_DIALOG(dlg));

    GSource *src = g_main_context_find_source_by_id(NULL, timeout_source_id);
    if (src)
        g_source_destroy(src);

    g_free (str);

    gtk_widget_destroy (GTK_WIDGET (dlg));
}
