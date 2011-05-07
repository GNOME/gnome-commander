/*
    ###########################################################################

    gnome-cmd-connection-treeview-model.cc

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

    Struct  : Model

    Parent  : GnomeCmdConnectionTreeview

    ###########################################################################
*/
#include    "gnome-cmd-connection-treeview.h"

//  ***************************************************************************
//  								Defines
//  ***************************************************************************

//  ***************************************************************************
//  								static
//  ***************************************************************************
static gboolean         _USE_GIO_               = GCMDGTKFOLDVIEW_USE_GIO;
static gboolean         _ALLOW_MONITORING_      = GCMDGTKFOLDVIEW_ALLOW_MONITORING;
static const gchar  *   _STRING_NOT_AVAILABLE_  = "* String not available *";

GtkTreeIter			GnomeCmdConnectionTreeview::Model::Iter_zero = { 0,0,0,0 };

const gchar* GnomeCmdConnectionTreeview::Model::Msg_names[] =
{
    "eAddDummyChild"        ,
    "eAddChild"             ,

    "eDel"                  ,

    "eSetReadable"          ,
    "eSetNotReadable"
};
const gchar* GnomeCmdConnectionTreeview::Model::Msg_name(eMsgType _type)
{
    switch ( _type )
    {
        case    eAddDummyChild      : return Msg_names[0];
        case    eAddChild           : return Msg_names[1];

        case    eDel                : return Msg_names[2];

        case    eSetReadable        : return Msg_names[3];
        case    eSetNotReadable     : return Msg_names[4];

        default                     : return _STRING_NOT_AVAILABLE_;
    }
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #						All structs are in this file                      #
//  #                                   |                                     #
//  ##									V									 ##
//  ###																		###
//  ###########################################################################
//#include    "gnome-cmd-connection-treeview-model.structs.snippet.h"

//  ###########################################################################
//
//							new, ...
//
//  ###########################################################################
GnomeCmdConnectionTreeview::Model::Model(
    GnomeCmdConnectionTreeview::Control     *   _control)
{
    raz_pointers();
    init_instance(_control);
}
GnomeCmdConnectionTreeview::Model::~Model()
{
    dispose();
    finalize();
}

void
GnomeCmdConnectionTreeview::Model::raz_pointers()
{
    a_control           = NULL;
	d_treestore			= NULL;
}
void
GnomeCmdConnectionTreeview::Model::init_instance(
    Control *_control)
{
    a_control   = _control;

    (sLogger.log_function(eLogGcmd, Logger::eLogInf))(sLogger.channel(eLogGcmd)->get_header(), "TEST");

    // log infos on config
    GCMD_INF("Model::create():Using %s library for filesystem access",
        _USE_GIO_ ? "GIO" : "GnomeVFS");


    GCMD_INF("Model::create():Monitoring is %s",
        _ALLOW_MONITORING_ ? "ENABLED" : "DISABLED");

	d_treestore = gnome_cmd_foldview_treestore_new();

	// dont do sink on a widget that will be parented !!!
	// else all the unref mechanism will be broken, and dispose & finalize
	// will never be called, or you will run in segfaults when unref
	//g_object_ref_sink(a_treestore);
}


void GnomeCmdConnectionTreeview::Model::dispose()
{
	GCMD_INF("GnomeCmdConnectionTreeview::Model::dispose()");

    GCMD_INF("GnomeCmdConnectionTreeview::Model::dispose::treestore refcount = %03i", d_treestore->refcount());
    g_object_unref(d_treestore);

    d_treestore = NULL;
}

void GnomeCmdConnectionTreeview::Model::finalize()
{
	GCMD_INF("GnomeCmdConnectionTreeview::Model::finalize()");

	//-delete d_quickstack;
}

//  ###########################################################################
//
//								Divers
//
//  ###########################################################################
GnomeCmdConnectionTreeview::Model::IterInfo::eFields operator | (
    GnomeCmdConnectionTreeview::Model::IterInfo::eFields a,
    GnomeCmdConnectionTreeview::Model::IterInfo::eFields b)
{
    return (GnomeCmdConnectionTreeview::Model::IterInfo::eFields)( (guint32)a | (guint32)b );
}



gchar*
GnomeCmdConnectionTreeview::Model::collate_key_new_from_utf8(
	const gchar* _utf8_str)
{
	return  g_utf8_collate_key(_utf8_str, g_utf8_strlen(_utf8_str, -1));
}

//  ###########################################################################
//
//  						GtkTreeModelIface wrappers
//
//  ###########################################################################
//  ===========================================================================
//  get_iter
//  ===========================================================================
gboolean GnomeCmdConnectionTreeview::Model::get_iter(
	GtkTreePath *path,
	GtkTreeIter *iter)
{
	return GnomeCmdFoldviewTreestore::get_iter(
		treemodel(),
		iter,
		path);
}

//  ===========================================================================
//  get value for iter
//  ===========================================================================
void GnomeCmdConnectionTreeview::Model::get_value(GtkTreeIter *in, gint column, GValue *value)
{
	GnomeCmdFoldviewTreestore::get_value(treemodel(), in, column, value);
}

//  ===========================================================================
//  Count childs of an iter
//  ===========================================================================
gint GnomeCmdConnectionTreeview::Model::iter_n_children(GtkTreeIter *parent)
{
	return GnomeCmdFoldviewTreestore::iter_n_children(treemodel(), parent);
}

//  ###########################################################################
//
//  						Custom methods
//
//  ###########################################################################

//  ===========================================================================
//  get the TreeRow for an iter
//  ===========================================================================
GnomeCmdConnectionTreeview::Model::Row*
GnomeCmdConnectionTreeview::Model::iter_get_treerow(
	GtkTreeIter *   _iter)
{
	TreestoreData   *   _data		= NULL;
	//.........................................................................

	if ( ! treestore()->ext_get_data(_iter, &_data) )
		return NULL;

	return (Row*)_data;
}
//  ===========================================================================
//  get the TreeRow for an iter
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_get_treerow(
	GtkTreeIter		*   _iter,
	Row				**  _row_ptr)
{
	GnomeCmdFoldviewTreestore::DataInterface *   _data		= NULL;
	//.........................................................................

	if ( ! treestore()->ext_get_data(_iter, &_data) )
		return FALSE;

	*_row_ptr  = (Row*)_data;
	return TRUE;
}
//  ===========================================================================
//  set the TreeRow for an iter
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_set_treerow(
	GtkTreeIter		*   _iter,
	Row             *   _row)
{
    g_return_val_if_fail( _iter, FALSE );
    g_return_val_if_fail( _row,  FALSE );

	treestore()->ext_set_data(_iter, _row);
	return TRUE;
}
//  ===========================================================================
//  iter_is_expanded / collapsed
//  given an iter, return its display status in the GtkTreeView
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_is_expanded(
	GtkTreeIter *   _iter)
{
	GtkTreePath *   gtk_path	= NULL;
	gboolean		b			= FALSE;
	//.........................................................................
	gtk_path = 	GnomeCmdFoldviewTreestore::get_path(treemodel(), _iter);
	if ( ! gtk_path )
		return FALSE;

	b = ( control()->row_state(gtk_path) == eRowStateExpanded );
	gtk_tree_path_free(gtk_path);
	return b;
}

gboolean
GnomeCmdConnectionTreeview::Model::iter_is_collapsed(
	GtkTreeIter *   _iter)
{
	return (! iter_is_expanded(_iter));
}
//  ===========================================================================
//  given an iter, return its display string in GtkTreeview
//  ===========================================================================
const gchar*
GnomeCmdConnectionTreeview::Model::iter_get_display_string(GtkTreeIter *iter)
{
	Row		*   r		= NULL;
	GValue		v		= {0};
	//.........................................................................
	gtk_tree_model_get_value(treemodel(), iter, 0, &v);

	r = (Row*)g_value_get_pointer(&v);

	return r->utf8_name_display();
}
//  ===========================================================================
//  given an iter, return a mallocated GnomeVFSURI
//  ===========================================================================
const GnomeCmdConnectionTreeview::Uri
GnomeCmdConnectionTreeview::Model::iter_get_uri(GtkTreeIter *final)
{
	Row		*   r   = NULL;
	GValue		v   = {0};
	//.........................................................................

	GnomeCmdFoldviewTreestore::get_value(treemodel(), final, 0, &v);
	r = (Row*)g_value_get_pointer(&v);

	// new uri
	return r->uri_utf8();
}
//  ===========================================================================
//  given an iter, return its root iter
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_get_root(
	GtkTreeIter *in_any,
	GtkTreeIter *out_root)
{
	return treestore()->ext_get_root(in_any, out_root);
}

//  ===========================================================================
//  given an iter, return its gtk-style path
//  ===========================================================================
gchar*
GnomeCmdConnectionTreeview::Model::iter_get_path_str_new(GtkTreeIter* in)
{
	return treestore()->ext_get_gtk_path_str_new(in);
}

//  ===========================================================================
//  Checks if an iter has a given uid
//  ===========================================================================
/*
gboolean
GnomeCmdConnectionTreeview::Model::iter_has_uid(
	GtkTreeIter*	in,
	gint			uid)
{
	//.........................................................................

	return ( treestore()->iter_get_uid() == uid );
}
*/
//  ===========================================================================
//  Checks is 2 iters points on the same node
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_same(
	GtkTreeIter*	iter1,
	GtkTreeIter*	iter2)
{
	return ( iter1->user_data == iter2->user_data );
}
//  ===========================================================================
//  Checks is an iter is a root iter
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_is_root(
	GtkTreeIter* iter)
{
	return treestore()->ext_is_root(iter);
}

//  ===========================================================================
//  Return an iter's depth
//  ===========================================================================
gint
GnomeCmdConnectionTreeview::Model::iter_depth(
	GtkTreeIter* iter)
{
	return treestore()->ext_iter_depth(iter);
}

//  ===========================================================================
//  iter_add_tree()
//  ===========================================================================
//
//  Add a tree starting from an iter
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_add_tree(
GtkTreeIter	 *iter_in,
GtkTreeIter	 *iter_tree_out)
{
	Row			*   rw				= NULL;
	//.........................................................................
	// get the value to copy
	treestore()->ext_get_data(iter_in, (TreestoreData**)&rw);

	return iter_add_tree(
        rw->uri_utf8(), rw->utf8_name_display(), rw->utf8_symlink_target_uri(),
        rw->access(), rw->is_link(),
        control()->is_con_samba(), control()->is_con_local(), control()->host_redmond(),
        iter_tree_out);
}
//  ===========================================================================
//  add_tree()
//  ===========================================================================
//
//  Add a tree starting from a GnomeCmdCon & GnomeCmdPath
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_add_tree(
	Uri						_utf8_uri,
	const gchar*			_utf8_display_name,
	Uri						_utf8_symlink_target_uri,
	eFileAccess				_access,
	gboolean				_is_symlink,
    gboolean                _is_samba,
    gboolean                _is_local,
    gboolean                _host_redmond,
	GtkTreeIter			*   _iter_tree_out)
{
	Row		*   rt		= NULL;
	//.........................................................................
    rt = new Row(
        eRowRoot, _utf8_uri, _utf8_display_name, _utf8_symlink_target_uri,
        _access, _is_symlink,
        _is_samba, _is_local, _host_redmond);

	// add it to the store
	iter_add_child(NULL, _iter_tree_out, rt);

	return TRUE;
}

//  ===========================================================================
//  iter_add_child()
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_add_child(
	GtkTreeIter			*   _parent,		// in
	GtkTreeIter			*   _child,			// out
	Row					*   _row_child)
{
	// add the child to the treestore
	treestore()->ext_add_child(_parent, _child, _row_child);

	// if the child is not a dummy child, start monitoring
	if ( ! _row_child->is_dummy() )
	{	//iter_monitor_start(_child);
    }                                         // _GWR_UNCOMMENT_
	else
		GCMD_ERR("USE iter_dummy_child_add instead of iter_add_child with a dummy");

	return TRUE;
}

//  ===========================================================================
//  iter_dummy_child_add()
//  ===========================================================================
//
//  For adding the dummy, we dont call iter_add_child(), since it would start
//  monitoring. Instead we call treestore()->ext_add_child.
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_dummy_child_add(
	GtkTreeIter *   _iter_parent,   // in
	GtkTreeIter *   _iter_dummy)	// out
{
	gint			n_parent		= 0;

	Row		*   rw_parent  = NULL;
	Row		*   rw_dummy   = NULL;
	//.........................................................................
	n_parent = iter_n_children(_iter_parent);
	if ( n_parent != 0 )
	{
		GCMD_ERR("Model::iter_dummy_child_add():parent has already children (%03i)", n_parent);
		return FALSE;
	}

	if ( ! iter_get_treerow(_iter_parent, &rw_parent) )
	{
		GCMD_ERR("Model::iter_dummy_child_add():could not get parent TreeRow");
		return FALSE;
	}

	rw_dummy = new Row(eRowDummy, rw_parent);

	// calling treestore directly, no monitoring
	treestore()->ext_add_child(_iter_parent, _iter_dummy, rw_dummy);

	return TRUE;
}

//  ===========================================================================
//  iter_dummy_child_remove()
//  ===========================================================================
//
//  We simply call iter_remove(), with some verifications.
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_dummy_child_remove(
	GtkTreeIter *   _iter_parent)
{
	gint			n_parent		= 0;

	GtkTreeIter		iter_child		= Iter_zero;
	Row			*   rw_child		= NULL;
	//.........................................................................
	n_parent = iter_n_children(_iter_parent);
	if ( n_parent != 1 )
	{
		GCMD_ERR("Model::iter_dummy_child_remove():parent has not exactly one child (%03i)", n_parent);
		return FALSE;
	}

	if ( ! GnomeCmdFoldviewTreestore::iter_children(treemodel(), &iter_child, _iter_parent) )
	{
		GCMD_ERR("Model::iter_dummy_child_remove():could not get child iter");
		return FALSE;
	}

	if ( ! iter_get_treerow(&iter_child, &rw_child) )
	{
		GCMD_ERR("Model::iter_dummy_child_remove():could not get child TreeRow");
		return FALSE;
	}

	if ( ! rw_child->is_dummy() )
	{
		GCMD_ERR("Model::iter_dummy_child_remove():child is not a dummy");
		return FALSE;
	}

	if ( treestore()->ext_iter_sterile_remove(&iter_child) < 0 )
        return FALSE;

	return TRUE;
}

//  ===========================================================================
//  iter_dummy_child_replace()
//  ===========================================================================
//
//  It seems that we can append rows during text_expand_row signal, but not		// _GWR_GTK_
//  remove rows. So instead of removing [DUMMY], we replace its value...
//
void
GnomeCmdConnectionTreeview::Model::iter_dummy_child_replace(
	GtkTreeIter			*   _iter_parent,
	GtkTreeIter			*   _iter_child,
	Row					*   _rw_child_new)
{
	Row				*   rw_dummy			= NULL;
	gint				n					= 0;
	//.........................................................................
	n = iter_n_children(_iter_parent);
	if ( n !=1 )
	{
		GCMD_ERR("Model::iter_dummy_child_replace():parent has %03i children", n);
		return;
	}

	if ( !GnomeCmdFoldviewTreestore::iter_children(treemodel(), _iter_child, _iter_parent) )
	{
		GCMD_ERR("Model::iter_dummy_child_replace():could not get first child");
		return;
	}

	rw_dummy = iter_get_treerow(_iter_child);
	if ( ! rw_dummy )
	{
		GCMD_ERR("Model::iter_dummy_child_replace():could not get first child's treerow");
		return;
	}
	if ( ! rw_dummy->is_dummy() )
	{
		GCMD_ERR("Model::iter_dummy_child_replace():first child's treerow is not dummy");
		return;
	}

	// we dont stop monitoring on child, since it is a dummy child, thus not monitored

	// replace child's dummy treerow by the new one, signal will be emitted
	treestore()->ext_set_data(_iter_child, _rw_child_new);

	// start monitoring on child
	//iter_monitor_start(_iter_child);                                          // _GWR_UNCOMMENT_
}

//  ===========================================================================
//  iter_dummy_child_check()
//  ===========================================================================
//
//	Check if an iter has a dummy child
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_dummy_child_check(
	GtkTreeIter *   _iter_parent)
{
	GtkTreeIter		iter_child		= Iter_zero;
	Row			*   rw_child		= NULL;
	//.........................................................................
	if ( iter_n_children(_iter_parent)  != 1 )
		return FALSE;

	if ( ! GnomeCmdFoldviewTreestore::iter_children(treemodel(), &iter_child, _iter_parent) )
	{
		GCMD_ERR("Model::iter_dummy_child_check():could not get child iter");
		return FALSE;
	}

	if ( ! iter_get_treerow(&iter_child, &rw_child) )
	{
		GCMD_ERR("Model::iter_dummy_child_check():could not get child TreeRow");
		return FALSE;
	}

	return rw_child->is_dummy();
}

//  ===========================================================================
//  iter_dummy_child_check()
//  ===========================================================================
//
//	Check if an iter has a dummy child
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_remove(
	GtkTreeIter     * _iter)
{
	gint					n			= 0;
	eRowState			    state		= eRowStateUnknown;
	GtkTreePath			*   gtk_path	= NULL;
	//.........................................................................
	GCMD_WNG("iter_remove");

	// get the # of children
	n = iter_n_children(_iter);

	// get the GtkTreePath
	gtk_path = GnomeCmdFoldviewTreestore::get_path(treemodel(), _iter);
	if ( ! gtk_path )
		goto lab_failure;

	// get the view state of the parent
	state = control()->row_state(gtk_path);

	//.........................................................................
	// iter is sterile : goto remove_sterile
	if (  n == 0 )
		goto remove_sterile;

	//.........................................................................
	// iter is fertile, and is expanded
	if ( state == eRowStateExpanded )
	{
		// It is a hack ! Let GtkTreeView update its cache by collapsing !
		// We dont have to manually delete each row and signals on them !
		if ( ! control()->row_collapse(gtk_path) )									// __GWR__HACK__
		{
			GCMD_ERR("Model::iter_remove():iter could not be collapsed");
			goto lab_failure;
		}

		// We dont return here, since the iter is in the following state !
	}

	//.........................................................................
	// iter is fertile, and is not expanded. Silently remove all children.
	// After that, we are in the following case.
	iter_collapsed_remove_children(_iter);

	//.........................................................................
	// iter is sterile :remove it, and send "removed row" signal. But first
	// stop monitoring
remove_sterile:
	//iter_monitor_stop(_iter);                                                 // _GWR_UNCOMMENT_

	if ( treestore()->ext_iter_sterile_remove(_iter) < 0 )
        goto lab_failure;

lab_exit:
	if ( gtk_path )
        gtk_tree_path_free(gtk_path);
	return TRUE;

lab_failure:
	if ( gtk_path )
        gtk_tree_path_free(gtk_path);
	return FALSE;
}

//  ===========================================================================
//  iter_collapsed_remove_children()
//  ===========================================================================
//
//  Remove all children of a _COLLAPSED_ iter
//
gint GnomeCmdConnectionTreeview::Model::iter_collapsed_remove_children(GtkTreeIter *parent)
{
	return treestore()->ext_iter_remove_children_no_signal_row_deleted(parent);
}

//  ===========================================================================
//  iter_remove_all()
//  ===========================================================================
//
//  Remove all children i.e. clear the treestore
//
void GnomeCmdConnectionTreeview::Model::iter_remove_all()
{
	treestore()->ext_clear();
}

//  ###########################################################################
//
//  							Match functions
//
//  ###########################################################################

//=============================================================================
//  Match iter by its data's uid
//=============================================================================
/*
gboolean
GnomeCmdConnectionTreeview::Model::Match_function_uid(
	GnomeCmdFoldviewTreestore::Data*	data,
	gint								uid)
{
	g_assert(FALSE);
	//return ( ((TreeRow*)data)->uid() != uid ? FALSE : TRUE );
	return TRUE;
}
*/
//=============================================================================
//  Match iter by its data's collate key
//=============================================================================
//
// GnomeCmdFoldviewTreestore::Data has access to collate keys
//

//  ###########################################################################
//
//  						Files functions
//
//  ###########################################################################

//=============================================================================
//  iter_files_file_filter()
//=============================================================================
//
//  Check if a file can be added
//
//  This is an adaptation of :
//	  gboolean GnomeCmdFileList::file_is_wanted(GnomeCmdFile *f)
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_files_file_filter(
	File	*   _file)
{
	GnomeVFSFileType		type	= GNOME_VFS_FILE_TYPE_DIRECTORY;
	//.........................................................................
    g_return_val_if_fail ( _file != NULL, FALSE );

	if ( !_file->is_dir() && !_file->is_symlink() )
	{
		FILES_ERR("Model::iter_files_file_filter:not a directory, nor a symlink");
        return FALSE;
	}

	if ( _file->is_symlink() )
        type	= GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK;

	//.........................................................................
    if (strcmp (_file->name_utf8(), ".") == 0)
	{
		FILES_INF("Model::iter_files_file_filter:dot");
        return FALSE;
	}
    if (strcmp (_file->name_utf8(), "..") == 0)
	{
		FILES_INF("Model::iter_files_file_filter:dot dot");
        return FALSE;
	}
    if (gnome_cmd_data.hide_type(type) )
	{
		FILES_INF("Model::iter_files_file_filter:hidden type");
        return FALSE;
	}
    if (_file->name_utf8()[0] == '.' && gnome_cmd_data.filter_settings.hidden)
	{
		FILES_INF("Model::iter_files_file_filter:hidden file");
        return FALSE;
	}
    if (gnome_cmd_data.filter_settings.backup && patlist_matches(gnome_cmd_data_get_backup_pattern_list (), _file->name_utf8()) )
	{
		FILES_INF("Model::iter_files_file_filter:backup");
        return FALSE;
	}

    return TRUE;
}

//  ===========================================================================
//  iter_files_add_at_least_one()
//  ===========================================================================
//
//  Check if at least on file from a gvfs_async_load_subdirs can be added
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_files_add_at_least_one(
	GList	*   _list)
{
	GList				*   list	= NULL;
	FoldviewFile		*   file		= NULL;
	//.........................................................................

	list = g_list_first(_list);

	while (list )
	{
		file = (File*)list->data;

		if ( iter_files_file_filter(file) )
			return TRUE;

		list = g_list_next(list);
	}
	return FALSE;
}

//  ===========================================================================
//  iter_files_add_file()
//  ===========================================================================
//
//  Add one file
//
gboolean
GnomeCmdConnectionTreeview::Model::iter_files_add_file(
	GtkTreeIter		*   _iter_parent,
	GtkTreeIter		*   _iter_child,
	FoldviewFile	*   _file,
	gboolean			_check_for_doubloon,
	gboolean			_replace_first_child)
{
	gchar				*		utf8_collate_key_file   = NULL;
	Row					*		row_parent				= NULL;
	Row					*		row_child				= NULL;
	//.........................................................................
	g_return_val_if_fail( _iter_parent,		FALSE );
	g_return_val_if_fail( _iter_child,		FALSE );
	g_return_val_if_fail( _file,			FALSE );

	// filter
	if ( ! iter_files_file_filter(_file) )
		return FALSE;

	// doubloons
	if ( _check_for_doubloon )
	{
		utf8_collate_key_file = collate_key_new_from_utf8(_file->name_utf8());

		if ( treestore()->ext_match_child_str(_iter_parent, _iter_child, utf8_collate_key_file) )
		{
			g_free(utf8_collate_key_file);
			return FALSE;
		}

		g_free(utf8_collate_key_file);
	}

	row_parent = iter_get_treerow(_iter_parent);
	if ( ! row_parent )
		return FALSE;

	// build the TreeRow
	row_child = new Row(eRowStd, row_parent, _file);

	// add it !
	if ( _replace_first_child )
		iter_dummy_child_replace(_iter_parent, _iter_child, row_child);
	else
		iter_add_child(_iter_parent, _iter_child, row_child);

	return TRUE;
}

//  ###########################################################################
//
//  			                Refresh
//
//  ###########################################################################
//
//  Rescan all subiters of an iter for
//
//  - checking filesystem modifications
//  - taking care of display criteria modifications
//
//  We need 2 async ops on each iter :
//
//  1. We first need to do a get_file_info call on the iter, since
//    enumerate_children dont tell us anything about it...
//
//  2. Call enumerate_children on the iter
//

//  ===========================================================================
//  iter_refresh_action()
//  ===========================================================================
//
//  Branch on different actions, depending on file new / old readability, and
//  iter expanded / collapsed status ( called "state" )
//
//  An iter state is summarized by 3 letters :
//
//      a-b-c
//      | | |
//      | | +---------------+
//      | |                 |
//      | |             c ( collapsed ) / e ( expanded )
//      | |
//      | +---------+
//      |           |
//      |       new readability : y / n
//      |
//    old readability : y / n
//
//  All readabilty switches can be handled by the iter_readable_set / unset
//  functions already written for monitoring events.
//
//  All readability equalities can not be handled by monitoring code, and have
//  specific treatment ( = new code = true refresh code )
//
void
GnomeCmdConnectionTreeview::Model::iter_refresh_action(
    GtkTreeIter     *   _iter,
    TreestorePath   *   _path,
    gboolean            _old_readable,
    gboolean            _new_readable,
    gboolean            _collapsed)
{
    if ( _new_readable )
    {
        if ( _old_readable )
        {
            if ( _collapsed )
                goto lab_y_y_c;
            else
                goto lab_y_y_e;
        }
        else
        {
            if ( _collapsed )
                goto lab_n_y_c;
            else
                goto lab_n_y_e;
        }
    }
    else
    {
       if ( _old_readable )
        {
            if ( _collapsed )
                goto lab_y_n_c;
            else
                goto lab_y_n_e;
        }
        else
        {
            if ( _collapsed )
                goto lab_n_n_c;
            else
                goto lab_n_n_e;
        }
    }

    REFRESH_ERR("Model::ira():*** MACHINE PB ***");
    goto lab_abort;

    if ( TRUE )                                                                 // _GWR_FOLD_
    {
    //.........................................................................
lab_y_y_c:
    // See if iter has children, for showing / hiding the GtkTreeView arrow
    REFRESH_INF("Model::ira():Readable - Readable - Collapsed")
    iter_check_if_empty(_iter);
    goto lab_exit;
    //.........................................................................
lab_y_y_e:
    // most complicated case, true refresh ( see iter_refresh_children_callback )
    // ( In fact there is a simple method, inspired from the _GTK_HACK_ of
    // iter_remove : collapse the GtkTreeView, silently delete all children, call
    // iter_expand, and finally expand the GtkTreeView - but the many collapsing
    // and expanding would make look the foldview like an animated GIF - or
    // a christmas tree :)
    REFRESH_INF("Model::ira():Readable - Readable - Expanded")
    iter_enumerate_children(_iter, Iter_refresh_children_callback);
    goto lab_exit;
    //.........................................................................
lab_n_y_c:
    REFRESH_INF("Model::ira():NOT Readable - Readable - Collapsed")
    control()->message_fifo_push( GCMD_STRUCT_NEW(MsgSetReadable, _path) );
    goto lab_exit;
    //.........................................................................
lab_n_y_e:
    // most complicated case, etc...
    REFRESH_INF("Model::ira():NOT Readable - Readable - Expanded")
    iter_enumerate_children(_iter, Iter_refresh_children_callback);
    //.........................................................................
lab_y_n_c:
lab_y_n_e:
    REFRESH_INF("Model::ira():Readable - Not Readable - ...")
    control()->message_fifo_push( GCMD_STRUCT_NEW(MsgSetNotReadable, _path) );
    goto lab_exit;
    //.........................................................................
lab_n_n_c:
    // do nothing
    REFRESH_INF("Model::ira():NOT Readable - NOT Readable - Collapsed")
    goto lab_exit;
    //.........................................................................
lab_n_n_e:
    // Program Logic Error : an iter can not be Not Readable and Expanded
    REFRESH_ERR("Model::ira():PLE: NOT Readable - NOT Readable - Expanded")
    goto lab_abort;
    //.........................................................................
lab_exit:
lab_abort:
    return;

    }
}

//  ===========================================================================
//  Iter_refresh_callback()
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::Iter_refresh_callback(
    AsyncCore   * _ac)
{
	AsyncGetFileInfo                    *	gfi					= NULL;
	AsyncCallerData						*	acd			        = NULL;
	Model				                *	THIS				= NULL;

    IterInfo                                info;

	GtkTreeIter								iter		        = Iter_zero;

    eFileAccess                             old_access          = eAccessUN;
    eFileAccess                             new_access          = eAccessUN;
    gboolean                                expanded            = FALSE;
    gboolean                                collapsed           = FALSE;
	//.........................................................................
	Lock();																		// _GDK_LOCK_

	gfi				= (AsyncGetFileInfo*)_ac;
	acd		        = gfi->caller_data();
	THIS			= acd->model();

	//.........................................................................
	//
	//  Step #1 : try to retrieve the iter & row for parent
	//
	if ( ! info.gather(THIS, acd->path(), IterInfo::eExp) )
	{
		REFRESH_WNG("Model::irc:Gather failed");
		goto lab_abort;
	}
    REFRESH_INF("Model::irc:Refreshing [%s]", info.row()->utf8_name_display());

	//.........................................................................
	//
	//  Step #2 : some vars
	//

    // current file access
    new_access  = gfi->access();
    // old file access
    old_access  = info.row()->access();

    if ( ( new_access == eAccessUN ) || ( old_access == eAccessUN ) )
    {
		REFRESH_ERR("Model::irc:Row has unknown access mode [old:%02i new:%02i]",
                        old_access, new_access);
		goto lab_abort;
    }

    // iter state ( old iter state, maybe not the same as the
    // current file access )
    expanded    = info.expanded();
    collapsed   = ! expanded;

	//.........................................................................
	//
	//  Step #3 : Branch on different actions
	//
    THIS->iter_refresh_action(
        info.iter(),
        info.path(),
        Access_readable(old_access),
        Access_readable(new_access),
        collapsed);

lab_abort:
    delete acd;
    Unlock();
    return;
}

//  ===========================================================================
//  Iter_refresh_children_callback()
//  ===========================================================================
//
//  Here we know that :
//
//      - Iter has been refreshed by Iter_refresh_callback
//
//      - The status of the iter was y-y-e
//
void
GnomeCmdConnectionTreeview::Model::Iter_refresh_children_callback(
    AsyncCore   * _ac)
{
	AsyncEnumerateChildren				*	aec					= NULL;
	AsyncCallerData						*	acd			        = NULL;
	Model				                *	THIS				= NULL;

    IterInfo                                info_parent;


    IterInfo                                info_child;
	GtkTreeIter								iter_child			= Iter_zero;
    GtkTreeIter                         *   itemp               = NULL;

    File                                *   file                = NULL;

    gboolean                                b                   = FALSE;
    gboolean                                b_persistent        = FALSE;

    GList                               *   list                = NULL;
    GList                               *   temp                = NULL;
    GList                               *   iters_to_delete     = NULL;
    GList                               *   iters_persistent    = NULL;

	//.........................................................................
	Lock();																		// _GDK_LOCK_

	aec				= (AsyncEnumerateChildren*)_ac;
	acd		        = aec->caller_data();
	THIS			= acd->model();

	//.........................................................................
	//
	//  Step #1 : Verify that parent iter is ?-y-e
	//
    if ( ! info_parent.gather(THIS, acd->path(), IterInfo::eRead | IterInfo::eExp) )
    {
		REFRESH_WNG("Model::ircc():gather failed");
		Error_Msg_Failure();    goto lab_exit_false;
    }

    if ( ! info_parent.readable() )
    {
		REFRESH_WNG("Model::ircc():Parent iter is NOT Readable");
		Error_Msg_Abort();      goto lab_exit_false;
    }

    if ( ! info_parent.expanded() )
    {
		REFRESH_WNG("Model::ircc():Parent iter is NOT Expanded");
		Error_Msg_Abort();      goto lab_exit_false;
    }

    REFRESH_TKI("Model::ircc():Parent [%s]", info_parent.row()->utf8_name_display());

	//.........................................................................
	//
	//  Step #2 : Check errors on async result
    //
	//  In fact, we should handle all error cases, and eventually continue      // _GWR_TODO_
    //  the process on some children...
    //
    if ( aec->error() )
    {
		REFRESH_WNG("Model::ircc():async_enumerate_children failed [%s]", aec->error_str());
		Error_Msg_Abort();      goto lab_exit_false;
    }

	//.........................................................................
	//
	//  Step #3.1 : Reduce async file list by filtering according
    //              to display settings
	//
    //REFRESH_INF("Model::ircc():  --- filter ---");
    //REFRESH_INF("Model::ircc():filter:%03i files", aec->list_card());

    list = g_list_first(aec->list());
    while ( list )
    {
        file = (File*)(list->data);

        if ( ! THIS->iter_files_file_filter(file) )
        {
            //REFRESH_INF("Model::ircc():filter:-[%s]", file->name_utf8());

            // remember next
            temp = list->next;

            // delete current Glist element and file struct
            aec->list_remove(list);

            // recall next
            list = temp;
        }
        else
        {
            //REFRESH_INF("Model::ircc():filter:+[%s]", file->name_utf8());

            // we will need this in 3.2
            file->compute_ck_utf8_name();
            list = list->next;
        }
    }

    //REFRESH_INF("Model::ircc():filter:%03i files", aec->list_card());

	//.........................................................................
	//
	//  Step #3.2 : - Persistent iters :
    //                  - delete corresponding files from async list ; this way
    //                    only new files remain in it.
    //                  - memorize them ( for "async recursivity" )
    //
    //              - Iters tah have to be deleted :
    //                  - memorize them
	//
    //REFRESH_INF("Model::ircc():  --- compare ---");

    // alloc the memorization lists
    iters_to_delete     = g_list_alloc();
    iters_persistent    = g_list_alloc();

    // + loop on GtkTreeModel iters
    b = GnomeCmdFoldviewTreestore::iter_children(THIS->treemodel(), &iter_child, info_parent.iter());

    // PLV : empty readable, expanded iter : failure
    if ( !b )
    {
        REFRESH_ERR("Model::ircc():Parent iter is readable, expanded, but empty");
		goto lab_exit_false;
    }

    while ( b )
    {
        //REFRESH_INF("Model::ircc():node [%08x]", iter_child.user_data);

        // we want to use the row of the child
        info_child.reset();
        if ( ! info_child.gather(THIS, &iter_child, IterInfo::eNothing) )
        {
            REFRESH_ERR("Model::ircc():Gather (eNothing) failed");
            goto lab_exit_false;
        }

        // When messages lag, user can expand before Counter Program goes here. // _GWR_BUG_#01_
        // And the frst child is "...Working...", that will be deleted.
        // And parent will be shown as empty.
        // So we test here
        if ( info_child.row()->is_dummy() )
        {
            REFRESH_WNG("Model::ircc():first child is a dummy child ( _GWR_BUG_#01_ )");
            Error_Msg_Abort();      goto lab_exit_false;
        }

        REFRESH_TKI("Model::ircc():compare:iter [%s]", info_child.row()->utf8_name_display());

        // + loop on async list
        b_persistent    = FALSE;
        list            = g_list_first(aec->list());

        while ( list && (! b_persistent) )
        {
            file = (File*)(list->data);

            // ck match : set persistent
            if ( ! strcmp(file->ck_utf8_name(), info_child.row()->utf8_collate_key(eCollateKeyRaw)) )
            {
                REFRESH_TKI("Model::ircc():  file [%s] Match", file->name_utf8());

                // async list : delete element & file struct
                temp = list->next;
                aec->list_remove(list);
                list = temp;

                // store a copy of iter_child in iters_persistent
                itemp               = g_try_new0(GtkTreeIter, 1);
                GCMD_ITER_COPY(itemp, &iter_child);
                iters_persistent    = g_list_append(iters_persistent, (gpointer)itemp);

                // mark this iter as persistent
                b_persistent = TRUE;
            }
            else
            {
                //REFRESH_TKI("Model::ircc():  file [%s]", file->name_utf8());
                list = list->next;
            }
        }
        // - loop on async list

        // iter is not persistent -> work is done , just log something
        if ( b_persistent )
        {
            REFRESH_TKI("Model::ircc():  iter [%s] =", info_child.row()->utf8_name_display());
        }
        // iter is not persistent -> store it in a list for further deletion
        else
        {
            REFRESH_TKI("Model::ircc():  iter [%s] -", info_child.row()->utf8_name_display());

            // store a copy of the path of iter_child in iters_to_delete
            iters_to_delete     = g_list_append( iters_to_delete, (gpointer)(info_child.path()->dup()) );
        }

        b = GnomeCmdFoldviewTreestore::iter_next(THIS->treemodel(), &iter_child);
    }
    // - loop on GtkTreeModel iters

	//.........................................................................
	//
	//  Step #4 : Send messages for deleting iters marked to be deleted
	//
    list = g_list_first(iters_to_delete)->next;                 // first is zero
    while (list)
    {
        //THIS->iter_remove( (GtkTreeIter*)(list->data) );
        //g_free( (GtkTreeIter*)(list->data) );

        THIS->control()->message_fifo_push
        (
            GCMD_STRUCT_NEW
            (
                Model::MsgDel,
                (TreestorePath*)(list->data)
            )
        );
        g_free(list->data);

        list = g_list_next(list);
    }
    g_list_free(iters_to_delete);

	//.........................................................................
	//
	//  Step #5 : Send messages for creating new iters from async list
	//
    list            = g_list_first(aec->list());

    while ( list )
    {
        file            = (File*)(list->data);

        //row_new_child   = new Row(eRowStd, info_parent.row(), file);
        //THIS->iter_add_child(info_parent.iter(), &iter_child, row_new_child);

        THIS->control()->message_fifo_push
        (
            GCMD_STRUCT_NEW
            (
                Model::MsgAddChild,
                info_parent.path(),
                file
            )
        );
        list = g_list_next(list);
    }

	//.........................................................................
	//
	//  Step #6 : Refresh persistent iters
    //            ( here is the "async recursive call" )
	//
    list = g_list_first(iters_persistent)->next;                // first is zero
    while (list)
    {
        THIS->iter_refresh( (GtkTreeIter*)(list->data) );
        g_free( (GtkTreeIter*)(list->data) );
        list = g_list_next(list);
    }
    g_list_free(iters_persistent);

	//.........................................................................
	//
	//  Exit
    //
lab_exit_true:
lab_exit_false:
	delete acd;
	Unlock();														            // _GDK_LOCK_
	return;
}

//  ===========================================================================
//  iter_refresh()
//  ===========================================================================
//
//  Main call for refreshing from an iter
//
void
GnomeCmdConnectionTreeview::Model::iter_refresh(
	GtkTreeIter *   _iter)
{
	AsyncCallerData			*	acd			= NULL;
    GtkTreeIter                 child       = Iter_zero;
	Row						*   row			= NULL;
	//.........................................................................
    //REFRESH_INF("Model::iter_sort():called");

    // iter given : refresh this iter
    if ( _iter )
    {
        // treerow
        row = iter_get_treerow(_iter);
        g_return_if_fail( row );

        // create a struct for knowing what to do with GVFS answers
        acd = GCMD_STRUCT_NEW(AsyncCallerData, this, Iter_refresh_callback, row->path()->dup());

        _USE_GIO_   ?   a_GIO.iter_get_file_info(acd, row->uri_utf8())                                          :
                        a_GnomeVFS.iter_get_file_info(acd, row->uri_utf8(), control()->access_check_mode())     ;

        return;
    }

    // iter not given : refresh all root nodes
    if ( ! treestore()->iter_children(treemodel(), &child, NULL) )
        return;

    do
    {
        // treerow
        row = iter_get_treerow(&child);
        g_return_if_fail( row );

        // create a struct for knowing what to do with GVFS answers
        acd = GCMD_STRUCT_NEW(AsyncCallerData, this, Iter_refresh_callback, row->path()->dup());

        _USE_GIO_   ?   a_GIO.iter_get_file_info(acd, row->uri_utf8())                                          :
                        a_GnomeVFS.iter_get_file_info(acd, row->uri_utf8(), control()->access_check_mode())     ;
    }
    while ( treestore()->iter_next(treemodel(), &child) );

}


//  ###########################################################################
//
//  			                Sorting
//
//  ###########################################################################

//  ===========================================================================
//  iter_sort()
//  ===========================================================================
//
//  Main call for sorting from an iter
//
void
GnomeCmdConnectionTreeview::Model::iter_sort(
	GtkTreeIter *   _iter)
{
    GtkTreeIter         child       = Iter_zero;
	Row				*   row			= NULL;
	//.........................................................................
    //SORT_INF("Model::iter_sort():called");

    // iter given : sort from this iter
    if ( _iter )
    {
        // treerow
        row = iter_get_treerow(_iter);
        g_return_if_fail( row );

        control()->sorting_list_add( GCMD_STRUCT_NEW( MsgSort_Insertion, row->path(), this ) );

        return;
    }

    // iter not given : sort from all root nodes
    if ( ! treestore()->iter_children(treemodel(), &child, NULL) )
        return;

    do
    {
        // treerow
        row = iter_get_treerow(&child);
        g_return_if_fail( row );

        control()->sorting_list_add( GCMD_STRUCT_NEW( MsgSort_Insertion, row->path(), this ) );
    }
    while ( treestore()->iter_next(treemodel(), &child) );
}


//  ###########################################################################
//
//  			                Check if empty
//
//  ###########################################################################
//
//  What we do here :
//
//  - Find all subdirs
//  - If at least one, add a dummy child so GtkTreeview will show a little
//	  arrow allowing expansion.
//  - If no subdir, remove dummy child
//

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Check if empty : async callback
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void
GnomeCmdConnectionTreeview::Model::Iter_check_if_empty_callback(
	AsyncCore *  _ac)
{
	AsyncEnumerateChildren				*	aec					= NULL;
	AsyncCallerData						*	cd			        = NULL;
	Model				                *	THIS				= NULL;

    IterInfo                                info;
	//.........................................................................
	Lock();																		// _GDK_LOCK_

	aec				= (AsyncEnumerateChildren*)_ac;
	cd		        = aec->caller_data();
	THIS			= cd->model();

	//.........................................................................
	//
	//  Step #1 : Try to retrieve the iter & row for parent
	//
    //  If error, that means that the path doesnt guide to an iter, so the iter
    //  has been deleted from model. So just abort.
    //
    if ( ! info.gather(THIS, cd->path(), IterInfo::eNothing) )
    {
        CHECK_WNG("Model::Iter_check_if_empty_callback():Gather Failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

	//.........................................................................
	//
	//  Step #2 : Check async error code
    //
    if ( aec->error() )
    {
		CHECK_WNG("Model::Iter_expanded_from_ui_callback():Async op failed [%s]", aec->error_str());
        THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgAsyncMismatchICIEC, info.path()) );
        Error_Msg_Abort();      goto lab_exit_false;
    }

	//.........................................................................
	//
	//  Step #3 : If something to add, send message
	//
	if ( THIS->iter_files_add_at_least_one(aec->list()) )
        THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgAddDummyChild, cd->path()) );

	//.........................................................................
	//
	//  Exit
	//
lab_exit_true:
lab_exit_false:
	// delete user data
	delete cd;
	Unlock();																	// _GDK_LOCK_
	return;
 }

//  ===========================================================================
//  Check if empty : public call
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::iter_check_if_empty(
	GtkTreeIter *   _iter)
{
	AsyncCallerData			*	acd			= NULL;
	Row						*   row			= NULL;
	//.........................................................................

	// treerow
	row = iter_get_treerow(_iter);
	g_return_if_fail( row );

	// create a struct for knowing what to do with async answers
	acd = GCMD_STRUCT_NEW(AsyncCallerData, this, Iter_check_if_empty_callback, row->path()->dup());

	_USE_GIO_   ?	a_GIO.iter_check_if_empty(acd, row->uri_utf8())                                         :
                    a_GnomeVFS.iter_check_if_empty(acd, row->uri_utf8(), control()->access_check_mode())    ;
}

//  ###########################################################################
//
//  			                Expand
//
//  ###########################################################################
//
//  - Remove the [DUMMY] item if present ( no dummy doesnt imply no subdirs ;
//	  things can change between two users clicks... )
//  - Add the subdirs
//

//  ===========================================================================
//  Expand : async callback
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::Iter_expanded_from_ui_callback(
	AsyncCore   *   _ac)
{
	AsyncEnumerateChildren				*	aec				= NULL;
	AsyncCallerData						*	cd				= NULL;
	Model               				*	THIS			= NULL;

    IterInfo                                info;

	GnomeCmdFoldviewTreestore::Path		*	path_parent		= NULL;

	GtkTreeIter								iter_parent		= GnomeCmdConnectionTreeview::Model::Iter_zero;
	GtkTreeIter								iter_child		= GnomeCmdConnectionTreeview::Model::Iter_zero;

    GList                               *   list            = NULL;
    File                                *   file            = NULL;
	//.........................................................................
	Lock();																		// _GDK_LOCK_

	aec				= (AsyncEnumerateChildren*)_ac;
	cd				= aec->caller_data();
	THIS			= cd->model();
	path_parent		= cd->path();

	//.........................................................................
	//
	//  Step #1 : Test iter -> must be dummy-childed ( PLV )
    //
    if ( ! info.gather(THIS, path_parent, IterInfo::eFCID) )
    {
        EXPAND_WNG("Model::Iter_expanded_from_ui_callback():Gather Failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

    if ( ! info.first_child_is_dummy() )
    {
		EXPAND_WNG("Model::Iter_expanded_from_ui_callback():Test failed");
        THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgAsyncMismatchIEFUC, path_parent) );
        Error_Msg_Abort();      goto lab_exit_false;
    }

	//.........................................................................
	//
	//  Step #2 : Check async error code
    //
    if ( aec->error() )
    {
		EXPAND_WNG("Model::Iter_expanded_from_ui_callback():Async op failed [%s]", aec->error_str());
        THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgAsyncMismatchIEFUC, path_parent) );
        Error_Msg_Abort();      goto lab_exit_false;
    }

	//.........................................................................
	//
	//  Step #3 : add files
	//
	list	= g_list_first(aec->list());
	do
	{
		file	= (File*)list->data;

		//EXPAND_INF("Model::iec():pushing [%s]", file->name_utf8());

        THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgAddChild, path_parent, file) );
		list	= g_list_next(list);
	}
    while ( list );

	//.........................................................................
	//
	//  Step #4 : Check if directories added are empty / not empty
    //            -> will be done in message_add_child()

	//.........................................................................
	//
	//  Exit
	//
lab_exit_true:
lab_exit_false:
	// delete user data
	delete cd;
	Unlock();																	// _GDK_LOCK_
	return;
}

//  ===========================================================================
//  Expand : public call
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::iter_expanded_from_ui(
	GtkTreeIter	 *  _iter,
	gboolean		_replace_dummy)
{
	gint							n		= 0;
	AsyncCallerData				*	acd		= NULL;
	Row							*   row		= NULL;
	//.........................................................................

	// Ensure that there is only one subitem ( the "...not empty..." one )
	n = iter_n_children(_iter);

	if ( n != 1 )
	{
		EXPAND_ERR("ccie   :control_iter_expand:parent has %i children", n);
		return;
	}

	// treerow
	row = iter_get_treerow(_iter);
	g_return_if_fail( row );

	// create a struct for knowing what to do with async answers
	acd = GCMD_STRUCT_NEW(AsyncCallerData, this, Iter_expanded_from_ui_callback, row->path()->dup());

	_USE_GIO_   ?   a_GIO.iter_enumerate_children(acd, row->uri_utf8())                                     :
                    a_GnomeVFS.iter_enumerate_children(acd, row->uri_utf8(), control()->access_check_mode());
}

//  ###########################################################################
//
//  			                Enumerate children
//
//  ###########################################################################
//
//  Enumerate children on iter
//

//  ===========================================================================
//  iter_enumerate_children()
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::iter_enumerate_children(
	GtkTreeIter	            *   _iter,
    AsyncCallerCallback         _callback)
{
	AsyncCallerData				*	acd		= NULL;
	Row							*   row		= NULL;
	//.........................................................................

	// treerow
	row = iter_get_treerow(_iter);
	g_return_if_fail( row );

	// create a struct for knowing what to do with async answers
	acd = GCMD_STRUCT_NEW(AsyncCallerData, this, _callback, row->path()->dup());

	_USE_GIO_   ?   a_GIO.iter_enumerate_children(acd, row->uri_utf8())                                     :
                    a_GnomeVFS.iter_enumerate_children(acd, row->uri_utf8(), control()->access_check_mode());
}

//  ###########################################################################
//
//  			                Collapse
//
//  ###########################################################################
//
//  Item collapsed
//
//  - Remove all subdirs
//  - Add [DUMMY] ( we have been callapsed, so there were subdirs in there )
//  This is useful, because when re-expanding, directory will be re-scanned
//  and thus we'll be more accurate.
//
//  With monitoring, it is now unuseful. But It save memory. So I keep It.
//

//  ===========================================================================
//  Collapse : public call
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::iter_collapsed_from_ui(
	GtkTreeIter * _iter)
{
	Row				*   row_parent		= NULL;

	gint				removed			= 0;

	GtkTreeIter			child			= Iter_zero;
	//.........................................................................
	row_parent = iter_get_treerow(_iter);
	g_return_if_fail( row_parent );

	removed = iter_collapsed_remove_children(_iter);
	//GCMD_INF("control::iter_collapsed:removed %03i children", removed);

	// we have been collapsed, so we had children ; so re-add dummy child
	iter_dummy_child_add(_iter, &child);
}

//  ###########################################################################
//
//  			                first tree
//
//  ###########################################################################
//
//  This method is called only once, at the creation of the
//  GnomeCmdConnectionTreeview object.
//

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Add_first_tree_callback()
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::Add_first_tree_callback(
	AsyncCore   *   _ac)
{
	AsyncGetFileInfo	*	gfi				= NULL;
	AsyncCallerData		*	cd				= NULL;
	Model               *	THIS			= NULL;

    IterInfo                info;

	TreestorePath		*	path		    = NULL;

	GtkTreeIter				iter		    = GnomeCmdConnectionTreeview::Model::Iter_zero;

    File                *   file            = NULL;
	//.........................................................................
	Lock();																		// _GDK_LOCK_

	gfi				= (AsyncGetFileInfo*)_ac;
	cd				= gfi->caller_data();
	THIS			= cd->model();
    path            = cd->path();
	//.........................................................................
	//
	//  Step #1 : Check async error code
    //
    if ( gfi->error() )
    {
		GCMD_ERR("Model::Add_first_tree_callback():Async op failed [%s]", gfi->error_str());
        THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgAsyncMismatchAFT, path) );
        Error_Msg_Abort();      goto lab_exit_false;
    }

	//.........................................................................
	//
	//  Step #2 : Set good infos on the iter ( build a file )
    //
    if ( gfi->is_symlink() )
        file = new FoldviewLink(gfi->name(), gfi->uri_symlink_target(), gfi->access());
    // it is a directory
    else
        file = new FoldviewDir(gfi->name(), gfi->access());

    THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgAddFirstTree, path, file) );

    delete file;

	//.........................................................................
	//
	//  Exit
	//
lab_exit_true:
lab_exit_false:
	// delete user data
	delete cd;
	Unlock();																	// _GDK_LOCK_
	return;
}

//  ===========================================================================
//  add_first_tree()
//  ===========================================================================
//
//  Although we add an iter to the model, we dont lock it ; indeed, the model
//  is empty at this moment :)
//
void
GnomeCmdConnectionTreeview::Model::add_first_tree(
	const   Uri         _uri,
    const   gchar   *   _alias)
{
    Row                         *   row         = NULL;
    GtkTreeIter                     iter_child  = Iter_zero;
	AsyncCallerData				*	acd		    = NULL;
	//.........................................................................

    // create the first iter
    row = new Row(eRowRoot, _uri, _alias, NULL,
        eAccessRW, FALSE,
        control()->is_con_samba(), control()->is_con_local(), control()->host_redmond());
    iter_add_child(NULL, &iter_child, row);

  	// create a struct for knowing what to do with async answer
	acd = GCMD_STRUCT_NEW(AsyncCallerData, this, Add_first_tree_callback, treestore()->ext_path_from_iter(&iter_child));

	_USE_GIO_   ?   a_GIO.iter_get_file_info(acd, _uri)                                         :
                    a_GnomeVFS.iter_get_file_info(acd, _uri, control()->access_check_mode())    ;
}

//  ###########################################################################
//
//  			                Monitoring
//
//  ###########################################################################

//  ===========================================================================
//  Monitoring : start monitoring an iter
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_monitor_start(
	GtkTreeIter * _iter)
{
	Row				*   rw			= NULL;
	//.........................................................................
	g_return_val_if_fail( _iter, FALSE );

    if ( ! _ALLOW_MONITORING_ )
        return TRUE;

	// row
	rw = iter_get_treerow(_iter);
	g_return_val_if_fail( rw, FALSE );

	// verify that iter is not a dummy
	if ( rw->is_dummy() )
	{
		MONITOR_ERR("GnomeCmdConnectionTreeview::Model::iter_monitor_start():[%s] is dummy", rw->utf8_name_display());
	}

	// verify that iter is not still monitored
	if ( rw->monitoring_started() )
	{
		MONITOR_ERR("GnomeCmdConnectionTreeview::Model::iter_monitor_start():[%s] already monitored", rw->utf8_name_display());
		return FALSE;
	}

	if ( rw->monitoring_start(this, rw) )
	{
		MONITOR_INF("GnomeCmdConnectionTreeview::Model::iter_monitor_start():Success [%s]" ,rw->utf8_name_display());

		return TRUE;
	}

	MONITOR_ERR("GnomeCmdConnectionTreeview::Model::iter_monitor_start():Failure [%s], calling stop", rw->utf8_name_display());

	return iter_monitor_stop(_iter);
}

//  ===========================================================================
//  Monitoring : stop monitoring an iter
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::iter_monitor_stop(
	GtkTreeIter * _iter)
{
	Row						*   rw			= NULL;
	//.........................................................................
	g_return_val_if_fail( _iter, FALSE );

    if ( ! _ALLOW_MONITORING_ )
        return TRUE;

	// row
	rw = iter_get_treerow(_iter);
	g_return_val_if_fail( rw, FALSE );

	// verify that iter is monitored
	if ( ! rw->monitoring_started() )
	{
		MONITOR_ERR("GnomeCmdConnectionTreeview::Model::iter_monitor_stop():[%s] not monitored", rw->utf8_name_display());
		return FALSE;
	}

	// stop monitoring
	if ( rw->monitoring_stop() )
	{
		MONITOR_INF("GnomeCmdConnectionTreeview::Model::iter_monitor_stop():[%s] stopped", rw->utf8_name_display());
		return TRUE;
	}

	// error
	MONITOR_ERR("GnomeCmdConnectionTreeview::Model::iter_monitor_stop():[%s] monitor was not stopped correctly", rw->utf8_name_display());
	return FALSE;
}

//  ===========================================================================
//  Iter_monitor_callback_del()
//  ===========================================================================
//
//  Monitoring callback : self deleted
//
//  We cant have the type of the file, since it is deleted. So just search
//  with the collate key.
//
void
GnomeCmdConnectionTreeview::Model::Iter_monitor_callback_del(
	MonitorData		*	_md)
{
    Model               			*   THIS			= NULL;
	GnomeCmdFoldviewTreestore		*   treestore		= NULL;
	GtkTreeModel					*   treemodel		= NULL;
	Row								*   row				= NULL;

	GtkTreeIter							iter			= Iter_zero;
	//.........................................................................
	MONITOR_INF("Model::Iter_monitor_callback_del()");

	THIS		= _md->a_model;
	row			= _md->a_row;

	treestore   = THIS->treestore();
	treemodel   = THIS->treemodel();

	Lock();																		// _GDK_LOCK_

	if ( ! treestore->ext_iter_from_path(row->path(), &iter) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_del():could not retrieve iter from Path [%s]", row->uri_utf8());
		goto exit;
	}

	// remove the iter
	THIS->iter_remove(&iter);                                                   // _GWR_SEND_MSG_

exit:
	Unlock();																	// _GDK_LOCK_
}

//  ===========================================================================
//  Iter_monitor_callback_child_del()
//  ===========================================================================
//
//  Monitoring callback : child deleted
//
//  If a child iter is deleted, we have 2 cases :
//
//  - The child is displayed ( represented by an iter ) : so it will be
//	  self-callbacked for delete, we do nothing.
//
//  - The child is not displayed ( no iter representing it ) :
//	  The parent iter is thus collapsed, and have a dummy ( c.f. program's logic )
//	  With must test if the deleted child was the last in the directory represented
//	  by the parent iter. In this case, the little expandarrow must disappear from
//	  the GtkTreeView. We do that by initiating a new check_if_empty async call.

void
GnomeCmdConnectionTreeview::Model::Iter_monitor_callback_child_del(
	MonitorData			*	_md,
	const gchar			*   _name_debug)
{
	Model				                *   THIS			= NULL;
	GnomeCmdFoldviewTreestore			*   treestore		= NULL;
	GtkTreeModel						*   treemodel		= NULL;
	Row									*   row				= NULL;

	GtkTreeIter								iter_parent		= Iter_zero;
	Row									*   row_parent		= NULL;
	//.........................................................................
	MONITOR_INF("Model::Iter_monitor_callback_child_del()");

	THIS		= _md->a_model;
	row			= _md->a_row;

	treestore   = THIS->treestore();
	treemodel   = THIS->treemodel();

	Lock();																		// _GDK_LOCK_

	//.........................................................................
	//
	//  Step #1 : Get parent iter & data
	//
	if ( ! treestore->ext_iter_from_path(row->path(), &iter_parent) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_del():could not retrieve parent iter from Path [??? / %s]", _name_debug);
		goto exit;
	}

	if ( ! treestore->ext_get_data(&iter_parent, (GnomeCmdFoldviewTreestore::DataInterface**)&row_parent) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_del():could not retrieve parent iter's TreeRow [??? / %s]", _name_debug);
		goto exit;
	}

	//.........................................................................
	//
	//  Step #2 : parent is expanded : abort
	//
	if ( THIS->iter_is_expanded(&iter_parent) )
	{
		MONITOR_INF("Model::Iter_monitor_callback_child_del():parent is expanded, skipping [%s / %s]", row_parent->utf8_name_display(), _name_debug);
		goto abort;
	}

	//.........................................................................
	//
	//  Step #3 : parent is collapsed : verify that it has only a dummy ( logic verif )
	//
	if ( ! THIS->iter_dummy_child_check(&iter_parent) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_del():parent has no dummy [%s / %s]", row_parent->utf8_name_display(), _name_debug);
		goto abort;
	}

	//.........................................................................
	//
	//  Step #4 : launch a check_if_empty async call on our collapsed,
	//	dummychilded parent iter
	//
	THIS->iter_check_if_empty(&iter_parent);

abort:
exit:
	Unlock();																	// _GDK_LOCK_
}

//  ===========================================================================
//  Iter_monitor_callback_child_new()
//  ===========================================================================
//
//  Monitoring callback : child created
//
void
GnomeCmdConnectionTreeview::Model::Iter_monitor_callback_child_new(
	MonitorData		*   _md,
	File			*   _file)
{
	Model				                *   THIS			= NULL;
	GnomeCmdFoldviewTreestore			*   treestore		= NULL;
	Row									*   row				= NULL;

	Row									*   row_parent		= NULL;
	GtkTreeIter								iter_parent		= Iter_zero;
	GtkTreePath							*   gtk_path_parent = NULL;
	gboolean								b_parent_exp	= FALSE;
	gboolean								b_parent_emp	= FALSE;

	Row									*   row_child		= NULL;
	GtkTreeIter								iter_child		= Iter_zero;
	//.........................................................................
	MONITOR_INF("Model::Iter_monitor_callback_child_new()");

	THIS		= _md->a_model;
	row			= _md->a_row;
	treestore   = THIS->treestore();

	Lock();																		// _GDK_LOCK_

	if ( ! treestore->ext_iter_from_path(row->path(), &iter_parent) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_new():could not retrieve parent iter [ %s / %s]", row->uri_utf8(), _file->name_utf8());
		goto exit;
	}

	if ( ! THIS->iter_get_treerow(&iter_parent, &row_parent) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_new():parent treerow is NULL [ %s / %s]", row->uri_utf8(), _file->name_utf8());
		goto exit;
	}

	// parent status
	gtk_path_parent = treestore->get_path(THIS->treemodel(), &iter_parent);
	b_parent_exp	= (THIS->control()->row_state(gtk_path_parent) == eRowStateExpanded);
	gtk_tree_path_free(gtk_path_parent);
	b_parent_emp	= ( THIS->iter_n_children(&iter_parent) == 0 );

	// parent expanded : add new child
	if ( b_parent_exp )
	{
		MONITOR_INF("Model::Iter_monitor_callback_child_new():parent EXP_xxx : adding new child [ %s / %s]", row_parent->utf8_name_display(), _file->name_utf8());

		row_child = new Row(eRowStd, row_parent, _file);

		if ( ! THIS->iter_add_child(&iter_parent, &iter_child, row_child) )
			MONITOR_ERR("Model::Iter_monitor_callback_child_new():could not add child iter [ %s / %s]", row_parent->utf8_name_display(), _file->name_utf8());

		goto exit;
	}

	// parent not expanded, and empty : add dummy child
	if ( b_parent_emp )
	{
		MONITOR_INF("Model::Iter_monitor_callback_child_new():parent NEXP_EMPTY : adding dummy child [ %s / %s]", row_parent->utf8_name_display(), _file->name_utf8());

		if ( ! THIS->iter_dummy_child_add(&iter_parent, &iter_child) )
			MONITOR_ERR("Model::Iter_monitor_callback_child_new():could not add dummy child iter [ %s / %s]", row_parent->utf8_name_display(), _file->name_utf8());

		goto exit;
	}

	// parent not expanded, and not empty : skip
	MONITOR_INF("Model::Iter_monitor_callback_child_new():parent NEX_NEMPTY : skipping [ %s / %s]", row_parent->utf8_name_display(), _file->name_utf8());

exit:
	Unlock();																	// _GDK_LOCK_
	return;
}

//  ===========================================================================
//  Iter_monitor_callback_child_acc()
//  ===========================================================================
//
//  Monitoring callback : child directory access changed
//
void
GnomeCmdConnectionTreeview::Model::Iter_monitor_callback_child_acc(
	MonitorData		*	_md,
	eFileAccess			_access,
	const gchar		*   _child_name)
{
	Model				                *   THIS			= NULL;
	GnomeCmdFoldviewTreestore			*   treestore		= NULL;
	GtkTreeModel						*   treemodel		= NULL;

	Row									*   row_parent		= NULL;
	Row									*   row_child		= NULL;

	GtkTreeIter								iter_parent		= Iter_zero;
	GtkTreeIter								iter_child		= Iter_zero;

	const gchar							*   ck_child		= NULL;

	gboolean								was_readable	= FALSE;
	gboolean								is_readable		= FALSE;
	//.........................................................................
	MONITOR_INF("Model::Iter_monitor_callback_child_acc()");

	THIS		= _md->a_model;
	row_parent	= _md->a_row;

	treestore   = THIS->treestore();
	treemodel   = THIS->treemodel();

	Lock();																		// _GDK_LOCK_

	//.........................................................................
	//
	//  Step #1.1 : Get the parent iter
	//
	if ( ! treestore->ext_iter_from_path(row_parent->path(), &iter_parent) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_acc():could not retrieve parent iter [%s]", row_parent->utf8_name_display());
		goto exit;
	}

	//.........................................................................
	//
	//  Step #1.2 : Verifications on parent
	//
	if ( ! THIS->iter_is_expanded(&iter_parent) )
	{
		MONITOR_INF("Model::Iter_monitor_callback_child_acc():parent is not expanded, skipping [%s]", row_parent->utf8_name_display());
		goto exit;
	}

	//.........................................................................
	//
	//  Step #2.1 : Get the child iter + data
	//

	// get the child
	ck_child = g_utf8_collate_key_for_filename(_child_name, -1);

	if ( ! treestore->ext_match_child_str(&iter_parent, &iter_child, ck_child ) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_acc():could not retrieve child iter [%s] of parent iter [%s]", _child_name, row_parent->utf8_name_display());
		g_free((void*)ck_child);
		goto exit;
	}

	g_free((void*)ck_child);

	// get the child's data
	if ( ! treestore->ext_get_data(&iter_child, (GnomeCmdFoldviewTreestore::DataInterface**)&row_child) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_acc():could not retrieve child iter's TreeRow [%s]", _child_name);
		goto exit;
	}

	//.........................................................................
	//
	//  Step #2.2 : verification that child file access has changed
	//
	if ( row_child->access() == _access )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_acc():child access didnt change [%s]", row_child->utf8_name_display());
		goto exit;
	}

	//.........................................................................
	//
	//  Step #3 : Action !

	//  convenience booleans
	was_readable = row_child->readable();
	is_readable	 = Access_readable(_access);

	//  #4.1 readable -> NOT readable
	if ( was_readable && ( !is_readable ) )
	{
		MONITOR_INF("Model::Iter_monitor_callback_child_acc():readable -> NOT readable [%s]", row_child->utf8_name_display());
		//THIS->iter_readable_unset(&iter_child);
		THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgSetNotReadable, row_child->path()) );
	}

	//  #4.2 NOT readable -> readable
	if ( ( !was_readable ) && is_readable )
	{
		MONITOR_INF("Model::Iter_monitor_callback_child_acc():NOT readable -> readable [%s]", row_child->utf8_name_display());
		//THIS->iter_readable_set(&iter_child);
		THIS->control()->message_fifo_push( GCMD_STRUCT_NEW(MsgSetReadable, row_child->path()) );
	}

	//  #4.3 other changes, dont mind & exit

exit:
	Unlock();																	// _GDK_LOCK_
}

//  ###########################################################################
//
//  			                    Lock
//
//  ###########################################################################

GStaticMutex Mutex = G_STATIC_MUTEX_INIT;

//  ===========================================================================
//  Lock()
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::Lock()
{
	//pid_t tid = syscall(SYS_gettid);
	//GCMD_INF("Model:: +++ Lock() +++");
	gdk_threads_enter();
	//g_static_mutex_lock(&Mutex);
	return TRUE;
}


//  ===========================================================================
//  Unlock()
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::Unlock()
{
	//pid_t tid = syscall(SYS_gettid);
	//GCMD_INF("Model:: --- Unlock() ---");
	//g_static_mutex_unlock(&Mutex);
	gdk_threads_leave();
	return TRUE;
}



