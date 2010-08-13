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
#include	"gnome-cmd-plain-path.h"
#include	"gnome-cmd-con.h"
#include	"gnome-cmd-con-smb.h"
#include	"gnome-cmd-con-device.h"
#include	"gnome-cmd-con-list.h"

#include	"gnome-cmd-foldview-private.h"
#include	"gnome-cmd-foldview-gvfs.h"

//  ***************************************************************************
//  *																		  *
//  *								Defines								      *
//  *																		  *
//  ***************************************************************************
//#define DEBUG_CCIE
//#define DEBUG_CIE

//  ***************************************************************************
//  *																		  *
//  *								Helpers								      *
//  *																		  *
//  ***************************************************************************

// Logging for "Control Check If Empty" functions & callbacks
#ifdef DEBUG_CCIE

	#define CCIE_INF(...)													\
	{																		\
		gwr_gvfs_inf(__VA_ARGS__);											\
	}
	#define CCIE_WNG(...)													\
	{																		\
		gwr_gvfs_wng(__VA_ARGS__);											\
	}
	#define CCIE_ERR(...)													\
	{																		\
		gwr_gvfs_err( __VA_ARGS__);											\
	}

#else

	#define CCIE_INF(...)
	#define CCIE_WNG(...)
	#define CCIE_ERR(...)

#endif

// Logging for "Control Iter Expand" functions & callbacks
#ifdef DEBUG_CIE

	#define CIE_INF(...)													\
	{																		\
		gwr_gvfs_inf(__VA_ARGS__);											\
	}
	#define CCE_WNG(...)													\
	{																		\
		gwr_gvfs_wng(__VA_ARGS__);											\
	}
	#define CIE_ERR(...)													\
	{																		\
		gwr_gvfs_err( __VA_ARGS__);											\
	}

#else

	#define CIE_INF(...)
	#define CIE_WNG(...)
	#define CIE_ERR(...)

#endif




//=============================================================================
//  Common vars
//=============================================================================
GcmdGtkFoldview::View::ctx_menu GcmdGtkFoldview:: s_context_menu =
{
	NULL, (gint)2,	
	{
		{
			_("Directory"), (gint)3,
			{
				//connect   title						callback
				{{ TRUE,	_("Open in active tab"),	G_CALLBACK(GcmdGtkFoldview::Control_set_active_tab)		},	NULL, (gulong)0  },
				{{ TRUE,	_("Open in a new tab"),		G_CALLBACK(GcmdGtkFoldview::Control_open_new_tab)		},	NULL, (gulong)0  },
				{{ FALSE,	_("Set as new root"),		G_CALLBACK(GcmdGtkFoldview::Control_set_new_root)		},	NULL, (gulong)0  },
				{{ FALSE,	_("nothing"),				NULL													},  NULL, (gulong)0  },
				{{ FALSE,	_("nothing"),				NULL													},  NULL, (gulong)0  }
			}
		},
		{
			_("Treeview"), (gint)2,
			{
				//connect   title						callback
				{{ FALSE,	_("Sync treeview"),			G_CALLBACK(GcmdGtkFoldview::Control_sync_treeview)	},  NULL, (gulong)0	},
				{{ FALSE ,	_("Unsync treeview"),		G_CALLBACK(GcmdGtkFoldview::Control_unsync_treeview)},  NULL, (gulong)0	},
				{{ FALSE,	_("nothing"),				NULL												},  NULL, (gulong)0 },
				{{ FALSE,	_("nothing"),				NULL												},  NULL, (gulong)0 },
				{{ FALSE,	_("nothing"),				NULL												},  NULL, (gulong)0 }
			}
		}
	}
};

//=============================================================================
//  init_instance, ...
//=============================================================================
void GcmdGtkFoldview::control_raz_pointers()
{
	// sync
	a_synced_list		= NULL;
	a_synced			= FALSE;
}
void GcmdGtkFoldview::control_init_instance()
{
	gwr_inf("TEST (avoiding gcc warnings at compilation)");
	gwr_wng("TEST (avoiding gcc warnings at compilation)");
	gwr_err("TEST (avoiding gcc warnings at compilation)");

	control_raz_pointers();

	model.init_instance();
	view.init_instance(GTK_WIDGET(this), model.treemodel());

	if ( !GVFS_qstack_initialized() )
		GVFS_qstack_initialize();

	control_create();

	root_uri_set_1((gchar*)"/");
}
gboolean GcmdGtkFoldview::control_create()
{
	return TRUE;
}
void GcmdGtkFoldview::control_destroy()
{
}

//=============================================================================
//	Contextual menu
//=============================================================================
void GcmdGtkFoldview::control_context_menu_populate_add_separator(GtkWidget *widget)
{
	gtk_menu_shell_append(GTK_MENU_SHELL(widget), gtk_separator_menu_item_new());
}

void GcmdGtkFoldview::control_context_menu_populate_add_section(
	GtkWidget				*widget,
	gint					i,
	View::ctx_menu_data		*ctxdata)
{
	gint j = 0;
	View::ctx_menu_section  *s = &( s_context_menu.a_section[i] );
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	for ( j = 0 ; j != s->a_card ; j++ )
	{
		View::ctx_menu_entry  *e = &( s->a_entry[j] );

		e->a_widget  = gtk_menu_item_new_with_label(e->a_desc.a_text);

		// disconnect current handler
		// -> no coz these are new widgets at each popup

		if ( ! e->a_desc.a_connect )
		{
			gtk_widget_set_sensitive(e->a_widget,FALSE);
		}
		else
		{
			// connect with new ctx data
			e->a_handle = g_signal_connect(
				e->a_widget, 
				"activate", 
				e->a_desc.a_callback, 
				(gpointer)ctxdata);
		}

		// add to menu
		gtk_menu_shell_append(GTK_MENU_SHELL(widget), e->a_widget);
	}
}

void GcmdGtkFoldview::Control_set_active_tab(
	GtkMenuItem				*menu_item, 
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_foldview->control_set_active_tab(ctxdata->d_path_selected);
	delete ctxdata;
}
void GcmdGtkFoldview::control_set_active_tab(GtkTreePath *path)
{
	GnomeCmdFileSelector	*fs		= NULL;
	GnomeVFSURI				*uri	= NULL;
	GtkTreeIter				iter	= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//gwr_inf("control_open_active_tab");

	fs = main_win->fs(ACTIVE);
	if ( ! GNOME_CMD_IS_FILE_SELECTOR (fs) )
		return;

	// get the path from the item selectionned in the treeview
	if ( !model.get_iter(path, &iter) )
		return;

	uri = model.iter_get_uri_new(&iter);
	if ( uri == NULL )
		return;

	control_gcmd_file_list_set_connection(fs->file_list(), uri);

	gnome_vfs_uri_unref(uri);
}

void GcmdGtkFoldview::Control_open_new_tab(
	GtkMenuItem				*menu_item, 
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_foldview->control_open_new_tab(ctxdata->d_path_selected);
	delete ctxdata;
}
void GcmdGtkFoldview::control_open_new_tab(GtkTreePath *path)
{
	GnomeVFSURI		*uri			= NULL;
	GnomeCmdPath	*gnomecmdpath   = NULL;
	GnomeCmdDir		*gnomecmddir	= NULL;
	GtkTreeIter		iter			= Model::Iter_zero;

	//gwr_inf("control_open_new_tab");

	if ( !model.get_iter(path, &iter) )
		return;

	uri = model.iter_get_uri_new(&iter);

	if ( uri == NULL )
		return;

	// open the new tab
	gnomecmdpath		= gnome_cmd_plain_path_new(uri->text);
	gnomecmddir			= gnome_cmd_dir_new(model.connection(), gnomecmdpath);
	main_win->fs(ACTIVE)->new_tab(gnomecmddir, TRUE);

	// free temp objects
	gnome_vfs_uri_unref(uri);
}

void GcmdGtkFoldview::Control_set_new_root(
	GtkMenuItem				*menu_item, 
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_foldview->control_set_new_root(ctxdata);
	delete ctxdata;
}
void GcmdGtkFoldview::control_set_new_root(GcmdGtkFoldview::View::ctx_menu_data *ctxdata)
{
	GnomeVFSURI		*uri			= NULL;
	GtkTreeIter		iter			= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//gwr_inf("control_set_new_root");

	if ( !model.get_iter(ctxdata->d_path_selected, &iter) )
		return;

	uri = model.iter_get_uri_new(&iter);

	if ( uri == NULL )
		return;

	// set new root
	control_root_uri_set(uri);

	// free temp objects
	gnome_vfs_uri_unref(uri);
}

GcmdGtkFoldview::eSyncState GcmdGtkFoldview::control_sync_check()
{
	if ( !a_synced )
	{
		if ( !a_synced_list )
		{
			return SYNC_N_LIST_N;
		}
		else
		{
			return SYNC_N_LIST_Y;
		}
	}
	else
	{
		if ( !a_synced_list )
		{
			return SYNC_Y_LIST_N;
		}
		else
		{
			return SYNC_Y_LIST_Y;
		}
	}
}

void GcmdGtkFoldview::Control_sync_update(
	GtkMenuItem				*menu_item, 
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_foldview->control_sync_update(ctxdata);
	delete ctxdata;
}
void GcmdGtkFoldview::control_sync_update(GcmdGtkFoldview::View::ctx_menu_data *ctxdata)
{
	GnomeVFSURI		*uri			= NULL;
	GtkTreeIter		iter			= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//gwr_inf("control_sync_update");
	
	if ( control_sync_check() != SYNC_Y_LIST_Y )
	{
		//gwr_wng("control_sync_update:not SYNC_Y_LIST_Y:%03i", control_sync_check());
		return;
	}

	if ( !model.get_iter(ctxdata->d_path_clicked, &iter) )
		return;

	uri = model.iter_get_uri_new(&iter);
	if ( uri == NULL )
		return;
	
	control_gcmd_file_list_set_connection(a_synced_list, uri);
}

void GcmdGtkFoldview::Control_unsync_treeview(
	GtkMenuItem				*menu_item, 
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_foldview->control_unsync_treeview();
	delete ctxdata;
}
void GcmdGtkFoldview::control_unsync_treeview()
{
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//gwr_inf("control_unsync_treeview");

	switch ( control_sync_check() )
	{
		case	SYNC_N_LIST_N:
		gwr_wng("control_unsync_treeview:not synced");
		return;
		break;

		case	SYNC_N_LIST_Y:
		gwr_err("control_unsync_treeview:* mismatch * SYNC_N_LIST_Y");
		break;

		case	SYNC_Y_LIST_Y:
		a_synced = FALSE;
		g_object_remove_weak_pointer( G_OBJECT(a_synced_list), (gpointer*)(&a_synced_list) );
		a_synced_list = NULL;
		break;

		case	SYNC_Y_LIST_N:
		gwr_inf("control_unsync_treeview:SYNC_Y_LIST_N");
		a_synced = FALSE;
		break;
	}
}

void GcmdGtkFoldview::Control_sync_treeview(
	GtkMenuItem				*menu_item, 
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_foldview->control_sync_treeview(ctxdata);
	delete ctxdata;
}
void GcmdGtkFoldview::control_sync_treeview(GcmdGtkFoldview::View::ctx_menu_data *ctxdata)
{
	GnomeCmdFileList	*list			= NULL;
	GnomeVFSURI			*uri			= NULL;
	GtkTreeIter			iter			= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//gwr_inf("control_sync_treeview");

	// We use weak pointers on the GnomeCmdFileList so if the user close
	// the tab, we can know it coz the weak pointer is set to NULL by glib

	if ( !model.get_iter(ctxdata->d_path_selected, &iter) )
	{
		gwr_wng("control_sync_treeview:no selected item");
		return;
	}

	uri = model.iter_get_uri_new(&iter);
	if ( uri == NULL )
	{
		gwr_wng("control_sync_treeview:iter_get_uri_new failed");
		return;
	}

	list = main_win->fs(ACTIVE)->file_list();
	if ( list == NULL )
	{
		gwr_wng("control_sync_treeview:coudnot get file_list");
		return;
	}

	switch ( control_sync_check() )
	{
		// synchronize with the given list
		case	SYNC_N_LIST_N:
		a_synced		= TRUE;
		a_synced_list   = list;

		g_object_add_weak_pointer( G_OBJECT(list), (gpointer*)(&a_synced_list) );

		control_gcmd_file_list_set_connection(a_synced_list, uri);
		break;

		// we are synchronized with a file_list that was deleted
		// simply synchronize with the given list
		case	SYNC_Y_LIST_N:
		gwr_wng("control_sync_treeview:SYNC_Y_LIST_N");

		a_synced		= TRUE;
		a_synced_list   = list;

		g_object_add_weak_pointer( G_OBJECT(list), (gpointer*)(&a_synced_list) );

		control_gcmd_file_list_set_connection(a_synced_list, uri);
		break;

		case	SYNC_Y_LIST_Y:
		gwr_wng("control_sync_treeview:already synced");
		break;

		case	SYNC_N_LIST_Y:
		gwr_err("control_unsync_treeview:* mismatch * SYNC_N_LIST_Y");
		break;
	}

	// free temp uri
	gnome_vfs_uri_unref(uri);
}


//=============================================================================
//	control_iter_expand
//
//  - Find all subdirs
//  - For each subdir : call control_check_if_empty
//=============================================================================

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Iter expansion static callback									MULTITHREAD
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MULTITHREAD

void GcmdGtkFoldview::control_iter_expand_callback_1(gvfs_async *ga)
{
	gvfs_async_load_subdirs				*ls			= NULL;
	gvfs_file							*file		= NULL;

	GcmdGtkFoldview						*foldview   = NULL;

	gint								i			= 0;
	gint								count		= 0;

	GtkTreeIter							child_iter			= GcmdGtkFoldview::Model::Iter_zero;

	GtkTreePath							*parent_path_gtk	= NULL;
	GtkTreeIter							parent_iter			= GcmdGtkFoldview::Model::Iter_zero;

	gboolean							b					= FALSE;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	gdk_threads_enter();														// _GDK_LOCK_
	
	foldview		= GcmdFoldview();

	ls				= (gvfs_async_load_subdirs*)(ga->ad());

	count			= ls->len();

	parent_path_gtk = gtk_tree_path_new_from_string(ls->ppath());

	if ( !foldview->model.get_iter(parent_path_gtk, &parent_iter) )
	{
		CIE_ERR("ciec_1:gtk_tree_model_get_iter failed for parent !");
		return;
	}
	gtk_tree_path_free(parent_path_gtk);

	CIE_INF("ciec_1:[0x%16x][0x%16x]", &parent_iter, &child_iter);
	// check is name is the same												// __GWR__TODO__

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// loop 1 : add children
	for ( i = 0 ; i != count ; i++ )
	{
		file = ls->element(i);
		CIE_INF("ciec_1:%s", file->name());		

		// determine icon number for the file element
		GcmdGtkFoldview::View::eIcon	icon	= GcmdGtkFoldview::View::eIconUnknown;
		if ( file->is_dir() )
		{
			// Thanks GVFS
			if ( file->flagged_symlink() )
			{
				icon = GcmdGtkFoldview::View::Icon_from_type_access(
					GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK,
					file->access());
			}
			else
			{
				icon = GcmdGtkFoldview::View::Icon_from_type_access(
					GNOME_VFS_FILE_TYPE_DIRECTORY,
					file->access());
			}
		}

		// add to the model
		if ( i == 0 )
		{
			foldview->model.iter_replace_first_child(&parent_iter, &child_iter, file->name(), icon);
		}
		else
		{
			foldview->model.iter_add_child(&parent_iter, &child_iter, file->name(), icon);
		}

		// NOTHING here ! Because the sort function mix all - iter or path !!!
		//child_uri = foldview->model.iter_get_uri_new(child_iter);
		//foldview->control_check_if_empty(child_iter);
		//gnome_vfs_uri_unref(child_uri);	
	}

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// loop 2 : check if children are empty or not
#ifdef __GTS__
	b = gtk_tree_model_iter_children( foldview->model.treemodel(), &child_iter, &parent_iter);
	while ( b )
	{
		foldview->control_check_if_empty(&child_iter);
		b = gtk_tree_model_iter_next( foldview->model.treemodel(), &child_iter);
	}
#else
	b = GnomeCmdFoldviewTreestore::iter_children( foldview->model.treemodel(), &child_iter, &parent_iter);
	while ( b )
	{
		foldview->control_check_if_empty(&child_iter);
		b = GnomeCmdFoldviewTreestore::iter_next( foldview->model.treemodel(), &child_iter);
	}
#endif


	// delete user data
	delete ga->cd();

	gdk_threads_leave();														// _GDK_LOCK_
}
//  MULTITHREAD

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Iter expansion private call
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void GcmdGtkFoldview::control_iter_expand_p(
	GnomeVFSURI *parent_uri,
	gchar		*parent_path)
{
	gvfs_async_caller_data		*cd	 = NULL;
	//control_user_data			*ud	 = NULL;

	//ud = struct_control_user_data::New(this, parent);

	cd = new (control_iter_expand_callback_1, NULL)gvfs_async_caller_data;

	GVFS_async_load_subdirectories(parent_uri, parent_path, cd,-1,TRUE);		// _ASYNC_CALL_
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Iter expansion public call
//  - Remove the [DUMMY] item if present ( no dummy doesnt imply no subdirs ;
//	  things can change between two users clicks... )
//  - Add the subdirs
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
GcmdGtkFoldview::control_iter_expand(
	GtkTreeIter *parent)
{
	GnomeVFSURI		*parent_uri;
	gchar			*parent_path;
	GtkTreePath		*temp_path;
	gint			n;

	// Ensure that there is only one subitem ( the [DUMMY] one ( we hope ) )
	n = model.iter_n_children(parent);
	if ( n != 1 )
	{
		CIE_ERR("ccie   :control_iter_expand:parent has %i children", n);
		return;
	}

	// new uri that will be destroyed at the end of GVFS callback
	parent_uri = model.iter_get_uri_new(parent);

	// string representing the GtkTreePath of the parent ; we dont use 
	// a GtkTreePath directly,since it is a GtkObject and maybe invalided
	// when GVFS callback code will run. Neither use we any GtkTreeIter.
#ifdef __GTS__
	temp_path   = gtk_tree_model_get_path(model.treemodel(), parent);
#else
	temp_path   = GnomeCmdFoldviewTreestore::get_path(model.treemodel(), parent);
#endif

	parent_path = gtk_tree_path_to_string(temp_path);
	gtk_tree_path_free(temp_path);

	control_iter_expand_p(parent_uri, parent_path);
}



//=============================================================================
//  control_check_if_empty static callback
//
//  - no   : add dummy element to the directory ( so the gtktreeview will show
//  an arrow )
//  - yes  : nop
//=============================================================================

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  result callback													MULTITHREAD
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MULTITHREAD

void GcmdGtkFoldview::control_check_if_empty_callback_1(gvfs_async *ga)
{
	gvfs_async_load_subdirs				*ls			= NULL;

	GcmdGtkFoldview						*foldview   = NULL;

	gint								count		= 0;

	gchar								*str		= NULL;

	GtkTreeIter							child_iter  = GcmdGtkFoldview::Model::Iter_zero;

	GtkTreePath							*parent_path_gtk	= NULL;
	GtkTreeIter							parent_iter	= GcmdGtkFoldview::Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	gdk_threads_enter();														// _GDK_LOCK_
	
	foldview		= GcmdFoldview();

	ls				= (gvfs_async_load_subdirs*)(ga->ad());

	count			= ls->len();

	// Since we are on an async call, GtkTreePath may points to something else	// __GWR__WARNING__
	// Ex : filesystem notification, user click...
	// This is a BIG problem. Maybe queue requests ?
	parent_path_gtk = gtk_tree_path_new_from_string(ls->ppath());

	if ( !foldview->model.get_iter(parent_path_gtk, &parent_iter) )
	{
		CCIE_ERR("cciec_1:gtk_tree_model_get_iter failed for parent !");
		return;
	}
	gtk_tree_path_free(parent_path_gtk);

	// check is name is the same												// __GWR__TODO__
	CCIE_INF("cciec_1:[0x%16x][0x%16x]", &parent_iter, &child_iter);
	if ( count > 0 )
	{
		foldview->model.iter_add_child(&parent_iter, &child_iter, "... Working ...", View::eIconUnknown);

		str = foldview->model.iter_get_string_new(&parent_iter);
		CCIE_INF("cciec_1:adding dummy to parent:[0x%16x]-[0x%16x]-[0x%16x][0x%16x][0x%16x] %s | %s",
			&parent_iter, parent_iter.stamp, parent_iter.user_data, parent_iter.user_data2, parent_iter.user_data3,
			str, ls->ppath());
		g_free(str);
	}
	else
	{
		str = foldview->model.iter_get_string_new(&parent_iter);
		CCIE_INF("cciec_1:adding dummy to parent:[0x%16x]-[0x%16x]-[0x%16x][0x%16x][0x%16x] %s | %s",
			&parent_iter, parent_iter.stamp, parent_iter.user_data, parent_iter.user_data2, parent_iter.user_data3,
			str, ls->ppath());
	}

	// delete user data
	delete ga->cd();

	gdk_threads_leave();	

}
//  MULTITHREAD



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  control_check_if_empty private call
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void GcmdGtkFoldview::control_check_if_empty_p(GnomeVFSURI *parent_uri, gchar *parent_path)
{
	//struct_control_user_data		*ud	 = NULL;
	gvfs_async_caller_data   *cd	 = NULL;

	//ud = struct_control_user_data::New(this, parent);

	cd = new(control_check_if_empty_callback_1, (gpointer)NULL) gvfs_async_caller_data;

	GVFS_async_load_subdirectories(parent_uri, parent_path, cd, 1,TRUE);		// _ASYNC_CALL_
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  control_check_if_empty public call
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void GcmdGtkFoldview::control_check_if_empty(GtkTreeIter *parent)
{
	GnomeVFSURI		*parent_uri;
	gchar			*parent_path;
	GtkTreePath		*temp_path;

	// new uri that will be destroyed at the end of GVFS callback
	parent_uri = model.iter_get_uri_new(parent);

	// string representing the GtkTreePath of the parent ; we dont use 
	// a GtkTreePath directly,since it is a GtkObject and maybe invalided
	// when GVFS callback code will run. Neither use we any GtkTreeIter.
#ifdef __GTS__
	temp_path   = gtk_tree_model_get_path(model.treemodel(), parent);
#else
	temp_path   = GnomeCmdFoldviewTreestore::get_path(model.treemodel(), parent);
#endif
	parent_path = gtk_tree_path_to_string(temp_path);

		gchar   *str = gtk_tree_path_to_string( temp_path );
		CCIE_INF("ccie   :control_check_if_empty:parent %s", str);
		g_free(str);


	gtk_tree_path_free(temp_path);

	control_check_if_empty_p(parent_uri, parent_path);
}

//============================================================================= // _GWR_TODO_ MONITORING
//  Item collapsed
//  - Remove all subdirs
//  - Add [DUMMY] ( we have been callapsed, so there were subdirs in there )
//  This is useful, because when re-expanding, directory will be re-scanned
//  and thus we'll be more accurate
//=============================================================================
void
GcmdGtkFoldview::control_iter_collapsed(
GtkTreeIter *parent)
{
	gint removed = 0;

	GtkTreeIter child;

	removed = model.iter_remove_children(parent);

	gwr_inf("control::iter_collapsed:removed %03i children", removed);

	// we have been collapsed, so we had children ; so re-add dummy child
	model.iter_add_child(parent, &child, "...Working...", View::eIconUnknown);
}
//=============================================================================
//  Divers
//=============================================================================
gboolean GcmdGtkFoldview::control_root_uri_set(GnomeVFSURI *uri)
{
	GtkTreeIter *parent = NULL;

	model.iter_remove_all();

	model.root.unset();

	if ( !model.root.set(uri) )
	{
		ERR_RET(FALSE,"control_root_uri_set:failed");
	}

	View::eIcon icon	= View::Icon_from_type_permissions(
			GNOME_VFS_FILE_TYPE_DIRECTORY,
			model.root.m_info->permissions);

	model.iter_add_child(parent, &model.root.m_iter, uri->text, icon);
	control_check_if_empty(&model.root.m_iter);

	return TRUE;
}

void GcmdGtkFoldview::control_gcmd_file_list_set_connection(
	GnomeCmdFileList	*list,
	GnomeVFSURI			*uri)
{
	GnomeCmdDir		*gnomecmddir	= NULL;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//gwr_inf("---------- control_gcmd_file_list_set_connection ----------");

	// connection is open
	if (model.connection()->state == CON_STATE_OPEN)
	{
		gnomecmddir = gnome_cmd_dir_new( model.connection(), gnome_cmd_con_create_path(model.connection(), uri->text) );

		//gnome_cmd_dir_log("control_gcmd_file_list_set_connection 1", gnomecmddir);

		// Avoid gcmd to call gtk_object_destroy on access-denied directories   // __GWR_TODO__
		// and lost+found in GnomeCmdFileList::on_dir_list_failed(...)
		//
		// The problem is that gcmd cant handle correctly such directories 
		// when calling GnomeCmdFileList::set_connection,set_directory,
		// goto_directory, ...I think you get the same result if you bookmark
		// (manually) such a directory
		//
		// Since gcmd increase GnomeCmdFile->ref_cnt until infinite (!), 
		// I feel allowed to call gnome_cmd_dir_ref rather than 
		// g_object_ref_sink :)
		//g_object_ref_sink(gnomecmddir);
		gnome_cmd_dir_ref(gnomecmddir);

		//gnome_cmd_dir_log("control_gcmd_file_list_set_connection 2", gnomecmddir);

		// call set_connection instead of set_/goto_ directory because
		// foldview has its own connection combo
		list->set_connection(model.connection(), gnomecmddir);
	}
	// connection is closed
	else
	{
		// if base_path, delete it
		if ( model.connection()->base_path )
			g_object_unref (model.connection()->base_path);

		// set connection base_path
		model.connection()->base_path = gnome_cmd_con_create_path(model.connection(), uri->text);
		g_object_ref(G_OBJECT(model.connection()->base_path));

		// go
		list->set_connection(model.connection(), NULL);
	}
}


void GcmdGtkFoldview::control_connection_combo_update()
{
	gboolean		found_my_con	= FALSE;
	GnomeCmdCombo   *combo			= GNOME_CMD_COMBO(view.connection_combo());

	view.connection_combo_reset();

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l = l->next)
    {
		GnomeCmdCon *con = (GnomeCmdCon *)l->data;

		// dont add connection if
		// it is closed AND it is not a device AND it is not smb
		if  (
				!gnome_cmd_con_is_open (con)		&& 
				!GNOME_CMD_IS_CON_DEVICE (con)		&&
				!GNOME_CMD_IS_CON_SMB (con)
			)  continue;

		// else add it
		view.connection_combo_add_connection(con);

		if ( con == model.connection())
			found_my_con = TRUE;
	}

	// If the connection is no longer available use the home connection
	if ( !found_my_con )
		model.connection(get_home_con());
	else
		combo->select_data(model.connection());
}



