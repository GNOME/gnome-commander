/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyleft      2010-2012 Guillaume Wardavoir

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

    Struct  : many

    Parent  : GnomeCmdConnectionTreeview::Model

    ###########################################################################
*/
#include    "gnome-cmd-connection-treeview.h"

static gboolean         _USE_GIO_               = GCMDGTKFOLDVIEW_USE_GIO;

//  ###########################################################################
//
//  			         IterInfo
//
//  ###########################################################################
GnomeCmdConnectionTreeview::Model::IterInfo::IterInfo()
{
    a_model     = NULL;

    a_iter      = GnomeCmdConnectionTreeview::Model::Iter_zero;
    d_path      = NULL;
    d_gtk_path  = NULL;
    a_row       = NULL;

    a_expanded              = FALSE;
    a_readable              = FALSE;
    a_children              = -1;
    a_first_child_is_dummy  = FALSE;
}
GnomeCmdConnectionTreeview::Model::IterInfo::~IterInfo()
{
    if ( d_path )
        delete d_path;

    if ( d_gtk_path )
        gtk_tree_path_free(d_gtk_path);
}

void
GnomeCmdConnectionTreeview::Model::IterInfo::reset()
{
    a_model     = NULL;

    a_iter      = GnomeCmdConnectionTreeview::Model::Iter_zero;

    if ( d_path )
    {
        delete d_path;
        d_path = NULL;
    }

    if ( d_gtk_path )
    {
        gtk_tree_path_free(d_gtk_path);
        d_gtk_path = NULL;
    }
    a_row       = NULL;

    a_expanded              = FALSE;
    a_readable              = FALSE;
    a_children              = -1;
    a_first_child_is_dummy  = FALSE;
}

gboolean
GnomeCmdConnectionTreeview::Model::IterInfo::gather(
    Model               *   _model,
    TreestorePath       *   _path,
    IterInfo::eFields       _fields)
{
    GtkTreeIter         iter    = Iter_zero;
    //.........................................................................
    if ( ! _model->treestore()->ext_iter_from_path(_path, &iter) )
        return FALSE;

    // store path
    d_path = _path->dup();
    if ( ! d_path )
        return FALSE;

    return gather(_model, &iter, _fields);
}

gboolean
GnomeCmdConnectionTreeview::Model::IterInfo::gather(
    Model               *   _model,
    GtkTreeIter         *   _iter,
    IterInfo::eFields       _fields)
{
    GtkTreeIter             iter_child  = Iter_zero;
    Row             *       row_child   = NULL;
    //.........................................................................
    a_model         = _model;

    //.........................................................................
    //
    //  Variables
    //

    // iter
    GCMD_ITER_COPY(&a_iter, _iter);

    // path
    if ( ! d_path )
    {
        d_path = a_model->treestore()->ext_path_from_iter(&a_iter);
        if ( ! d_path )
            return FALSE;
    }

    // row
    if ( ! a_model->iter_get_treerow(&a_iter, &a_row) )
        return FALSE;

    // GtkTreePath
    if ( _fields & eGtkPath )
    {
        d_gtk_path = GnomeCmdFoldviewTreestore::get_path(a_model->treemodel(), &a_iter);
        if ( ! d_gtk_path )
            return FALSE;
    }

    //.........................................................................
    //
    //  Tests
    //

    // expanded
    if ( _fields & eExp )
    {
        a_expanded  = a_model->iter_is_expanded(&a_iter);
    }

    // readability
    if ( _fields & eRead )
    {
        a_readable = a_row->readable();
    }

    if ( _fields & eChildren )
    {
        a_children = _model->iter_n_children(&a_iter);
    }

    if ( _fields & eFCID )
    {
        if ( ! ( _fields & eChildren) )
            return FALSE;

        if ( ! a_children )
            return TRUE;

        if ( ! GnomeCmdFoldviewTreestore::iter_children(a_model->treemodel(), &iter_child, &a_iter) )
            return FALSE;

        if ( ! a_model->iter_get_treerow(&iter_child, &row_child) )
            return FALSE;

        a_first_child_is_dummy = row_child->is_dummy();
    }

    return TRUE;
}

//  ###########################################################################
//
//  			         File, Directory, Symlink
//
//  ###########################################################################
GnomeCmdConnectionTreeview::Model::File::File(
	const gchar					*   _utf8_display_name,
	eFileAccess						_access,
	eFileType						_type)
{
	d_utf8_name			= g_strdup(_utf8_display_name);

	a_access	= _access;
	a_type		= _type;
}
GnomeCmdConnectionTreeview::Model::File::~File()
{
	g_free(d_utf8_name);

    if ( d_disk_name )
        g_free(d_disk_name);

    if ( d_ck_utf8_name )
        g_free(d_ck_utf8_name);
}
//  ===========================================================================
GnomeCmdConnectionTreeview::Model::Directory::Directory(
	const gchar					*   _utf8_display_name,
	eFileAccess						_access)
		: File(_utf8_display_name, _access, GnomeCmdConnectionTreeview::eTypeDirectory)
{
}
GnomeCmdConnectionTreeview::Model::Directory::~Directory()
{
}
void*
GnomeCmdConnectionTreeview::Model::Directory::operator new(size_t size)
{
	GnomeCmdConnectionTreeview::Model::Directory	*d	= g_try_new0(GnomeCmdConnectionTreeview::Model::Directory, 1);

	if ( !d )
		GCMD_ERR("GnomeCmdConnectionTreeview::Model::Directory::new():g_try_new0 failed");

	return d;
}
void
GnomeCmdConnectionTreeview::Model::Directory::operator delete(void *p)
{
	g_free(p);
}

GnomeCmdConnectionTreeview::Model::File*
GnomeCmdConnectionTreeview::Model::Directory::dup()
{
    File    *   file = NULL;
    //.........................................................................
    file = new Directory(name_utf8(), access());

    return file;
}

//  ===========================================================================
GnomeCmdConnectionTreeview::Model::Symlink::Symlink(
	const gchar					*   _utf8_display_name,
	const gchar					*   _utf8_target_uri,
	eFileAccess						_access)
		: File(_utf8_display_name, _access, GnomeCmdConnectionTreeview::eTypeSymlink)
{
	d_utf8_target_uri = g_strdup(_utf8_target_uri);
}
GnomeCmdConnectionTreeview::Model::Symlink::~Symlink()
{
	g_free(d_utf8_target_uri);
}
void*
GnomeCmdConnectionTreeview::Model::Symlink::operator new(size_t size)
{
	GnomeCmdConnectionTreeview::Model::Symlink	*s	= g_try_new0(GnomeCmdConnectionTreeview::Model::Symlink, 1);

	if ( !s )
		GCMD_ERR("GnomeCmdConnectionTreeview::Model::Symlink::new():g_try_new0 failed");

	return s;
}
void
GnomeCmdConnectionTreeview::Model::Symlink::operator delete(void *p)
{
	g_free(p);
}
GnomeCmdConnectionTreeview::Model::File*
GnomeCmdConnectionTreeview::Model::Symlink::dup()
{
    File    *   file = NULL;
    //.........................................................................
    file = new Symlink(name_utf8(), target_uri(), access());

    return file;
}

//  ###########################################################################
//
//      	                Refresh, RefreshList
//
//  ###########################################################################
/*
//  ===========================================================================
//  Model::Refresh
//  ===========================================================================
void*
GnomeCmdConnectionTreeview::Model::Refresh::operator new(size_t size)
{
	Refresh	*r	= g_try_new0(GnomeCmdConnectionTreeview::Model::Refresh, 1);

	if ( ! r )
		GCMD_ERR("GnomeCmdConnectionTreeview::Model::Refresh::new():g_try_new0 failed");

	return r;
}
void
GnomeCmdConnectionTreeview::Model::Refresh::operator delete(void *p)
{
	g_free(p);
}

GnomeCmdConnectionTreeview::Model::Refresh::Refresh(
    TreestorePath   *   _path_mallocated,
    const gchar     *   _uri)
{
    d_path      = _path_mallocated;
    d_uri       = g_strdup(_uri);
}

GnomeCmdConnectionTreeview::Model::Refresh::~Refresh()
{
    delete d_path;
    g_free(d_uri);
}
//  ===========================================================================
//  Model::RefreshList
//  ===========================================================================
void*
GnomeCmdConnectionTreeview::Model::RefreshList::operator new(size_t size)
{
	RefreshList	*r	= g_try_new0(GnomeCmdConnectionTreeview::Model::RefreshList, 1);

	if ( ! r )
		GCMD_ERR("GnomeCmdConnectionTreeview::Model::RefreshList::new():g_try_new0 failed");

	return r;
}
void
GnomeCmdConnectionTreeview::Model::RefreshList::operator delete(void *p)
{
	g_free(p);
}

GnomeCmdConnectionTreeview::Model::RefreshList::RefreshList(
    Model   *   _model,
    GList   *   _list)
{
    a_model     = _model;
    d_list      = _list;
    a_current   = NULL;
}

GnomeCmdConnectionTreeview::Model::RefreshList::~RefreshList()
{
    g_list_free(d_list);
}

GnomeCmdConnectionTreeview::Model*
GnomeCmdConnectionTreeview::Model::RefreshList::model()
{
    return a_model;
}

void
GnomeCmdConnectionTreeview::Model::RefreshList::reset()
{
    d_list      =   g_list_first(d_list);
    a_current   =   d_list;
}

void
GnomeCmdConnectionTreeview::Model::RefreshList::add(Refresh*)
{
}

GnomeCmdConnectionTreeview::Model::Refresh*
GnomeCmdConnectionTreeview::Model::RefreshList::getplusplus()
{
    Refresh *   r = NULL;
    //.........................................................................
    if ( ! a_current )
        return NULL;

    r           = (Refresh*)(a_current->data);
    a_current   = a_current->next;

    return r;
}
*/
//  ###########################################################################
//
//  			                TreeRowStd
//
//  ###########################################################################

//  ***************************************************************************
//  ctor / dtor
//  ***************************************************************************
void
GnomeCmdConnectionTreeview::Model::TreeRowStd::init_flags_and_collate_keys(
	eFileAccess _access,
	gboolean	_is_link,
	gboolean	_is_dummy,
    eRowStatus  _status,
	eRowType	_type,
	//................................
	gboolean	_is_samba,
	gboolean	_is_local,
	gboolean	_host_redmond)
{
	eIcon   icon	= eIconUnknown;
	glong   l		= 0;
	gchar*  temp	= NULL;
	//.........................................................................
	//  Info flags

	// icon
	if ( ! _is_dummy )
		if ( ! _is_link )
			icon = View::Icon_from_type_access(eTypeDirectory, _access);
		else
			icon = View::Icon_from_type_access(eTypeSymlink, _access);
	else
			icon = eIconUnknown;

	// flags
	a_info_1													=
		( (guint32)icon							<< e_ICON_SHIFT		)   +
		( (guint32)(_is_link ?		1 : 0 )		<< e_LINK_SHIFT		)   +
		( (guint32)(_access)					<< e_ACCESS_SHIFT	)   +
		( (guint32)(_status)					<< e_STATUS_SHIFT	)   +
		( (guint32)(_type)						<< e_TYPE_SHIFT		)   +
		//...................................................
		( (guint32)(_is_samba ?		1 : 0 )	 << e_SAMBA_SHIFT	)   +
		( (guint32)(_is_local ?		1 : 0 )  << e_LOCAL_SHIFT	)   +
		( (guint32)(_host_redmond ? 1 : 0 )  << e_REDMOND_SHIFT	);

	//.........................................................................

	// -> raw collate key ( case sensitive )
	l													= g_utf8_strlen(d_utf8_name_display, -1);
	d_utf8_collate_keys[eCollateKeyRaw]					= g_utf8_collate_key_for_filename(d_utf8_name_display, l);

	// -> case-insensitive collate key
	temp												= g_utf8_casefold   (d_utf8_name_display, l);
	l													= g_utf8_strlen		(temp, -1);
	d_utf8_collate_keys[eCollateKeyCaseInsensitive]		= g_utf8_collate_key_for_filename(temp, l);
	g_free(temp);
}

GnomeCmdConnectionTreeview::Model::TreeRowStd::TreeRowStd()
{
	// For TreeRowRoot  & TreeRowDummy
}
GnomeCmdConnectionTreeview::Model::TreeRowStd::TreeRowStd(
	Row			*   _row,
	Row			*   _row_parent,
	File		*	_file)
{
	if ( _file->is_symlink() )
		d_utf8_symlink_target_uri   = g_strdup(((Symlink*)_file)->target_uri());
	else
		d_utf8_symlink_target_uri   = g_strdup(_("Not a symlink - ( BUG ! THIS SHOULD NEVER BE DISPLAYED )"));

	d_utf8_name_display				= g_strdup(_file->name_utf8());

	init_flags_and_collate_keys(
		_file->access(), _file->is_symlink(), FALSE,
		eStatusOK, eRowStd,
		_row_parent->is_samba(), _row_parent->is_local(), _row_parent->host_redmond());
}

GnomeCmdConnectionTreeview::Model::TreeRowStd::~TreeRowStd()
{
	gint i = 0;
	//.........................................................................

	//GCMD_INF("Model::~TreeRowStd()");

	// delete all
	g_free(d_utf8_name_display);
	g_free(d_utf8_symlink_target_uri);

	for ( i = 0 ; i != eCollateKeyCard ; i++ )
		g_free(d_utf8_collate_keys[i]);
}

//  ***************************************************************************
//  	Some accessors
//  ***************************************************************************
GnomeCmdConnectionTreeview::eIcon
GnomeCmdConnectionTreeview::Model::TreeRowStd::icon()
{
	return (eIcon)( (a_info_1 & e_ICON_BITS ) >> e_ICON_SHIFT );
}
void
GnomeCmdConnectionTreeview::Model::TreeRowStd::icon(eIcon _icon)
{
	a_info_1 = a_info_1 & e_ICON_MASK;
    a_info_1 = a_info_1 | ( (guint32)_icon << e_ICON_SHIFT );
}
gboolean
GnomeCmdConnectionTreeview::Model::TreeRowStd::is_link()
{
	return (gboolean)((a_info_1 & e_LINK_BITS ) >> e_LINK_SHIFT);
}
GnomeCmdConnectionTreeview::eFileAccess
GnomeCmdConnectionTreeview::Model::TreeRowStd::access()
{
	return (GnomeCmdConnectionTreeview::eFileAccess)( (a_info_1 & e_ACCESS_BITS ) >> e_ACCESS_SHIFT );
}
GnomeCmdConnectionTreeview::Model::eRowStatus
GnomeCmdConnectionTreeview::Model::TreeRowStd::row_status()
{
	return (GnomeCmdConnectionTreeview::Model::eRowStatus)( (a_info_1 & e_STATUS_BITS ) >> e_STATUS_SHIFT );
}
GnomeCmdConnectionTreeview::Model::eRowType
GnomeCmdConnectionTreeview::Model::TreeRowStd::rowtype()
{
	return (GnomeCmdConnectionTreeview::Model::eRowType)( (a_info_1 & e_TYPE_BITS ) >> e_TYPE_SHIFT );
}
gboolean
GnomeCmdConnectionTreeview::Model::TreeRowStd::is_std()
{
	return ( (eRowType)((a_info_1 & e_TYPE_BITS ) >> e_TYPE_SHIFT) == eRowStd);
}
gboolean
GnomeCmdConnectionTreeview::Model::TreeRowStd::is_root()
{
	return ( (eRowType)((a_info_1 & e_TYPE_BITS ) >> e_TYPE_SHIFT) == eRowRoot);
}
gboolean
GnomeCmdConnectionTreeview::Model::TreeRowStd::is_dummy()
{
	return ( (eRowType)((a_info_1 & e_TYPE_BITS ) >> e_TYPE_SHIFT) == eRowDummy);
}
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::TreeRowStd::is_samba()
{
	return ( ( (a_info_1 & e_SAMBA_BITS ) >> e_SAMBA_SHIFT ) != 0 );
}
gboolean
GnomeCmdConnectionTreeview::Model::TreeRowStd::is_local()
{
	return ( ( (a_info_1 & e_LOCAL_BITS ) >> e_LOCAL_SHIFT ) != 0 );
}
gboolean
GnomeCmdConnectionTreeview::Model::TreeRowStd::host_redmond()
{
	return ( ( (a_info_1 & e_REDMOND_BITS ) >> e_REDMOND_SHIFT ) != 0 );
}
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::TreeRowStd::readable()
{
	return  (
				(
					(guint32)( (a_info_1 & e_ACCESS_BITS ) >> e_ACCESS_SHIFT )  &
					(guint32)( ePermRead )
				)
				!= 0
			);
}
void
GnomeCmdConnectionTreeview::Model::TreeRowStd::readable(gboolean _readable)
{
	eIcon icon		= eIconUnknown;
	eFileAccess a			= eAccessFB;

	// access
	a = (eFileAccess)
		(
			_readable											?
			((guint32)access())	 |  ( ((guint32)ePermRead))		:
			((guint32)access())	 &  (~((guint32)ePermRead))
		);

	// icon
	if ( ! is_dummy() )
	{
		if ( ! is_link() )
			icon = View::Icon_from_type_access(eTypeDirectory, a);
		else
			icon = View::Icon_from_type_access(eTypeSymlink, a);
	}

	// set new values
	a_info_1 = (a_info_1 & e_ACCESS_MASK) & e_ICON_SHIFT;
	a_info_1 = a_info_1										+
				( (guint32)a		<< e_ACCESS_SHIFT	)   +
				( (guint32)icon		<< e_ICON_SHIFT		);
}
//  ===========================================================================
const   gchar*
GnomeCmdConnectionTreeview::Model::TreeRowStd::utf8_name_display()
{
	return d_utf8_name_display;
}
const GnomeCmdConnectionTreeview::Uri
GnomeCmdConnectionTreeview::Model::TreeRowStd::utf8_symlink_target_uri()
{
	return d_utf8_symlink_target_uri;
}
const   gchar*
GnomeCmdConnectionTreeview::Model::TreeRowStd::utf8_collate_key(
	gint collate_key_to_use)
{
	return d_utf8_collate_keys[collate_key_to_use];
}
//  ###########################################################################
//
//  			                TreeRowRoot
//
//  ###########################################################################
GnomeCmdConnectionTreeview::Model::TreeRowRoot::TreeRowRoot(
	Uri					_utf8_uri,
	const gchar	 *		_utf8_name_display,
	Uri					_utf8_symlink_target_uri,
	eFileAccess			_access,
	gboolean			_is_symlink,
    gboolean            _is_samba,
    gboolean            _is_local,
    gboolean            _host_redmond)
{
	//.........................................................................
	d_utf8_name_display				= g_strdup(_utf8_name_display);

	if( _is_symlink )
		d_utf8_symlink_target_uri   = g_strdup( _utf8_symlink_target_uri );
	else
		d_utf8_symlink_target_uri   = g_strdup(_("NOT SYMLINK - This should not be displayed"));

	init_flags_and_collate_keys(
		_access, _is_symlink, FALSE,
		eStatusOK, eRowRoot,
		_is_samba,
		_is_local,
		FALSE);																	// _GWR_TODO_ : _host_redmond
}

GnomeCmdConnectionTreeview::Model::TreeRowRoot::~TreeRowRoot()
{
	//GCMD_INF("Model::~TreeRowRoot()");
}
//  ###########################################################################
//
//  			                TreeRowDummy
//
//  ###########################################################################
GnomeCmdConnectionTreeview::Model::TreeRowDummy::TreeRowDummy(
	Row		*   _row,
	Row		*   _row_parent)
{
	d_utf8_name_display			= g_strdup(_("...Working..."));
	d_utf8_symlink_target_uri   = g_strdup(_("Not a symlink (dummy) - ( BUG ! THIS SHOULD NEVER BE DISPLAYED )"));

	init_flags_and_collate_keys(
		eAccessRW, FALSE, TRUE,
		eStatusOK, eRowDummy,
		_row_parent->is_samba(), _row_parent->is_local(), _row_parent->host_redmond());
}

GnomeCmdConnectionTreeview::Model::TreeRowDummy::~TreeRowDummy()
{
	//GCMD_INF("Model::~TreeRowDummy()");
}
//  ###########################################################################
//
//  			                Row
//
//  ###########################################################################

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Row() [ aggregate of TreeRowStd ]
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GnomeCmdConnectionTreeview::Model::Row::Row(
	eRowType		_rowtype,
	Row			*   _row_parent,
	File		*   _file)
{
	g_return_if_fail( _rowtype == eRowStd );

	d_path		= NULL;
	d_utf8_uri  = NULL;
	d_monitor   = NULL;
	d_treerow   = NULL;

	// uri
	if ( _row_parent->is_samba() )
		d_utf8_uri = g_strconcat(_row_parent->uri_utf8(), "\\", _file->name_utf8(), NULL);
	else
		if ( _row_parent->is_local() && _row_parent->host_redmond() )
			d_utf8_uri = g_strconcat(_row_parent->uri_utf8(), "\\", _file->name_utf8(), NULL);
		else
		{
			gchar   *temp = g_utf8_collate_key(_row_parent->uri_utf8(), -1);
			if ( g_utf8_collate(temp, Collate_key_uri_01) )
				d_utf8_uri = g_strconcat(_row_parent->uri_utf8(), "/", _file->name_utf8(), NULL);
			else
				d_utf8_uri = g_strconcat("file:///", _file->name_utf8(), NULL);
			g_free(temp);
		}
	// treerow
	d_treerow = (TreeRowInterface*)( new TreeRowStd(this, _row_parent, _file) );

	// monitor : will be created when the Row will be added to the treestore
}
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Row() [ aggregate of TreeRowRoot ]
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GnomeCmdConnectionTreeview::Model::Row::Row(
	eRowType				_rowtype,
	Uri						_utf8_uri,
	const gchar			*   _utf8_display_name,
	Uri						_utf8_symlink_target_uri,
	eFileAccess				_access,
	gboolean				_is_symlink,
    gboolean                _is_samba,
    gboolean                _is_local,
    gboolean                _host_redmond)
{
	g_return_if_fail( _rowtype == eRowRoot );

	d_path		= NULL;
	d_utf8_uri  = NULL;
	d_monitor   = NULL;
	d_treerow   = NULL;

	d_utf8_uri = g_strdup( _utf8_uri );

	d_treerow = (TreeRowInterface*)( new TreeRowRoot(_utf8_uri, _utf8_display_name, _utf8_symlink_target_uri, _access, _is_symlink, _is_samba, _is_local, _host_redmond) );

	// monitor : will be created when the Row will be added to the treestore
}
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Row() [ aggregate of TreeRowDummy ]
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GnomeCmdConnectionTreeview::Model::Row::Row(
	eRowType			_rowtype,
	Row			*		_row_parent)
{
	g_return_if_fail( _rowtype == eRowDummy );

	d_path		= NULL;
	d_utf8_uri  = NULL;
	d_monitor   = NULL;
	d_treerow   = NULL;

	d_utf8_uri	= g_strdup(_("file:///...Working..."));

	d_treerow = (TreeRowInterface*)( new TreeRowDummy(this, _row_parent) );

	// monitor : will be created when calling monitoring_start(), because we
	//  dont know the Path yet. The path will be known when the treestore will
	//  call set_path_from_treestore().
}

GnomeCmdConnectionTreeview::Model::Row::~Row()
{
	if ( is_std() )
	{
		//GCMD_INF("Model::~Row()::std");
	}
	else
	{
		if ( is_dummy() )
		{
			//GCMD_INF("Model::~Row()::dummy");
		}
		else
		{
			if ( is_root() )
			{
				//GCMD_INF("Model::~Row()::root");
			}
			else
			{
				GCMD_ERR("Model::~Row()::???");
			}
		}
	}

	g_free(d_utf8_uri);
	delete d_path;

	delete d_treerow;

	if ( d_monitor )
	{
		if ( d_monitor->monitoring_started() )
			d_monitor->monitoring_stop();

		delete d_monitor;
		d_monitor = NULL;
	}
}


//  ***************************************************************************
//  Accessors
//  ***************************************************************************


//  ***************************************************************************
//  GnomeCmdFoldviewTreestore::DataInterface impl
//  ***************************************************************************

// inlined : GnomeCmdConnectionTreeview::Model::Row::path()

void
GnomeCmdConnectionTreeview::Model::Row::set_path_from_treestore(
	GnomeCmdFoldviewTreestore::Path * _path)
{
	g_return_if_fail( ! d_path );

	d_path = _path;
}

gint
GnomeCmdConnectionTreeview::Model::Row::compare(
	GnomeCmdFoldviewTreestore::DataInterface * _data)
{
    Row *   row = NULL;
    //.........................................................................
    row = (Row*)_data;

    return  strcmp
            (
                this->utf8_collate_key(eCollateKeyRaw),
                 row->utf8_collate_key(eCollateKeyRaw)
            );
}

gint
GnomeCmdConnectionTreeview::Model::Row::compare_str(
	const gchar *   _str)
{
    return  strcmp
            (
                this->utf8_collate_key(eCollateKeyRaw),
                _str
            );
}

//  ***************************************************************************
//  GnomeCmdFoldviewTreestore::MonitorInterface impl
//  ***************************************************************************
gboolean
GnomeCmdConnectionTreeview::Model::Row::monitoring_started()
{
	if ( ! d_monitor )
		return FALSE;

	return d_monitor->monitoring_started();
}

gboolean
GnomeCmdConnectionTreeview::Model::Row::monitoring_start(
	Model   *   _model,
	Row		*   _row)
{
	g_return_val_if_fail( this == _row, FALSE );

	// new monitor
	if ( ! d_monitor )
	{
		if ( _USE_GIO_ )
			d_monitor	= new GioMonitor();
		else
			d_monitor	= new GnomeVFSMonitor();
	}

	return d_monitor->monitoring_start(_model, this);
}

gboolean
GnomeCmdConnectionTreeview::Model::Row::monitoring_stop()
{
	if ( ! d_monitor )
		return TRUE;

	if ( d_monitor->monitoring_stop() )
	{
		delete d_monitor;
		d_monitor = NULL;
		return TRUE;
	}

	return FALSE;
}

//  ###########################################################################
//
//  			             Filesystem access
//
//  ###########################################################################


//  ***************************************************************************
//
//						Directory listing
//
//  ***************************************************************************

//=============================================================================
//  Model::AsyncCallerData
//=============================================================================
void*
GnomeCmdConnectionTreeview::Model::AsyncCallerData::operator new ( size_t _size)
{
	AsyncCallerData  *acd = g_try_new0(AsyncCallerData, 1);

	return (void*)acd;
}

void
GnomeCmdConnectionTreeview::Model::AsyncCallerData::operator delete(void* _p)
{
	g_free(_p);
}


GnomeCmdConnectionTreeview::Model::AsyncCallerData::AsyncCallerData(
	Model					*   _model,
	AsyncCallerCallback			_callback,
	TreestorePath           *   _path)
{
	a_model			= _model;
	a_callback		= _callback;
	d_path			= _path;
}


GnomeCmdConnectionTreeview::Model::AsyncCallerData::~AsyncCallerData()
{
    delete d_path;
};

//=============================================================================
//  Model::AsyncCore
//=============================================================================
GnomeCmdConnectionTreeview::Model::AsyncCore::AsyncCore(
	AsyncCallerData     *   _acd)
{
	a_caller_data	    = _acd;

    a_error             = eErrorNone;
    d_error_str         = NULL;
}

GnomeCmdConnectionTreeview::Model::AsyncCore::~AsyncCore()
{
    //delete d_caller_data;                                                     // _GWR_TODO_

    if ( d_error_str )
        g_free( d_error_str );
}

void
GnomeCmdConnectionTreeview::Model::AsyncCore::error_set(
    eFileError          _error,
	const gchar     *   _str)
{
    a_error     = _error;
    d_error_str = g_strdup(_str);
}

//=============================================================================
//  Model::AsyncGet
//=============================================================================
GnomeCmdConnectionTreeview::Model::AsyncGet::AsyncGet(
	AsyncCallerData     *   _acd) : AsyncCore(_acd)
{
}

GnomeCmdConnectionTreeview::Model::AsyncGet::~AsyncGet()
{
}

//=============================================================================
//  Model::AsyncEnumerateChildren
//=============================================================================
GnomeCmdConnectionTreeview::Model::AsyncEnumerateChildren::AsyncEnumerateChildren(
	AsyncCallerData	  *		_caller_data,
    const Uri               _uri,
	gint					_max_result,
	gboolean				_follow_links) : AsyncGet(_caller_data)
{
    d_uri               = g_strdup(_uri);
	a_max_result		= _max_result;
	a_follow_links		= _follow_links;

	d_list				= NULL;
	a_list_card			= NULL;
}

GnomeCmdConnectionTreeview::Model::AsyncEnumerateChildren::~AsyncEnumerateChildren()
{
	GList   *   list	= NULL;
	File	*   file	= NULL;
	//.........................................................................
	list = g_list_first(d_list);
	while ( list )
	{
		file	= (File*)list->data;
		delete file;
		list	= g_list_next(list);
	}
	g_list_free(d_list);

    g_free(d_uri);
}

void
GnomeCmdConnectionTreeview::Model::AsyncEnumerateChildren::list_remove(
	GList   *   _list)
{
	g_return_if_fail( _list );

	delete (File*)(_list->data);
    d_list = g_list_delete_link(d_list, _list);

	a_list_card--;
}

void
GnomeCmdConnectionTreeview::Model::AsyncEnumerateChildren::list_append(
	File	*   _file)
{
	g_return_if_fail( _file );

	d_list = g_list_append(d_list, _file);
	a_list_card++;
}


//=============================================================================
//  Model::AsyncGetFileInfo
//=============================================================================
GnomeCmdConnectionTreeview::Model::AsyncGetFileInfo::AsyncGetFileInfo(
	AsyncCallerData	  *		_caller_data,
    const Uri               _uri) : AsyncGet(_caller_data)
{
    d_uri                   = g_strdup(_uri);
    a_name                  = NULL;
    b_file_not_found        = FALSE;
    a_access                = eAccessUN;
    a_is_symlink            = FALSE;
    a_symlink_name          = NULL;
}

GnomeCmdConnectionTreeview::Model::AsyncGetFileInfo::~AsyncGetFileInfo()
{
    g_free(d_uri);
}

//  ***************************************************************************
//
//								Monitoring
//
//  ***************************************************************************

//  ===========================================================================
//  Model::Monitor
//  ===========================================================================
GnomeCmdConnectionTreeview::Model::Monitor::Monitor()
{
}
GnomeCmdConnectionTreeview::Model::Monitor::~Monitor()
{
	MONITOR_INF("Model::Monitor::~Monitor()");

	// ensure we are no more monitoring anything
	if ( monitoring_started() )
	{
		MONITOR_ERR("GnomeCmdConnectionTreeview::Model::Monitor::~Monitor():still monitoring");
	}
}

gboolean
GnomeCmdConnectionTreeview::Model::Monitor::monitoring_started()
{
	return a_started;
}



//  ###########################################################################
//
//  			                Sorting
//
//  ###########################################################################

//  ***************************************************************************
//
//                              IMsgSort
//
//  ***************************************************************************




//  ***************************************************************************
//
//                              IMsgSort
//
//  ***************************************************************************
GnomeCmdConnectionTreeview::Model::MsgSort_Insertion::MsgSort_Insertion(
    TreestorePath   * _path,
    Model           * _model) : IMsgSort(_path, _model)
{
    a_i0        = 2;
    d_list_work = NULL;
}

GnomeCmdConnectionTreeview::Model::MsgSort_Insertion::~MsgSort_Insertion()
{
    list_delete(&d_list_work);
}

gboolean
GnomeCmdConnectionTreeview::Model::MsgSort_Insertion::init()
{
    a_i0        = 2;
    list_delete(&d_list_work);
    list_original_copy(&d_list_work);

    return TRUE;
}

gboolean
GnomeCmdConnectionTreeview::Model::MsgSort_Insertion::restart()
{
    list_delete(&d_list_work);

    return TRUE;
}

gboolean
GnomeCmdConnectionTreeview::Model::MsgSort_Insertion::step()
{
    guint       i   = 0;
    Row     *   r1  = NULL;
    Row     *   r2  = NULL;
    GList   *   l1  = NULL;
    GList   *   l2  = NULL;
    //.........................................................................
    SORT_TKI("MsgSort_Insertion::next():(%03i / %03i) %02i%%", a_i0, list_card(), a_i0 * 100 / list_card());

    if ( a_i0 > list_card() )
        goto lab_sorted;

    i = a_i0;

    do
    {
        // get elements
        r1  =   (Row*)g_list_nth_data(d_list_work, i - 2);
        r2  =   (Row*)g_list_nth_data(d_list_work, i - 1);

        // elements are ordered : exit
        if ( r1->compare(r2) <= 0 )
            goto lab_exit;

        // elements not ordered : swap them and loop
        g_list_nth(d_list_work, i - 2   )->data = r2;
        g_list_nth(d_list_work, i - 1   )->data = r1;

        if ( --i == 1 )
            goto lab_exit;
    }
    while ( TRUE );


lab_exit:
    a_i0++;
    return FALSE;

lab_sorted:
    SORT_TKI("MsgSort_Insertion::next():sorted !");

    l1 = d_list_actual;
    l2 = d_list_work;

    for ( i = 0 ; i != list_card() ; i++ )
    {
        SORT_INF("%05s %05s",
            ((Row*)(l1->data))->utf8_name_display(),
            ((Row*)(l2->data))->utf8_name_display() );

        l1 = g_list_next(l1);
        l2 = g_list_next(l2);
    }

    return TRUE;
}



















