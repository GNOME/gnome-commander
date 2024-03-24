/**
 * @file search-progress-dlg.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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
 *
 */

#include <config.h>

#include "libgviewer.h"

using namespace std;


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


G_DEFINE_TYPE (GViewerSearchProgressDlg, gviewer_search_progress_dlg, GTK_TYPE_DIALOG)


static void search_progress_dlg_action_response(GtkDialog *dlg, gint arg1, GViewerSearchProgressDlg *sdlg)
{
    g_return_if_fail (sdlg != nullptr);
    g_return_if_fail (sdlg->priv != nullptr);
    g_return_if_fail (sdlg->priv->abort_indicator != nullptr);

    g_atomic_int_add(sdlg->priv->abort_indicator, 1);
}


static void gviewer_search_progress_dlg_class_init(GViewerSearchProgressDlgClass *klass)
{
    GtkObjectClass *object_class = (GtkObjectClass *) klass;

    object_class->destroy = search_progress_dlg_destroy;
}


static void gviewer_search_progress_dlg_init (GViewerSearchProgressDlg *sdlg)
{
    sdlg->priv = g_new0 (GViewerSearchProgressDlgPrivate, 1);

    GtkDialog *dlg = GTK_DIALOG(sdlg);
    // sdlg->priv->progress = 0;

    gtk_window_set_title (GTK_WINDOW (dlg), _("Searching…"));
    gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
    gtk_dialog_add_button (dlg, GTK_STOCK_STOP, 12);

    g_signal_connect_swapped (GTK_WIDGET (dlg), "response", G_CALLBACK (search_progress_dlg_action_response), sdlg);

    // Text Label
    sdlg->priv->label = gtk_label_new("");

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area (dlg)), sdlg->priv->label, FALSE, TRUE, 5);

    // Progress Bar
    sdlg->priv->progressbar = gtk_progress_bar_new();
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR (sdlg->priv->progressbar), "0.0");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (sdlg->priv->progressbar), 0.0);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area (dlg)), sdlg->priv->progressbar, TRUE, TRUE, 0);

    gtk_widget_show_all(gtk_dialog_get_content_area (dlg));

    gtk_widget_show (GTK_WIDGET (dlg));
}


static void search_progress_dlg_destroy (GtkObject *object)
{
    g_return_if_fail (IS_GVIEWER_SEARCH_PROGRESS_DLG (object));

    GViewerSearchProgressDlg *w = GVIEWER_SEARCH_PROGRESS_DLG (object);

    g_free (w->priv);
    w->priv = nullptr;

    GTK_OBJECT_CLASS (gviewer_search_progress_dlg_parent_class)->destroy (object);
}


static GtkWidget *gviewer_search_progress_dlg_new (GtkWindow *parent)
{
    auto dlg = static_cast<GViewerSearchProgressDlg*> (g_object_new (gviewer_search_progress_dlg_get_type(), NULL));

    return GTK_WIDGET (dlg);
}


static gboolean search_progress_dlg_timeout(gpointer data)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_PROGRESS_DLG (data), FALSE);

    gdouble progress;
    gchar text[20];

    GViewerSearchProgressDlg *w = GVIEWER_SEARCH_PROGRESS_DLG (data);

    progress = g_atomic_int_get (w->priv->progress_value);

    g_snprintf(text, sizeof(text), "%3.1f%%", progress/10.0);
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR (w->priv->progressbar), text);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (w->priv->progressbar), progress/1000.0);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

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
    g_return_if_fail (abort != nullptr);
    g_return_if_fail (complete != nullptr);
    g_return_if_fail (progress != nullptr);
    g_return_if_fail (searching_text != nullptr);

    gdouble dprogress;
    gchar text[20];

    GtkWidget *w = gviewer_search_progress_dlg_new(parent);
    GViewerSearchProgressDlg *dlg = GVIEWER_SEARCH_PROGRESS_DLG (w);

    gchar *str = g_strdup_printf (_("Searching for “%s”"), searching_text);
    gtk_label_set_text (GTK_LABEL (dlg->priv->label), str);

    dlg->priv->abort_indicator = abort;
    dlg->priv->progress_value = progress;
    dlg->priv->completed_indicator = complete;

    gint timeout_source_id = g_timeout_add (300, search_progress_dlg_timeout, (gpointer) dlg);

    dprogress = g_atomic_int_get (dlg->priv->progress_value);
    g_snprintf(text, sizeof(text), "%3.1f%%", dprogress/10.0);
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR (dlg->priv->progressbar), text);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (dlg->priv->progressbar), dprogress/1000.0);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    gtk_dialog_run(GTK_DIALOG(dlg));

    GSource *src = g_main_context_find_source_by_id(nullptr, timeout_source_id);
    if (src)
        g_source_destroy(src);

    g_free (str);

    gtk_widget_destroy (GTK_WIDGET (dlg));
}
