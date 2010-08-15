/*
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
*/

#ifndef __GCMDGTKFOLDVIEW_GVFS_H__
#define __GCMDGTKFOLDVIEW_GVFS_H__


#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>


//*****************************************************************************
//								Helpers
//*****************************************************************************
// Because debugging GVFS generates many logs, we define apart log functions
//#define DEBUG_SHOW_GVFS_INF
#define DEBUG_SHOW_GVFS_WNG
#define DEBUG_SHOW_GVFS_ERR

void gwr_gvfs_inf(const char* fmt, ...);
void gwr_gvfs_wng(const char* fmt, ...);
void gwr_gvfs_err(const char* fmt, ...);

//*****************************************************************************
//								Structs
//*****************************************************************************

/*

	caller function
		|
		v
	launch GVFS op
		|
		+------------ gvfs_async
						|
						+------------ handle_index, ...
						|
						+------------ gvfs_async_caller_data
						|						|
						|						+-------------- gvfs_async_user_callback
						|						|
						|						+-------------- user_data
						|
						+-------------------------------------- gvfs_async_load_subdirs

*/


//=============================================================================
// File structs
//=============================================================================
struct gvfs_file
{
	private:
	gchar*							d_name;
	GnomeVFSResult					a_vfsresult;
	GnomeVFSFileType				a_vfstype;
	GnomeVFSFileFlags				a_vfsflags;
	GcmdGtkFoldview::eFileAccess	a_access;
	public:
	gchar*&							name()			{ return d_name;	}
	GcmdGtkFoldview::eFileAccess&	access()		{ return a_access;  }

	gboolean						flagged_symlink()   { return  ( (a_vfsflags & GNOME_VFS_FILE_FLAGS_SYMLINK) != 0	);	}
	gboolean						is_symlink()		{ return ( a_vfstype == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK		);  }
	gboolean						is_dir()			{ return ( a_vfstype == GNOME_VFS_FILE_TYPE_DIRECTORY			);  }


	public:
				gvfs_file(gchar *name, GnomeVFSResult result, GnomeVFSFilePermissions permissions, GnomeVFSFileType type, GnomeVFSFileFlags flags);
	virtual		~gvfs_file() = 0;	// forbid instantiation
};

struct gvfs_dir : gvfs_file
{
	public:
				gvfs_dir(gchar *name, GnomeVFSFilePermissions permissions, GnomeVFSFileFlags flags);
				~gvfs_dir();
};

struct gvfs_symlink : gvfs_file
{
	private:
	GArray		*d_array;
	guint		a_array_card;

	public:
	void		append(gvfs_symlink* link)
				{

				}
	private:
	gvfs_file   *d_target;
	public:
	gvfs_file*& target()	{ return d_target; }

	public:
				gvfs_symlink(gchar *name, GnomeVFSFilePermissions permissions, GnomeVFSFileFlags flags);
				~gvfs_symlink();
};
//=============================================================================
//  Async "core" structs
//=============================================================================


//-----------------------------------------------------------------------------
//  gvfs async part : struct_gvfs_async
//-----------------------------------------------------------------------------

struct  gvfs_async_caller_data
;
//  Function for freeing the GList and eventually freeing the data it contains
//  gvfs_async_xxx specific
typedef void gvs_async_func_del_list(GList*);

//  Struct created by the gvfs async "main call" function
//  gvfs_async_xxx generic
struct gvfs_async
{
	private:
	gint32						a_handle_index;
	gint						a_max_result;
	gvfs_async_caller_data		*a_caller_data;
	gpointer					a_async_data;
	public:
	gint32						hi()		{ return a_handle_index;	}
	gvfs_async_caller_data*		cd()		{ return a_caller_data;		}
	gint						mr()		{ return a_max_result;		}
	gpointer&					ad()		{ return a_async_data;		}

	public:
	void*		operator new	(size_t size, gint32 handle_index, gint max_result, gvfs_async_caller_data* caller_data)
				{
					gvfs_async	*ga	= g_try_new0(gvfs_async, 1);

					if ( !ga )
					{
						gwr_err("gvfs_async::new:g_try_new0 failed");
						return ga;
					}

					// no up & down function, since we have no inheritance
					// for this struct
					ga->a_handle_index	= handle_index;
					ga->a_caller_data	= caller_data;
					ga->a_max_result	= max_result;

					return ga;
				}

	void		operator delete (void *p)
				{
					g_free (p);
				}
};

//-----------------------------------------------------------------------------
//  Caller part :
//  * gvfs_async_callback
//  * struct gvfs_async_caller_data
//-----------------------------------------------------------------------------
typedef void gvfs_async_user_callback(gvfs_async*);

struct  gvfs_async_caller_data
{
	gvfs_async_user_callback	*a_callback;
	gpointer					a_user_data;

	public:
	void*		operator new	(size_t size, gvfs_async_user_callback callback, gpointer p)
				{
					gvfs_async_caller_data	*cd	= g_try_new0(gvfs_async_caller_data, 1);

					if ( !cd )
					{
						gwr_err("gvfs_async_caller_data::new:g_try_new0 failed");
						return cd;
					}

					cd->a_callback		= callback;
					cd->a_user_data		= p;

					return cd;
				}

	void		operator delete (void *p)
				{
					g_free (p);
				}
};



//=============================================================================
//  Async structs for "Load subdirectories of a directory"
//=============================================================================

//-----------------------------------------------------------------------------
//  struct helper that will updated in each GVFS callback call		MULTITHREAD
//-----------------------------------------------------------------------------
struct gvfs_async_load_subdirs
{
	//.........................................................................
	private:
	GnomeVFSURI					*m_parent_uri;
	gchar						*m_parent_path;
	gboolean					a_follow_links;
	public:
	GnomeVFSURI*&				puri()			{ return m_parent_uri;		}
	gchar*&						ppath()			{ return m_parent_path;		}
	gboolean&					follow_links()	{ return a_follow_links;	}

	//.........................................................................
	private:
	GArray		*m_array;
	gint		a_array_card;
	public:
	GArray*&							array()			{ return m_array;		}
	gint&								len()			{ return a_array_card;	}

	gvfs_file*	element(gint i)	{ return g_array_index(m_array, gvfs_file*, i);	}

	void	append(gvfs_file *file)
			{
				g_return_if_fail( file != NULL );
				g_array_append_val(m_array, file);
				a_array_card++;
			}

	public:
	void*		operator new	(size_t size, GnomeVFSURI* parent_uri, gchar *parent_path) ;
	void		operator delete (void *p);
};


//*****************************************************************************
//								Functions
//*****************************************************************************
gboolean		GVFS_qstack_initialized();
void			GVFS_qstack_initialize();
void			GVFS_qstack_destroy();

GnomeVFSURI*	GVFS_uri_new(const gchar *text);
gboolean		GVFS_info_from_uri(GnomeVFSURI* uri, GnomeVFSFileInfo* info);

gboolean		GVFS_vfsinfo_has_type_directory (GnomeVFSFileInfo *info);
gboolean		GVFS_vfsinfo_has_type_symlink   (GnomeVFSFileInfo *info);

void			GVFS_async_cancel_all();

void			GVFS_async_load_subdirectories(
					GnomeVFSURI					*parent_uri,
					gchar						*parent_path,
					gvfs_async_caller_data		*cd,
					gint						max_result,
					gboolean					follow_links);




#endif //__GCMDGTKFOLDVIEW_GVFS_H__
