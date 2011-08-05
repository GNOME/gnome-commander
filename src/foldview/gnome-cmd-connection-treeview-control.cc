/*
    ###########################################################################

    gnome-cmd-connection-treeview-control.cc

    ---------------------------------------------------------------------------

    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak
    Copyright (C) 2010-2011 Guillaume Wardavoir

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

    Struct  : Control

    Parent  : GnomeCmdConnectionTreeview

    ###########################################################################
*/
#include    "gnome-cmd-connection-treeview.h"

#include    "gnome-cmd-file-selector.h"
#include    "gnome-cmd-main-win.h"


extern  GnomeCmdMainWin *main_win;

//  ***************************************************************************
//
//  Constructor, ...
//
//  ***************************************************************************
GnomeCmdConnectionTreeview::Control::Control(
    GnomeCmdConnectionTreeview  *   _ctv)
{
    raz_pointers();
    init_instance(_ctv);
}

GnomeCmdConnectionTreeview::Control::~Control()
{
    message_fifo_stop();
    sorting_list_stop();
}

void
GnomeCmdConnectionTreeview::Control::raz_pointers()
{
    a_connection_treeview   = NULL;

    d_message_fifo_first    = NULL;
    d_message_fifo_last     = NULL;
    a_message_fifo_card     = 0;
    a_message_fifo_id       = 0;

    d_sorting_list_first    = NULL;
    d_sorting_list_last     = NULL;
    a_sorting_list_card     = 0;
    a_sorting_list_id       = 0;
}
void
GnomeCmdConnectionTreeview::Control::init_instance(GnomeCmdConnectionTreeview * _ctv)
{
    a_connection_treeview   = _ctv;

	if ( ! message_fifo_start() )
    {
        GCMD_ERR("Control::create():Message FIFO didnt start");
    }

	if ( ! sorting_list_start() )
    {
        GCMD_ERR("Control::create():Sorting list didnt start");
    }
}

void GnomeCmdConnectionTreeview::Control::dispose()
{
	GCMD_INF("GnomeCmdConnectionTreeview::Control::dispose()");
}

void GnomeCmdConnectionTreeview::Control::finalize()
{
	GCMD_INF("GnomeCmdConnectionTreeview::Control::finalize()");
}


//  ***************************************************************************
//								Signals
//  ***************************************************************************
void
GnomeCmdConnectionTreeview::Control::Signal_button_refresh_clicked(
    GtkButton * _button,
    gpointer    _user_data)
{
	Control		*   ctv = NULL;
	//.........................................................................
	//GCMD_INF("view:Signal_refresh_clicked");

	ctv = ((View::HeadBand*)_user_data)->view()->control();

	ctv->model()->iter_refresh(NULL);
}
void
GnomeCmdConnectionTreeview::Control::Signal_button_sort_clicked(
    GtkButton * _button,
    gpointer    _user_data)
{
	Control		*   ctv = NULL;
	//.........................................................................
	//GCMD_INF("view:Signal_sort_clicked");

	ctv = ((View::HeadBand*)_user_data)->view()->control();

	ctv->model()->iter_sort(NULL);
}


void
GnomeCmdConnectionTreeview::Control::Signal_button_show_hide_clicked(
    GtkButton * _button,
    gpointer    _user_data)
{
	Control		*   ctv = NULL;
	//.........................................................................
	ctv = ((View::HeadBand*)_user_data)->view()->control();

    if ( ctv->view()->treeview_showed() )
    {
        ctv->view()->hide_treeview();
        ctv->connection_treeview()->set_packing_expansion(FALSE);
    }
    else
    {
        ctv->view()->show_treeview();
        ctv->connection_treeview()->set_packing_expansion(TRUE);
    }
}

void
GnomeCmdConnectionTreeview::Control::Signal_button_close_clicked(
    GtkButton * _button,
    gpointer    _user_data)
{
	Control		*   ctv = NULL;
	//.........................................................................
	ctv = ((View::HeadBand*)_user_data)->view()->control();

    ctv->connection_treeview()->close();
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							Contextual menu								  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ***************************************************************************
//
//  							The menu
//
//  ***************************************************************************

GnomeCmdConnectionTreeview::Control::ctx_menu GnomeCmdConnectionTreeview::Control::Context_menu =
{
	NULL, (gint)3,
	{
		{
			_("Directory"), (gint)2,
			{
				// context				connect		title						callback
				{{ GCMD_B8(00000111),  TRUE,		_("Open in active tab"),	G_CALLBACK(Set_active_tab)		},	NULL, (gulong)0  },
				{{ GCMD_B8(00000111),  TRUE,		_("Open in a new tab"),		G_CALLBACK(Open_new_tab)		},	NULL, (gulong)0  },
				{{ GCMD_B8(00000000),  FALSE,		_(""),						NULL									},	NULL, (gulong)0  },
				{{ GCMD_B8(00000000),  FALSE,		_(""),						NULL									},	NULL, (gulong)0  },
				{{ GCMD_B8(00000000),  FALSE,		_(""),						NULL									},	NULL, (gulong)0  },
			}
		},

		{
			_("Tree"), (gint)4,
			{
				//connect   title						callback
				{{ GCMD_B8(00000111),  TRUE,		_("Create new tree"),		G_CALLBACK(Tree_create)			},	NULL, (gulong)0  },
				{{ GCMD_B8(00001111),  TRUE,		_("Delete this tree"),		G_CALLBACK(Tree_delete)			},	NULL, (gulong)0  },
				{{ GCMD_B8(00000111),  TRUE,		_("Refresh from item"),		G_CALLBACK(Refresh)				},	NULL, (gulong)0  },
				{{ GCMD_B8(00000111),  TRUE,		_("Sort from item"),		G_CALLBACK(Sort)				},	NULL, (gulong)0  },
				{{ GCMD_B8(00000000),  FALSE,		_(""),						NULL									},	NULL, (gulong)0  },
			}
		},

		{
			_("Treeview"), (gint)2,
			{
				//connect   title						callback
				{{ GCMD_B8(00000111),  FALSE,		_("Sync treeview"),			G_CALLBACK(Sync_treeview)		},  NULL, (gulong)0	 },
				{{ GCMD_B8(10000000),  FALSE,		_("Unsync treeview"),		G_CALLBACK(Unsync_treeview)		},  NULL, (gulong)0	 },
				{{ GCMD_B8(00000000),  FALSE,		_(""),						NULL									},	NULL, (gulong)0  },
				{{ GCMD_B8(00000000),  FALSE,		_(""),						NULL									},	NULL, (gulong)0  },
				{{ GCMD_B8(00000000),  FALSE,		_(""),						NULL									},	NULL, (gulong)0  },
			}
		}
	}
};

void GnomeCmdConnectionTreeview::Control::context_menu_populate_add_separator(GtkWidget *widget)
{
	gtk_menu_shell_append(GTK_MENU_SHELL(widget), gtk_separator_menu_item_new());
}

void GnomeCmdConnectionTreeview::Control::context_menu_pop(
	View::ctx_menu_data		*ctxdata)
{
	gboolean				b_inserted		= FALSE;
	gint					is,ie;
	eContext				context			= eNothing;

	GtkTreeIter				iter_clicked	= Model::Iter_zero;
	GtkTreeIter				iter_selected	= Model::Iter_zero;

	//.........................................................................

	//
	// paths -> iters & logic
	//
	if ( ctxdata->d_path_clicked )
		if ( model()->get_iter(ctxdata->d_path_clicked, &iter_clicked) )
			context = (eContext)(context | eClicked);

	if ( ctxdata->d_path_selected )
		if ( model()->get_iter(ctxdata->d_path_selected, &iter_selected) )
			context = (eContext)(context | eSelected);

	if ( context == eBoth )
		if ( model()->iter_same(&iter_clicked, &iter_selected) )
			context = (eContext)(context | eBothEqual);

	if ( context & eSelected )
		if ( model()->iter_is_root(&iter_selected) )
			context = (eContext)(context | eSelectedIsRoot);

	//
	// create the popup & populate it
	//
	Context_menu.a_widget = gtk_menu_new();

	for ( is = 0 ; is != Context_menu.a_card ; is++ )
	{
		ctx_menu_section section	= Context_menu.a_section[is];
		b_inserted					= FALSE;

		for ( ie = 0 ; ie != section.a_card ; ie++ )
		{
			ctx_menu_entry  entry   = section.a_entry[ie];
			ctx_menu_desc   desc	= entry.a_desc;

			// context does not match, quit
			if ( ( desc.context() & context ) != desc.context() )
				if ( ! ( desc.context() & eAlways ) )
					continue;

			// eventually add a separator
			if ( !b_inserted && ie == 0 && is != 0 )
			{
				b_inserted = TRUE;
				gtk_menu_shell_append(GTK_MENU_SHELL(Context_menu.a_widget), gtk_separator_menu_item_new());
			}

			// add the entry
			entry.a_widget  = gtk_menu_item_new_with_label(desc.a_text);

			// dont disconnect current handler, coz these are new widgets
			// at each popup

			if ( ! desc.a_sensitive )
				gtk_widget_set_sensitive(entry.a_widget,FALSE);
			else
			{
				// connect with new ctx data
				entry.a_handle = g_signal_connect(
					entry.a_widget,
					"activate",
					desc.a_callback,
					(gpointer)ctxdata);
			}

			// add to menu
			gtk_menu_shell_append(GTK_MENU_SHELL(Context_menu.a_widget), entry.a_widget);
		}
	}

	gtk_widget_show_all(Context_menu.a_widget);

	// pop
	// void gtk_menu_popup(
	//	GtkMenu					*menu,
	//	GtkWidget				*parent_menu_shell,
	//	GtkWidget				*parent_menu_item,
	//	GtkMenuPositionFunc		func,
	//	gpointer				data,
	//	guint					button,
	//	guint32					activate_time);
	//
	//	menu				:	a GtkMenu.
	//	parent_menu_shell   :	the menu shell containing the triggering menu item, or NULL. [allow-none]
	//	parent_menu_item	:	the menu item whose activation triggered the popup, or NULL. [allow-none]
	//	func				:	a user supplied function used to position the menu, or NULL. [allow-none]
	//	data				:	user supplied data to be passed to func. [allow-none]
	//	button				:	the mouse button which was pressed to initiate the event.
	//	activate_time		:	the time at which the activation event occurred.
	gtk_menu_popup(
		GTK_MENU(Context_menu.a_widget), NULL, NULL,
		NULL, NULL,
		ctxdata->a_button, ctxdata->a_time);
}

//  ***************************************************************************
//
//  							The actions
//
//  ***************************************************************************
void GnomeCmdConnectionTreeview::Control::Set_active_tab(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->set_active_tab(ctxdata->d_path_selected);
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::set_active_tab(GtkTreePath *_path)
{
	GnomeCmdFileSelector	*   fs				= NULL;
    GnomeVFSURI             *   uri             = NULL;
	GnomeCmdPath			*   path			= NULL;
	GnomeCmdDir				*   dir				= NULL;

	GtkTreeIter					iter			= Model::Iter_zero;

	Model::Row				*   row				= NULL;
	//.........................................................................
	//GCMD_INF("control_open_active_tab");

	fs = main_win->fs(ACTIVE);
	if ( ! GNOME_CMD_IS_FILE_SELECTOR (fs) )
		return;

	if ( ! model()->get_iter(_path, &iter) )
		return;

	row = model()->iter_get_treerow(&iter);
	g_return_if_fail( row );

	// create a temporary GnomeVFSURI for getting rid of scheme, using uri->text
    uri     = gnome_vfs_uri_new(row->uri_utf8());

    // special case of GnomeCmdConDevice, with mount point
    if ( is_con_device() )
	{
        gchar * v = g_utf8_offset_to_pointer(uri->text, connection_treeview()->con_device_mount_point_len());

        GCMD_WNG("Control::set_active_tab():path [%s]", v);

        path    = gnome_cmd_con_create_path(gnome_cmd_connection(), v);
    }
    // normal case
    else
    {
        path    = gnome_cmd_con_create_path(gnome_cmd_connection(), uri->text);
    }

    // go
	dir	    = gnome_cmd_dir_new(gnome_cmd_connection(), path);
	fs->file_list()->set_connection(gnome_cmd_connection(), dir);
    gnome_vfs_uri_unref(uri);
}
//  ===========================================================================
void GnomeCmdConnectionTreeview::Control::Open_new_tab(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->open_new_tab(ctxdata->d_path_selected);
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::open_new_tab(GtkTreePath *_path)
{
	//GnomeCmdDir		*   dir			= NULL;
	//GnomeCmdPath	*   path		= NULL;

	GtkTreeIter			iter		= Model::Iter_zero;
	GtkTreeIter			iter_tree	= Model::Iter_zero;

	Model::Row		*   rw			= NULL;
	Model::Row		*   rt			= NULL;
	//.........................................................................
	//GCMD_INF("control_open_new_tab");

	if ( !model()->get_iter(_path, &iter) )
		return;

	if ( !model()->iter_get_root(&iter, &iter_tree) )
		return;

	rw =						model()->iter_get_treerow(&iter);
	rt = model()->iter_get_treerow(&iter_tree);
	g_return_if_fail( rt->is_root() );

	// open the new tab
	////path = gnome_cmd_con_create_path(trt->connection(), trw->uri());
	////dir	 = gnome_cmd_dir_new(trt->connection(), path);
	////main_win->fs(ACTIVE)->new_tab(dir, TRUE);
}
//  ===========================================================================
void GnomeCmdConnectionTreeview::Control::Tree_create(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->tree_create(ctxdata);
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::tree_create(GnomeCmdConnectionTreeview::View::ctx_menu_data *ctxdata)
{
	GtkTreeIter		iter_in			= Model::Iter_zero;
	GtkTreeIter		iter_tree		= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//GCMD_INF("control_tree_create");

	if ( !model()->get_iter(ctxdata->d_path_selected, &iter_in) )
		return;

	model()->iter_add_tree(&iter_in, &iter_tree);
	model()->iter_check_if_empty(&iter_tree);
}
//  ===========================================================================
void GnomeCmdConnectionTreeview::Control::Tree_delete(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->tree_delete(ctxdata);
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::tree_delete(GnomeCmdConnectionTreeview::View::ctx_menu_data *ctxdata)
{
	//GtkTreeIter		iter_in			= Model::Iter_zero;
	GtkTreeIter		iter_tree		= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	GCMD_INF("Control::control_tree_delete()");

	model()->get_iter(ctxdata->d_path_selected, &iter_tree);
	model()->iter_remove(&iter_tree);                                              // _GWR_SEND_MSG_
}
//  ===========================================================================
void GnomeCmdConnectionTreeview::Control::Refresh(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->refresh(ctxdata);
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::refresh(GnomeCmdConnectionTreeview::View::ctx_menu_data *ctxdata)
{
	GtkTreeIter		iter			= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//GCMD_INF("control_refresh");

	if ( !model()->get_iter(ctxdata->d_path_selected, &iter) )
		return;

	model()->iter_refresh(&iter);
}
//  ===========================================================================
void GnomeCmdConnectionTreeview::Control::Sort(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->sort(ctxdata);
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::sort(GnomeCmdConnectionTreeview::View::ctx_menu_data *ctxdata)
{
	GtkTreeIter		iter			= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//GCMD_INF("control_sort");

	if ( ! model()->get_iter(ctxdata->d_path_selected, &iter) )
		return;

	model()->iter_sort(&iter);
}
//  ===========================================================================
GnomeCmdConnectionTreeview::Control::eSyncState
GnomeCmdConnectionTreeview::Control::sync_check()
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
//  ===========================================================================
void GnomeCmdConnectionTreeview::Control::Sync_update(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->sync_update(ctxdata);
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::sync_update(GnomeCmdConnectionTreeview::View::ctx_menu_data *ctxdata)
{
	GnomeVFSURI		*uri			= NULL;
	GtkTreeIter		iter			= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//GCMD_INF("control_sync_update");

	if ( sync_check() != SYNC_Y_LIST_Y )
	{
		//GCMD_WNG("control_sync_update:not SYNC_Y_LIST_Y:%03i", control_sync_check());
		return;
	}

	if ( !model()->get_iter(ctxdata->d_path_clicked, &iter) )
		return;

	uri = gnome_vfs_uri_new( model()->iter_get_uri(&iter) );
	if ( uri == NULL )
		return;

	////control_gcmd_file_list_set_connection(a_synced_list, uri);              // _GWR_STANDBY_
}
//  ===========================================================================
void GnomeCmdConnectionTreeview::Control::Unsync_treeview(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->unsync_treeview();
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::unsync_treeview()
{
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//GCMD_INF("control_unsync_treeview");

	switch ( sync_check() )
	{
		case	SYNC_N_LIST_N:
		GCMD_WNG("control_unsync_treeview:not synced");
		return;
		break;

		case	SYNC_N_LIST_Y:
		GCMD_ERR("control_unsync_treeview:* mismatch * SYNC_N_LIST_Y");
		break;

		case	SYNC_Y_LIST_Y:
		a_synced = FALSE;
		g_object_remove_weak_pointer( G_OBJECT(a_synced_list), (gpointer*)(&a_synced_list) );
		a_synced_list = NULL;
		break;

		case	SYNC_Y_LIST_N:
		GCMD_INF("control_unsync_treeview:SYNC_Y_LIST_N");
		a_synced = FALSE;
		break;
	}
}
//  ===========================================================================
void GnomeCmdConnectionTreeview::Control::Sync_treeview(
	GtkMenuItem				*menu_item,
	View::ctx_menu_data		*ctxdata)
{
	ctxdata->a_treeview->view()->control()->sync_treeview(ctxdata);
	delete ctxdata;
}
void GnomeCmdConnectionTreeview::Control::sync_treeview(GnomeCmdConnectionTreeview::View::ctx_menu_data *ctxdata)
{
	GnomeCmdFileList	*list			= NULL;
	GnomeVFSURI			*uri			= NULL;
	GtkTreeIter			iter			= Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//GCMD_INF("control_sync_treeview");

	// We use weak pointers on the GnomeCmdFileList so if the user close
	// the tab, we can know it coz the weak pointer is set to NULL by glib

	if ( !model()->get_iter(ctxdata->d_path_selected, &iter) )
	{
		GCMD_WNG("control_sync_treeview:no selected item");
		return;
	}

	uri = gnome_vfs_uri_new( model()->iter_get_uri(&iter) );
	if ( uri == NULL )
	{
		GCMD_WNG("control_sync_treeview:iter_get_uri_new failed");
		return;
	}

	list = main_win->fs(ACTIVE)->file_list();
	if ( list == NULL )
	{
		GCMD_WNG("control_sync_treeview:coudnot get file_list");
		return;
	}

	switch ( sync_check() )
	{
		// synchronize with the given list
		case	SYNC_N_LIST_N:
		a_synced		= TRUE;
		a_synced_list   = list;

		g_object_add_weak_pointer( G_OBJECT(list), (gpointer*)(&a_synced_list) );

		////control_gcmd_file_list_set_connection(a_synced_list, uri);          // _GWR_STANDBY_
		break;

		// we are synchronized with a file_list that was deleted
		// simply synchronize with the given list
		case	SYNC_Y_LIST_N:
		GCMD_WNG("control_sync_treeview:SYNC_Y_LIST_N");

		a_synced		= TRUE;
		a_synced_list   = list;

		g_object_add_weak_pointer( G_OBJECT(list), (gpointer*)(&a_synced_list) );

		////control_gcmd_file_list_set_connection(a_synced_list, uri);          // _GWR_STANDBY_
		break;

		case	SYNC_Y_LIST_Y:
		GCMD_WNG("control_sync_treeview:already synced");
		break;

		case	SYNC_N_LIST_Y:
		GCMD_ERR("control_unsync_treeview:* mismatch * SYNC_N_LIST_Y");
		break;
	}

	// free temp uri
	gnome_vfs_uri_unref(uri);
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							    Messages                                  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ***************************************************************************
//
//                          Messages : queue
//
//  ***************************************************************************
//
//  We use a GList :
//
//      Push                             Pop
//      +---+                           +---+
//      |   |                           |   |
//      v   |                           v   |
//
//      x   x---x---x---x---x---x---x---x---x
//          ^                               ^
//          |                               |
//        Last                            First

//GStaticMutex Message_queue_mutex = G_STATIC_MUTEX_INIT;

//		GThread				*		a_message_queue_thread;
//		GList				*		d_message_queue_list;

void GnomeCmdConnectionTreeview::Control::message_fifo_lock()
{
	//g_static_mutex_lock(&Message_queue_mutex);
}
void GnomeCmdConnectionTreeview::Control::message_fifo_unlock()
{
	//g_static_mutex_unlock(&Message_queue_mutex);
}

//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Control::message_fifo_start()
{
	d_message_fifo_first        = g_list_alloc();
    d_message_fifo_first->data  = NULL;

	d_message_fifo_last         = d_message_fifo_first;

    a_message_fifo_card         = 0;

	a_message_fifo_id = g_timeout_add(GCMDGTKFOLDVIEW_REFRESH_CYCLE_PERIOD_MS, Message_fifo_func_timeout, (gpointer)this);
	if ( a_message_fifo_id == 0 )
		return FALSE;

   	//a_message_queue_thread = g_thread_create(
	//	Message_queue_func_thread,
	//	(gpointer)this,
	//	FALSE,
	//	NULL);

	return TRUE;
}
gboolean
GnomeCmdConnectionTreeview::Control::message_fifo_stop()
{
    g_source_remove(a_message_fifo_id);
	return TRUE;
}

//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Control::message_fifo_push(
    Model::MsgCore*    _msg)
{
	message_fifo_lock();

    if ( ! _msg )
        goto lab_failure;

    d_message_fifo_last->data   = (gpointer)_msg;
	d_message_fifo_last         = g_list_prepend(d_message_fifo_last, NULL);

    a_message_fifo_card++;
    //FIFO_INF("FIFO:message_fifo_push():Total    :%04i", d_message_fifo_count);

lab_exit:
	sorting_list_unlock();
	return TRUE;

lab_failure:
	sorting_list_unlock();
	return FALSE;
}

GnomeCmdConnectionTreeview::Model::MsgCore*
GnomeCmdConnectionTreeview::Control::message_fifo_pop()
{
	Model::MsgCore *   msg	 = NULL;
    //.........................................................................
	message_fifo_lock();

	msg	= (Model::MsgCore*)d_message_fifo_first->data;

    // message : change d_message_fifo_first
    if ( msg )
    {
        d_message_fifo_first = d_message_fifo_first->prev;
        g_list_free1(d_message_fifo_first->next);

        a_message_fifo_card--;
        //FIFO_INF("FIFO:message_fifo_pop() :Remaining:%04i", d_message_fifo_count);
    }

	message_fifo_unlock();

	return msg;
}

//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Control::Message_fifo_func_timeout(
    gpointer _data)
{
	GtkTreeIter			    iter	= Model::Iter_zero;
	Control	            *   THIS	= NULL;
	Model::MsgCore	    *   msg		= NULL;
    guint                   i       = 0;
    //.........................................................................
	THIS	= (Control*)_data;

	//FIFO_INF("Model::FIFO:Called");

    for ( i = 0 ; i != GCMDGTKFOLDVIEW_REFRESH_CYCLE_N_MESSAGES ; i++ )
    {
        THIS->message_fifo_lock();
        msg = THIS->message_fifo_pop();
        THIS->message_fifo_unlock();

        if ( ! msg )
            break;

        FIFO_INF("message loop       :( %03i of %03i ) [%s]",
            (1 + i),
            GCMDGTKFOLDVIEW_REFRESH_CYCLE_N_MESSAGES,
            Model::Msg_name(msg->type()));


        switch ( msg->type() )
        {

        case Model::eAddFirstTree   :
        THIS->iter_message_add_first_tree( (Model::MsgAddFirstTree*)msg );
        break;

        case Model::eAddChild       :
        THIS->iter_message_add_child( (Model::MsgAddChild*)msg );
        break;

        case Model::eAddDummyChild  :
        THIS->iter_message_add_dummy_child( (Model::MsgAddDummyChild*)msg );
        break;

        case Model::eDel            :
        THIS->iter_message_del( (Model::MsgDel*)msg );
        break;

        case Model::eSetReadable    :
        THIS->iter_message_set_readable( (Model::MsgSetReadable*)msg );
        break;

        case Model::eSetNotReadable :
        THIS->iter_message_set_not_readable( (Model::MsgSetNotReadable*)msg );
        break;

        case Model::eAsyncMismatchIEFUC     :
        THIS->iter_message_AsyncMismatchIEFUC( (Model::MsgAsyncMismatchIEFUC*)msg );
        break;

        case Model::eAsyncMismatchICIEC     :
        THIS->iter_message_AsyncMismatchICIEC( (Model::MsgAsyncMismatchICIEC*)msg );
        break;

        default:
        break;
        }
    }


	return TRUE;
}

//  ***************************************************************************
//
//                          Messages : actions
//
//  ***************************************************************************

// ============================================================================
// iter_message_add_first_tree
// ============================================================================
//
// Create the first iter
//
gboolean
GnomeCmdConnectionTreeview::Control::iter_message_add_first_tree(
    Model::MsgAddFirstTree     *   _msg)
{
    Model::IterInfo         info;
    Model::Row          *   row_new         = NULL;
	//.........................................................................
    Model::Lock();                                                              // _GDK_LOCK_

    //
	//  Step #1 : Test iter :
    //              - existenz
    //
    if ( ! info.gather(model(), _msg->path(), IterInfo::eNothing) )
    {
        MSG_WNG("iter_message_add_first_tree():gather failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

	//.........................................................................
	//
	//  Step #2 : Check if directory added is empty / not empty
	//
    MSG_INF("iter_message_add_first_tree():adding %s", _msg->file()->name_utf8() );

    if ( _msg->file()->is_symlink() )
    {
        row_new = new Model::Row(
            Model::eRowRoot, info.row()->uri_utf8(), _msg->file()->name_utf8(), (gchar*)((Model::Symlink*)_msg->file())->target_uri(),
            _msg->file()->access(), TRUE,
            is_con_samba(), is_con_local(), host_redmond());
    }
    else
    {
        row_new = new Model::Row(
            Model::eRowRoot, info.row()->uri_utf8(), _msg->file()->name_utf8(), NULL,
            _msg->file()->access(), FALSE,
            is_con_samba(), is_con_local(), host_redmond());
    }

	// replace treerow by the new one, signal will be emitted
	model()->iter_set_treerow(info.iter(), row_new);

	// launch check if empty
    model()->iter_check_if_empty(info.iter());

	//.........................................................................
	//
	//  Exit
	//
lab_exit_true:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
	Model::Unlock();															// _GDK_LOCK_
	return TRUE;

lab_exit_false:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
	Model::Unlock();															// _GDK_LOCK_
	return FALSE;
}


/// ==========================================================================
/// iter_message_add_child()
/// ==========================================================================
///
/// Add a child to an iter
///
gboolean
GnomeCmdConnectionTreeview::Control::iter_message_add_child(
    Model::MsgAddChild     *   _msg)
{
    Model::IterInfo         info;
    GtkTreeIter             iter_child      = Model::Iter_zero;
	//.........................................................................
    Model::Lock();                                                              // _GDK_LOCK_

    //
	//  Step #1 : Test iter :
    //              - readable
    //              - expanded
    //
    if ( ! info.gather(model(), _msg->path(), IterInfo::eRead | IterInfo::eExp | IterInfo::eChildren | IterInfo::eFCID) )
    {
        MSG_WNG("iter_message_add_child():gather failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

    if ( info.collapsed() )
    {
        MSG_WNG("iter_message_add_child():iter is collapsed");                  // _GWR_CHECK_ arrow after expand
        Error_Msg_Abort();      goto lab_exit_false;
    }

    if ( info.first_child_is_dummy() )
    {
        MSG_INF("iter_message_add_child():replacing dummy %s", _msg->file()->name_utf8() );
        model()->iter_files_add_file(info.iter(), &iter_child, _msg->file(), FALSE, TRUE);
    }
    else
    {
        MSG_INF("iter_message_add_child():adding %s", _msg->file()->name_utf8() );
        model()->iter_files_add_file(info.iter(), &iter_child, _msg->file(), FALSE, FALSE);
    }

	//.........................................................................
	//
	//  Step #2 : Check if directory added is empty / not empty
	//
    if ( Access_readable(_msg->file()->access()) )
        model()->iter_check_if_empty(&iter_child);

	//.........................................................................
	//
	//  Exit
	//
lab_exit_true:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
	Model::Unlock();															// _GDK_LOCK_
	return TRUE;

lab_exit_false:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
	Model::Unlock();															// _GDK_LOCK_
	return FALSE;
}

/// ==========================================================================
/// iter_message_add_dummy_child()
/// ==========================================================================
///
/// Add a dummy child to an iter
///
gboolean
GnomeCmdConnectionTreeview::Control::iter_message_add_dummy_child(
    Model::MsgAddDummyChild    *   _msg)
{
    Model::IterInfo         info;
    GtkTreeIter             iter_dummy      = Model::Iter_zero;
	//.........................................................................
    Model::Lock();                                                              // _GDK_LOCK_

    //.........................................................................
    //
	//  Step #1 : Test iter :
    //              - readable
    //              - no children, except one dummy child
    //
    if ( ! info.gather(model(), _msg->path(), IterInfo::eRead | IterInfo::eFCID) )
    {
        MSG_WNG("iter_message_add_dummy_child():Gather failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

    if ( ! info.readable() )
    {
        MSG_WNG("iter_message_add_dummy_child():NOT Readable");
        Error_Msg_Abort();      goto lab_exit_false;
    }

    if ( info.children() )
    {
        if ( info.children() > 1 )
        {
            MSG_WNG("iter_message_add_dummy_child():iter has more than one child");
            goto lab_exit_false;
        }
        if ( ! info.first_child_is_dummy() )
        {
            MSG_WNG("iter_message_add_dummy_child():iter has an unique child, but not a dummy");
            goto lab_exit_false;
        }
        // here already dummy child, silently exir
        goto lab_exit_true;
    }

    //.........................................................................
    //
	//  Step #1 : Add the dummy child
    //

    //MSG_INF("iter_message_add_dummy_child():adding");
    model()->iter_dummy_child_add(info.iter(), &iter_dummy);

	//.........................................................................
	//
	//  Exit
	//
lab_exit_true:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
	Model::Unlock();			        										// _GDK_LOCK_
	return TRUE;
lab_exit_false:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
	Model::Unlock();			        										// _GDK_LOCK_
	return FALSE;
}


/// ==========================================================================
/// iter_message_del()
/// ==========================================================================
///
/// Delete an iter
///
gboolean
GnomeCmdConnectionTreeview::Control::iter_message_del(
	Model::MsgDel   *   _msg)
{
    Model::IterInfo         info;
	//.........................................................................
    Model::Lock();                                                              // _GDK_LOCK_

    //
	//  Step #1 : Test iter :
    //              - must exist
    //

    if ( ! info.gather(model(), _msg->path(), IterInfo::eExp | IterInfo::eChildren | IterInfo::eGtkPath ) )
    {
        MSG_WNG("iter_message_del():Gather failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

	//.........................................................................
	// iter is sterile : goto remove_sterile
	if (  ! info.children() )
		goto remove_sterile;

	//.........................................................................
	// iter is fertile, and is expanded
	if ( info.expanded() )
	{
		// It is a hack ! Let GtkTreeView update its cache by collapsing !
		// We dont have to manually delete each row and signals on them !
		if ( ! view()->row_collapse(info.gtk_path()) )							// __GWR__HACK__
		{
			GCMD_ERR("iter_message_del():iter could not be collapsed");
			goto lab_exit_false;
		}

		// We dont return here, since the iter is in the following state !
	}

	//.........................................................................
	// iter is fertile, and is not expanded. Silently remove all children.
	// After that, we are in the following case.
	model()->iter_collapsed_remove_children(info.iter());

	//.........................................................................
	// iter is sterile :remove it, and send "removed row" signal. But first
	// stop monitoring
remove_sterile:
	model()->iter_monitor_stop(info.iter());

	if ( model()->treestore()->ext_iter_sterile_remove(info.iter()) < 0 )
    {
		GCMD_ERR("iter_message_del():ext_iter_sterile_remove < 0");
        goto lab_exit_false;
    }

	//.........................................................................
	//
	//  Exit
	//
lab_exit_true:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
	Model::Unlock();															// _GDK_LOCK_
	return TRUE;

lab_exit_false:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
	Model::Unlock();															// _GDK_LOCK_
	return FALSE;
}

/// ==========================================================================
/// iter_message_set_readable()
/// ==========================================================================
///
/// Set an iter readable
///
gboolean
GnomeCmdConnectionTreeview::Control::iter_message_set_readable(
    Model::MsgSetReadable      *   _msg)
{
    Model::IterInfo info;
	//.........................................................................
    Model::Lock();                                                              // _GDK_LOCK_

	//.........................................................................
	// Step #1 : Test iter : iter must :
    //              - be not readable
    //              - be not expanded
    //              - have no child
    //
    if ( ! info.gather(model(), _msg->path() , IterInfo::eExp | IterInfo::eChildren) )
    {
        MSG_WNG("iter_message_set_readable():Gather failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

    // already readable : abort
    if ( info.readable() )
    {
		MSG_WNG("iter_message_set_readable():iter already readable [%s]", info.row()->utf8_name_display());
		Error_Msg_Abort();      goto lab_exit_false;
	}

    if ( info.expanded() )
    {
		MSG_WNG("iter_message_set_readable():iter is expanded");
		Error_Msg_Abort();      goto lab_exit_false;
    }

    if ( info.children() )
    {
		MSG_WNG("iter_message_set_readable():iter has children");
		Error_Msg_Abort();      goto lab_exit_false;
	}

	//.........................................................................
	// All is ok

	// change data
	info.row()->readable(TRUE);

	// inform treview that row has changed
	model()->treestore()->ext_data_changed(info.iter());

	// launch check if empty
	model()->iter_check_if_empty(info.iter());

	//.........................................................................
lab_exit_true:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
    Model::Unlock();                                                            // _GDK_LOCK_
	return TRUE;

lab_exit_false:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
    Model::Unlock();                                                            // _GDK_LOCK_
	return FALSE;
}

/// ==========================================================================
/// iter_message_set_not_readable()
/// ==========================================================================
///
/// Set an iter not readable
///
gboolean
GnomeCmdConnectionTreeview::Control::iter_message_set_not_readable(
    Model::MsgSetNotReadable   *   _msg)
{
    Model::IterInfo     info;
	//.........................................................................
    Model::Lock();                                                              // _GDK_LOCK_

    //.........................................................................
    //
	//  Step #1 : Test iter : iter must :
    //              - be readable
    //
    if ( ! info.gather(model(), _msg->path(), IterInfo::eRead | IterInfo::eExp | IterInfo::eGtkPath | IterInfo::eChildren) )
    {
        MSG_WNG("iter_message_set_not_readable():Gather failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

    // already not-readable : abort
    if ( ! info.readable() )
    {
		MSG_WNG("iter_message_set_not_readable():already not-readable [%s]", info.row()->utf8_name_display());
        Error_Msg_Abort();      goto lab_exit_false;
	}

    //.........................................................................
    //
	//  Step #2 : Go !
    //

	// if no children, unset directly
	if ( ! info.children() )
		goto lab_iter_sterile;

	//.........................................................................
	// iter is fertile, and expanded : collapse it
	if ( info.expanded() )
	{
		// It is a hack ! Let GtkTreeView update its cache by collapsing !
		// We dont have to manually delete each row and signals on them !
		if ( ! view()->row_collapse( info.gtk_path() ) )                        // __GWR__HACK__
		{
			MSG_ERR("iter_message_set_not_readable():iter could not be collapsed");
		}

		// We dont return here, since the iter is in the following state !
	}

	//.........................................................................
	// iter is fertile, and collapsed : silently remove children
	model()->iter_collapsed_remove_children(info.iter());

	//.........................................................................
	// Here iter is sterile
lab_iter_sterile:

	// change data
	info.row()->readable(FALSE);

	// inform treview that row has changed
	model()->treestore()->ext_data_changed(info.iter());

	//.........................................................................
lab_exit_true:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
    Model::Unlock();                                                            // _GDK_LOCK_
	return TRUE;

lab_exit_false:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
    Model::Unlock();                                                            // _GDK_LOCK_
	return FALSE;
}

/// ==========================================================================
/// iter_message_AsyncMismatchIEFUC()
/// ==========================================================================
///
/// ...
///
gboolean
GnomeCmdConnectionTreeview::Control::iter_message_AsyncMismatchIEFUC(
    Model::MsgAsyncMismatchIEFUC    * _msg)
{
    Model::IterInfo     info;
	//.........................................................................
    Model::Lock();                                                              // _GDK_LOCK_

    //.........................................................................
    //
	//  Step #1 : Test iter : iter must
    //              - be expanded
    //              - dummychilded
    //
    if ( ! info.gather(model(), _msg->path(), IterInfo::eRead | IterInfo::eExp | IterInfo::eFCID) )
    {
        MSG_WNG("iter_message_AsyncMismatchIEFUC():Gather failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

    if ( ! info.first_child_is_dummy() )
    {
        MSG_WNG("iter_message_AsyncMismatchIEFUC():Not dummychilded");
        Error_Msg_Abort();      goto lab_exit_false;
    }

    if ( ! info.expanded() )
    {
        MSG_WNG("iter_message_AsyncMismatchIEFUC():Not expanded");
        Error_Msg_Abort();      goto lab_exit_false;
    }

    if ( ! info.readable() )
    {
        MSG_WNG("iter_message_AsyncMismatchIEFUC():Not readable");
        Error_Msg_Abort();      goto lab_exit_false;
    }

    //.........................................................................
    //
	//  Step #2 : Show warning icon
    //
    info.row()->icon(eIconWarning);
    model()->treestore()->ext_data_changed(info.iter());

	//.........................................................................
    //
    //  Exit
    //
lab_exit_true:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
    Model::Unlock();                                                            // _GDK_LOCK_
	return TRUE;
lab_exit_false:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
    Model::Unlock();                                                            // _GDK_LOCK_
	return FALSE;
}
/// ==========================================================================
/// iter_message_AsyncMismatchICIEC()
/// ==========================================================================
///
/// ...
///
gboolean
GnomeCmdConnectionTreeview::Control::iter_message_AsyncMismatchICIEC(
    Model::MsgAsyncMismatchICIEC    * _msg)
{
    Model::IterInfo     info;
	//.........................................................................
    Model::Lock();                                                              // _GDK_LOCK_

    //.........................................................................
    //
	//  Step #1 : Test iter : iter must
    //              - exist
    //
    if ( ! info.gather(model(), _msg->path(), IterInfo::eNothing) )
    {
        MSG_WNG("iter_message_AsyncMismatchIEFUC():Gather failed");
        Error_Msg_Failure();    goto lab_exit_false;
    }

    //.........................................................................
    //
	//  Step #2 : Show warning icon
    //
    info.row()->icon(eIconWarning);
    model()->treestore()->ext_data_changed(info.iter());

	//.........................................................................
    //
    //  Exit
    //
lab_exit_true:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
    Model::Unlock();                                                            // _GDK_LOCK_
	return TRUE;
lab_exit_false:
    delete _msg;
    model()->treestore()->ext_dump_tree(NULL);
    Model::Unlock();                                                            // _GDK_LOCK_
	return FALSE;
}
//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							    Sorting                                   #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ***************************************************************************
//
//                      Sorting : list & timeout function
//
//  ***************************************************************************
//
//  We use a GList :
//
//       Add ( outside code )        control code
//      +---+                           +---+
//      |   |                           |   |
//      v   |                           v   |
//
//      x   x---x---x---x---x---x---x---x---x
//          ^                               ^
//          |                               |
//        Last                            First
//

//GStaticMutex Sorting_list_mutex = G_STATIC_MUTEX_INIT;

//		GThread				*		a_sorting_list_thread;

void GnomeCmdConnectionTreeview::Control::sorting_list_lock()
{
	//g_static_mutex_lock(&Message_queue_mutex);
}
void GnomeCmdConnectionTreeview::Control::sorting_list_unlock()
{
	//g_static_mutex_unlock(&Message_queue_mutex);
}
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Control::sorting_list_start()
{
	d_sorting_list_first        = g_list_alloc();
	d_sorting_list_first->data  = NULL;

	d_sorting_list_last         = d_sorting_list_first;

    a_sorting_list_card     = 0;

	a_sorting_list_id = g_timeout_add(GCMDGTKFOLDVIEW_SORTING_CYCLE_PERIOD_MS, Sorting_list_func_timeout, (gpointer)this);
	if ( a_sorting_list_id == 0 )
		return FALSE;

   	//a_message_queue_thread = g_thread_create(
	//	Message_queue_func_thread,
	//	(gpointer)this,
	//	FALSE,
	//	NULL);

	return TRUE;
}
gboolean
GnomeCmdConnectionTreeview::Control::sorting_list_stop()
{
    g_source_remove(a_sorting_list_id);
	return TRUE;
}
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Control::sorting_list_add(
    Model::MsgCore*    _msg)
{
	sorting_list_lock();

    if ( ! _msg )
        goto lab_failure;

    d_sorting_list_last->data   = (gpointer)_msg;
    a_sorting_list_card++;

	d_sorting_list_last         = g_list_prepend(d_sorting_list_last, NULL);

    //FIFO_INF("FIFO:message_fifo_push():Total    :%04i", d_message_fifo_count);

lab_exit:
	sorting_list_unlock();
	return TRUE;

lab_failure:
	sorting_list_unlock();
	return FALSE;
}
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Control::Sorting_list_func_timeout(
    gpointer _data)
{
	GtkTreeIter			    iter	= Model::Iter_zero;
	Control	            *   THIS	= NULL;
	Model::IMsgSort	    *   msg		= NULL;

    guint                   i       = 0;
    GList               *   list    = NULL;
    GList               *   temp    = NULL;
    //.........................................................................
	THIS	= (Control*)_data;

	//SORT_INF(" --> Sort Function : %03i sort messages", THIS->a_sorting_list_card);

    THIS->sorting_list_lock();

    i       = 1;
    list    = THIS->d_sorting_list_first;

    while ( list->data )
    {
        //SORT_INF(" --> Sort function <--");

        msg = (Model::IMsgSort*)(list->data);

        //SORT_INF(" --> Sort function :sorting msg ( %03i of %03i )",
        //    i,
        //    THIS->a_sorting_list_card);

        // something happend...
        if ( msg->run() )
        {
            //  ...in all cases, we have to delete the message :
            //
            //              <== way
            //
            //         .
            //         .
            //         .
            //    last .                        first
            //      |  .                          |
            //      v  .                          v
            //      x-----x-----x-----x-----x-----x
            //      ^  .  ^
            //      |  .  |
            //     NULL. msg
            //         .
            //

            // ...something bad
            if ( msg->error() )
            {
                SORT_ERR(" --> Sort function :sorting failed");
            }

            // ...something good
            if ( msg->done() )
            {
                SORT_TKI(" --> Sort function :sorting done");
            }

            // delete message
            delete msg;

            // delete link
            if ( list == THIS->d_sorting_list_first )
            {
                THIS->d_sorting_list_first = THIS->d_sorting_list_first->prev;
                g_list_free1(list);
                temp = THIS->d_sorting_list_first;
            }
            else
            {
                temp = g_list_previous(list);
                list->prev->next = list->next;
                list->next->prev = list->prev;
                g_list_free1(list);
            }

            // prepare next round
            list    = temp;
            i++;
        }
        // nothing happend
        else
        {
            // prepare next round
            list = g_list_previous(list);
            i++;
        }
    }

    THIS->sorting_list_unlock();

    return TRUE;
}

//  ***************************************************************************
//
//                      Sorting : IMsgSort
//
//  ***************************************************************************

GnomeCmdConnectionTreeview::Model::IMsgSort::IMsgSort(
    TreestorePath   *   _path,
    Model           *   _model) : MsgCore(_path)
{
    a_step          = eInit;
    a_iteration     = 0;
    a_initialized   = FALSE;
    a_error         = FALSE;
    a_done          = FALSE;

    a_model     = _model;

    d_list_original = NULL;
    d_list_actual   = NULL;
    a_list_card     = 0;
}

GnomeCmdConnectionTreeview::Model::IMsgSort::~IMsgSort()
{
    list_delete(&d_list_original);
    list_delete(&d_list_actual);
}


//  ===========================================================================
//  List manipulation
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::IMsgSort::list_changed()
{
    GList   *   l1  = NULL;
    GList   *   l2  = NULL;
    //.........................................................................
    if ( ! d_list_actual || ! d_list_original )
        return TRUE;

    l1 = d_list_original;   // since we prepend, it is the first element
    l2 = d_list_actual;     // ...idem...

    while ( TRUE )
    {
        // one NULL
        if ( ! l1 || ! l2 )
        {
            // two NULL : no change
            if ( l1 == l2 )
            {
                //SORT_TEK("IMsgSort::list_changed():l1 [%08x]->[%08x] l2 [%08x]->[%08x]", l1, l1 ? l1->data : 0, l2, l2 ? l2->data : 0);
                return FALSE;
            }

            // one NULL, one not NULL : change
            //if ( l1 ) SORT_TEK("IMsgSort::list_changed():l1 [%08x]->[%08x] l2 [%08x]->[%08x]", l1, l1 ? l1->data : 0, l2, l2 ? l2->data : 0);
            //if ( l2 ) SORT_TEK("IMsgSort::list_changed():l1 [%08x]->[%08x] l2 [%08x]->[%08x] l2+[%08x]", l1, l1 ? l1->data : 0, l2, l2 ? l2->data : 0, l2->next);
            return TRUE;
        }

        // here two are not NULL ; if data differ : change
        //SORT_TEK("IMsgSort::list_changed():l1 [%08x]->[%08x] l2 [%08x]->[%08x]", l1, l1 ? l1->data : 0, l2, l2 ? l2->data : 0);

        if ( l1->data != l2->data )
            return TRUE;

        // loop
        l1 = g_list_next(l1);
        l2 = g_list_next(l2);
    }

    return FALSE;
}
void
GnomeCmdConnectionTreeview::Model::IMsgSort::list_delete(
    GList   **  _list)
{
    GList   *   list    = NULL;
    //.........................................................................
    if ( ! _list )
        return;

    list = *_list;

    // list = g_list_first(_list);
    //while ( list )
    //{
    //    delete (TreestorePath*)(list->data);
    //    list = g_list_next(list);
    //}

    g_list_free(*_list);
    *_list = NULL;
}
gboolean
GnomeCmdConnectionTreeview::Model::IMsgSort::list_get(
    GList ** _list)
{
    Model::IterInfo         info;
    GtkTreeIter             child   = Iter_zero;
    guint                   count   = 0;
    //.........................................................................
    //
    // get & verify info on iter
    //
    if ( ! info.gather(a_model, path(), IterInfo::eRead | IterInfo::eExp | IterInfo::eChildren) )
    {
        SORT_TKE("IMsgSort::list_update():Gather failed");
        return FALSE;
    }

    if  (
            ! info.readable()   ||
            ! info.expanded()
        )
    {
        SORT_TKI("IMsgSort::list_update():Iter is not sortable");
        return FALSE;
    }

    //
    // delete the list
    //
    list_delete(_list);

    //
    // build the list from model
    //
    if ( ! GnomeCmdFoldviewTreestore::iter_children(model()->treemodel(), &child, info.iter()) )
        return FALSE;

    do
    {
        *_list = g_list_prepend
            (
                *_list,
                (gpointer)(model()->iter_get_treerow(&child))
            );

        count++;
    }
    while ( GnomeCmdFoldviewTreestore::iter_next(model()->treemodel(), &child) );

    //SORT_TEK("IMsgSort::list_update():%03i children [%08x] [%08x] [%08x]", count, g_list_last((*_list)), g_list_last((*_list))->prev, g_list_last((*_list))->next);

    return TRUE;
}
void
GnomeCmdConnectionTreeview::Model::IMsgSort::list_original_copy(
    GList ** _dest)
{
    if ( *_dest )
    {
        SORT_TKE("IMsgSort::list_original_copy():destion list not NULL");
        return;
    }

    // affect the list
    *_dest = g_list_copy(d_list_original);
}
//  ===========================================================================
//  Run
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::IMsgSort::run()
{

    //.........................................................................
    if ( a_step == eInit )
    {
        // build list_original
        if ( ! list_get(&d_list_original) )
        {
            SORT_TKI("IMsgSort::run(): [init   ] list_get(original) failed - aborting");
            a_step = eAbort;
            goto lab_exit;
        }

        a_list_card = g_list_length(d_list_original);

        // build list_actual
        list_delete(&d_list_actual);
        list_original_copy(&d_list_actual);

        if ( ! init() )
        {
            SORT_TKE("IMsgSort::run(): [init   ] init() failed - aborting");
            a_step = eAbort;
            goto lab_exit;
        }

        a_initialized = TRUE;

        a_step = eRun;

        return 0;
    }

    //.........................................................................
    if ( a_step == eRun )
    {
        // build list_actual
        if ( ! list_get(&d_list_actual) )
        {
            SORT_TKI("IMsgSort::run(): [run    ] list_get(actual) failed - aborting");
            a_step = eAbort;
            goto lab_exit;
        }

        // if list is modified, restart the sort process
        if ( list_changed() )
        {
            SORT_TKI("IMsgSort::run(): [run    ] list changed - restarting");
            a_step = eRestart;
            goto lab_exit;
        }

        // else do an iteration
        a_iteration++;

        if ( step() )
        {
            a_step = eDone;
            goto lab_exit;
        }
    }

    //.........................................................................
    if ( a_step == eRestart )
    {
        list_delete(&d_list_original);
        list_delete(&d_list_actual);

        a_iteration     = 0;
        a_initialized   = FALSE;
        a_error         = FALSE;
        a_done          = FALSE;

        d_list_original = NULL;
        d_list_actual   = NULL;
        a_list_card     = 0;

        // restart the sort
        if ( ! restart() )
        {
            SORT_TKE("IMsgSort::run(): [restart] sort restart failed - aborting");
            a_step = eAbort;
            goto lab_exit;
        }

        a_step = eInit;
        goto lab_exit;
    }

    //.........................................................................
    if ( a_step == eDone )
    {
        list_delete(&d_list_original);
        list_delete(&d_list_actual);

        a_done  = TRUE;
        a_step  = eNop;

        return TRUE;
    }

    //.........................................................................
    if ( a_step == eAbort )
    {
        list_delete(&d_list_original);
        list_delete(&d_list_actual);

        a_error = TRUE;
        a_step  = eNop;

        return TRUE;
    }

    //.........................................................................
    if ( a_step == eNop )
    {
        SORT_TKW("IMsgSort::run(): [nop    ] running for nothing !!!");
        return TRUE;
    }

    //.........................................................................
lab_exit:
    return FALSE;
}
//  ===========================================================================
//  Spread
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::Model::IMsgSort::spread()
{
    SORT_INF("IMsgSort::spread():not implemented");
    return TRUE;
}



