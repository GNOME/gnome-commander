/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2004 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef __GNOME_CMD_INTERNAL_VIEWER_H__
#define __GNOME_CMD_INTERNAL_VIEWER_H__

#define GNOME_CMD_INTERNAL_VIEWER(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_internal_viewer_get_type (), GnomeCmdInternalViewer)
#define GNOME_CMD_INTERNAL_VIEWER_CLASS(clss) \
    GTK_CHECK_CLASS_CAST (clss, gnome_cmd_internal_viewer_get_type (), GnomeCmdInternalViewerClass)
#define GNOME_CMD_IS_INTERNAL_VIEWER(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_internal_viewer_get_type ())

typedef struct _GnomeCmdInternalViewer GnomeCmdInternalViewer;
typedef struct _GnomeCmdInternalViewerPrivate GnomeCmdInternalViewerPrivate;
typedef struct _GnomeCmdInternalViewerClass GnomeCmdInternalViewerClass;
typedef struct _GnomeCmdStateVariabes GnomeCmdStateVariables;

struct _GnomeCmdStateVariabes
{
    GdkRectangle rect;
    gchar fixed_font_name[256];
    gchar variable_font_name[256];
    guint font_size;
    guint ascii_wrap;
    guint binary_bytes_per_line ;
    gchar charset[256];
    gboolean hex_decimal_offset ;
    guint tab_size ;
} ;

struct _GnomeCmdInternalViewer
{
    GtkWindow parent;

    GnomeCmdInternalViewerPrivate *priv;
};


struct _GnomeCmdInternalViewerClass
{
    GtkWindowClass parent_class;
};


GtkType
gnome_cmd_internal_viewer_get_type            (void);

GtkWidget*
gnome_cmd_internal_viewer_new                 (void);

void
gnome_cmd_internal_viewer_load_file    (GnomeCmdInternalViewer *iv, const gchar *filename);

void do_internal_file_view(const gchar * path);

#endif
