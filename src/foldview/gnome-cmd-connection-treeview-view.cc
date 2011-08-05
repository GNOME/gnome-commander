/*
    ###########################################################################

    gnome-cmd-connection-treeview-view.cc

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

    Struct  : View

    Parent  : GnomeCmdConnectionTreeview

    ###########################################################################
*/
#include    "gnome-cmd-connection-treeview.h"

//  ###########################################################################
//
//  			    GnomeCmdConnectionTreeview::View::HeadBand
//
//  ###########################################################################
GnomeCmdConnectionTreeview::View::HeadBand::HeadBand(
    GnomeCmdConnectionTreeview::View    *   _view)
{
    raz_pointers();
    init_instance(_view);
}
GnomeCmdConnectionTreeview::View::HeadBand::~HeadBand()
{
}

void
GnomeCmdConnectionTreeview::View::HeadBand::raz_pointers()
{
    a_view                              = NULL;

    d_hbox_main                         = NULL;
        d_button_connection_image       = NULL;
        d_entry_path                    = NULL;
        d_alignement_padder             = NULL;
        d_button_refresh                = NULL;
        d_button_sort                   = NULL;
        d_button_show_hide              = NULL;
        d_button_close                  = NULL;
}

void
GnomeCmdConnectionTreeview::View::HeadBand::init_instance(
    View         *   _view)
{
    g_return_if_fail( _view );

    a_view = _view;

    d_hbox_main                     = gtk_hbox_new(FALSE, 0);
        d_button_connection_image   = gtk_button_new();
        d_entry_path                = gtk_entry_new();
        d_alignement_padder         = gtk_alignment_new(1.0f, 1.0f, 1.0f, 1.0f);
        d_button_refresh            = gtk_button_new();
        d_button_sort               = gtk_button_new();
        d_button_show_hide          = gtk_button_new();
        d_button_close              = gtk_button_new();

    //.........................................................................
    //
    // Path entry setup
    //
    gtk_entry_set_has_frame(GTK_ENTRY(d_entry_path), TRUE);
    gtk_entry_set_alignment(GTK_ENTRY(d_entry_path), 0.0f);
    gtk_entry_set_editable(GTK_ENTRY(d_entry_path), FALSE);
    gtk_entry_set_text(GTK_ENTRY(d_entry_path), view()->control()->gnome_cmd_connection()->alias);
    //.........................................................................
    //
    // Buttons setup
    //
    gtk_button_set_image(GTK_BUTTON(d_button_connection_image), gtk_image_new_from_pixbuf(gnome_cmd_con_get_open_pixmap(view()->control()->gnome_cmd_connection())->pixbuf));
    gtk_button_set_image(GTK_BUTTON(d_button_refresh),          gtk_image_new_from_pixbuf(GnomeCmdConnectionTreeview::s_gdk_pixbuf[eIconGtkRefresh]));
    gtk_button_set_image(GTK_BUTTON(d_button_sort),             gtk_image_new_from_pixbuf(GnomeCmdConnectionTreeview::s_gdk_pixbuf[eIconGtkSortDescending]));
    gtk_button_set_image(GTK_BUTTON(d_button_show_hide),        gtk_image_new_from_pixbuf(GnomeCmdConnectionTreeview::s_gdk_pixbuf[eIconFolderOpened]));
    gtk_button_set_image(GTK_BUTTON(d_button_close),            gtk_image_new_from_pixbuf(GnomeCmdConnectionTreeview::s_gdk_pixbuf[eIconGtkClose]));

	g_signal_connect(d_button_refresh,  "clicked",    G_CALLBACK(Control::Signal_button_refresh_clicked),  (gpointer)this);
	g_signal_connect(d_button_sort,     "clicked",    G_CALLBACK(Control::Signal_button_sort_clicked),     (gpointer)this);
	g_signal_connect(d_button_show_hide,"clicked",    G_CALLBACK(Control::Signal_button_show_hide_clicked),(gpointer)this);
	g_signal_connect(d_button_close,    "clicked",    G_CALLBACK(Control::Signal_button_close_clicked),    (gpointer)this);

    //.........................................................................
    //
    // Packing
    //
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_button_connection_image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_entry_path,              TRUE , TRUE , 0);
    //gtk_box_pack_start(GTK_BOX(d_hbox_main), d_alignement_padder,       TRUE , TRUE, 0);
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_button_refresh,          FALSE, FALSE, 0);
    //gtk_box_pack_start(GTK_BOX(d_hbox_main), d_button_sort,             FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_button_show_hide,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_button_close,            FALSE, FALSE, 0);
}

void
GnomeCmdConnectionTreeview::View::HeadBand::show()
{
    gtk_widget_show_all(d_hbox_main);
}
void
GnomeCmdConnectionTreeview::View::HeadBand::hide()
{
    gtk_widget_hide_all(d_hbox_main);
}

void
GnomeCmdConnectionTreeview::View::HeadBand::set_image_closed()
{
    gtk_button_set_image(GTK_BUTTON(d_button_show_hide),        gtk_image_new_from_pixbuf(GnomeCmdConnectionTreeview::s_gdk_pixbuf[eIconFolderClosed]));
}

void
GnomeCmdConnectionTreeview::View::HeadBand::set_image_opened()
{
    gtk_button_set_image(GTK_BUTTON(d_button_show_hide),        gtk_image_new_from_pixbuf(GnomeCmdConnectionTreeview::s_gdk_pixbuf[eIconFolderOpened]));
}


//  ###########################################################################
//
//                  GnomeCmdConnectionTreeview::View::Treeview
//
//  ###########################################################################
GnomeCmdConnectionTreeview::View::Treeview::Treeview(
    View            *   _view,
    GtkTreeModel    *   _treemodel)
{
    raz_pointers();
    init_instance(_view, _treemodel);
}
GnomeCmdConnectionTreeview::View::Treeview::~Treeview()
{
}

void
GnomeCmdConnectionTreeview::View::Treeview::raz_pointers()
{
    a_view              = NULL;

    d_scrolled_window   = NULL;
    d_treeview          = NULL;
}

void
GnomeCmdConnectionTreeview::View::Treeview::init_instance(
    View            *   _view,
    GtkTreeModel    *   _treemodel)
{
	GtkTreeViewColumn		*col0		= NULL;
	GtkTreeViewColumn		*col1		= NULL;
	GtkCellRenderer			*renderer   = NULL;
    //.........................................................................
    g_return_if_fail( _view );
    g_return_if_fail( _treemodel );
    a_view              = _view;

    a_treemodel         = _treemodel;

	d_scrolled_window   = gtk_scrolled_window_new(NULL, NULL);
	d_treeview          = gtk_tree_view_new();

    //.........................................................................
    //
    // Scrolled window setup
    //
	gtk_scrolled_window_set_policy
    (   GTK_SCROLLED_WINDOW (d_scrolled_window),
		GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC    );

    //.........................................................................
    //
    // GtkTreeView setup
    //
    if ( TRUE )
    {
    //
    // treeview
    //

	// Drag & Drop

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
	//	  GTK_TARGET_SAME_APP = 1 << 0,    //< nick=same-app >
	//	  GTK_TARGET_SAME_WIDGET = 1 << 1  //< nick=same-widget >
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

	// create columns
	col0 = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col0, _("Folders"));
	gtk_tree_view_column_set_visible(col0,TRUE);

	col1 = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col1, _("Access Rights"));
	gtk_tree_view_column_set_visible(col1,FALSE);

	// column 0  : create pixbuf renderer
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col0, renderer, FALSE);
	g_object_set(renderer, "xalign", 0.0, NULL);
	gtk_tree_view_column_set_cell_data_func(col0, renderer, CellRendererPixbuf, (gpointer)this, NULL);

	// column 0  : create text renderer
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col0, renderer, FALSE);
	g_object_set(renderer, "xalign", 0.0, NULL);
	gtk_tree_view_column_set_cell_data_func(col0, renderer, CellRendererTxt, (gpointer)this, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(d_treeview), col0);

	// connect signals
	g_signal_connect(d_treeview, "test_expand_row",		G_CALLBACK(signal_test_expand_row), (gpointer)this);
	g_signal_connect(d_treeview, "row_expanded",		G_CALLBACK(signal_row_expanded), (gpointer)this);
	g_signal_connect(d_treeview, "row_collapsed",		G_CALLBACK(signal_row_collapsed), (gpointer)this);

	g_signal_connect(d_treeview, "button-press-event",  G_CALLBACK(signal_button_press_event), (gpointer)this);

	g_signal_connect(d_treeview, "cursor-changed",      G_CALLBACK(signal_cursor_changed), (gpointer)this);

	// some settings
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(d_treeview), FALSE);
	gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(d_treeview), TRUE);
	gtk_tree_view_set_model(GTK_TREE_VIEW(d_treeview), treemodel());

	// contextual menu
        // since we do a dynamic menu, and there is no menu_shell_remove()
        // function in gtk, we must create it on the fly, during the
        // callback for single-right-click
    }
    //.........................................................................
    //
    // Packing
    //
	gtk_container_add(GTK_CONTAINER(d_scrolled_window), d_treeview);
}

void
GnomeCmdConnectionTreeview::View::Treeview::show()
{
    gtk_widget_show_all(d_scrolled_window);
}
void
GnomeCmdConnectionTreeview::View::Treeview::hide()
{
    gtk_widget_hide_all(d_scrolled_window);
}
void
GnomeCmdConnectionTreeview::View::Treeview::update_style()
{
}

//  ***************************************************************************
//  cells renderers
//  ***************************************************************************
void
GnomeCmdConnectionTreeview::View::Treeview::CellRendererPixbuf(
	GtkTreeViewColumn *     _col,
	GtkCellRenderer   *     _renderer,
	GtkTreeModel      *     _model,
	GtkTreeIter       *     _iter,
	gpointer                _udata)
{
	gint					            icon;
	Treeview		                *   tv	    = NULL;
    Model::Row                      *   row     = NULL;
	GValue					            v		= {0};
    //.........................................................................
	tv = (GnomeCmdConnectionTreeview::View::Treeview*)_udata;

	GnomeCmdFoldviewTreestore::get_value(_model, _iter, 0, &v);
	row = (Model::Row*)g_value_get_pointer(&v);

	icon = row->icon();
	g_object_set(_renderer, "pixbuf", GnomeCmdConnectionTreeview::s_gdk_pixbuf[(eIcon)icon], NULL);
}

void
GnomeCmdConnectionTreeview::View::Treeview::CellRendererTxt(
	GtkTreeViewColumn               *   _col,
	GtkCellRenderer                 *   _renderer,
	GtkTreeModel                    *   _model,
	GtkTreeIter                     *   _iter,
	gpointer                            _udata)
{
    Model::Row                      *   row     = NULL;
	GValue v = {0};
	//.........................................................................
	GnomeCmdFoldviewTreestore::get_value(_model, _iter, 0, &v);
	row = (Model::Row*)g_value_get_pointer(&v);

	g_object_set(_renderer, "text", row->utf8_name_display() , NULL);
}

//  ***************************************************************************
//  row state / actions
//  ***************************************************************************
GnomeCmdConnectionTreeview::eRowState
GnomeCmdConnectionTreeview::View::Treeview::row_state(
	GtkTreePath *   _path)
{
	return ( gtk_tree_view_row_expanded(treeview(), _path)  ?
				eRowStateExpanded							:
				eRowStateCollapsed  );
}
gboolean
GnomeCmdConnectionTreeview::View::Treeview::row_collapse(
	GtkTreePath *   _path)
{
	return gtk_tree_view_collapse_row(treeview(), _path);
}

//  ***************************************************************************
//  gtk+ callbacks
//  ***************************************************************************
//  ===========================================================================
//  row selection
//  ===========================================================================
void
GnomeCmdConnectionTreeview::View::Treeview::signal_cursor_changed(
	GtkTreeView		*tree_view,
	gpointer 	   user_data)
{
	GtkTreeSelection	*tree_sel		= NULL;
	gint				tree_sel_card   = 0;

	GtkTreeIter			iter_selected   = Model::Iter_zero;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//GCMD_INF("signal_cursor_changed");

	//  get path to currently selected iter
	tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if ( ! tree_sel )
	{
		GCMD_ERR("View::signal_cursor_changed:gtk_tree_view_get_selection failed");
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
			GCMD_WNG("View::signal_cursor_changed:gtk_tree_selection_get_selected failed");
		}
	}
	else
	{
		GCMD_ERR("View::signal_cursor_changed");
	}
}

//  ===========================================================================
//  row expand / collapse
//  ===========================================================================
// row about to be expanded : FALSE = allow , TRUE = deny
gboolean
GnomeCmdConnectionTreeview::View::Treeview::signal_test_expand_row(
	GtkTreeView		*   _tree_view,
	GtkTreeIter		*   _iter,
	GtkTreePath		*   _path,
	gpointer 		    _udata)
{
	Treeview		*   tv = NULL;
	//.........................................................................
	//GCMD_INF("view:signal [test_expand_row]");

	tv = (Treeview*)_udata;

	tv->view()->control()->iter_expanded_from_ui(_iter, TRUE);

	// Allow expansion
	return FALSE;
}

void
GnomeCmdConnectionTreeview::View::Treeview::signal_row_expanded(
	GtkTreeView		*   _tree_view,
	GtkTreeIter		*   _iter,
	GtkTreePath		*   _path,
	gpointer 		    _udata)
{
	//GCMD_INF("view:signal [row_expanded]");
}

void
GnomeCmdConnectionTreeview::View::Treeview::signal_row_collapsed(
	GtkTreeView		*   _tree_view,
	GtkTreeIter		*   _iter,
	GtkTreePath		*   _path,
	gpointer 		    _udata)
{
	Treeview		*   tv = NULL;
	//.........................................................................
	//GCMD_INF("view:signal [row_collapsed]");

	tv = (Treeview*)_udata;

	tv->view()->control()->iter_collapsed_from_ui(_iter);
}

//  ===========================================================================
//  Drag n Drop
//  ===========================================================================
//GnomeCmdConnectionTreeview::View::Treeview::signal_drag_begin(
//	GtkWidget	   *widget,
//	GdkDragContext *drag_context,
//	gpointer		user_data)
//{
//	GCMD_INF("View:signal_drag_begin");
//}

//  ===========================================================================
//  Buttons clicks
//  ===========================================================================
gboolean
GnomeCmdConnectionTreeview::View::Treeview::signal_button_press_event(
	GtkWidget	   *tree_view,
	GdkEventButton *event,
	gpointer		_udata)
{
	//  signal handled		: return TRUE
	//  signal not handled  : return FALSE
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    Treeview                *   THIS            = NULL;
	GtkTreePath				*   path_clicked    = NULL;

	GtkTreeIter					iter_selected   = Model::Iter_zero;
	GtkTreePath				*   path_selected   = NULL;

	GtkTreeSelection		*   tree_sel		= NULL;
	gint						tree_sel_card   = 0;

	View::ctx_menu_data	    *   ctxdata		    = NULL;
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    THIS = (Treeview*)_udata;

	// get path to iter over which the user clicked, and the iter too
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
		GCMD_ERR("View::signal_button_press_event:gtk_tree_view_get_selection failed");
		return FALSE;
	}

	// Note: gtk_tree_selection_count_selected_rows() does not
	//   exist in gtk+-2.0, only in gtk+ >= v2.2 !
	tree_sel_card = gtk_tree_selection_count_selected_rows(tree_sel);

	if ( tree_sel_card == 1 )
	{
		if ( gtk_tree_selection_get_selected( tree_sel, NULL, &iter_selected) )
		{
			path_selected = GnomeCmdFoldviewTreestore::get_path(
				gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view)),
				&iter_selected);

			if ( !path_selected )
			{
				GCMD_WNG("View::signal_button_press_event:gtk_tree_model_get_path failed");
			}
		}
		else
		{
			GCMD_WNG("View::signal_button_press_event:gtk_tree_selection_get_selected failed");
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

		ctxdata				= new ctx_menu_data();
		ctxdata->a_treeview = THIS;
		ctxdata->a_time		= event->time;
		ctxdata->a_event_x	= event->x;
		ctxdata->a_event_y	= event->y;
		ctxdata->a_button	= event->button;
		ctxdata->d_path_selected = path_selected;
		ctxdata->d_path_clicked	 = path_clicked;

		return THIS->click_left_single(ctxdata);
	}

	//.........................................................................
	// single right click : ctx menu
	if ( ( event->type == GDK_BUTTON_PRESS )  &&  ( event->button == 3 ) )
	{
		ctxdata				= new ctx_menu_data();
		ctxdata->a_treeview = THIS;
		ctxdata->a_time		= event->time;
		ctxdata->a_event_x	= event->x;
		ctxdata->a_event_y	= event->y;
		ctxdata->a_button	= event->button;
		ctxdata->d_path_selected = path_selected;
		ctxdata->d_path_clicked	 = path_clicked;

		return THIS->context_menu_pop(ctxdata);
	}

	//.........................................................................
	// double left click
	if ( ( event->type == GDK_2BUTTON_PRESS )  &&  ( event->button == 1 ) )
	{
		ctxdata				= new ctx_menu_data();
		ctxdata->a_treeview = THIS;
		ctxdata->a_time		= event->time;
		ctxdata->a_event_x	= event->x;
		ctxdata->a_event_y	= event->y;
		ctxdata->a_button	= event->button;
		ctxdata->d_path_selected = path_selected;
		ctxdata->d_path_clicked	 = path_clicked;

		return THIS->click_left_double(ctxdata);
	}

    if ( path_clicked )
        gtk_tree_path_free(path_clicked);

	// signal was not handled
	return FALSE;
}

gboolean GnomeCmdConnectionTreeview::View::Treeview::click_left_single(ctx_menu_data *ctxdata)
{
	//GnomeCmdConnectionTreeview::Control_sync_update(NULL, ctxdata);           // _GWR_STANDBY_
    delete ctxdata;
	// tell gtk to continue signal treatment, else treeview wont
	// expand / collapse anymore if that was the arrow that was clicked
	return FALSE;
}

gboolean GnomeCmdConnectionTreeview::View::Treeview::click_left_double(ctx_menu_data *ctxdata)
{
	Control::Set_active_tab(NULL, ctxdata);

	return TRUE;
}

//  ===========================================================================
//							Contextual menu
//  ===========================================================================
gboolean GnomeCmdConnectionTreeview::View::Treeview::context_menu_pop(ctx_menu_data *ctxdata)
{
	ctxdata->a_treeview->view()->control()->context_menu_pop(ctxdata);

	return TRUE;
}




//  ###########################################################################
//
//                  GnomeCmdConnectionTreeview::View
//
//  ###########################################################################

//  ***************************************************************************
//
//  Constructor, ...
//
//  ***************************************************************************
GnomeCmdConnectionTreeview::View::View(
    GnomeCmdConnectionTreeview::Control     *   _control,
    GtkTreeModel                            *   _treemodel)
{
    raz_pointers();
    init_instance(_control, _treemodel);
}
GnomeCmdConnectionTreeview::View::~View()
{
    delete d_headband;
    delete d_treeview;
}

void
GnomeCmdConnectionTreeview::View::raz_pointers()
{
    a_control   = NULL;

    d_vbox_main = NULL;
    d_headband  = NULL;
    d_treeview  = NULL;

    a_treeview_showed = TRUE;
}

void
GnomeCmdConnectionTreeview::View::init_instance(
    GnomeCmdConnectionTreeview::Control     *   _control,
    GtkTreeModel                            *   _treemodel)
{
    g_return_if_fail( _control      );
    //g_return_if_fail( _treemodel    );

    a_control   = _control;

    d_vbox_main = gtk_vbox_new(FALSE, 0);

    d_headband  = GCMD_STRUCT_NEW(HeadBand, this);
    d_treeview  = GCMD_STRUCT_NEW(Treeview, this, _treemodel);

    //.........................................................................
    //
    // Packing
    //
    gtk_box_pack_start(GTK_BOX(d_vbox_main), d_headband->widget(), FALSE, TRUE , 0);
    gtk_box_pack_start(GTK_BOX(d_vbox_main), d_treeview->widget(), TRUE , TRUE , 0);
}


void
GnomeCmdConnectionTreeview::View::show()
{
    gtk_widget_show_all(d_vbox_main);
}
void
GnomeCmdConnectionTreeview::View::hide()
{
    gtk_widget_hide_all(d_vbox_main);
}

void
GnomeCmdConnectionTreeview::View::update_style()
{
}
void
GnomeCmdConnectionTreeview::View::show_treeview()
{
    if ( treeview_showed() )
    {
        GCMD_ERR("View::hide_treeview():already shown");
        return;
    }

    gtk_box_pack_start(GTK_BOX(d_vbox_main), d_treeview->widget(), TRUE , TRUE , 0);
    g_object_unref(d_treeview->widget());

    headband()->set_image_opened();

    a_treeview_showed = TRUE;
}
void
GnomeCmdConnectionTreeview::View::hide_treeview()
{
    if ( ! treeview_showed() )
    {
        GCMD_ERR("View::hide_treeview():already hidden");
        return;
    }

    g_object_ref(d_treeview->widget());
    gtk_container_remove(GTK_CONTAINER(d_vbox_main), d_treeview->widget());

    headband()->set_image_closed();

    a_treeview_showed = FALSE;
}




GnomeCmdConnectionTreeview::eIcon
GnomeCmdConnectionTreeview::View::Icon_from_type_access(
	eFileType		type,
	eFileAccess	access)
{
	if ( type == eTypeDirectory )
	{
		if ( Access_readable(access) )
			return GnomeCmdConnectionTreeview::eIconDirReadWrite;
		else
			return GnomeCmdConnectionTreeview::eIconDirReadOnly;

		//switch ( access )
		//{
		//	case	eAccessRW   : return GcmdGtkFoldview::View::eIconDirReadWrite; break;
		//	case	eAccessRO   : return GcmdGtkFoldview::View::eIconDirReadOnly;  break;
		//	case	eAccessFB   : return GcmdGtkFoldview::View::eIconDirForbidden; break;
		//	default				: return GcmdGtkFoldview::View::eIconUnknown;	   break;
		//}
	}

	if ( type == eTypeSymlink )
	{
		if ( Access_readable(access) )
			return GnomeCmdConnectionTreeview::eIconSymlinkToDirReadWrite;
		else
			return GnomeCmdConnectionTreeview::eIconSymlinkToDirReadOnly;

		//switch ( access )
		//{
		//	case	eAccessRW		:   return GcmdGtkFoldview::View::eIconSymlinkToDirReadWrite;	 break;
		//	case	eAccessRO		:   return GcmdGtkFoldview::View::eIconSymlinkToDirReadOnly;	 break;
		//	case	eAccessFB		:   return GcmdGtkFoldview::View::eIconSymlinkToDirForbidden;	 break;
		//	default					:   return GcmdGtkFoldview::View::eIconUnknown;						break;
		//}
	}

	return eIconUnknown;
}















