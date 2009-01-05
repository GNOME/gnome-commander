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

#ifndef __LIBGVIEWER_SEARCH_DLG_H__
#define __LIBGVIEWER_SEARCH_DLG_H__

#define GVIEWER_SEARCH_DLG(obj)          GTK_CHECK_CAST (obj, gviewer_search_dlg_get_type(), GViewerSearchDlg)
#define GVIEWER_SEARCH_DLG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gviewer_search_dlg_get_type(), GViewerSearchDlgClass)
#define IS_GVIEWER_SEARCH_DLG(obj)       GTK_CHECK_TYPE (obj, gviewer_search_dlg_get_type())

struct GViewerSearchDlgPrivate;

enum SEARCHMODE
{
    SEARCH_MODE_TEXT,
    SEARCH_MODE_HEX
};

struct GViewerSearchDlg
{
    GtkDialog dialog;
    GViewerSearchDlgPrivate *priv;
};

struct GViewerSearchDlgClass
{
    GtkDialogClass parent_class;
};

GType gviewer_search_dlg_get_type ();

GtkWidget *gviewer_search_dlg_new (GtkWindow *parent);

SEARCHMODE gviewer_search_dlg_get_search_mode (GViewerSearchDlg *sdlg);

/* returns string is strdup-ed, caller must "g_free" it */
gchar *gviewer_search_dlg_get_search_text_string (GViewerSearchDlg *sdlg);

/* returned buffer is "g_new0-ed", caller must "g_free" it */
guint8 *gviewer_search_dlg_get_search_hex_buffer (GViewerSearchDlg *sdlg, /*out*/ guint *buflen);

gboolean gviewer_search_dlg_get_case_sensitive (GViewerSearchDlg *sdlg);

void gviewer_show_search_dlg (GtkWindow *parent);

#endif /* __LIBGVIEWER_SEARCH_DLG_H__ */
