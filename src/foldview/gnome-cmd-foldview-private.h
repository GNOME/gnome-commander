/*
    ###########################################################################

    gnome-cmd-foldview-private.h

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

    Main header for GcmdGtkFoldview GObject

    ###########################################################################
*/
#ifndef __GCMDGTKFOLDVIEW_PRIVATE_H__
#define __GCMDGTKFOLDVIEW_PRIVATE_H__

#include	<config.h>

#include    <unistd.h>
#include    <sys/syscall.h>
#include    <sys/types.h>
//  ...........................................................................
#include	<glib.h>
#include	<glib-object.h>
#include	<gio/gio.h>

#include	<gtk/gtkvbox.h>
#include	<gtk/gtklabel.h>
//  ...........................................................................
#include	"gnome-cmd-includes.h"
#include	"gnome-cmd-con.h"
//  ...........................................................................
#include    "gnome-cmd-foldview-utils.h"
#include    "gnome-cmd-foldview-logger.h"
#include    "gnome-cmd-foldview-quickstack.h"
#include    "gnome-cmd-foldview-treestore.h"
//  ***************************************************************************
//								#define
//  ***************************************************************************

//  ===========================================================================
//							#define : GObject
//  ===========================================================================
#define GCMDGTKFOLDVIEW_TYPE			(gcmdgtkfoldview_get_type ())
#define GCMDGTKFOLDVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCMDGTKFOLDVIEW_TYPE, GcmdGtkFoldview))
#define GCMDGTKFOLDVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCMDGTKFOLDVIEW_TYPE, GcmdGtkFoldviewClass))
#define IS_GCMDGTKFOLDVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCMDGTKFOLDVIEW_TYPE))
#define IS_GCMDGTKFOLDVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCMDGTKFOLDVIEW_TYPE))
//  ===========================================================================
//							#define : Foldview config
//  ===========================================================================
#define GCMDGTKFOLDVIEW_ALLOW_MONITORING                    FALSE
#define GCMDGTKFOLDVIEW_USE_GIO								FALSE

#define GCMDGTKFOLDVIEW_GVFS_ITEMS_PER_NOTIFICATION			30

#define GCMDGTKFOLDVIEW_REFRESH_CYCLE_N_MESSAGES            10
#define GCMDGTKFOLDVIEW_REFRESH_CYCLE_PERIOD_MS		        200

#define GCMDGTKFOLDVIEW_SORTING_CYCLE_PERIOD_MS             60

//  ===========================================================================
//							some vars
//  ===========================================================================

struct GnomeCmdConnectionTreeview;

//  ###########################################################################
//
//                          GcmdGtkFoldview
//
//  ###########################################################################
struct GcmdGtkFoldview
{
    //  ***********************************************************************
	//  *																	  *
	//  *							GObject stuff							  *
	//  *																	  *
	//  ***********************************************************************
    public:
	GtkVBox	vbox;
	static  GObjectClass *  Parent_class;
    //  ======================================================================
	static  void	        G_object_init		(GcmdGtkFoldview *foldview);
	static  void	        Gtk_object_destroy	(GtkObject*);
	static  void	        G_object_dispose	(GObject*);
	static  void	        G_object_finalize   (GObject*);

	inline	gint	        ref_count()
                            {
                                return ((GInitiallyUnowned*)this)->ref_count;
                            }
    //  ***********************************************************************
	//  *																	  *
	//  *							Enums, ...    							  *
	//  *																	  *
	//  ***********************************************************************
    enum eIcon
    {
        eIconGtkAdd =   0,
        eIconCard   =   1
    };
    //  ***********************************************************************
	//  *																	  *
	//  *							Structs     							  *
	//  *																	  *
	//  ***********************************************************************

    //  =======================================================================
    //  HeadBand
    //  =======================================================================
    private:
    struct  HeadBand
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        GtkWidget   *   d_hbox_main;
        GtkWidget   *       d_button_add;
        GtkWidget   *       d_con_combo;
        GtkWidget   *       d_alignement_padder;
        GtkWidget   *       d_button_up;
        GtkWidget   *           d_arrow_up;
        GtkWidget   *       d_button_down;
        GtkWidget   *           d_arrow_down;

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        void                add_connection(GnomeCmdCon *con);
        //.................................................................
        public:
        inline GtkWidget *      widget()        { return d_hbox_main;       }

        void            show();
        void            hide();
        void            update_style();

        gboolean            can_add_that_connection(GnomeCmdCon*);
        void                update_connections();
        void                reset_connections();
        GnomeCmdCon     *   get_selected_connection();

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        void    *   operator new    (size_t);
        void        operator delete (void*);

        public:
         HeadBand(GcmdGtkFoldview*);
        ~HeadBand();
    };

    //  ***********************************************************************
	//  *																	  *
	//  *							Members     							  *
	//  *																	  *
	//  ***********************************************************************
    private:
    static  GdkPixbuf               *   s_gdk_pixbuf[eIconCard];
    static  gboolean                    s_gdk_pixbuf_loaded;

            GtkWidget               *   d_scrolled_window;
            GtkWidget               *   d_vbox_sw;
            GcmdStruct<HeadBand>    *   d_headband;

            GList                   *   d_list_widget;
            guint                       a_list_widget_card;

    //  =======================================================================
	// divers
    //  =======================================================================
            void		control_gcmd_file_list_set_connection(GnomeCmdFileList*, GnomeVFSURI*);
            void		control_connection_combo_update();


    //  ***********************************************************************
	//  *																	  *
	//  *							Methods     							  *
	//  *																	  *
	//  ***********************************************************************

    //  =======================================================================
	// Instance init / destroy
    //  =======================================================================
	private:
	void		init_instance();
    void		raz_pointers();
	void		dispose();
	void		finalize();

    //  =======================================================================
	//  Accessors
    //  =======================================================================
    private:
    inline  HeadBand    *   headband()  { return d_headband; }

    //  =======================================================================
	//  Icons
    //  =======================================================================
    private:
    static  gboolean    Pixbuf_load(GcmdGtkFoldview::eIcon, const gchar*);
    static  gboolean    Pixbuf_load_all();
    static  void        Pixbuf_unload_all();

    //  =======================================================================
	//  Widgets signals
    //  =======================================================================
    public:
    static  void    Signal_button_add_clicked   (GtkButton*, gpointer);
    static  void    Signal_button_up_clicked    (GtkButton*, gpointer);
    static  void    Signal_button_down_clicked  (GtkButton*, gpointer);

    //  =======================================================================
	//  Treeviews
    //  =======================================================================
    void            treeview_add(GnomeCmdCon*);
    void            treeview_del(GnomeCmdConnectionTreeview*);
    void            treeview_set_packing_expansion(GnomeCmdConnectionTreeview*, gboolean);
};

GType          gcmdgtkfoldview_get_type ();
GtkWidget*     gcmdgtkfoldview_new		();
//void	       gcmdgtkfoldview_clear	(GcmdGtkFoldview *foldview);


//  ###########################################################################
//
//                          GcmdGtkFoldviewClass
//
//  ###########################################################################
struct GcmdGtkFoldviewClass
{
  GtkVBoxClass parent_class;

  void (* gcmdgtkfoldview) (GcmdGtkFoldview* fv);
};


#endif //__GCMDGTKFOLDVIEW_PRIVATE_H__
