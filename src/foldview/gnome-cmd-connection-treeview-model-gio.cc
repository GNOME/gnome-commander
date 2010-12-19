/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyleft      2010-2010 Guillaume Wardavoir

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

    Struct  : many, vfs specific

    Parent  : GnomeCmdConnectionTreeview::Model

    ###########################################################################
*/
#include    "gnome-cmd-connection-treeview.h"

//  ***************************************************************************
//  *								Defines								      *
//  ***************************************************************************

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							GIO structs									  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ***************************************************************************
//
//  						Enumerate children
//
//  ***************************************************************************

//  ===========================================================================
//  GioAsyncEnumerateChildren
//  ===========================================================================
struct GnomeCmdConnectionTreeview::Model::GioAsyncEnumerateChildren : AsyncEnumerateChildren
{
	private:
	GFile			*   d_gfile;
	GCancellable	*   d_gcancellable_1;   // ..._enumerate_children_async
	GCancellable	*   d_gcancellable_2;   // ..._next_files_async

	public:
	void*		operator new	(size_t) ;
	void		operator delete (void*);

				 GioAsyncEnumerateChildren(AsyncCallerData*, const Uri, gint _max_result, gboolean _follow_links) ;
	virtual		~GioAsyncEnumerateChildren();

	public:
	GFile			*   gfile()				{ return d_gfile;			}
	GCancellable	*   gcancellable_1()	{ return d_gcancellable_1;  }
	GCancellable	*   gcancellable_2()	{ return d_gcancellable_2;  }
};

void*
GnomeCmdConnectionTreeview::Model::GioAsyncEnumerateChildren::operator new(size_t _size)
{
	return (void*)(g_try_new0(GioAsyncEnumerateChildren, 1));
}

void
GnomeCmdConnectionTreeview::Model::GioAsyncEnumerateChildren::operator delete (void *_p)
{
	g_free(_p);
}


GnomeCmdConnectionTreeview::Model::GioAsyncEnumerateChildren::GioAsyncEnumerateChildren(
	AsyncCallerData		*		_caller_data,
    const Uri                   _uri,
	gint						_max_result,
	gboolean					_follow_links)
		: AsyncEnumerateChildren(_caller_data, _uri, _max_result, _follow_links)
{
	d_gfile				= g_file_new_for_uri( uri() );
	d_gcancellable_1	= g_cancellable_new();
	d_gcancellable_2	= g_cancellable_new();
}

GnomeCmdConnectionTreeview::Model::GioAsyncEnumerateChildren::~GioAsyncEnumerateChildren()
{
	g_object_unref(d_gcancellable_1);
	g_object_unref(d_gcancellable_2);
}

//  ***************************************************************************
//
//  							Monitoring
//
//  ***************************************************************************

//  ===========================================================================
//  GioMonitorHelper
//  ===========================================================================
//
//  This struct is for g_file_query_info_finish usage simplification
//
struct GnomeCmdConnectionTreeview::Model::GioMonitorHelper
{
	GFileInfo				*		d_g_file_info;
	GFileType						a_type;
	eFileAccess						a_access;
	const gchar				*		a_display_name;
	gboolean						a_is_symlink;
	const gchar				*		a_symlink_target_display_name;
};

//  ===========================================================================
//  GioMonitor
//  ===========================================================================
void*
GnomeCmdConnectionTreeview::Model::GioMonitor::operator new(size_t size)
{
	return (void*)g_try_new0(GioMonitor, 1);
}
void
GnomeCmdConnectionTreeview::Model::GioMonitor::operator delete(void *p)
{
	g_free(p);
}

GnomeCmdConnectionTreeview::Model::GioMonitor::GioMonitor()
{
}
GnomeCmdConnectionTreeview::Model::GioMonitor::~GioMonitor()
{
	MONITOR_INF("Model::GioMonitor::~GioMonitor()");

	g_object_unref(d_file);
	g_object_unref(d_monitor);
}

gboolean
GnomeCmdConnectionTreeview::Model::GioMonitor::monitoring_start(Model* _model, Row* row)
{
	GCancellable			*   cancellable = NULL;
	GError					*   gerror		= NULL;

	MonitorData				*   md			= NULL;
	//.........................................................................

	//	GFileMonitor*   g_file_monitor_directory(
	//		GFile					*   file,
	//		GFileMonitorFlags			flags,
	//		GCancellable			*   cancellable,
	//		GError					**  error);
	//
	//		file		:	input GFile.
	//		flags		:	a set of GFileMonitorFlags.
	//		cancellable :	optional GCancellable object, NULL to ignore.
	//		error		:	a GError, or NULL.
	//		Returns		:	a GFileMonitor for the given file, or NULL on error.

	md = (MonitorData*)g_try_new0(MonitorData,1);
	if ( ! md )
	{
		MONITOR_ERR("Model::GioMonitor::start():g_try_new0 failed for m0");
		return FALSE;
	}
	md->a_model		= _model;
	md->a_row		= row;

	d_file = g_file_new_for_uri(row->uri_utf8());
	if ( ! d_file )
	{
		MONITOR_ERR("Model::GioMonitor::start():Could not create GFile for uri %s", row->uri_utf8());
		return FALSE;
	}

	d_monitor = g_file_monitor_directory(
					d_file,
					G_FILE_MONITOR_NONE,
					cancellable,			// NULL, since we are monothreaded
					&gerror);

	if ( ! d_monitor )
	{
		MONITOR_ERR("Model::GioMonitor::start():Failure [%s] %s", row->uri_utf8(), gerror->message);
		return FALSE;
	}

	a_signal_handle = g_signal_connect(d_monitor, "changed", G_CALLBACK(GnomeCmdConnectionTreeview::Model::GIO::Monitor_callback), (gpointer)md);
	if ( ! a_signal_handle )
	{
		MONITOR_ERR("Model::GioMonitor::start():Could not connect signal [%s]", row->uri_utf8());
		return FALSE;
	}
	else
	{
		MONITOR_INF("Model::GioMonitor::start():Signal handle [%s] [0x%16x]", row->uri_utf8(), a_signal_handle);
	}

	MONITOR_INF("Model::GioMonitor::start():Success [%s] [%s]", row->path()->dump(), row->uri_utf8());

	Monitor::a_started = TRUE;

	return TRUE;
}
gboolean
GnomeCmdConnectionTreeview::Model::GioMonitor::monitoring_stop()
{
	if ( !g_file_monitor_cancel(d_monitor ))
	{
		MONITOR_ERR("Model::GioMonitor::stop():Failed");
		return FALSE;
	}

	MONITOR_INF("Model::GioMonitor::stop():Success");

	Monitor::a_started = FALSE;

	return TRUE;
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							Model Wrappers				                  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ***************************************************************************
//
//  						Enumerate children
//
//  ***************************************************************************

//  ===========================================================================
//  GIO::iter_check_if_empty
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::GIO::iter_check_if_empty(
	AsyncCallerData  *  _acd,
    const Uri           _uri)
{
	GioAsyncEnumerateChildren   *   gaec				= NULL;
	//.........................................................................
	gaec = new GioAsyncEnumerateChildren(_acd, _uri, 1, TRUE);


	CHECK_INF("Model::GIO::iter_check_if_empty:%s", gaec->uri());

	g_file_enumerate_children_async(
		gaec->gfile(),
		"standard::display-name,standard::type,standard::is-symlink,standard::symlink-target,access::can-read,access::can-write",
		G_FILE_QUERY_INFO_NONE,
		G_PRIORITY_DEFAULT,
		NULL,
		Iter_enumerate_children_callback_1,
		(gpointer)gaec);
}

//  ===========================================================================
//  GIO::iter_expand
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::GIO::iter_enumerate_children(
	AsyncCallerData  *  _acd,
    const Uri           _uri)
{
	GioAsyncEnumerateChildren   *   gaec				= NULL;
	//.........................................................................
	gaec = new GioAsyncEnumerateChildren(_acd, _uri, -1, TRUE);

	//  void g_file_enumerate_children_async(
	//	  GFile					*   file,
	//	  const char			*   attributes,
	//	  GFileQueryInfoFlags		flags,
	//	  int						io_priority,
	//	  GCancellable			*   cancellable,
	//	  GAsyncReadyCallback		callback,
	//	  gpointer					user_data)
	//
	//  file		: input GFile.
	//  attributes  : an attribute query string.
	//  flags		: a set of GFileQueryInfoFlags.
	//  io_priority : the I/O priority of the request.
	//  cancellable : optional GCancellable object, NULL to ignore.
	//  callback	: a GAsyncReadyCallback to call when the request is satisfied
	//  user_data   : the data to pass to callback function
	g_file_enumerate_children_async(
		gaec->gfile(),
		"standard::display-name,standard::type,standard::is-symlink,standard::symlink-target,access::can-read,access::can-write",
		G_FILE_QUERY_INFO_NONE,
		G_PRIORITY_DEFAULT,
		NULL,
		Iter_enumerate_children_callback_1,
		(gpointer)gaec);
}

//=============================================================================
//  GnomeVFS::iter_file_info
//=============================================================================
void
GnomeCmdConnectionTreeview::Model::GIO::iter_get_file_info(
	AsyncCallerData  *  _acd,
    const Uri           _uri)
{
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							GIO methods									  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ***************************************************************************
//
//  						Enumerate children
//
//  ***************************************************************************

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//	  Enumerate children : GIO callback 1
//
//	  Here we get a GFileEnumerator, on which we have to call another
//	  async function to get GFileInfos.
//
//	  We init the request, and in the callback, this call we be done again
//	  and again, until all GFileInfos have been get
//
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GIO::Iter_enumerate_children_callback_1(
	GObject			*   _source_object,
	GAsyncResult	*   _res,
	gpointer			_data)
{
	GError						*	   gerror   = NULL;
	GFileEnumerator				*	   gfe		= NULL;
	//.........................................................................

	// finish async query
	gfe = g_file_enumerate_children_finish((GFile*)_source_object, _res, &gerror);
	if ( !gfe )
	{
		ENUMERATE_ERR("Model::GIO::ecc_1():g_file_enumerate_children_finish failed [%s]", gerror->message);
		return;
	}

	// get the GFileInfos :

	//	void g_file_enumerator_next_files_async(
	//	  GFileEnumerator		*   enumerator,
	//		int						num_files,
	//		int						io_priority,
	//		GCancellable		*   cancellable,
	//		GAsyncReadyCallback		callback,
	//		gpointer				user_data);
	//
	//	enumerator  :	a GFileEnumerator.
	//	num_files   :	the number of file info objects to request
	//	io_priority :	the io priority of the request.
	//	cancellable :	optional GCancellable object, NULL to ignore.
	//	callback	:	a GAsyncReadyCallback to call when the request is satisfied
	//	user_data   :	the data to pass to callback function

	g_file_enumerator_next_files_async(
		gfe,
		10,
		G_PRIORITY_DEFAULT,
		NULL,
		Iter_enumerate_children_callback_2,
		_data);
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Enumerate children : GIO callback 2
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GIO::Iter_enumerate_children_callback_2(
	GObject			*   _source_object,
	GAsyncResult	*   _res,
	gpointer			_data)
{
	GioAsyncEnumerateChildren   *		gaec	= NULL;
	AsyncCallerData				*		acd		= NULL;

	GError						*		gerror  = NULL;
	GList						*		list	= NULL;

	gint								count						= 0;
	gint								added						= 0;
	GFileInfo					*		file_info					= NULL;
	GFileType							file_type					= G_FILE_TYPE_UNKNOWN;
	const gchar					*		file_display_name			= NULL;
	const gchar					*		file_display_name_ck		= NULL;
	gboolean							b_read;
	gboolean							b_write;
	eFileAccess							file_access					= eAccessUN;
	gboolean							file_is_symlink				= FALSE;
	const gchar					*		file_symlink_target_name	= NULL;

	File						*		file				= NULL;
	//.........................................................................

	gaec	= (GioAsyncEnumerateChildren*)_data;
	acd		= (AsyncCallerData*)(gaec->caller_data());

	//.........................................................................
	// finish query
	list = g_file_enumerator_next_files_finish((GFileEnumerator*)_source_object, _res, &gerror);
	if ( ! list )
	{
		// end of enumeration
		if ( ! gerror )
			goto lab_eof;

		// error
		ENUMERATE_ERR("Model::GIO::ecc_2():g_file_enumerator_next_files_finish failed [%s]", gerror->message);
			goto lab_error;
	}

	//.........................................................................
	// init loop
	count   = 0;
	list	= g_list_first(list);

	//.........................................................................
	// lOOooOOp
lab_loop:

	count++;

	// get infos
	file_info	= (GFileInfo*)(list->data);

	file_display_name		= g_file_info_get_attribute_string(file_info, "standard::display-name");
	if ( file_display_name == NULL )
	{
		ENUMERATE_ERR("Model::GIO::ecc_2():g_file_info_get_attribute_string (standard::display_name) failed");
		goto lab_error;
	}
	file_display_name_ck	= g_utf8_collate_key(file_display_name, -1);
	//ENUMERATE_INF("Model:ecc_2():%s", file_display_name);

	file_type = (GFileType)g_file_info_get_attribute_uint32(file_info, "standard::type");
	if ( file_type == 0 )
	{
		ENUMERATE_ERR("Model::GIO::ecc_2():g_file_info_get_attribute_uint32 (standard::type) failed");
		goto lab_error;
	}

	b_read		= g_file_info_get_attribute_boolean(file_info, "access::can-read");
	b_write		= g_file_info_get_attribute_boolean(file_info, "access::can-write");
	file_access = Access_from_read_write(b_read, b_write);

	file_is_symlink				= g_file_info_get_attribute_boolean(file_info, "standard::is-symlink");
	file_symlink_target_name	= g_file_info_get_attribute_byte_string(file_info, "standard::symlink-target");


	switch ( file_type )
	{
		//.....................................................................
		case G_FILE_TYPE_SYMBOLIC_LINK:

		// if follow_links is TRUE, we should not be here - except if we have
		// a BROKEN LINK. We thus cant know if the link was pointing on a
		// directory or something else. LNK struct handle broken links
		// if you pass NULL as the file pointer in constructor.
		if ( gaec->follow_links() )
		{
			ENUMERATE_WNG("Model::GIO::ecc_2():[%03i][%03i] - broken symlink, ignored [%s]", count, added, file_display_name);
			break;
		}

		//ENUMERATE_INF("Model:ecc_2():[%03i][%03i] - symlink, ignored [%s]", count, added, file_display_name);

		//added++;
		//ALSC_INF("alsc:[%03i][0x%16x] [%03i][%03i][%03i] S<%s>", ga->hi(), l, count, added, ga->mr(), info->name);
		//lnk = new LNK(g_strdup(info->name), g_strdup(info->symlink_name), info->permissions, info->flags );
		//ls->append( (gvfs_file*)lnk );
		// if the caller want partial listing
		//if  (
		//		( ga->mr()  >= 0		)  &&
		//		( ls->len() >= ga->mr() )
		//	)
		//	goto lab_abort;

		break;

		//.....................................................................
		case G_FILE_TYPE_DIRECTORY:

		if ( strcmp(GnomeCmdConnectionTreeview::Collate_key_dot,	file_display_name_ck ) &&
			 strcmp(GnomeCmdConnectionTreeview::Collate_key_dotdot,	file_display_name_ck ))
		{
			added++;

			ENUMERATE_INF("Model::GIO::ecc_2():[%03i][%03i] +[%s]", count, added, file_display_name);

			// it is a symlink pointing on  a directory
			if ( file_is_symlink )
				file = new FoldviewLink(file_display_name, file_symlink_target_name, file_access);
			// it is a directory
			else
				file = new FoldviewDir(file_display_name, file_access);

			gaec->list_append( file );

			// if the caller want partial listing
			if  (
					( gaec->max_result()	>= 0					)  &&
					( gaec->list_card()		>= gaec->max_result()   )
				)
				goto lab_partial_listing;
		}
		break;

		//.....................................................................
		default:
		ENUMERATE_INF("Model::GIO::ecc_2():[%03i][%03i]  [%s]", count, added, file_display_name);
	}

	list = g_list_next(list);
	if ( ! list )
		goto lab_no_more_entry;

	goto lab_loop;

//.............................................................................
lab_eof:

	ENUMERATE_INF("Model::GIO::ecc_2():EOF");
	// Call caller callback ; caller is responsible for freeing memory
	// eventually allocated in m_user_data member
	// of the struct_gvfs_caller_data
	gaec->caller_data()->callback()(gaec);

	goto lab_end;

//.............................................................................
lab_no_more_entry:

	ENUMERATE_INF("Model::GIO::ecc_2():NO MORE ENTRY - relaunching");
	// relaunch same request
	g_file_enumerator_next_files_async(
		(GFileEnumerator*)_source_object,
		10,
		G_PRIORITY_DEFAULT,
		NULL,
		Iter_enumerate_children_callback_2,
		_data);

	g_free((void*)file_display_name_ck);
	return;

//.............................................................................
lab_partial_listing:

	ENUMERATE_INF("Model::GIO::ecc_2():PARTIAL LISTING REACHED [%03i]", gaec->max_result());
	// Call caller callback ; caller is responsible for freeing memory
	// eventually allocated in m_user_data member
	// of the struct_gvfs_caller_data
	gaec->caller_data()->callback()(gaec);

	//goto lab_end;

//.............................................................................
lab_error:
lab_end:

	// delete the GioAsyncEnumerateChildren
	delete gaec;

	if ( file_display_name_ck )
		g_free((void*)file_display_name_ck);

	return;
}

//  ***************************************************************************
//
//  							Monitoring
//
//  ***************************************************************************

//  ===========================================================================
//  Monitoring : static callback
//  ===========================================================================
void
GnomeCmdConnectionTreeview::Model::GIO::Monitor_callback(
	GFileMonitor		*   _monitor,
	GFile				*   _file,
	GFile				*   _other_file,
	GFileMonitorEvent		_event_type,
	gpointer				_data)
{
	MonitorData				*   md				= NULL;

	GAsyncReadyCallback			callback		= NULL;
	GFile					*   gfile_to_scan   = NULL;
	gboolean					b_self			= FALSE;
	//.........................................................................
	//  G_FILE_MONITOR_EVENT_CHANGED
	//  G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT
	//  G_FILE_MONITOR_EVENT_DELETED
	//  G_FILE_MONITOR_EVENT_CREATED
	//  G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED
	//  G_FILE_MONITOR_EVENT_PRE_UNMOUNT
	//  G_FILE_MONITOR_EVENT_UNMOUNTED
	//.........................................................................

	//MONITOR_INF("Model::GIO::Monitor_callback():EVENT");

	md = (MonitorData*)_data;
	g_return_if_fail( md );

	b_self = ( _file == _other_file ? TRUE : FALSE );

	switch ( _event_type )
	{
		//.....................................................................
		// file created in directory
		// chmod, ... on directory
		case G_FILE_MONITOR_EVENT_CHANGED:
		MONITOR_INF("Model::GIO::Monitor_callback():EVENT_CHANGED [%s]", md->a_row->utf8_name_display());

		// file created : exit
		if ( ! b_self )
			goto abort;

		// chmod, ...
		callback		= Monitor_callback_acc;
		gfile_to_scan   = _file;
		break;

		//.....................................................................
		// file / subdirectory  deleted in directory
		//
		//  _other_file is always NULL ; the file that was deleted is _file.
		//

		case G_FILE_MONITOR_EVENT_DELETED:

		MONITOR_INF("Model::GIO::Monitor_callback():EVENT_DELETED [%s] [%s]", md->a_row->utf8_name_display(), g_file_get_basename(_file));


		if ( b_self )
		{
			// Parent deleted :
			//MONITOR_INF("Model::GIO::Monitor_callback():EVENT_DELETED (SELF)", md->a_row->utf8_name_display());
			//Monitor_callback_del(md);
			goto exit;
		}
		else
		{
			// Child deleted :
			// not getting file info, since the file is...deleted
			//MONITOR_INF("Model::GIO::Monitor_callback():EVENT_DELETED (CHILD) [%s] [0x%016x]", md->a_row->utf8_name_display(), _other_file);
			//Monitor_callback_child_del(md);
			goto exit;
		}
		break;

		//.....................................................................
		// subdirectory created in directory
		case G_FILE_MONITOR_EVENT_CREATED:
		MONITOR_INF("Model::GIO::Monitor_callback():EVENT_CREATED [%s]", md->a_row->utf8_name_display());
		callback		= Monitor_callback_child_new;
		gfile_to_scan   = _file;
		break;

		//.....................................................................
		case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
		MONITOR_INF("Model::GIO::Monitor_callback():ATTRIBUTE_CHANGED [%s]", md->a_row->utf8_name_display());
		callback		= Monitor_callback_child_acc;
		gfile_to_scan   = _other_file;
		goto exit;
		break;

		case G_FILE_MONITOR_EVENT_PRE_UNMOUNT		:
		case G_FILE_MONITOR_EVENT_UNMOUNTED			:
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT :
		MONITOR_INF("Model::GIO::Monitor_callback():OTHER_EVENT [%s]", md->a_row->utf8_name_display());
		goto abort;
	}

	//  void g_file_query_info_async(
	//		GFile					*   file,
	//		const char				*   attributes,
	//		GFileQueryInfoFlags			flags,
	//		int							io_priority,
	//		GCancellable			*   cancellable,
	//		GAsyncReadyCallback			callback,
	//		gpointer					user_data);
	//
	//  file			:	input GFile.
	//  attributes		:	an attribute query string.
	//  flags			:	a set of GFileQueryInfoFlags.
	//  io_priority		:	the I/O priority of the request.
	//  cancellable		:	optional GCancellable object, NULL to ignore.
	//  callback		:	a GAsyncReadyCallback to call when the request is satisfied
	//  user_data		:	the data to pass to callback function
	//
	//  Asynchronously gets the requested information about specified file.
	//  The result is a GFileInfo object that contains key-value attributes
	//  (such as type or size for the file). For more details, see
	//  g_file_query_info() which is the synchronous version of this call.
	//  When the operation is finished, callback will be called. You can then
	//  call g_file_query_info_finish() to get the result of the operation.

	g_file_query_info_async(
		gfile_to_scan,
		"standard::type,standard::display-name,standard::is-symlink,standard::symlink-target,access::*",
		G_FILE_QUERY_INFO_NONE,
		G_PRIORITY_DEFAULT,
		NULL,
		callback,
		(gpointer)md);

	return;

abort:
exit:
	return;
}

//  ===========================================================================
//  Monitoring : query_info static callback
//  ===========================================================================

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitoring : query_info static callback helper
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gboolean
GnomeCmdConnectionTreeview::Model::GIO::Monitor_callback_helper_query_info(
	const gchar			*		_event_str,
	GObject				*		_source_object,
	GAsyncResult		*		_res,
	gpointer					_data,
	MonitorData			*		_md,
	GioMonitorHelper	*		_gmh)
{
	GError			*	   gerror = NULL;
	//.........................................................................
	_gmh->d_g_file_info					=   NULL;
	_gmh->a_type						=   G_FILE_TYPE_UNKNOWN;
	_gmh->a_access						=   eAccessUN;
	_gmh->a_display_name				=   NULL;
	_gmh->a_is_symlink					=   FALSE;
	_gmh->a_symlink_target_display_name	=   NULL;

	//	GFileInfo*
	//	g_file_query_info_finish(
	//		GFile			*   file,
	//		GAsyncResult	*   res,
	//		GError			**  error)
	//
	//	Finishes an asynchronous file info query.
	//
	//	file	:	input GFile.
	//	res		:	a GAsyncResult.
	//	error   :	a GError.
	//	Returns :	GFileInfo for given file or NULL on error. Free the returned
	//				object with g_object_unref().
	_gmh->d_g_file_info = g_file_query_info_finish((GFile*)_source_object, _res, &gerror);
	if ( ! _gmh->d_g_file_info )
	{
		MONITOR_ERR("Model::GIO::Monitor_callback_%s():g_file_query_info_finish failed - %s [%s]", _event_str, gerror->message, _md->a_row->uri_utf8());
		return FALSE;
	}

	// file type
	_gmh->a_type = (GFileType)g_file_info_get_attribute_uint32(_gmh->d_g_file_info, "standard::type");
	if ( _gmh->a_type == 0 )
	{
		MONITOR_ERR("Model::GIO::Monitor_callback_%s():g_file_info_get_attribute_uint32 (standard::type) failed - %s [%s]", _event_str, gerror->message, _md->a_row->uri_utf8());
		return FALSE;
	}

	// eFileAccess
	gboolean		b_read	= g_file_info_get_attribute_boolean(_gmh->d_g_file_info, "access::can-read");
	gboolean		b_write	= g_file_info_get_attribute_boolean(_gmh->d_g_file_info, "access::can-write");
	_gmh->a_access			= Access_from_read_write(b_read, b_write);

	// display name
	_gmh->a_display_name = g_file_info_get_attribute_string(_gmh->d_g_file_info, "standard::display-name");
	if ( _gmh->a_display_name == NULL )
	{
		MONITOR_ERR("Model::GIO::Monitor_callback_%s():g_file_info_get_attribute_string (standard::display-name) failed - %s [%s]", _event_str, gerror->message, _md->a_row->uri_utf8());
		return FALSE;
	}

	// symlink ?
	_gmh->a_is_symlink = g_file_info_get_attribute_boolean(_gmh->d_g_file_info, "standard::is-symlink");

	if ( _gmh->a_is_symlink )
	{
		_gmh->a_symlink_target_display_name = g_file_info_get_attribute_string(_gmh->d_g_file_info, "standard::symlink-target");
		if ( _gmh->a_symlink_target_display_name == NULL )
		{
			MONITOR_ERR("Model::GIO::Monitor_callback_%s():g_file_info_get_attribute_string (standard::symlink-target) failed - %s [%s]", _event_str, gerror->message, _md->a_row->uri_utf8());
			return FALSE;
		}
	}

	return TRUE;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitoring : Monitor_callback_del
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GIO::Monitor_callback_del(
	MonitorData		*	_md)
{
	MONITOR_INF("Model::GIO::Monitor_callback_del()");

	Iter_monitor_callback_del(_md);

	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitoring : Monitor_callback_child_del
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GIO::Monitor_callback_child_del(
	MonitorData		*	_md)
{
	gchar * name_debug = _("a child directory");
	//.........................................................................
	MONITOR_INF("Model::GIO::Monitor_callback_child_del()");

	Iter_monitor_callback_child_del(_md, name_debug);

	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitoring : Monitor_callback_child_new
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GIO::Monitor_callback_child_new(
	GObject			*   _source_object,
	GAsyncResult	*   _res,
	gpointer			_data)
{
	MonitorData				*   md			= NULL;
	GioMonitorHelper			gmh			= {NULL, G_FILE_TYPE_UNKNOWN, eAccessUN, NULL, FALSE, NULL};
	File					*   child		= NULL;
	//.........................................................................
	MONITOR_INF("Model::GIO::Monitor_callback_child_new()");

	md = (MonitorData*)_data;
	if ( !md )
	{
		MONITOR_ERR("  MonitorData is NULL");
		goto exit;
	}

	if ( ! Monitor_callback_helper_query_info("child_new", _source_object, _res, _data, md, &gmh) )
		goto exit;

	if ( gmh.a_type != G_FILE_TYPE_DIRECTORY )
	{
		MONITOR_INF("  Type is not G_FILE_TYPE_DIRECTORY, ignored");
		goto exit;
	}

	child = new Directory(gmh.a_display_name, eAccessRW);

	Iter_monitor_callback_child_new(md, child);

	delete child;

exit:

	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitoring : Monitor_callback_acc
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GIO::Monitor_callback_acc(
	GObject			*   _source_object,
	GAsyncResult	*   _res,
	gpointer			_data)
{
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitoring : Monitor_callback_child_acc
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GIO::Monitor_callback_child_acc(
	GObject			*   _source_object,
	GAsyncResult	*   _res,
	gpointer			_data)
{
}




















