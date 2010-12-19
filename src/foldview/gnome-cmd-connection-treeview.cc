/*
    ###########################################################################

    gnome-cmd-connection-treeview.cc

    ---------------------------------------------------------------------------

    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyright (C) 2010-2010 Guillaume Wardavoir

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

    ---------------------------------------------------------------------------

    Struct  : GnomeCmdConnectionTreeview

    ###########################################################################
*/
#include    "gnome-cmd-connection-treeview.h"

//  ###########################################################################
//
//  			    GnomeCmdConnectionTreeview
//
//  ###########################################################################

GdkPixbuf   *   GnomeCmdConnectionTreeview::s_gdk_pixbuf[GnomeCmdConnectionTreeview::eIconCard];
gboolean        GnomeCmdConnectionTreeview::s_gdk_pixbuf_loaded = FALSE;

gchar *   GnomeCmdConnectionTreeview::Collate_key_dot		= NULL;
gchar *   GnomeCmdConnectionTreeview::Collate_key_dotdot	= NULL;
gchar *   GnomeCmdConnectionTreeview::Collate_key_uri_01	= NULL;

//  ***************************************************************************
//
//  Constructor, ...
//
//  ***************************************************************************
GnomeCmdConnectionTreeview::GnomeCmdConnectionTreeview(
    GcmdGtkFoldview *   _foldview,
    GnomeCmdCon     *   _con)
{
    // UPS rocks :)
    GnomeVFSURI     * u     = NULL;
    GnomeCmdPath    * p     = NULL;
    gchar           * s     = NULL;
    //.........................................................................
    g_return_if_fail( _foldview );
    g_return_if_fail( _con );

    Pixbuf_load_all();

    // static collate keys : static vars are not correctly initialized
    // in this form :
    // gchar *   GnomeCmdConnectionTreeview::Collate_key_dot = g_utf8_collate_key(_("."),   -1);
    if ( ! Collate_key_dot )
    {
        Collate_key_dot     = g_utf8_collate_key(_("."),   -1);
        Collate_key_dotdot  = g_utf8_collate_key(_(".."),  -1);
        Collate_key_uri_01  = g_utf8_collate_key(_("file:///"),  -1);
    }

    a_foldview      = _foldview;
    a_connection    = _con;

    // create the control
    d_control   = GCMD_STRUCT_NEW(Control, this);

    // create the model
    d_model     = GCMD_STRUCT_NEW(Model, d_control);

    // create the view
    d_view      = GCMD_STRUCT_NEW(View, d_control, model()->treemodel());

    // go !
    if ( gnome_cmd_con_is_local(a_connection) )
    {
        p = gnome_cmd_con_create_path(a_connection, "");
        u = gnome_cmd_con_create_uri(a_connection, p);
        s = gnome_vfs_uri_to_string(u, GNOME_VFS_URI_HIDE_NONE);

        model()->add_first_tree(s, gnome_cmd_con_get_alias(a_connection));

        GCMD_INF("GnomeCmdConnectionTreeview()::first tree uri [%s]", s);

        g_object_ref_sink(p);
        g_object_unref(p);
        gnome_vfs_uri_unref(u);
        g_free(s);
    }


}

GnomeCmdConnectionTreeview::~GnomeCmdConnectionTreeview()
{
    delete d_model;
    delete d_view;
    delete d_control;
}


//  ***************************************************************************
//
//  Icons
//
//  ***************************************************************************
gboolean
GnomeCmdConnectionTreeview::Pixbuf_load(
    GnomeCmdConnectionTreeview::eIcon       _icon,
    const gchar                         *   _filename)
{
	GError  *   g_error		= NULL;
    //.........................................................................
	s_gdk_pixbuf[_icon] = gdk_pixbuf_new_from_file(_filename, &g_error);

    if ( g_error )
    {
        GCMD_ERR("GnomeCmdConnectionTreeview::Pixbuf_load():%s", g_error->message );
        g_error_free(g_error);
        return FALSE;
    }
    return TRUE;
}
gboolean
GnomeCmdConnectionTreeview::Pixbuf_load_all()
{
    if ( s_gdk_pixbuf_loaded )
        return TRUE;

	Pixbuf_load(eIconUnknown,                 "../pixmaps/file-type-icons/file_type_unknown.xpm");

	Pixbuf_load(eIconDirReadWrite,            "../pixmaps/file-type-icons/file_type_dir.xpm");
	Pixbuf_load(eIconDirReadOnly,             "../pixmaps/file-type-icons/file_type_dir_orange.xpm");
	Pixbuf_load(eIconDirForbidden,            "../pixmaps/file-type-icons/file_type_dir_red.xpm");

	Pixbuf_load(eIconSymlinkToDirReadWrite,   "../pixmaps/file-type-icons/file_type_symlink_to_dir_green.xpm");
	Pixbuf_load(eIconSymlinkToDirReadOnly,    "../pixmaps/file-type-icons/file_type_symlink_to_dir_orange.xpm");
	Pixbuf_load(eIconSymlinkToDirForbidden,   "../pixmaps/file-type-icons/file_type_symlink_to_dir_red.xpm");

	Pixbuf_load(eIconDummy,                   "../pixmaps/file-type-icons/file_type_unknown.xpm");
	Pixbuf_load(eIconWarning,                 "../pixmaps/gtk-dialog-warning.png");

	Pixbuf_load(eIconGtkRefresh,              "../pixmaps/gtk-refresh.png");
	Pixbuf_load(eIconGtkSortAscending,        "../pixmaps/gtk-sort-ascending.png");
	Pixbuf_load(eIconGtkSortDescending,       "../pixmaps/gtk-sort-descending.png");
	Pixbuf_load(eIconFolderOpened,            "../pixmaps/folder-opened.png");
	Pixbuf_load(eIconFolderClosed,            "../pixmaps/folder-closed.png");
	Pixbuf_load(eIconGtkClose,                "../pixmaps/gtk-close.png");

    s_gdk_pixbuf_loaded = TRUE;

    return TRUE;
}
void
GnomeCmdConnectionTreeview::Pixbuf_unload_all()
{
	g_object_unref(s_gdk_pixbuf[eIconUnknown]		);

	g_object_unref(s_gdk_pixbuf[eIconDirReadWrite]  );
	g_object_unref(s_gdk_pixbuf[eIconDirReadOnly]   );
	g_object_unref(s_gdk_pixbuf[eIconDirForbidden]  );

	g_object_unref(s_gdk_pixbuf[eIconSymlinkToDirReadWrite] );
	g_object_unref(s_gdk_pixbuf[eIconSymlinkToDirReadOnly]  );
	g_object_unref(s_gdk_pixbuf[eIconSymlinkToDirForbidden] );

	g_object_unref(s_gdk_pixbuf[eIconDummy]			);
	g_object_unref(s_gdk_pixbuf[eIconWarning]		);

	g_object_unref(s_gdk_pixbuf[eIconGtkRefresh] );
	g_object_unref(s_gdk_pixbuf[eIconGtkSortAscending] );
	g_object_unref(s_gdk_pixbuf[eIconGtkSortDescending] );
	g_object_unref(s_gdk_pixbuf[eIconFolderOpened] );
	g_object_unref(s_gdk_pixbuf[eIconFolderClosed] );
	g_object_unref(s_gdk_pixbuf[eIconGtkClose] );
}

//  ***************************************************************************
//
//  Connection infos
//
//  ***************************************************************************
gboolean
GnomeCmdConnectionTreeview::is_samba()
{
    return ( a_connection->method == CON_SMB );
}
gboolean
GnomeCmdConnectionTreeview::is_local()
{
    return ( a_connection->method == CON_LOCAL );
}

gboolean
GnomeCmdConnectionTreeview::host_redmond()
{
    return FALSE;                                                               // _GWR_TODO_
}

//  ***************************************************************************
//
//  Actions
//
//  ***************************************************************************
void
GnomeCmdConnectionTreeview::set_packing_expansion(gboolean b)
{
    foldview()->treeview_set_packing_expansion(this, b);
}

void
GnomeCmdConnectionTreeview::close()
{
    foldview()->treeview_del(this);
}


//  ***************************************************************************
//
//  Divers
//
//  ***************************************************************************
GnomeCmdConnectionTreeview::eFileAccess
GnomeCmdConnectionTreeview::Access_from_permissions(
	eFilePerm   _p1,
	eFilePerm   _p2)
{
	return  (eFileAccess)( (guint32)_p1 + (guint32)_p2 );
}

GnomeCmdConnectionTreeview::eFileAccess
GnomeCmdConnectionTreeview::Access_from_read_write(
	gboolean	_read,
	gboolean	_write)
{
	eFilePerm   p1 = ( _read	?   ePermRead   : ePermNone );
	eFilePerm   p2 = ( _write   ?   ePermWrite  : ePermNone );

	return  Access_from_permissions(p1,p2);
}

gboolean
GnomeCmdConnectionTreeview::Access_readable(
	eFileAccess _access)
{
	return ( ((guint32)_access & (guint32)ePermRead) != 0 );
}
