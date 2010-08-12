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

#include	<libgnomevfs/gnome-vfs.h>
#include	<libgnomevfs/gnome-vfs-utils.h>

#include	"gnome-cmd-includes.h"

#include	"gnome-cmd-combo.h"
#include	"gnome-cmd-con-device.h"
#include	"gnome-cmd-con-smb.h"
#include	"gnome-cmd-con-list.h"

#include	"gnome-cmd-main-win.h"
#include	"gnome-cmd-style.h"
#include	"gnome-cmd-data.h"

#include	"gnome-cmd-foldview-private.h"

//  ***************************************************************************
//  *																		  *
//  *								Defines								      *
//  *																		  *
//  ***************************************************************************

//  ***************************************************************************
//  *																		  *
//  *								Helpers								      *
//  *																		  *
//  ***************************************************************************


//  ***************************************************************************
//  *																		  *
//  *					View ( GtkTreeview ) implementation					  *
//  *																		  *
//  ***************************************************************************
static void
myfunc(
	GtkTreeViewColumn *col,
	GtkCellRenderer   *renderer,
	GtkTreeModel      *model,
	GtkTreeIter       *iter,
	gpointer           user_data)
{
	gint icon;

	gtk_tree_model_get(model, iter, 1, &icon, -1);

	g_object_set(renderer, "pixbuf", GcmdFoldview()->view.pixbuf( (GcmdGtkFoldview::View::eIcon)icon), NULL);
}

//=============================================================================
//	init_instance, ...
//=============================================================================


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  icons
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gboolean GcmdGtkFoldview::View::icons_load()
{
	GError  *error		= NULL;

	m_pixbuf[eIconUnknown]			= gdk_pixbuf_new_from_file("../pixmaps/file-type-icons/file_type_unknown.xpm",		&error);

	m_pixbuf[eIconDirReadWrite]		= gdk_pixbuf_new_from_file("../pixmaps/file-type-icons/file_type_dir.xpm",	&error);
	m_pixbuf[eIconDirReadOnly]		= gdk_pixbuf_new_from_file("../pixmaps/file-type-icons/file_type_dir_orange.xpm",   &error);
	m_pixbuf[eIconDirForbidden]		= gdk_pixbuf_new_from_file("../pixmaps/file-type-icons/file_type_dir_red.xpm",		&error);

	m_pixbuf[eIconSymlinkToDirReadWrite]	= gdk_pixbuf_new_from_file("../pixmaps/file-type-icons/file_type_symlink_to_dir_green.xpm",		&error);
	m_pixbuf[eIconSymlinkToDirReadOnly]		= gdk_pixbuf_new_from_file("../pixmaps/file-type-icons/file_type_symlink_to_dir_orange.xpm",	&error);
	m_pixbuf[eIconSymlinkToDirForbidden]	= gdk_pixbuf_new_from_file("../pixmaps/file-type-icons/file_type_symlink_to_dir_red.xpm",		&error);

	return TRUE;
}

void GcmdGtkFoldview::View::icons_unload()
{
	g_object_unref(m_pixbuf[eIconUnknown]		);

	g_object_unref(m_pixbuf[eIconDirReadWrite]  );
	g_object_unref(m_pixbuf[eIconDirReadOnly]   );
	g_object_unref(m_pixbuf[eIconDirForbidden]  );

	g_object_unref(m_pixbuf[eIconSymlinkToDirReadWrite] );
	g_object_unref(m_pixbuf[eIconSymlinkToDirReadOnly]  );
	g_object_unref(m_pixbuf[eIconSymlinkToDirForbidden] );

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  init_instance
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void GcmdGtkFoldview::View::init_instance(GtkWidget *_this, GtkTreeModel *_treemodel)
{
	raz_pointers();
	create(_this, _treemodel);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  init
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void GcmdGtkFoldview::View::raz_pointers()
{
	a_size_request_handle   = 0;
	a_size_request_width	= 65;

	m_con_hbox				= NULL;
	m_con_combo				= NULL;
	m_vol_label				= NULL;
	m_scrolledwindow		= NULL;
	m_info_label			= NULL;

	m_treeview				= NULL;
	m_treemodel				= NULL;

	for ( gint i = 0 ; i != 50 ; i++ )  m_pixbuf[i] = NULL;

	a_this					= NULL;
}


void signal_cursor_changed(
	GtkTreeView		*tree_view,
	gpointer 	   user_data)
{
	GtkTreeSelection	*tree_sel		= NULL;
	gint				tree_sel_card   = 0;

	GtkTreeIter			iter_selected   = GcmdGtkFoldview::Model::s_iter_NULL;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//gwr_inf("signal_cursor_changed");

	//  get path to currently selected iter
	tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if ( ! tree_sel )
	{
		gwr_err("View::signal_cursor_changed:gtk_tree_view_get_selection failed");
	}

	// Note: gtk_tree_selection_count_selected_rows() does not
	//   exist in gtk+-2.0, only in gtk+ >= v2.2 !
	tree_sel_card = gtk_tree_selection_count_selected_rows(tree_sel);

	if ( tree_sel_card == 1 )
	{
		if ( gtk_tree_selection_get_selected( tree_sel, NULL, &iter_selected) )
		{

		}
		else
		{
			gwr_wng("View::signal_cursor_changed:gtk_tree_selection_get_selected failed");
		}
	}
	else
	{
		gwr_err("View::signal_cursor_changed");
	}
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  create / destroy
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gboolean GcmdGtkFoldview::View::create(GtkWidget *_this, GtkTreeModel *_treemodel)
{
	m_treemodel = _treemodel;
	a_this		= _this;

	//-------------------------------------------------------------------------
	// create composite widgets
	gtk_box_set_homogeneous(GTK_BOX(foldview()), FALSE);

	// create the box used for packing the con_combo and information
	m_con_hbox = create_hbox (GTK_WIDGET(foldview()), FALSE, 2);
	gtk_box_set_homogeneous(GTK_BOX(m_con_hbox), FALSE);

	// create the connection combo
	m_con_combo = (GtkWidget*)( new GnomeCmdCombo(2, 1, NULL) );

	gtk_object_set_data_full (GTK_OBJECT (foldview()), "con_combo", m_con_combo, (GtkDestroyNotify) g_object_unref);
	gtk_widget_set_size_request (m_con_combo, 150, -1);
		gtk_clist_set_row_height (GTK_CLIST (GNOME_CMD_COMBO (m_con_combo)->list), 20);
		gtk_entry_set_editable (GTK_ENTRY (GNOME_CMD_COMBO (m_con_combo)->entry), FALSE);
		gtk_clist_set_column_width (GTK_CLIST (GNOME_CMD_COMBO (m_con_combo)->list), 0, 20);
		gtk_clist_set_column_width (GTK_CLIST (GNOME_CMD_COMBO (m_con_combo)->list), 1, 60);
		GTK_WIDGET_UNSET_FLAGS (GNOME_CMD_COMBO (m_con_combo)->button, GTK_CAN_FOCUS);

	// create the free space on volume label
	//m_vol_label = gtk_label_new ("* LABEL *");
	//g_object_ref (m_vol_label);
	//gtk_object_set_data_full (GTK_OBJECT (m_this), "vol_label", m_vol_label, (GtkDestroyNotify) g_object_unref);
	//gtk_misc_set_alignment (GTK_MISC (m_vol_label), 1, 0.5);

		// __GWR__ Unable to reuse the gnome-cmd-dir-indicator since it is
		// crossed with gnome-cmd-file-selector
		// create the directory indicator
		//m_dir_indicator = gnome_cmd_dir_indicator_new(m_dir_indicator);
		//g_object_ref (m_dir_indicator);
		//gtk_object_set_data_full (GTK_OBJECT (foldview),
		//                         "dir_indicator", m_dir_indicator,
		//                         (GtkDestroyNotify) g_object_unref);

	// create the info label
	//m_info_label = gtk_label_new ("not initialized");
	//g_object_ref(m_info_label);
	//gtk_object_set_data_full (GTK_OBJECT (m_this), "infolabel", m_info_label,
	//	                      (GtkDestroyNotify) g_object_unref);
	//gtk_misc_set_alignment (GTK_MISC (m_info_label), 0.0f, 0.5f);

	// create the scrollwindow that we'll place the treeview in
	m_scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_object_set_data_full (GTK_OBJECT (foldview()),
		                      "scrolledwindow", m_scrolledwindow,
		                      (GtkDestroyNotify) g_object_unref);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (m_scrolledwindow),
		                            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	// create the pane that will be inserted in
	// done in gnome-cmd-main-win.cc now

	//-------------------------------------------------------------------------
	// create the treeview
	GtkTreeViewColumn		*col0		= NULL;
	GtkTreeViewColumn		*col1		= NULL;
	GtkCellRenderer			*renderer   = NULL;

	icons_load();

	m_treeview = gtk_tree_view_new();

	// Drag & Drop
	//
	//  void gtk_tree_view_enable_model_drag_source(
	//			GtkTreeView				*tree_view,
	//			GdkModifierType			start_button_mask,
	//			const GtkTargetEntry	*targets,
	//			gint					n_targets,
	//			GdkDragAction			actions);
	//
	//	typedef struct
	//	{
	//	  gchar *target;
	//	  guint  flags;
	//	  guint  info;
	//	}
	//  GtkTargetEntry;
	//
	//  typedef enum
	//  {
	//	  GTK_TARGET_SAME_APP = 1 << 0,    /*< nick=same-app >*/
	//	  GTK_TARGET_SAME_WIDGET = 1 << 1  /*< nick=same-widget >*/
	//  }
	//  GtkTargetFlags;

	//GtkTargetEntry  drag_source_entry[2]	=
	//	{
	//		{   (gchar*)"text/plain", 0, 0X30 },
	//		{   (gchar*)"text/plain", 0, 0X31 }
	//	};

	//gtk_tree_view_enable_model_drag_source(
	//	treeview(),
	//	GDK_BUTTON1_MASK,
 	//	drag_source_entry,
	//	2,
	//	GDK_ACTION_PRIVATE );

    //g_signal_connect(m_treeview, "drag-begin", G_CALLBACK (signal_drag_begin), (gpointer)this);

	// columns
	col0 = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col0, _("Folders"));
	gtk_tree_view_column_set_visible(col0,TRUE);

	col1 = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col1, _("Access Rights"));
	gtk_tree_view_column_set_visible(col1,FALSE);

	// column 0 - pixbuf renderer
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col0, renderer, FALSE);
	g_object_set(renderer, "xalign", 0.0, NULL);
	gtk_tree_view_column_set_cell_data_func(col0, renderer, myfunc, NULL, NULL);

	// column 0 - text renderer
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col0, renderer, FALSE);
	gtk_tree_view_column_add_attribute(col0, renderer, "text", 0);
	g_object_set(renderer, "xalign", 0.0, NULL);

	// column 1 - text renderer
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col1, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col1, renderer, "text", 1);
	g_object_set(renderer, "xalign", 0.0, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeview), col0);
	gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeview), col1);

	// signals
	g_signal_connect(m_treeview, "test_expand_row",		G_CALLBACK(signal_test_expand_row), NULL);
	g_signal_connect(m_treeview, "row_expanded",		G_CALLBACK(signal_row_expanded), NULL);
	g_signal_connect(m_treeview, "row_collapsed",		G_CALLBACK(signal_row_collapsed), NULL);

	g_signal_connect(m_treeview, "button-press-event",  G_CALLBACK(signal_button_press_event), foldview());

	g_signal_connect(m_treeview, "cursor-changed",  G_CALLBACK(signal_cursor_changed), NULL);

	// some settings
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(m_treeview), FALSE);
	gtk_tree_view_set_model(GTK_TREE_VIEW(m_treeview), m_treemodel);
	gtk_tree_view_set_show_expanders(treeview(), TRUE);

	// treeview contextual menu :
	// since we do a dynamic menu, and there is no menu_shell_remove()
	// function in gtk, we must create it on the fly, during the
	// callback for single-right-click

	//-------------------------------------------------------------------------
	// pack
	gtk_box_pack_start(GTK_BOX(m_con_hbox), m_con_combo, FALSE, FALSE, 0);
	//gtk_box_pack_start (GTK_BOX(m_con_hbox), m_vol_label, FALSE, FALSE, 0);

		 //gtk_box_pack_start (GTK_BOX (m_con_hbox), m_dir_indicator, FALSE, FALSE, 0);
		 //padding = create_hbox (*fs, FALSE, 6);
		 //gtk_box_pack_start (GTK_BOX (vbox), padding, FALSE, TRUE, 0);
		 //gtk_box_pack_start (GTK_BOX (padding), info_label, FALSE, TRUE, 6);

	gtk_box_pack_start(GTK_BOX(foldview()), m_con_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(foldview()), m_scrolledwindow, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(m_scrolledwindow), m_treeview);

	//-------------------------------------------------------------------------
	// ref objects for showing / hiding since I dont understand gtk ref / unref
	g_object_ref(foldview());

	g_object_ref(m_con_hbox);
	g_object_ref(m_con_combo);
	//g_object_ref(m_dir_indicator);
	//g_object_ref(m_vol_label);
	g_object_ref(m_scrolledwindow);
	g_object_ref(m_treeview);

	//-------------------------------------------------------------------------
	// show, except the pane container

	//fs->update_concombo_visibility();
	gtk_widget_show(m_con_combo);
	//gtk_widget_show(m_vol_label);
		//gtk_widget_show(m_info_label);
		//gtk_widget_show (m_dir_indicator);
		//gtk_widget_show (fs->list_widget);
	gtk_widget_show(m_treeview);

	gtk_widget_show(m_scrolledwindow);

	gtk_widget_show(GTK_WIDGET(foldview()));

	return TRUE;
}

void GcmdGtkFoldview::View::destroy()
{

}

//=============================================================================
//  gtk signals
//=============================================================================

// row about to be expanded : FALSE = allow , TRUE = deny
gboolean
GcmdGtkFoldview::View::signal_test_expand_row(
	GtkTreeView		*tree_view,
	GtkTreeIter		*iter,
	GtkTreePath		*path,
	gpointer 		user_data)
{
	//gwr_inf("view:signal [test_expand_row]");

	GcmdFoldview()->control_iter_expand(iter);

	// Allow expansion
	return FALSE;
}

// row expanded
void	GcmdGtkFoldview::View::signal_row_expanded(
	GtkTreeView		*tree_view,
	GtkTreeIter		*iter,
	GtkTreePath		*path,
	gpointer		user_data)
{
	//gwr_inf("view:signal [row_expanded]");
}

// row collapsed
void	GcmdGtkFoldview::View::signal_row_collapsed(
	GtkTreeView		*tree_view,
	GtkTreeIter		*iter,
	GtkTreePath		*path,
	gpointer		user_data)
{
	//gwr_inf("view:signal [row_collapsed]");

	GcmdFoldview()->control_iter_collapsed(iter);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Buttons
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gboolean GcmdGtkFoldview::View::signal_button_press_event(
	GtkWidget	   *tree_view,
	GdkEventButton *event,
	gpointer		data)
{
	//  signal handled		: return TRUE
	//  signal not handled  : return FALSE
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	GtkTreePath								*path_clicked   = NULL;

	GtkTreeIter								iter_selected   = GcmdGtkFoldview::Model::s_iter_NULL;
	GtkTreePath								*path_selected  = NULL;

	GtkTreeSelection						*tree_sel		= NULL;
	gint									tree_sel_card  = 0;

	GcmdGtkFoldview::View::ctx_menu_data	*ctxdata		= NULL;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// get path to iter over which the user clicked
	gtk_tree_view_get_path_at_pos(
		GTK_TREE_VIEW(tree_view),
		(gint) event->x,
		(gint) event->y,
		&path_clicked, NULL, NULL, NULL);

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//  get path to currently selected iter
	tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if ( ! tree_sel )
	{
		gwr_err("View::signal_button_press_event:gtk_tree_view_get_selection failed");
		return FALSE;
	}

	// Note: gtk_tree_selection_count_selected_rows() does not
	//   exist in gtk+-2.0, only in gtk+ >= v2.2 !
	tree_sel_card = gtk_tree_selection_count_selected_rows(tree_sel);

	if ( tree_sel_card == 1 )
	{
		if ( gtk_tree_selection_get_selected( tree_sel, NULL, &iter_selected) )
		{
			path_selected = gtk_tree_model_get_path(
				gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view)),
				&iter_selected);

			if ( !path_selected )
			{
				gwr_wng("View::signal_button_press_event:gtk_tree_model_get_path failed");
			}
		}
		else
		{
			gwr_wng("View::signal_button_press_event:gtk_tree_selection_get_selected failed");
			return FALSE;
		}
	}

	//.........................................................................
	// single left click
	if ( ( event->type == GDK_BUTTON_PRESS )  &&  ( event->button == 1 ) )
	{
		// The GtkTreeSelection will be updated only
		// in the next loop of gtk_main(), and when calling sync(),
		// the path_selected is still the old selected item, not the
		// clicked one.

		// So in the sync() function, we use path_clicked instead of
		// path_selected

		// And this doesnt work - next gtk_main loop too
		//gtk_tree_selection_unselect_all(tree_sel);
		//gtk_tree_selection_select_path(tree_sel, path_clicked);

		ctxdata				= new GcmdGtkFoldview::View::ctx_menu_data();
		ctxdata->a_foldview = (GcmdGtkFoldview*)data;
		ctxdata->a_time		= event->time;
		ctxdata->a_event_x	= event->x;
		ctxdata->a_event_y	= event->y;
		ctxdata->a_button	= event->button;
		ctxdata->d_path_selected = path_selected;
		ctxdata->d_path_clicked	 = path_clicked;

		return ((GcmdGtkFoldview*)data)->view.click_left_single(ctxdata);
	}

	//.........................................................................
	// single right click : ctx menu
	if ( ( event->type == GDK_BUTTON_PRESS )  &&  ( event->button == 3 ) )
	{
		ctxdata				= new GcmdGtkFoldview::View::ctx_menu_data();
		ctxdata->a_foldview = (GcmdGtkFoldview*)data;
		ctxdata->a_time		= event->time;
		ctxdata->a_event_x	= event->x;
		ctxdata->a_event_y	= event->y;
		ctxdata->a_button	= event->button;
		ctxdata->d_path_selected = path_selected;
		ctxdata->d_path_clicked	 = path_clicked;

		return ((GcmdGtkFoldview*)data)->view.context_menu_pop(ctxdata);
	}

	//.........................................................................
	// double left click
	if ( ( event->type == GDK_2BUTTON_PRESS )  &&  ( event->button == 1 ) )
	{
		ctxdata				= new GcmdGtkFoldview::View::ctx_menu_data();
		ctxdata->a_foldview = (GcmdGtkFoldview*)data;
		ctxdata->a_time		= event->time;
		ctxdata->a_event_x	= event->x;
		ctxdata->a_event_y	= event->y;
		ctxdata->a_button	= event->button;
		ctxdata->d_path_selected = path_selected;
		ctxdata->d_path_clicked	 = path_clicked;

		return ((GcmdGtkFoldview*)data)->view.click_left_double(ctxdata);
	}

	// signal was not handled
	return FALSE;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Drag n Drop
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void GcmdGtkFoldview::View::signal_drag_begin(
	GtkWidget	   *widget,
	GdkDragContext *drag_context,
	gpointer		user_data)
{
	gwr_inf("View:signal_drag_begin");
}

//=============================================================================
//  Contextual menu
//=============================================================================
gboolean GcmdGtkFoldview::View::context_menu_pop(ctx_menu_data *ctxdata)
{
	s_context_menu.a_widget = gtk_menu_new();

	if ( ctxdata->d_path_selected )
	{
		if ( ctxdata->d_path_clicked )
		{
			if ( gtk_tree_path_compare(
					ctxdata->d_path_clicked,
					ctxdata->d_path_selected) == 0 )
			{
				ctxdata->a_foldview->control_context_menu_populate_add_section  (s_context_menu.a_widget, 0, ctxdata);
				ctxdata->a_foldview->control_context_menu_populate_add_separator(s_context_menu.a_widget);
				ctxdata->a_foldview->control_context_menu_populate_add_section  (s_context_menu.a_widget, 1, ctxdata);
			}
			else
			{
				ctxdata->a_foldview->control_context_menu_populate_add_section(s_context_menu.a_widget, 1, ctxdata);
			}
		}
		else
		{
			ctxdata->a_foldview->control_context_menu_populate_add_section  (s_context_menu.a_widget, 0, ctxdata);
			ctxdata->a_foldview->control_context_menu_populate_add_separator(s_context_menu.a_widget);
			ctxdata->a_foldview->control_context_menu_populate_add_section  (s_context_menu.a_widget, 1, ctxdata);
		}
	}
	else
	{
		ctxdata->a_foldview->control_context_menu_populate_add_section(s_context_menu.a_widget, 1, ctxdata);
	}

	gtk_widget_show_all(s_context_menu.a_widget);

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
		GTK_MENU(s_context_menu.a_widget), NULL, NULL,
		NULL, NULL,
		ctxdata->a_button, ctxdata->a_time);

	return TRUE;
}

//=============================================================================
//  Selection
//=============================================================================
gboolean GcmdGtkFoldview::View::click_left_single(ctx_menu_data *ctxdata)
{
	// simulate a callback, avoiding accessing to GcmdGtkFoldview singleton
	GcmdGtkFoldview::Control_sync_update(NULL, ctxdata);

	// tell gtk to continue signal treatment, else treeview wont
	// expand / collapse anymore if that was the arrow that was clicked
	return FALSE;
}

//=============================================================================
//  Contextual menu
//=============================================================================
gboolean GcmdGtkFoldview::View::click_left_double(ctx_menu_data *ctxdata)
{
	// simulate a callback, avoiding accessing to GcmdGtkFoldview singleton
	GcmdGtkFoldview::Control_set_active_tab(NULL, ctxdata);

	return TRUE;
}

//=============================================================================
// composite widget con_combo
//=============================================================================
void GcmdGtkFoldview::View::connection_combo_add_connection(GnomeCmdCon *con)
{
	GnomeCmdCombo   *combo			= GNOME_CMD_COMBO(connection_combo());
    gchar			*text[3];

    text[0] = NULL;
    text[1] = (gchar *)gnome_cmd_con_get_alias(con);
    text[2] = NULL;

    GnomeCmdPixmap *pixmap = gnome_cmd_con_get_go_pixmap (con);
    if (pixmap)
    {
        gint row = combo->append(text, con);
        combo->set_pixmap(row, 0, pixmap);
    }
}

void GcmdGtkFoldview::View::connection_combo_reset()
{
	GnomeCmdCombo   *combo			= GNOME_CMD_COMBO(m_con_combo);

    combo->clear();
    combo->highest_pixmap   = 20;
    combo->widest_pixmap	= 20;
    gtk_clist_set_row_height (GTK_CLIST (combo->list), 20);
    gtk_clist_set_column_width (GTK_CLIST (combo->list), 0, 20);
}

//=============================================================================
// Theming
//=============================================================================
void GcmdGtkFoldview::View::update_style()
{
	gtk_widget_set_style (GTK_WIDGET(treeview()), style_foldview);
	((GnomeCmdCombo*)connection_combo())->update_style();
}









