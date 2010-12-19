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
#define SVFSRESULT(result) gnome_vfs_result_to_string(result)

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #								Divers					  				  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
GnomeCmdConnectionTreeview::eFileAccess
GnomeCmdConnectionTreeview::Model::GnomeVFS::Access_from_GnomeVFSFilePermissions(
	GnomeVFSFilePermissions _permissions)
{
	return Access_from_read_write
			(
				( ( _permissions & GNOME_VFS_PERM_ACCESS_READABLE ) != 0 ),
				( ( _permissions & GNOME_VFS_PERM_ACCESS_WRITABLE ) != 0 )
			);
}

GnomeCmdConnectionTreeview::eFileType
GnomeCmdConnectionTreeview::Model::GnomeVFS::Type_from_GnomeVFSFileType(
	GnomeVFSFileType _type)
{
	eFileType		type	= eTypeUnknown;

	if ( _type == GNOME_VFS_FILE_TYPE_DIRECTORY )
		type = GnomeCmdConnectionTreeview::eTypeDirectory;

	if ( _type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK )
		type = GnomeCmdConnectionTreeview::eTypeSymlink;

	return type;
}

GnomeCmdConnectionTreeview::eFileError
GnomeCmdConnectionTreeview::Model::GnomeVFS::Error_from_GnomeVFSResult(
	GnomeVFSResult _result)
{
    switch ( _result )
    {
        case    GNOME_VFS_ERROR_NOT_FOUND               : return eErrorFileNotFound;
        case    GNOME_VFS_ERROR_ACCESS_DENIED           : return eErrorAccessDenied;
        case    GNOME_VFS_ERROR_CANCELLED               : return eErrorCancelled;
        case    GNOME_VFS_ERROR_TIMEOUT                 : return eErrorTimeout;
        default                                         : return eErrorOther;
    }
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							GnomeVFS structs							  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ***************************************************************************
//
//                          GnomeVFSAsyncTpl <>
//
//  ***************************************************************************
//
//  Because
//
//      GnomeVFSAsyncEnumerateChildren
//
//  and
//
//      GnomeVFSAsyncGetFileInfo
//
//  have no differences in their GnomeVFS members / fields, we use a template
//  to add these.
//
template <typename T> struct GnomeCmdConnectionTreeview::Model::GnomeVFSAsyncTpl : public T
{
	private:
	GnomeVFSURI					*   d_gnomeVFS_uri;
	GnomeVFSAsyncHandle			*	a_handle;

	public:
                 // constructor for GnomeVFSAsyncTpl<GnomeVFSAsyncGetFileInfo>
				 GnomeVFSAsyncTpl(AsyncCallerData*, const Uri);
                 // constructor for GnomeVFSAsyncTpl<GnomeVFSAsyncEnumerateChildren>
				 GnomeVFSAsyncTpl(AsyncCallerData*, const Uri, gint _max_result, gboolean _follow_links);

	virtual		~GnomeVFSAsyncTpl();

	void*		operator new	(size_t) ;
	void		operator delete (void*);

	public:
	GnomeVFSURI				*			gnomevfs_uri()		{ return d_gnomeVFS_uri;				}
	GnomeVFSAsyncHandle		*			handle()			{ return a_handle;						}
	GnomeVFSAsyncHandle		**			handle_ptr()		{ return &a_handle;						}
	void								cancel_async_op()   { gnome_vfs_async_cancel(handle());		}
};

template <typename T>
void*
GnomeCmdConnectionTreeview::Model::GnomeVFSAsyncTpl<T>::operator new(size_t _size)
{
	return (void*)(g_try_new0(GnomeVFSAsyncTpl<T>, 1));
}

template <typename T>
void
GnomeCmdConnectionTreeview::Model::GnomeVFSAsyncTpl<T>::operator delete (void *_p)
{
	g_free(_p);
}

template <typename T>
GnomeCmdConnectionTreeview::Model::GnomeVFSAsyncTpl<T>::~GnomeVFSAsyncTpl()
{
    gnome_vfs_uri_unref(d_gnomeVFS_uri);
}


//  ***************************************************************************
//
//                          Get File Info
//
//      ( GnomeVFSAsyncGetFileInfo = GnomeVFSAsyncTpl<AsyncGetFileInfo> )
//
//  ***************************************************************************
//
//  Has been declared as specified-template in gnome-cmd-foldview-private.h
//  Only need to code constructor => templates rock !
//

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Constructor for GnomeVFSAsyncTpl<AsyncGetFileInfo>
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template <typename T>
GnomeCmdConnectionTreeview::Model::GnomeVFSAsyncTpl<T>::GnomeVFSAsyncTpl(
    AsyncCallerData*    _acd,
    const Uri           _uri) : T(_acd, _uri)
{
	d_gnomeVFS_uri  = gnome_vfs_uri_new(_uri);
    a_handle        = NULL;
}
//  ***************************************************************************
//
//  						Enumerate children
//
//  ( GnomeVFSAsyncEnumerateChildren = GnomeVFSAsyncTpl<AsyncEnumerateChildren> )
//
//  ***************************************************************************
//
//  Has been declared as specified-template in gnome-cmd-foldview-private.h
//  Only need to code constructor => templates rock !
//

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Constructor for GnomeVFSAsyncTpl<AsyncEnumerateChildren>
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template <typename T>
GnomeCmdConnectionTreeview::Model::GnomeVFSAsyncTpl<T>::GnomeVFSAsyncTpl(
    AsyncCallerData*    _acd,
    const Uri           _uri,
	gint				_max_result,
	gboolean			_follow_links) : T(_acd, _uri, _max_result, _follow_links)
{
	d_gnomeVFS_uri  = gnome_vfs_uri_new(_uri);
    a_handle        = NULL;
}

//  ***************************************************************************
//
//  							Monitoring
//
//  ***************************************************************************

//  ===========================================================================
//  GnomeVFSMonitor
//  ===========================================================================
void*
GnomeCmdConnectionTreeview::Model::GnomeVFSMonitor::operator new(size_t size)
{
	return (void*)g_try_new0(GnomeVFSMonitor, 1);
}
void
GnomeCmdConnectionTreeview::Model::GnomeVFSMonitor::operator delete(void *p)
{
	g_free(p);
}

GnomeCmdConnectionTreeview::Model::GnomeVFSMonitor::GnomeVFSMonitor()
{
	a_monitor_handle = 0;
}
GnomeCmdConnectionTreeview::Model::GnomeVFSMonitor::~GnomeVFSMonitor()
{
	MONITOR_INF("Model::GnomeVFSMonitor::~GnomeVFSMonitor()");
}

gboolean
GnomeCmdConnectionTreeview::Model::GnomeVFSMonitor::monitoring_start(Model* _model, Row *row)
{
	GnomeVFSResult				result		= GNOME_VFS_OK;
	MonitorData				*   md			= NULL;
	//.........................................................................


	//  GnomeVFSResult gnome_vfs_monitor_add(
	//	  GnomeVFSMonitorHandle **		handle,
	//	  const gchar			*		text_uri,
	//	  GnomeVFSMonitorType			monitor_type,
	//	  GnomeVFSMonitorCallback		callback,
	//	  gpointer						user_data);
	//
	//	Watch the file or directory at text_uri for changes (or the creation/
	//	deletion of the file) and call callback when there is a change. If a
	//	directory monitor is added, callback is notified when any file in
	//	the directory changes.
	//
	//	handle			: 	after the call, handle will be a pointer to an operation handle.
	//	text_uri		: 	string representing the uri to monitor.
	//	monitor_type	: 	add a directory or file monitor.
	//	callback		: 	function to call when the monitor is tripped.
	//	user_data		: 	data to pass to callback.
	//	Returns			: 	an integer representing the result of the operation.

	md = (MonitorData*)g_try_new0(MonitorData,1);
	if ( ! md )
	{
		MONITOR_ERR("Model::GnomeVFSMonitor::start():g_try_new0 failed for MonitorData");
		return FALSE;
	}
	md->a_model		= _model;
	md->a_row		= row;

	result = gnome_vfs_monitor_add(
				monitor_handle_ptr(),
				row->uri_utf8(),
				GNOME_VFS_MONITOR_DIRECTORY,
				(GnomeVFSMonitorCallback)GnomeCmdConnectionTreeview::Model::GnomeVFS::Monitor_callback,
				(gpointer)md);

	if ( result != GNOME_VFS_OK )
	{
		MONITOR_ERR("Model::GnomeVFSMonitor::start():[%s]", SVFSRESULT(result));
		return FALSE;
	}

	MONITOR_INF("Model::GnomeVFSMonitor::start():Success [%s] [%s]", row->path()->dump(), row->uri_utf8());

	Monitor::a_started = TRUE;

	return TRUE;
}
gboolean
GnomeCmdConnectionTreeview::Model::GnomeVFSMonitor::monitoring_stop()
{
	GnomeVFSResult  result  = GNOME_VFS_OK;
	//.........................................................................
	result = gnome_vfs_monitor_cancel(monitor_handle());

	if ( result != GNOME_VFS_OK )
	{
		MONITOR_ERR("Model::GnomeVFSMonitor::stop():[%s]", SVFSRESULT(result));
		return FALSE;
	}

	MONITOR_INF("Model::GnomeVFSMonitor::stop():Success");

	Monitor::a_started = FALSE;

	return TRUE;
}



//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							GnomeVFS methods							  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################


//  ***************************************************************************
//
//  						Get file info
//
//  ***************************************************************************

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	  Model::GnomeVFS::Iter_get_file_info_callback()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::Iter_get_file_info_callback(
    GnomeVFSAsyncHandle     *   _handle,
    GList                   *   _results,
    gpointer                    _data)
{
    GcmdStruct<GnomeVFSAsyncGetFileInfo>    *   gfi             = NULL;
	GList								    *   list			= NULL;
	GnomeVFSGetFileInfoResult			    *   result			= NULL;
	GnomeVFSFileInfo					    *   info			= NULL;

	//.........................................................................
	// #warning [*] Model::GnomeVFS::GnomeVFS::igfic() : GList delete ? NO !
	//REFRESH_INF("Model::GnomeVFS::GnomeVFS::igfic()");

	gfi =  (GcmdStruct<GnomeVFSAsyncGetFileInfo>*)_data;
	g_return_if_fail( gfi );

	list = g_list_first(_results);
	if ( ! list )
	{
		REFRESH_ERR("Model::GnomeVFS::GnomeVFS::igfic():empty result list");
		goto lab_exit_false;
	}

	//  GnomeVFSGetFileInfoResult :
	//
	//	  GnomeVFSURI		*   uri;
	//	  GnomeVFSResult		result;
	//	  GnomeVFSFileInfo  *   file_info;
	//
	result = (GnomeVFSGetFileInfoResult*)(list->data);

	if ( result->result != GNOME_VFS_OK )
	{
		REFRESH_ERR("Model::GnomeVFS::GnomeVFS::igfic():async call failure [%s]", SVFSRESULT(result->result));
        gfi->error_set(Error_from_GnomeVFSResult(result->result), gnome_vfs_result_to_string(result->result));
        goto lab_call_caller;
	}

	info = (GnomeVFSFileInfo*)(result->file_info);
	if ( ! info )
	{
		REFRESH_ERR("Model::GnomeVFS::GnomeVFS::igfic():file info is NULL");
        gfi->error_set(Error_from_GnomeVFSResult(result->result), gnome_vfs_result_to_string(result->result));
        goto lab_call_caller;
	}

    gfi->a_name         = info->name;
    gfi->a_is_symlink	= GNOME_VFS_FILE_INFO_SYMLINK(info);
    gfi->a_symlink_name	= info->symlink_name;
    gfi->a_type         = Type_from_GnomeVFSFileType(info->type);
    gfi->a_access       = Access_from_GnomeVFSFilePermissions(info->permissions);

lab_call_caller:
	(gfi->caller_data()->callback())(gfi);

lab_exit_true:
    delete gfi;
    return;

lab_exit_false:
    delete gfi;
	return;

}

//  ***************************************************************************
//
//  						Enumerate children
//
//  ***************************************************************************

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	  Model::GnomeVFS::Iter_enumerate_children_callback()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::Iter_enumerate_children_callback(
	GnomeVFSAsyncHandle		*   _handle,
	GnomeVFSResult				_result,
	GList					*   _list,
	guint						_entries_read,
	gpointer					_data)
{
	GnomeVFSFileInfo					*   info						= NULL;

	GList								*   l							= NULL;
	guint									count						= 0;
	guint									added						= 0;

	GnomeVFSAsyncEnumerateChildren		*   vaec						= NULL;

	FoldviewFile						*   file						= NULL;
	const gchar							*	file_display_name			= NULL;
	const gchar							*	file_display_name_ck		= NULL;
	gboolean								b_read;
	gboolean								b_write;
	eFileAccess								file_access					= eAccessUN;
	gboolean								file_has_flag_symlink		= FALSE;
	const gchar							*	file_symlink_target_name	= NULL;
	//.........................................................................

	vaec  =   (GnomeVFSAsyncEnumerateChildren*)(_data);

	// counter
	count   = 0;

	// handle the '0-entry-case'
	if ( _entries_read == 0 )
	{
		ENUMERATE_INF("alsc:[handle:%03i] entries:%03i name:%s, parent:%s", vaec->handle(), _entries_read, "no_more_entry" , vaec->uri());
		goto lab_no_more_entry;
	}

	// init loop - we have at least one entry
	l = g_list_first(_list);

lab_loop:

	b_read = b_write = FALSE;

	count++;

	info	= (GnomeVFSFileInfo*)(l->data);

	ENUMERATE_INF("alsc:[handle:%03i] entries:%03i name[%s] parent[%s]", vaec->handle(), _entries_read,  info->name, vaec->uri());

	file_display_name		= info->name;
	file_display_name_ck	= g_utf8_collate_key(file_display_name, -1);

	if ( ( info->permissions & GNOME_VFS_PERM_ACCESS_READABLE ) != 0 )	b_read  = TRUE;
	if ( ( info->permissions & GNOME_VFS_PERM_ACCESS_WRITABLE ) != 0 )	b_write = TRUE;
	file_access = Access_from_read_write(b_read,b_write);

	file_has_flag_symlink		= ( ( info->flags & GNOME_VFS_FILE_FLAGS_SYMLINK ) != 0 );
	file_symlink_target_name	= info->symlink_name;

	//ENUMERATE_INF("alsc:[%03i] entry name:%s", vaec->handle(), file_display_name);

	switch ( info->type )
	{
		//.....................................................................
		case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:

        g_free((void*)file_display_name_ck);

		// if follow_links is TRUE, we should not be here - except if we have
		// a BROKEN LINK. We thus cant know if the link was pointing on a
		// directory or something else. LNK struct handle broken links
		// if you pass NULL as the file pointer in constructor.
		if ( vaec->follow_links() )
		{
			ENUMERATE_WNG("alsc:[%03i][0x%16x] [%03i][%03i][%03i] [broken link, ignored]<%s>", vaec->handle(), l, count, added, vaec->max_result(), file_display_name);
			//lnk = new LNK(g_strdup(info->name), ,NULL);
			break;
		}

		//ENUMERATE_INF("alsc:[%03i][0x%16x] [%03i][%03i][%03i]  Symbolic link, ignored <%s>", vaec->handle(), l, count, added, vaec->max_result(), file_display_name);


		//added++;

		//ALSC_INF("alsc:[%03i][0x%16x] [%03i][%03i][%03i] S<%s>", vaec->handle(), l, count, added, vaec->max_result(), file_display_name);

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
		case GNOME_VFS_FILE_TYPE_DIRECTORY :

		if ( strcmp(GnomeCmdConnectionTreeview::Collate_key_dot,	file_display_name_ck ) &&
			 strcmp(GnomeCmdConnectionTreeview::Collate_key_dotdot,	file_display_name_ck ))
		{
            ENUMERATE_INF("strcmp:%i", strcmp(GnomeCmdConnectionTreeview::Collate_key_dot,	    file_display_name_ck ));
            ENUMERATE_INF("strcmp:%i", strcmp(GnomeCmdConnectionTreeview::Collate_key_dotdot,	file_display_name_ck ));

			added++;

			ENUMERATE_INF("alsc:[%03i][0x%16x] [%03i][%03i][%03i] +<%s>", vaec->handle(), l, count, added, vaec->max_result(), file_display_name);

			// it is a symlink pointing on  a directory
			if ( file_has_flag_symlink )
				file = new FoldviewLink(file_display_name, file_symlink_target_name, file_access);
			// it is a directory
			else
				file = new FoldviewDir(file_display_name, file_access);

			vaec->list_append( file );

			// if the caller want partial listing
			if  (
					( vaec->max_result()	>= 0					)  &&
					( vaec->list_card()		>= vaec->max_result()   )
				)
            {
                g_free((void*)file_display_name_ck);
				goto lab_abort;
            }
		}
        g_free((void*)file_display_name_ck);
		break;

		//.....................................................................
		default:
        g_free((void*)file_display_name_ck);
		ENUMERATE_INF("alsc:[%03i][0x%16x] [%03i][%03i][%03i]  <%s>", vaec->handle(), l, count, added, vaec->max_result(), file_display_name);
        break;
	}

	// if gvfs bugs on entries_read, we bug too with this
	if ( count == _entries_read )
		goto lab_no_more_entry;

	l = g_list_next(l);

	goto lab_loop;

//.............................................................................
lab_no_more_entry:

	// if OK, simply return, we will be re-called for further entries
	if ( _result == GNOME_VFS_OK )
	{
		ENUMERATE_INF("alsc:[%03i][0x%16x] (GNOME_VFS_OK)", vaec->handle(), l);
		return;
	}

	// else ensure we are in EOF case : GNOME_VFS_ERROR_EOF should be set
	if ( _result == GNOME_VFS_ERROR_EOF )
		goto lab_eof;

	// else an error as occured : result is not OK, neither EOF.
	// this occurs for example with symlinks, or access-denied directories ;
	// - show a little warning
    // - set error fileds in AsyncEnumerateChildren
    // - and do as EOF, since there is no more entry.
	ENUMERATE_ERR("alsc:[%03i][0x%16x]  (NO ENTRY - Jumping to EOF):%s",
		vaec->handle(), l, gnome_vfs_result_to_string(_result));

    vaec->error_set(Error_from_GnomeVFSResult(_result), gnome_vfs_result_to_string(_result));
//.............................................................................
lab_eof:

	ENUMERATE_INF("alsc:[%03i][0x%16x] (EOF)", vaec->handle(), l);
	// Call caller callback ; caller is responsible for freeing memory
	// eventually allocated in m_user_data member
	// of the struct_gvfs_caller_data
	vaec->caller_data()->callback()(vaec);

	delete vaec;

	// "Final end"
	return;

//.............................................................................
lab_abort:

	ENUMERATE_INF("alsc:[%03i][0x%16x] (ABORT)", vaec->handle(), l);
	// Call caller callback ; caller is responsible for freeing memory
	// eventually allocated in m_user_data member
	// of the struct_gvfs_caller_data
    (vaec->caller_data()->callback())(vaec);
	// cancel async op
	vaec->cancel_async_op();

	delete vaec;

	// "Final end"
	//return;
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
GnomeCmdConnectionTreeview::Model::GnomeVFS::Monitor_callback(
	GnomeVFSMonitorHandle		*   _handle,
	const gchar					*   _monitor_uri,
	const gchar					*   _info_uri,
	GnomeVFSMonitorEventType		_event_type,
	gpointer						_data)
{
	MonitorData							*   md				= NULL;

	GnomeVFSAsyncHandle					*   handle			= NULL;
	GList								*   list			= NULL;
	GnomeVFSAsyncGetFileInfoCallback		callback		= NULL;

	GnomeVFSURI							*   uri_mon			= NULL;
	GnomeVFSURI							*   uri_inf			= NULL;
	gchar								*   basename_mon	= NULL;
	gchar								*   basename_inf	= NULL;
	gboolean								b_self			= FALSE;
	//.........................................................................
	//  GNOME_VFS_MONITOR_EVENT_CHANGED 			file data changed (FAM, inotify).
	//  GNOME_VFS_MONITOR_EVENT_DELETED 			file deleted event (FAM, inotify).
	//  GNOME_VFS_MONITOR_EVENT_STARTEXECUTING 		file was executed (FAM only).
	//  GNOME_VFS_MONITOR_EVENT_STOPEXECUTING 		executed file isn't executed anymore (FAM only).
	//  GNOME_VFS_MONITOR_EVENT_CREATED 			file created event (FAM, inotify).
	//  GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED 	file metadata changed (inotify only).
	//.........................................................................
	md = (MonitorData*)_data;
	g_return_if_fail( md );

	// GnomeVFS is bad. It gives file:///toto/, with a trailing slash. Thus we
	// are forced to extract the basename from temporary GnomeVFSURIs
	uri_mon			= gnome_vfs_uri_new(_monitor_uri);
	uri_inf			= gnome_vfs_uri_new(_info_uri);
	basename_mon	= gnome_vfs_uri_extract_short_name(uri_mon);
	basename_inf	= gnome_vfs_uri_extract_short_name(uri_inf);
	MONITOR_INF("Model::GnomeVFS::Monitor_callback(): [%s] [%s]", basename_mon, basename_inf);
	b_self			= g_utf8_collate(basename_mon, basename_inf) == 0 ? TRUE : FALSE;
	gnome_vfs_uri_unref(uri_mon);
	gnome_vfs_uri_unref(uri_inf);
	switch ( _event_type )
	{
		//.....................................................................
		case GNOME_VFS_MONITOR_EVENT_DELETED:

		if ( b_self )
		{
			// Parent deleted :
			MONITOR_INF("Model::GnomeVFS::Monitor_callback():EVENT_DELETED (SELF) [mon:%s] [inf:%s]", _monitor_uri, _info_uri);
			Monitor_callback_del(md);
			goto exit;
		}
		else
		{
			// Child deleted :
			MONITOR_INF("Model::GnomeVFS::Monitor_callback():EVENT_DELETED (CHILD) [mon:%s] [inf:%s]", _monitor_uri, _info_uri);
			Monitor_callback_child_del(md, basename_inf);
			goto exit;
		}
		break;

		//.....................................................................
		case GNOME_VFS_MONITOR_EVENT_CREATED:

		if ( b_self )
		{
			MONITOR_INF("Model::GnomeVFS::Monitor_callback():EVENT_CREATED (SELF, skipping) [mon:%s] [inf:%s]", _monitor_uri, _info_uri);
			goto abort;
		}
		else
		{
			MONITOR_INF("Model::GnomeVFS::Monitor_callback():EVENT_CREATED (CHILD) [mon:%s] [inf:%s]", _monitor_uri, _info_uri);
			callback	= Monitor_callback_child_new;
			list		= g_list_append(list, gnome_vfs_uri_new(_info_uri));
		}

		break;

		//.....................................................................
		case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED   :

		//																		// _GWR_GVFS_BUG_
		// Due to a GnomeVFS bug,we are forced to use child event. When a dir
		//  is not readable, showed in foldview, and when it becomes readable,
		//  we dont get { self-metadata-changed, parent-metadata-changed } events.
		//  Instead of that, we get { self-CREATED , parent-metadata-changed }.
		//
		// Its annoying, because working on children force us to check if the parent
		//  is collapsed, etc...
		//

		if ( b_self )
		{
			MONITOR_INF("Model::GnomeVFS::Monitor_callback():EVENT_METADATA_CHANGED (SELF, skipping) [mon:%s] [inf:%s]", _monitor_uri, _info_uri);
			//callback	= Monitor_callback_acc;
			//list		= g_list_append(list, gnome_vfs_uri_new(_info_uri));
			goto abort;
		}
		else
		{
			MONITOR_INF("Model::GnomeVFS::Monitor_callback():EVENT_METADATA_CHANGED (CHILD) [mon:%s] [inf:%s]", _monitor_uri, _info_uri);
			callback	= Monitor_callback_child_acc;
			list		= g_list_append(list, gnome_vfs_uri_new(_info_uri));
		}

		break;

		//.....................................................................
		case GNOME_VFS_MONITOR_EVENT_CHANGED			: goto abort;
		case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING		: goto abort;
		case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING		: goto abort;
	}


	// void gnome_vfs_async_get_file_info(
	//		GnomeVFSAsyncHandle					**  handle_return,
	//		GList								*   uri_list,
	//		GnomeVFSFileInfoOptions					options,
	//		int										priority,
	//		GnomeVFSAsyncGetFileInfoCallback		callback,
	//		gpointer								callback_data);
	//
	//  Fetch information about the files indicated in uri_list and return the
	//  information progressively to callback.
	//
	//  handle_return   : when the function returns, will point to a handle for
	//					  the async operation.
	//  uri_list		: a GList of GnomeVFSURIs to fetch information about.
	//  options			: packed boolean type providing control over various details
	//					  of the get_file_info operation.
	//  priority		: a value from GNOME_VFS_PRIORITY_MIN to GNOME_VFS_PRIORITY_MAX
	//					  (normally should be GNOME_VFS_PRIORITY_DEFAULT) indicating
	//					  the priority to assign this job in allocating threads from the thread pool.
	//  callback		: function to be called when the operation is complete.
	//  callback_data   : data to pass to callback.

	gnome_vfs_async_get_file_info(
		&handle,
		list,
		(GnomeVFSFileInfoOptions)
		(
			GNOME_VFS_FILE_INFO_FOLLOW_LINKS		|
			GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS
		),
		GNOME_VFS_PRIORITY_DEFAULT,
		(GnomeVFSAsyncGetFileInfoCallback)callback,
		_data);

abort:
exit:
	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitor_callback_del()
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::Monitor_callback_del(
	MonitorData		*	_md)
{
	MONITOR_INF("Model::GnomeVFS::Monitor_callback_del()");

	Iter_monitor_callback_del(_md);

	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitor_callback_child_del()
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::Monitor_callback_child_del(
	MonitorData		*	_md,
	const Uri			_uri_child)
{
	MONITOR_INF("Model::GnomeVFS::Monitor_callback_child_del()");

	GnomeVFSURI * temp	= gnome_vfs_uri_new(_uri_child);
	gchar		* s		= gnome_vfs_uri_extract_short_name(temp);

	Iter_monitor_callback_child_del(_md, s);

	g_free(s);
	gnome_vfs_uri_unref(temp);

	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitor_callback_child_new()
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::Monitor_callback_child_new(
	GnomeVFSAsyncHandle		*   _handle,
	GList					*   _results,
	gpointer					_data)
{
	MonitorData							*   md				= NULL;
	GList								*   list			= NULL;
	GnomeVFSGetFileInfoResult			*   result			= NULL;
	GnomeVFSFileInfo					*   info			= NULL;

	char								*   name			= NULL;
	eFileType								type			= eTypeUnknown;
	eFileAccess								access			= eAccessUN;
	gboolean								is_symlink		= FALSE;
	gchar								*   symlink_name	= NULL;

	File								*   child			= NULL;
	//.........................................................................
	// #warning [*] Model::GnomeVFS::Monitor_callback_child_new() : GList delete ? NO !
	MONITOR_INF("Model::GnomeVFS::Monitor_callback_child_new()");

	md = (MonitorData*)_data;
	g_return_if_fail( md );

	list = g_list_first(_results);
	if ( ! list )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_child_new():empty result list");
		goto abort;
	}

	//  GnomeVFSGetFileInfoResult :
	//
	//	  GnomeVFSURI		*   uri;
	//	  GnomeVFSResult		result;
	//	  GnomeVFSFileInfo  *   file_info;
	//
	result = (GnomeVFSGetFileInfoResult*)(list->data);

	if ( result->result != GNOME_VFS_OK )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_child_new():async call failure [%s]", SVFSRESULT(result->result));
		goto abort;
	}

	info = (GnomeVFSFileInfo*)(result->file_info);
	if ( ! info )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_child_new():file info is NULL");
		goto abort;
	}

	name			= info->name;
	type			= Type_from_GnomeVFSFileType(info->type);
	access			= Access_from_GnomeVFSFilePermissions(info->permissions);
	is_symlink		= GNOME_VFS_FILE_INFO_SYMLINK(info);
	symlink_name	= info->symlink_name;

	if ( type != eTypeDirectory )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_child_new():type is not directory [GnomeVFS:0x%08x]", info->type);
		goto abort;
	}
	if ( access == eAccessUN )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_child_new():access not resolved [GnomeVFS:0x%08x]", info->permissions);
		goto abort;
	}

	if ( ! is_symlink )
		child = new Directory(name, access);
	else
		child = new Symlink(name, symlink_name, access);

	Iter_monitor_callback_child_new(md, child);

	delete child;

abort:
//exit:

	return;
}

/*
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitor_callback_acc()
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::Monitor_callback_acc(
	GnomeVFSAsyncHandle		*   _handle,
	GList					*   _results,
	gpointer					_data)
{
	MonitorData							*   md				= NULL;
	GList								*   list			= NULL;
	GnomeVFSGetFileInfoResult			*   result			= NULL;
	GnomeVFSFileInfo					*   info			= NULL;

	char								*   name			= NULL;
	eFileType								type			= eTypeUnknown;
	eFileAccess								access			= eAccessUN;
	gboolean								is_symlink		= FALSE;
	gchar								*   symlink_name	= NULL;
	//.........................................................................
	#warning Model::GnomeVFS::Monitor_callback_acc() : GList delete ?
	MONITOR_INF("Model::GnomeVFS::Monitor_callback_acc()");

	md = (MonitorData*)_data;
	g_return_if_fail( md );

	list = g_list_first(_results);
	if ( ! list )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_acc():empty result list");
		goto abort;
	}

	//  GnomeVFSGetFileInfoResult :
	//
	//	  GnomeVFSURI		*   uri;
	//	  GnomeVFSResult		result;
	//	  GnomeVFSFileInfo  *   file_info;
	//
	result = (GnomeVFSGetFileInfoResult*)(list->data);

	if ( result->result != GNOME_VFS_OK )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_acc():async call failure [%s]", SVFSRESULT(result->result));
		goto abort;
	}

	info = (GnomeVFSFileInfo*)(result->file_info);
	if ( ! info )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_acc():file info is NULL");
		goto abort;
	}

	name			= info->name;
	type			= Type_from_GnomeVFSFileType(info->type);
	access			= Access_from_GnomeVFSFilePermissions(info->permissions);
	is_symlink		= GNOME_VFS_FILE_INFO_SYMLINK(info);
	symlink_name	= info->symlink_name;

	Iter_monitor_callback_acc(md, access, name);

abort:
//exit:
	return;

}
*/

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Monitor_callback_child_acc()
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::Monitor_callback_child_acc(
	GnomeVFSAsyncHandle		*   _handle,
	GList					*   _results,
	gpointer					_data)
{
	MonitorData							*   md				= NULL;
	GList								*   list			= NULL;
	GnomeVFSGetFileInfoResult			*   result			= NULL;
	GnomeVFSFileInfo					*   info			= NULL;

	char								*   name			= NULL;
	eFileType								type			= eTypeUnknown;
	eFileAccess								access			= eAccessUN;
	gboolean								is_symlink		= FALSE;
	gchar								*   symlink_name	= NULL;
	//.........................................................................
	// #warning [*] Model::GnomeVFS::Monitor_callback_acc() : GList delete ? NO !
	MONITOR_INF("Model::GnomeVFS::Monitor_callback_acc()");

	md = (MonitorData*)_data;
	g_return_if_fail( md );

	list = g_list_first(_results);
	if ( ! list )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_acc():empty result list");
		goto abort;
	}

	//  GnomeVFSGetFileInfoResult :
	//
	//	  GnomeVFSURI		*   uri;
	//	  GnomeVFSResult		result;
	//	  GnomeVFSFileInfo  *   file_info;
	//
	result = (GnomeVFSGetFileInfoResult*)(list->data);

	if ( result->result != GNOME_VFS_OK )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_acc():async call failure [%s]", SVFSRESULT(result->result));
		goto abort;
	}

	info = (GnomeVFSFileInfo*)(result->file_info);
	if ( ! info )
	{
		MONITOR_ERR("Model::GnomeVFS::Monitor_callback_acc():file info is NULL");
		goto abort;
	}

	name			= info->name;
	type			= Type_from_GnomeVFSFileType(info->type);
	access			= Access_from_GnomeVFSFilePermissions(info->permissions);
	is_symlink		= GNOME_VFS_FILE_INFO_SYMLINK(info);
	symlink_name	= info->symlink_name;

	Iter_monitor_callback_child_acc(md, access, name);

abort:
//exit:

	return;

}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							Model Wrappers				                  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//=============================================================================
//  GnomeVFS::iter_check_if_empty
//=============================================================================
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::iter_check_if_empty(
	AsyncCallerData  *  _acd,
    const Uri           _uri)
{
	GnomeVFSAsyncEnumerateChildren  * vaec = NULL;
	//.........................................................................
	//vaec = new GnomeVFSAsyncEnumerateChildren(_acd, _uri, 1, TRUE);
	vaec = GCMD_STRUCT_NEW(GnomeVFSAsyncEnumerateChildren, _acd, _uri, 1, TRUE);

	// Launch gvfs async op !
	// uri ref_count is not incremented
	gnome_vfs_async_load_directory_uri(
		vaec->handle_ptr(),
		vaec->gnomevfs_uri(),
		(GnomeVFSFileInfoOptions)
			(
				GNOME_VFS_FILE_INFO_DEFAULT				|
				GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS   |
				GNOME_VFS_FILE_INFO_FOLLOW_LINKS
			),
		GCMDGTKFOLDVIEW_GVFS_ITEMS_PER_NOTIFICATION,
		GNOME_VFS_PRIORITY_DEFAULT,
		Iter_enumerate_children_callback,
		(gpointer)vaec);
}

//=============================================================================
//  GnomeVFS::iter_enumerate_children
//=============================================================================
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::iter_enumerate_children(
	AsyncCallerData  *  _acd,
    const Uri           _uri)
{
	GnomeVFSAsyncEnumerateChildren  * vaec = NULL;
	//.........................................................................
	vaec = GCMD_STRUCT_NEW(GnomeVFSAsyncEnumerateChildren, _acd, _uri, -1, TRUE);

	// Launch gvfs async op !
	// uri ref_count is not incremented
	gnome_vfs_async_load_directory_uri(
		vaec->handle_ptr(),
		vaec->gnomevfs_uri(),
		(GnomeVFSFileInfoOptions)
			(
				GNOME_VFS_FILE_INFO_DEFAULT				|
				GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS   |
				GNOME_VFS_FILE_INFO_FOLLOW_LINKS
			),
		GCMDGTKFOLDVIEW_GVFS_ITEMS_PER_NOTIFICATION,
		GNOME_VFS_PRIORITY_DEFAULT,
		GnomeVFS::Iter_enumerate_children_callback,
		(gpointer)vaec);
}

//=============================================================================
//  GnomeVFS::iter_file_info
//=============================================================================
void
GnomeCmdConnectionTreeview::Model::GnomeVFS::iter_get_file_info(
	AsyncCallerData  *  _acd,
    const Uri           _uri)
{
	GnomeVFSAsyncGetFileInfo    * gfi   = NULL;
    GList                       * list  = NULL;
	//.........................................................................
	gfi = GCMD_STRUCT_NEW(GnomeVFSAsyncGetFileInfo, _acd, _uri);
    list = g_list_append(list, gfi->gnomevfs_uri());
	//GCMD_INF_vfs("als :[%03i] [0x%16x][0x%16x][%03i] Launch", hi, ga, ls, max_result);

	// Launch gvfs async op !
	// uri ref_count is not incremented

	gnome_vfs_async_get_file_info(
		gfi->handle_ptr(),
		list,
		(GnomeVFSFileInfoOptions)
			(
				GNOME_VFS_FILE_INFO_DEFAULT				|
				GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS   |
				GNOME_VFS_FILE_INFO_FOLLOW_LINKS
			),
		GNOME_VFS_PRIORITY_DEFAULT,
		GnomeVFS::Iter_get_file_info_callback,
		(gpointer)gfi);
}


















