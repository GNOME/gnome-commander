/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyright (C) 2010-2010 Guillaume Wardavoir

	---------------------------------------------------------------------------

	C++

	Contain variadic macros

	---------------------------------------------------------------------------

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

#include	<config.h>

#include	<stdlib.h>

#include	<gtk/gtksignal.h>
#include	<gtk/gtkvbox.h>

#include	<libgnomevfs/gnome-vfs.h>
#include	<libgnomevfs/gnome-vfs-utils.h>

#include	"gnome-cmd-includes.h"
#include	"gnome-cmd-combo.h"
#include	"gnome-cmd-main-win.h"
#include	"gnome-cmd-style.h"
#include	"gnome-cmd-data.h"

#include	"gnome-cmd-foldview-private.h"
#include	"gnome-cmd-foldview-gvfs.h"

//  ***************************************************************************
//  *																		  *
//  *								Defines								      *
//  *																		  *
//  ***************************************************************************
#define GVFS_ITEMS_PER_NOTIFICATION			30

#define DEBUG_GVFS_ALSC

//  ***************************************************************************
//  *																		  *
//  *								Helpers								      *
//  *																		  *
//  ***************************************************************************
static  char	sLogStr	[1024];

void gwr_gvfs_inf(const char* fmt, ...)
{
	#ifndef DEBUG_SHOW_GVFS_INF
		return;
	#endif
	va_list val; va_start(val, fmt); vsprintf(sLogStr, fmt, val); va_end(val);
	gwr_inf("GVFS:%s", sLogStr);
}
void gwr_gvfs_wng(const char* fmt, ...)
{
	#ifndef DEBUG_SHOW_GVFS_WNG
		return;
	#endif
	va_list val; va_start(val, fmt); vsprintf(sLogStr, fmt, val); va_end(val);
	gwr_wng("GVFS:%s", sLogStr);
}
void gwr_gvfs_err(const char* fmt, ...)
{
	#ifndef DEBUG_SHOW_GVFS_ERR
		return;
	#endif
	va_list val; va_start(val, fmt); vsprintf(sLogStr, fmt, val); va_end(val);
	gwr_err("GVFS:%s", sLogStr);
}

//=============================================================================
#ifdef DEBUG_GVFS_ALSC

	#define ALSC_INF(...)													\
	{																		\
		gwr_gvfs_inf(__VA_ARGS__);											\
	}
	#define ALSC_WNG(...)													\
	{																		\
		gwr_gvfs_wng(__VA_ARGS__);											\
	}
	#define ALSC_ERR(...)													\
	{																		\
		gwr_gvfs_err( __VA_ARGS__);											\
	}

#else

	#define ALSC_INF(...)
	#define ALSC_WNG(...)
	#define ALSC_ERR(...)

#endif

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #						File, Dir, ...	Structs							  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//
//  Hope it is standard C++ 
//
//  new in code -> operator new -> constructor
//  delete in code -> destructor -> operator delete
//
gvfs_file::gvfs_file(gchar *name, GnomeVFSResult result, GnomeVFSFilePermissions permissions, GnomeVFSFileType type, GnomeVFSFileFlags flags)
{
	d_name			= name;

	a_vfsresult		= result;
	a_vfstype		= type;
	a_vfsflags		= flags;

	a_access		= GcmdGtkFoldview::Access_from_permissions(permissions);
}
gvfs_file::~gvfs_file()
{
	//printf("==>~gvfs_fil()\n");
	g_free (d_name);					
}




gvfs_dir::gvfs_dir(gchar *name, GnomeVFSFilePermissions permissions, GnomeVFSFileFlags flags) :
	gvfs_file(name, GNOME_VFS_OK, permissions, GNOME_VFS_FILE_TYPE_DIRECTORY, flags)
{
}
gvfs_dir::~gvfs_dir()
{
	//printf("==>~gvfs_dir()\n");
}




gvfs_symlink::gvfs_symlink(gchar *name, GnomeVFSFilePermissions permissions, GnomeVFSFileFlags flags) :
	gvfs_file(name, GNOME_VFS_OK, permissions, GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK, flags)
{
}
gvfs_symlink::~gvfs_symlink()
{
	//printf("==>~gvfs_dir()\n");
}
//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #						Gnome VFS - Misc								  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//=============================================================================
//  Common vars
//=============================================================================
static  GnomeVFSResult  sVFSResult		= GNOME_VFS_OK;	 // for sync operations


gboolean																		// __GWR__TODO__ inline
GVFS_vfsinfo_has_type_directory(
	GnomeVFSFileInfo	*info)
{
	if ( !(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) )
		return FALSE;

	return ( (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) ? TRUE : FALSE );
}
gboolean																		// __GWR__TODO__ inline
GVFS_vfsinfo_has_type_symlink(
	GnomeVFSFileInfo	*info)
{
	if ( !(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) )
		return FALSE;

	return ( (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) ? TRUE : FALSE );
}

gboolean
GVFS_vfsinfo_is_true_directory(
	GnomeVFSFileInfo	*info)
{
	// This is memory-access broken but Im fed up with utf8						_GWR_TODO_

	if  (
			( info->name[0] == '.'  )	&&
			( info->name[1] == 0	)
		)
		return FALSE;

	if  (
			( info->name[0] == '.'  )   &&
			( info->name[1] == '.'  )   &&
			( info->name[2] == 0	)
		)
		return FALSE;

	return TRUE;
}


GnomeVFSURI*
GVFS_uri_new(const gchar *text)
{
	// well-formed_text
	gchar		*temp = gnome_vfs_make_uri_from_input(text);
	// new uri
	GnomeVFSURI *uri = gnome_vfs_uri_new(temp);
	g_free(temp);

	return uri;
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #						Gnome VFS - Sync ops							  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Test if an uri exists
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gboolean GVFS_info_from_uri(GnomeVFSURI* uri, GnomeVFSFileInfo* info)
{
	sVFSResult = gnome_vfs_get_file_info_uri(uri, info,
		(GnomeVFSFileInfoOptions)(GNOME_VFS_FILE_INFO_DEFAULT |
		GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS));

	ERR_VFS_RET(FALSE, "vfs_sync_uri_is_directory: couldnt get info for uri");

	return TRUE;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Test is a directory contains subdirectories
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*
static gboolean gvfs_dir_has_subdirs(GnomeVFSURI *uri)
{
	GList				*list;
	GList				*l;
	GnomeVFSFileInfo	*info;
	gboolean			bRes;

	sVFSResult =  gnome_vfs_directory_list_load(
		&list,
		uri->text,
		GNOME_VFS_FILE_INFO_DEFAULT);

	ERR_VFS_RET(FALSE, "vfs_sync_dir_has_subdirs:gnome_vfs_directory_list_load failed");

	bRes	= FALSE;
	l		= g_list_first(list);
	while ( l != NULL )
	{
		info = (GnomeVFSFileInfo*)(l->data);

		if  (
				GVFS_vfsinfo_has_type_directory(info) &&
				GVFS_vfsinfo_is_true_directory(info)
			)
		{
			//warn_print("subfolder:%s\n", info->name);
			bRes = TRUE;
			break;
		}

		l = g_list_next(l);
	}

	g_list_free(list);
	return bRes;;
}
*/
//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #					Gnome VFS - Async ops ( handles )					  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
//static  GStaticMutex GVFS_qstack_Mutex	= G_STATIC_MUTEX_INIT;						__GWR__TODO__ ?

//=============================================================================
//  Handles for vfs async ops
//
//  Handle creation : from user action only -> monothread
//  Handle release  : from gvfs callbacks, but always on different handles
//	  -> monothread ; the only risk is to return "no more handle ptr available"
//	  when a gvfs callback just freed one.
//=============================================================================

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  QuickStack
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static  gboolean				GVFS_qstack_init			= FALSE;

static  guint32					GVFS_qstack_el_core_size	= sizeof(GnomeVFSAsyncHandle*);
static  guint32					GVFS_qstack_ix_core_size	= sizeof(guint32);

static  guint32					GVFS_qstack_step	= 10;
static  guint32					GVFS_qstack_size	= 0;

static  GnomeVFSAsyncHandle**	GVFS_qstack_el		= NULL;
static  guint32*				GVFS_qstack_ix		= NULL;
static  guint32					GVFS_qstack_ff		= 0; 

gboolean GVFS_qstack_initialized()
{
	return GVFS_qstack_init;
}

void GVFS_qstack_initialize()
{
	guint32 i = 0;

	// alloc for pointers
	if ( GVFS_qstack_el )
	{
		gwr_err("GVFS_qstack_init:GVFS_qstack_el is not NULL - already initialized ?");
		return;
	}
	GVFS_qstack_el = (GnomeVFSAsyncHandle**)(g_malloc0(GVFS_qstack_el_core_size * GVFS_qstack_step));
	if ( !GVFS_qstack_el )
	{
		gwr_err("GVFS_qstack_init:g_malloc0 failed for GVFS_qstack_el");
		return;
	}

	// alloc for indexes
	if ( GVFS_qstack_ix )
	{
		gwr_err("GVFS_qstack_init:GVFS_qstack_ix is not NULL - already initialized ?");
		return;
	}
	GVFS_qstack_ix = (guint32*)(g_malloc0(GVFS_qstack_ix_core_size * GVFS_qstack_step));
	if ( !GVFS_qstack_ix )
	{
		gwr_err("GVFS_qstack_init:g_malloc0 failed for GVFS_qstack_ix");
		return;
	}

	// init indexes stack
	for ( i = 0 ; i != GVFS_qstack_step ; i++ )
	{
		GVFS_qstack_ix[i] = GVFS_qstack_step - i - 1;
	}
	GVFS_qstack_ff = GVFS_qstack_step - 1;

	// size
	GVFS_qstack_size = GVFS_qstack_step;
}

gboolean GVFS_qstack_realloc()
{
	gpointer temp = NULL;
	guint32	 i	  = 0;

	//gwr_wng("GVFS_qstack_realloc at size:%04i", GVFS_qstack_size);

	//.........................................................................
	// realloc free indexes stack ; since there is no more index free,
	// we could just realloc - but there is no g_realloc0 in Glib

	// free the old indexes stack
	g_free(GVFS_qstack_ix);

	// alloc a new stack
	GVFS_qstack_ix = (guint32*)(g_malloc0(GVFS_qstack_ix_core_size * ( GVFS_qstack_size + GVFS_qstack_step ) ));
	if ( !GVFS_qstack_ix )
	{
		gwr_err("GVFS_qstack_realloc:g_malloc0 failed for GVFS_qstack_ix");
		return FALSE;
	}

	// store new free indexes
	for ( i = 0 ; i != GVFS_qstack_step ; i++ )
	{
		GVFS_qstack_ix[i] = GVFS_qstack_size + GVFS_qstack_step - i - 1;
	}

	//.........................................................................
	// realloc pointers array
	temp			= (gpointer)GVFS_qstack_el;
	GVFS_qstack_el  = (GnomeVFSAsyncHandle**)(g_malloc0(GVFS_qstack_el_core_size * ( GVFS_qstack_size + GVFS_qstack_step ) ));
	if ( !GVFS_qstack_el )
	{
		gwr_err("GVFS_qstack_realloc:g_malloc0 failed for GVFS_qstack_el");
		return FALSE;
	}

	// copy used pointers
	memcpy( GVFS_qstack_el, temp, GVFS_qstack_el_core_size * GVFS_qstack_size);

	// update stack size
	GVFS_qstack_size += GVFS_qstack_step;

	// update first free index
	GVFS_qstack_ff = GVFS_qstack_step - 1;

	return TRUE;
}


guint32 GVFS_qstack_pop()
{
	if ( GVFS_qstack_ff == 0 )
	{
		//guint32 temp = GVFS_qstack_ff;
		// index on indexes is zero ; we are missing storage places
		// -> realloc
		guint32 index = GVFS_qstack_ix[0];
		GVFS_qstack_realloc();

		//gwr_inf("GVFS_qstack_pop (realloc):index:%04i ff:%04i->%04i of %04i", index, temp, GVFS_qstack_ff, GVFS_qstack_size-1);
		return index;
	}

	//gwr_inf("GVFS_qstack_pop:ff:%04i->%04i of %04i", GVFS_qstack_ff, GVFS_qstack_ff-1, GVFS_qstack_size-1);
	return GVFS_qstack_ix[GVFS_qstack_ff--];
}
void GVFS_qstack_push(guint32 index)
{

	if ( GVFS_qstack_ff >= GVFS_qstack_size )
	{
		gwr_err("GVFS_qstack_push:GVFS_qstack_ff too high");
		return;
	}

	//gwr_inf("GVFS_qstack_push:index:%04i ff:%04i->%04i of %04i", index, GVFS_qstack_ff, GVFS_qstack_ff+1, GVFS_qstack_size-1);
	GVFS_qstack_ix[++GVFS_qstack_ff] = index;
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  User calls
//
//  Maybe should I modify this to GVFS_qstack_get(GVFS_qstack_pop())			__GWR__TODO__
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static guint32 GVFS_async_handle_hold()
{
	return GVFS_qstack_pop();
}
static void GVFS_async_handle_release(guint32 index)
{
	GVFS_qstack_push(index);
}

static GnomeVFSAsyncHandle *GVFS_async_handle_get(guint32 index)
{
	return GVFS_qstack_el[index];
}
static GnomeVFSAsyncHandle **GVFS_async_handle_ptr_get(gint32 index)
{
	return &( GVFS_qstack_el[index] );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Cancel an async op
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void GVFS_async_cancel(gint32 handle_index)
{
	gnome_vfs_async_cancel( GVFS_async_handle_get( handle_index ) );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Cancel all async ops
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void GVFS_async_cancel_all()
{
	for ( guint32 i = GVFS_qstack_ff ; i != GVFS_qstack_size ; i++ )
	{
		gnome_vfs_async_cancel( GVFS_async_handle_get( i ) );
	}

	GVFS_qstack_ff = GVFS_qstack_size - 1;
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #					Gnome VFS - Async ops							      #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//=============================================================================
//  Base structs for vfs async ops
//=============================================================================

//.............................................................................
//  gvfs_async_load_subdirs new / del
//.............................................................................
void* gvfs_async_load_subdirs::operator new(size_t size, GnomeVFSURI* parent_uri, gchar *parent_path)
{
	gvfs_async_load_subdirs	*ls	= g_try_new0(gvfs_async_load_subdirs, 1);

	if ( !ls )
	{
		gwr_err("gvfs_async_load_subdirs::new:g_try_new0 failed");
		return ls;
	}

	ls->puri()  = parent_uri;													// __GWR__TODO__ check ref_count
	ls->ppath() = parent_path;

	//GArray* g_array_sized_new(
	//gboolean		zero_terminated,	zero supplementary element
	//gboolean		clear_,				new element to 0
	//guint			element_size,
	//guint			reserved_size);		growness
	ls->array() = g_array_sized_new(
		FALSE, 
		TRUE, 
		sizeof(gvfs_file*),
		10);
	ls->len()   = 0;

	return ls;
}

void gvfs_async_load_subdirs ::operator delete (void *p)
{
	gvfs_file				*file = NULL;
	gvfs_async_load_subdirs *ls = (gvfs_async_load_subdirs*)p;

	// parent uri & path
	gnome_vfs_uri_unref(ls->puri());
	g_free(ls->ppath());

	// array elements
	for ( int i = 0 ; i < ls->len() ; i++ )
	{
		file = g_array_index( ls->array(), gvfs_file*, i);
		delete file;
	}

	// array
	g_array_free(ls->array(), TRUE);

	// struct
	g_free(ls);
}

//-----------------------------------------------------------------------------
//  Callbacks														MULTITHREAD
//-----------------------------------------------------------------------------

//.............................................................................
// Add entries...goto is the best
//.............................................................................
static void 
GVFS_async_load_subdirectories_callback(
	GnomeVFSAsyncHandle		*handle,
	GnomeVFSResult			result,
	GList					*list,
	guint					entries_read,
	gpointer				data)
{
	GnomeVFSFileInfo			*info   = NULL;

	GList						*l		= NULL;
	guint						count   = 0;
	guint						added   = 0;

	gboolean					symlink = FALSE;

	gvfs_async							*ga		= NULL;
	gvfs_async_load_subdirs				*ls		= NULL;
	gvfs_dir							*dir	= NULL;
	gvfs_symlink						*lnk	= NULL;
	//.........................................................................

	ga  =   (gvfs_async*)				(data);
	ls  =   (gvfs_async_load_subdirs*)  (ga->ad());

	ALSC_INF("alsc:[%03i] entries:%03i list:%16x parent:[%s]%s", ga->hi(), entries_read, list, ls->ppath(), ls->puri()->text);

	// counter
	count   = 0;						

	// handle the '0-entry-case'
	if ( entries_read == 0 )
	{
		goto lab_no_more_entry;
	}

	// init loop - we have at least one entry
	l = g_list_first(list);
	
lab_loop:

	count++;

	info	= (GnomeVFSFileInfo*)(l->data);
	symlink = FALSE;

	//ALSC_INF("alsc:[%03i] entry name:%s", ga->hi(), info->name);

	switch ( info->type )
	{

		//.....................................................................
		case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:

		// if follow_links is TRUE, we should not be here - except if we have
		// a broken link. We thus cant know if the link was pointing on a
		// directory or something else. So ignore it.
		if ( ls->follow_links() )
		{
			ALSC_WNG("alsc:[%03i][0x%16x] [%03i][%03i][%03i] [broken link, ignored]<%s>", ga->hi(), l, count, added, ga->mr(), info->name);
			break;
		}
	
		added++;
		
		ALSC_INF("alsc:[%03i][0x%16x] [%03i][%03i][%03i] S<%s>", ga->hi(), l, count, added, ga->mr(), info->name);

		lnk = new() gvfs_symlink(g_strdup(info->name), info->permissions, info->flags );
		ls->append( (gvfs_file*)lnk );
		
		// if the caller want partial listing
		if  (
				( ga->mr()  >= 0		)  &&
				( ls->len() >= ga->mr() )
			)
			goto lab_abort;

		break;

		//.....................................................................
		case GNOME_VFS_FILE_TYPE_DIRECTORY :
			
		if  ( GVFS_vfsinfo_is_true_directory(info) )
		{
			added++;

			ALSC_INF("alsc:[%03i][0x%16x] [%03i][%03i][%03i] +<%s>", ga->hi(), l, count, added, ga->mr(), info->name);

			//dir = new(g_strdup(info->name), info->permissions, info->flags);// gvfs_dir;
			dir = new() gvfs_dir(g_strdup(info->name), info->permissions, info->flags);

			ls->append( (gvfs_file*)dir );
			
			// if the caller want partial listing
			if  (
					( ga->mr()  >= 0		)  &&
					( ls->len() >= ga->mr() )
				)
				goto lab_abort;
		}
		break;

		//.....................................................................
		default:
		ALSC_INF("alsc:[%03i][0x%16x] [%03i][%03i][%03i]  <%s>", ga->hi(), l, count, added, ga->mr(), info->name);
	}
	
	// if gvfs bugs on entries_read, we bug too with this
	if ( count == entries_read )		
		goto lab_no_more_entry;

	l = g_list_next(l);

	goto lab_loop;

//.............................................................................	
lab_no_more_entry:

	// if OK, simply return, we will be re-called for further entries
	if ( result == GNOME_VFS_OK )
	{
		ALSC_INF("alsc:[%03i][0x%16x] (GNOME_VFS_OK)", ga->hi(), l);
		return;
	}

	// else ensure GNOME_VFS_ERROR_EOF is set
	if ( result == GNOME_VFS_ERROR_EOF )
		goto lab_eof;

	// else an error as occured : result is not OK, neither EOF.
	// this occurs for example with symlinks, or access-denied directories ;
	// show a little warning, and do as EOF, since there is no more entry.
	ALSC_INF("alsc:[%03i][0x%16x]  (NO ENTRY - Jumping to EOF):%s", 
		ga->hi(), l, gnome_vfs_result_to_string(result));
	
//.............................................................................	
lab_eof:

	ALSC_INF("alsc:[%03i][0x%16x] (EOF)", ga->hi(), l);
	// Call caller callback ; caller is responsible for freeing memory
	// eventually allocated in m_user_data member
	// of the struct_gvfs_caller_data
	ga->cd()->a_callback( ga );

	// release the handle
	GVFS_async_handle_release(ga->hi());

	// delete the list
	delete ls;
	delete ga;

	// "Final end"
	return;

//.............................................................................	
lab_abort:

	ALSC_INF("alsc:[%03i][0x%16x] (ABORT)", ga->hi(), l);
	// Call caller callback ; caller is responsible for freeing memory
	// eventually allocated in m_user_data member
	// of the struct_gvfs_caller_data
	ga->cd()->a_callback( ga );

	// cancel async op
	GVFS_async_cancel( ga->hi() );

	// release the handle
	GVFS_async_handle_release(ga->hi());

	// delete the lists
	delete ls;
	delete ga;

	// "Final end"
	//return;
}

//-----------------------------------------------------------------------------
//  Main call
//-----------------------------------------------------------------------------
void GVFS_async_load_subdirectories(
	GnomeVFSURI					*parent_uri,
	gchar						*parent_path,
	gvfs_async_caller_data		*cd,
	gint						max_result,
	gboolean					follow_links)
{
	gint32						hi		= 0;
	gvfs_async					*ga		= NULL;
	gvfs_async_load_subdirs		*ls		= NULL;

	// Obtain a handle
	hi = GVFS_async_handle_hold();

	// Create struct_gvfs_async
	ga = new (hi,max_result,cd) gvfs_async;

	ls = new(parent_uri, parent_path)gvfs_async_load_subdirs;
	ls->follow_links() = follow_links;

	ga->ad() = ls;

	gwr_gvfs_inf("als :[%03i] [0x%16x][0x%16x][%03i] Launch", hi, ga, ls, max_result);

	// Launch gvfs async op !
	// uri ref_count is not incremented
	gnome_vfs_async_load_directory_uri(
		GVFS_async_handle_ptr_get(hi),
		parent_uri,
		(GnomeVFSFileInfoOptions)
			(
				GNOME_VFS_FILE_INFO_DEFAULT				|
				GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS   |
				( follow_links ? GNOME_VFS_FILE_INFO_FOLLOW_LINKS : 0 )
			),
		GVFS_ITEMS_PER_NOTIFICATION,
		//GNOME_VFS_PRIORITY_DEFAULT,
		GNOME_VFS_PRIORITY_MIN,  
		GVFS_async_load_subdirectories_callback,
		(gpointer)ga);
}

