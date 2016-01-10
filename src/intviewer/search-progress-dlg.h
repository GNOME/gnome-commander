/**
 * @file search-progress-dlg.h
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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

#ifndef __LIBGVIEWER_SEARCH_PROGRESS_DLG_H__
#define __LIBGVIEWER_SEARCH_PROGRESS_DLG_H__

#define GVIEWER_SEARCH_PROGRESS_DLG(obj)          GTK_CHECK_CAST (obj, gviewer_search_progress_dlg_get_type(), GViewerSearchProgressDlg)
#define GVIEWER_SEARCH_PROGRESS_DLG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gviewer_search_progress_dlg_get_type(), GViewerSearchProgressDlgClass)
#define IS_GVIEWER_SEARCH_PROGRESS_DLG(obj)       GTK_CHECK_TYPE (obj, gviewer_search_progress_dlg_get_type())

struct GViewerSearchProgressDlgPrivate;

struct GViewerSearchProgressDlg
{
    GtkDialog dialog;
    GViewerSearchProgressDlgPrivate *priv;
};

struct GViewerSearchProgressDlgClass
{
    GtkDialogClass parent_class;
};

GType gviewer_search_progress_dlg_get_type ();

void gviewer_show_search_progress_dlg(GtkWindow *parent,
                                      const gchar *searching_text,
                                      gint *abort, gint *complete, gint *progress);

#endif /* __LIBGVIEWER_SEARCH_PROGRESS_DLG_H__ */
