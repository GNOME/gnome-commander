/*
    ###########################################################################

    gnome-cmd-foldview-private.h

    ---------------------------------------------------------------------------

    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2010-2012 Guillaume Wardavoir

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

    - Public methods
    - GcmdGtkFoldview GObject

    ###########################################################################
*/
#include	<config.h>

#include	<stdlib.h>
//  ...........................................................................
#include	<gtk/gtksignal.h>
#include	<gtk/gtkvbox.h>

#include	<libgnomevfs/gnome-vfs.h>
#include	<libgnomevfs/gnome-vfs-utils.h>
//  ...........................................................................
#include	"gnome-cmd-foldview-private.h"
//#include	"gnome-cmd-includes.h"
#include	"gnome-cmd-combo.h"
#include	"gnome-cmd-main-win.h"
#include	"gnome-cmd-style.h"
#include	"gnome-cmd-data.h"
#include    "gnome-cmd-con-list.h"
#include    "gnome-cmd-con-smb.h"
//  ...........................................................................
#include    "gnome-cmd-connection-treeview.h"

extern  GnomeCmdMainWin *main_win;

Logger  sLogger(30);

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #						Public methods									  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
void
gnome_cmd_foldview_update_style(GtkWidget *widget)
{
	//g_return_if_fail( widget != NULL );

	//(GCMDGTKFOLDVIEW(widget))->view.update_style();
}

GtkWidget*
gnome_cmd_foldview_new()
{
	return gcmdgtkfoldview_new();
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #				        GcmdGtkFoldview::HeadBand                         #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
GcmdGtkFoldview::HeadBand::HeadBand(
GcmdGtkFoldview *   _foldview)
{
    d_hbox_main             = gtk_hbox_new(FALSE, 0);
        d_button_add        = gtk_button_new();
        d_con_combo         = (GtkWidget*)( new GnomeCmdCombo(2, 1, NULL) );
        d_alignement_padder = gtk_alignment_new(1.0f, 1.0f, 1.0f, 1.0f);
        d_button_up         = gtk_button_new();
            d_arrow_up      = gtk_arrow_new(GTK_ARROW_UP,   GTK_SHADOW_NONE);
        d_button_down       = gtk_button_new();
            d_arrow_down    = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);

    //.........................................................................
    //
    // Setup
    //

    // connection combo
    //gtk_object_set_data_full (GTK_OBJECT (_foldview), "con_combo", m_con_combo, (GtkDestroyNotify) g_object_unref);
	gtk_widget_set_size_request (d_con_combo, 150, -1);
	gtk_clist_set_row_height    (GTK_CLIST (GNOME_CMD_COMBO (d_con_combo)->list), 20);
	gtk_entry_set_editable      (GTK_ENTRY (GNOME_CMD_COMBO (d_con_combo)->entry), FALSE);
	gtk_clist_set_column_width  (GTK_CLIST (GNOME_CMD_COMBO (d_con_combo)->list), 0, 20);
	gtk_clist_set_column_width  (GTK_CLIST (GNOME_CMD_COMBO (d_con_combo)->list), 1, 60);
	GTK_WIDGET_UNSET_FLAGS (GNOME_CMD_COMBO (d_con_combo)->button, GTK_CAN_FOCUS);

    // buttons
    gtk_button_set_image(GTK_BUTTON(d_button_add),  gtk_image_new_from_pixbuf(GcmdGtkFoldview::s_gdk_pixbuf[eIconGtkAdd]));
    gtk_button_set_image(GTK_BUTTON(d_button_up),   d_arrow_up);
    gtk_button_set_image(GTK_BUTTON(d_button_down), d_arrow_down);

	g_signal_connect(d_button_add,  "clicked",    G_CALLBACK(Signal_button_add_clicked),    (gpointer)_foldview);
	g_signal_connect(d_button_up,   "clicked",    G_CALLBACK(Signal_button_up_clicked),     (gpointer)_foldview);
	g_signal_connect(d_button_down, "clicked",    G_CALLBACK(Signal_button_down_clicked),   (gpointer)_foldview);

    //.........................................................................
    //
    // Packing
    //
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_button_add,          FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_con_combo,           FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_alignement_padder,   TRUE , TRUE, 0);
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_button_up,           FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(d_hbox_main), d_button_down,         FALSE, TRUE, 0);
}
GcmdGtkFoldview::HeadBand::~HeadBand()
{
}

void
GcmdGtkFoldview::HeadBand::show()
{
    gtk_widget_show_all(d_hbox_main);
    GNOME_CMD_COMBO(d_con_combo)->select_data((gpointer)gnome_cmd_con_list_get_home(gnome_cmd_con_list_get()));
}
void
GcmdGtkFoldview::HeadBand::hide()
{
    gtk_widget_hide_all(d_hbox_main);
}
void
GcmdGtkFoldview::HeadBand::update_style()
{
	((GnomeCmdCombo*)d_con_combo)->update_style();
}
//  ===========================================================================
//  Connection combo specific
//  ===========================================================================
void
GcmdGtkFoldview::HeadBand::reset_connections()
{
	GnomeCmdCombo   *   combo = NULL;
    //.........................................................................
	combo = GNOME_CMD_COMBO(d_con_combo);

    combo->clear();
    combo->highest_pixmap   = 20;
    combo->widest_pixmap	= 20;
    gtk_clist_set_row_height    (GTK_CLIST (combo->list), 20);
    gtk_clist_set_column_width  (GTK_CLIST (combo->list), 0, 20);
}

void
GcmdGtkFoldview::HeadBand::add_connection(
    GnomeCmdCon *   _con)
{
	GnomeCmdCombo   *   combo   = NULL;
    gchar			*   text    [3];
    gint                row     = 0;
    //.........................................................................
	combo			= GNOME_CMD_COMBO(d_con_combo);

    text[0] = NULL;
    text[1] = (gchar *)gnome_cmd_con_get_alias(_con);
    text[2] = NULL;

    GnomeCmdPixmap *pixmap = gnome_cmd_con_get_go_pixmap(_con);
    if (pixmap)
    {
        row = combo->append(text, _con);
        combo->set_pixmap(row, 0, pixmap);
    }
}

gboolean
GcmdGtkFoldview::HeadBand::can_add_that_connection(
    GnomeCmdCon *   _con)
{
    // dont add connection if
    // it is closed AND it is not a device AND it is not smb
    if  (
            !gnome_cmd_con_is_open (_con)		&&
            !GNOME_CMD_IS_CON_DEVICE (_con)		&&
            !GNOME_CMD_IS_CON_SMB (_con)
        )  return FALSE;

    return TRUE;
}


void
GcmdGtkFoldview::HeadBand::update_connections()
{
    GnomeCmdCombo   *   combo			= NULL;
    GList           *   l               = NULL;
    //.........................................................................

    combo			= GNOME_CMD_COMBO(d_con_combo);

    reset_connections();

    for ( l = gnome_cmd_con_list_get_all(gnome_cmd_con_list_get ()); l ; l = l->next )
    {
        GnomeCmdCon *con = (GnomeCmdCon *)l->data;

        if ( ! can_add_that_connection(con) )
            continue;

        // else add it
        add_connection(con);
    }
}

GnomeCmdCon*
GcmdGtkFoldview::HeadBand::get_selected_connection()
{
    return GNOME_CMD_CON(GNOME_CMD_COMBO(d_con_combo)->sel_data);
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #					        GcmdGtkFoldview          					  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ###########################################################################
//
//  GcmdGtkFoldview : GObject
//
//  ###########################################################################
enum
{
  GCMDGTKFOLDVIEW_SIGNAL,
  LAST_SIGNAL
};

static  void	            gcmdgtkfoldview_class_init(GcmdGtkFoldviewClass   *klass);

static  guint               gcmdgtkfoldview_signals[LAST_SIGNAL] = { 0 };

        GObjectClass    *   GcmdGtkFoldview::Parent_class = NULL;

//  ===========================================================================
GType
gcmdgtkfoldview_get_type (void)
{
	static GType fv_type = 0;

	if (!fv_type)
	{
		const GTypeInfo fv_info =
		{
			sizeof (GcmdGtkFoldviewClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gcmdgtkfoldview_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GcmdGtkFoldview),
			0,
			(GInstanceInitFunc)GcmdGtkFoldview::G_object_init,
		};

	fv_type = g_type_register_static(GTK_TYPE_VBOX, "gtkGcmdFoldview", &fv_info, (GTypeFlags)0);
	}

	return fv_type;
}
static void
gcmdgtkfoldview_class_init (GcmdGtkFoldviewClass *klass)
{
	gcmdgtkfoldview_signals[GCMDGTKFOLDVIEW_SIGNAL] = g_signal_new(
		"gcmdgtkfoldview",
		G_TYPE_FROM_CLASS (klass),
		(GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
		G_STRUCT_OFFSET (GcmdGtkFoldviewClass, gcmdgtkfoldview),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	// For exiting properly
    GtkObjectClass  *gtk_object_class   = GTK_OBJECT_CLASS  (klass);
    //GtkWidgetClass  *gtk_widget_class   = GTK_WIDGET_CLASS  (klass);
	GObjectClass	*g_object_class		= G_OBJECT_CLASS	(klass);


	GcmdGtkFoldview::Parent_class	= (GObjectClass*) g_type_class_peek_parent (klass);

	// override dispose & finalize
	gtk_object_class->destroy   = GcmdGtkFoldview::Gtk_object_destroy;
	g_object_class->dispose		= GcmdGtkFoldview::G_object_dispose;
	g_object_class->finalize	= GcmdGtkFoldview::G_object_finalize;
}
GtkWidget*
gcmdgtkfoldview_new ()
{
    return GTK_WIDGET(g_object_new (GCMDGTKFOLDVIEW_TYPE, NULL));
}
//  ===========================================================================
void
GcmdGtkFoldview::G_object_init (GcmdGtkFoldview *foldview)
{
	foldview->init_instance();
}
void GcmdGtkFoldview::Gtk_object_destroy(GtkObject* object)
{
	//g_return_if_fail( IS_GCMDGTKFOLDVIEW(object) );
	//GCMD_INF("Control_gtk_object_destroy:%03i", GCMDGTKFOLDVIEW(object)->control_ref_count());
	GCMD_INF("GcmdGtkFoldview::Control_gtk_object_destroy");

	GCMDGTKFOLDVIEW(object)->dispose();

	(* GTK_OBJECT_CLASS (Parent_class)->destroy)(object);
}
void GcmdGtkFoldview::G_object_dispose(GObject* object)
{
	//g_return_if_fail( IS_GCMDGTKFOLDVIEW(object) );
	//GCMD_INF("Control_g_object_dispose:%03i", GCMDGTKFOLDVIEW(object)->control_ref_count());
	GCMD_INF("GcmdGtkFoldview::Control_g_object_dispose");

	(*Parent_class->dispose)(object);
}
void GcmdGtkFoldview::G_object_finalize(GObject* object)
{
	g_return_if_fail( IS_GCMDGTKFOLDVIEW(object) );
	//GCMD_INF("Control_g_object_finalize:%03i", GCMDGTKFOLDVIEW(object)->control_ref_count());
	GCMD_INF("GcmdGtkFoldview::Control_g_object_finalize");

	GCMDGTKFOLDVIEW(object)->finalize();

	(*Parent_class->finalize)(object);
}
//  ###########################################################################
//
//  GcmdGtkFoldview : the rest
//
//  ###########################################################################

GdkPixbuf   *   GcmdGtkFoldview::s_gdk_pixbuf[GcmdGtkFoldview::eIconCard];
gboolean        GcmdGtkFoldview::s_gdk_pixbuf_loaded    = FALSE;

//  ***************************************************************************
//
//  init_instance, ...
//
//  ***************************************************************************
void GcmdGtkFoldview::init_instance()
{
    raz_pointers();

    //.........................................................................
    // Init logger
    //

    //
    //                           Tki
    //                            |
    //                         Wng| Tke
    //                          | | |
    // Logger format : GCMD_B8(xxxxxx)
    //                         | | |
    //                        Inf| Tkw
    //                           |
    //                          Err
    //
    //

    if ( ! sLogger.channel(eLogGcmd) )
    {
        // common
        sLogger.channel_create(eLogGcmd,      "GCMD     ", GCMD_B8(011000));
        sLogger.channel_create(eLogLeaks,     "LEAKS    ", GCMD_B8(111111));
        // control
        sLogger.channel_create(eLogFifo,      "FIFO     ", GCMD_B8(011000));
        sLogger.channel_create(eLogMsg,       "MESSAGES ", GCMD_B8(011000));
        // model
        sLogger.channel_create(eLogFiles,     "FILES    ", GCMD_B8(000000));
        sLogger.channel_create(eLogRefresh,   "REFRESH  ", GCMD_B8(011011));
        sLogger.channel_create(eLogSort,      "SORT     ", GCMD_B8(011111));
        sLogger.channel_create(eLogEnumerate, "ENUMERATE", GCMD_B8(011000));
        sLogger.channel_create(eLogCheck,     "CHECK    ", GCMD_B8(011000));
        sLogger.channel_create(eLogExpand,    "EXPAND   ", GCMD_B8(011000));
        sLogger.channel_create(eLogMonitor,   "MONITOR  ", GCMD_B8(000000));

        sLogger.channel_create(eLogTreeNode,  "TREENODE ", GCMD_B8(111011));
        sLogger.channel_create(eLogTreeBlock, "TREEBLOCK", GCMD_B8(111011));
        sLogger.channel_create(eLogTreeStore, "TREESTORE", GCMD_B8(111011));
    }
    //.........................................................................
    //
    //  Images
    //
    Pixbuf_load_all();

    //.........................................................................
    //
    //  Members
    //
    d_headband          = GCMD_STRUCT_NEW(HeadBand, this);

    d_scrolled_window   = gtk_scrolled_window_new(NULL,NULL);
    d_vbox_sw           = gtk_vbox_new(FALSE, 0);

    //.........................................................................
    //
    //  Setup
    //
    gtk_container_set_reallocate_redraws(GTK_CONTAINER(this), TRUE);
	gtk_box_set_homogeneous(GTK_BOX(this), FALSE);

    d_headband->update_connections();

    gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(d_scrolled_window), GTK_CORNER_BOTTOM_RIGHT);
	gtk_scrolled_window_set_policy
    (   GTK_SCROLLED_WINDOW(d_scrolled_window),
		GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC    );

	gtk_box_set_homogeneous(GTK_BOX(d_vbox_sw), FALSE);

    //.........................................................................
    //
    //  Packing
    //
	gtk_box_pack_start(GTK_BOX(this), d_headband->widget(), FALSE, FALSE, 0);

    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(d_scrolled_window), d_vbox_sw);
	gtk_box_pack_start(GTK_BOX(this), d_scrolled_window, TRUE, TRUE, 0);

	//-------------------------------------------------------------------------
	// show, except the pane container
    d_headband->show();
	gtk_widget_show_all(GTK_WIDGET(this));

    //=========================================================================
    //  Control
    //=========================================================================
}
void GcmdGtkFoldview::raz_pointers()
{
    d_headband          = NULL;

    d_scrolled_window   = NULL;
    d_vbox_sw           = NULL;

    d_list_widget       = NULL;
    a_list_widget_card  = 0;
}

void GcmdGtkFoldview::dispose()
{
    GList                                   *   l   = NULL;
    GcmdStruct<GnomeCmdConnectionTreeview>  *   ctv = NULL;
    //.........................................................................
    l = g_list_first( d_list_widget );

    while ( l )
    {
        if ( ! GTK_IS_HSEPARATOR((GtkSeparator*)l->data) )
        {
            ctv = (GcmdStruct<GnomeCmdConnectionTreeview>*)(l->data);

            gtk_container_remove(GTK_CONTAINER(d_vbox_sw), ctv->widget());

            delete ctv;
        }
        else
        {
            gtk_container_remove(GTK_CONTAINER(d_vbox_sw), GTK_WIDGET(l->data));
        }

        l = g_list_next(l);
    }

    gtk_container_remove(GTK_CONTAINER(this), headband()->widget());
    delete d_headband;

    g_list_free(d_list_widget);

}

void GcmdGtkFoldview::finalize()
{
    Pixbuf_unload_all();
}
//  ***************************************************************************
//
//  Icons
//
//  ***************************************************************************
gboolean
GcmdGtkFoldview::Pixbuf_load(
    GcmdGtkFoldview::eIcon      _icon,
    const gchar             *   _filename)
{
	GError  *   g_error		= NULL;
    //.........................................................................
	s_gdk_pixbuf[_icon] = gdk_pixbuf_new_from_file(_filename, &g_error);

    if ( g_error )
    {
        GCMD_ERR("GcmdGtkFoldview::Pixbuf_load():%s", g_error->message );
        g_error_free(g_error);
        return FALSE;
    }
    return TRUE;
}
gboolean
GcmdGtkFoldview::Pixbuf_load_all()
{
    if ( s_gdk_pixbuf_loaded )
        return TRUE;

	Pixbuf_load(eIconGtkAdd, "../pixmaps/gtk-add.png");

    return TRUE;
}
void
GcmdGtkFoldview::Pixbuf_unload_all()
{
	g_object_unref(s_gdk_pixbuf[eIconGtkAdd]);
}
//  ***************************************************************************
//
//  Widget signals
//
//  ***************************************************************************
void
GcmdGtkFoldview::Signal_button_add_clicked(
    GtkButton    *  _button,
    gpointer        _udata)
{
    GcmdGtkFoldview     *   gfv     = NULL;
    GnomeCmdCon         *   con     = NULL;
    GList               *   l       = NULL;
    gboolean                b       = FALSE;
    //.........................................................................
    g_return_if_fail( _udata );
    gfv = GCMDGTKFOLDVIEW(_udata);

    // get selected connection
    con = gfv->headband()->get_selected_connection();
    if ( ! con )
    {
        GCMD_ERR("GcmdGtkFoldview::Signal_button_add_clicked():No connection selected");
        return;
    }

    // if connection no more available, abort
    for ( l = gnome_cmd_con_list_get_all(gnome_cmd_con_list_get ()); l ; l = l->next )
    {
        if ( con == (GnomeCmdCon *)l->data )
        {
            b = TRUE;
            break;
        }
    }
    if ( ! b )
    {
        GCMD_ERR("GcmdGtkFoldview::Signal_button_add_clicked():Connection no more available");
        gfv->headband()->update_connections();
        return;
    }

    if ( ! gfv->headband()->can_add_that_connection(con) )
    {
        GCMD_ERR("GcmdGtkFoldview::Signal_button_add_clicked():Cant add that connection");
        return;
    }

    // add a new treeview
    gfv->treeview_add(con);
}
void
GcmdGtkFoldview::Signal_button_up_clicked(
    GtkButton    *  _button,
    gpointer        _udata)
{
}
void
GcmdGtkFoldview::Signal_button_down_clicked(
    GtkButton    *  _button,
    gpointer        _udata)
{
}
//  ***************************************************************************
//
//  Treeviews
//
//  ***************************************************************************
void
GcmdGtkFoldview::treeview_add(
    GnomeCmdCon     *   _con)
{
    GcmdStruct<GnomeCmdConnectionTreeview>  *   ctv = NULL;
    GtkWidget                               *   sep = NULL;
    //.........................................................................
    g_return_if_fail( _con );

    // the separator
    if ( a_list_widget_card != 0 )
    {
        sep = gtk_hseparator_new();
        gtk_widget_set_size_request(sep, -1, 10);
        gtk_widget_show(sep);

        d_list_widget = g_list_append(d_list_widget, sep);
        a_list_widget_card++;
    }

    // the gct
    ctv = GCMD_STRUCT_NEW(GnomeCmdConnectionTreeview, this, _con);
    ctv->show();

    d_list_widget = g_list_append(d_list_widget, ctv);
    a_list_widget_card++;

    // pack
    if ( sep )
        gtk_box_pack_start(GTK_BOX(d_vbox_sw), sep, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(d_vbox_sw), ctv->widget(), TRUE, TRUE, 0);
}

void
GcmdGtkFoldview::treeview_del(
    GnomeCmdConnectionTreeview   *   _ctv)
{
    GList                                   *   l   = NULL;
    GList                                   *   l1  = NULL;
    GList                                   *   l2  = NULL;
    GcmdStruct<GnomeCmdConnectionTreeview>  *   ctv = NULL;
    GtkWidget                               *   sep = NULL;
    //.........................................................................
    g_return_if_fail( _ctv );

    l = g_list_first(d_list_widget);
    while ( l )
    {
        if ( ! GTK_IS_HSEPARATOR((GtkSeparator*)l->data) )
        {
            ctv = (GcmdStruct<GnomeCmdConnectionTreeview>*)(l->data);

            if ( ((GnomeCmdConnectionTreeview*)ctv) == _ctv )
            {
                l1 = l;
                l  = g_list_next(l);
                if ( l )
                {
                    l2  = l;
                    sep = GTK_WIDGET(l->data);
                }

                gtk_container_remove(GTK_CONTAINER(d_vbox_sw), ctv->widget());

                delete ctv;
                d_list_widget = g_list_delete_link(d_list_widget, l1);
                a_list_widget_card--;


                if ( sep )
                {
                    gtk_container_remove(GTK_CONTAINER(d_vbox_sw), sep);

                    d_list_widget = g_list_delete_link(d_list_widget, l2);
                    a_list_widget_card--;
                }
            }
        }

        l = g_list_next(l);
    }
}


void
GcmdGtkFoldview::treeview_set_packing_expansion(
    GnomeCmdConnectionTreeview  *   _ctv,
    gboolean                        _expand)
{
    gtk_box_set_child_packing(GTK_BOX(d_vbox_sw), _ctv->widget(), _expand, TRUE, 0, GTK_PACK_START);
}



