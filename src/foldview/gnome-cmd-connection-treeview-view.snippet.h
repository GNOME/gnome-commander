/*
    ###########################################################################

    gnome-cmd-connection-treeview-view.snippet.h

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

    Struct  : View

    Parent  : GnomeCmdConnectionTreeView

    This file is directly inluded in gnome-cmd-connection-treeview.h

    ###########################################################################
*/

//  ***************************************************************************
//								#define
//  ***************************************************************************
//  ===========================================================================
//
//  ===========================================================================


//  ###########################################################################
//
//                  GnomeCmdConnectionTreeview::View
//
//  ###########################################################################
struct View
{
    //  ***********************************************************************
	//  *																	  *
	//  *							Enums, ...    							  *
	//  *																	  *
	//  ***********************************************************************
    //  =======================================================================
    //
    //  =======================================================================
    public:
    //  ***********************************************************************
	//  *																	  *
	//  *							Structs     							  *
	//  *																	  *
	//  ***********************************************************************
    public:
    struct HeadBand;
    struct Treeview;
    //  =======================================================================
    //  Context menu
    //  =======================================================================
    public:
    struct  ctx_menu_data
    {
        Treeview		*   a_treeview;
        GtkTreePath		*   d_path_clicked;
        GtkTreePath		*   d_path_selected;
        guint32				a_time;
        gint				a_event_x;
        gint				a_event_y;
        guint				a_button;

                            ~ctx_menu_data()
                            {
                                if ( d_path_selected )
                                {
                                    gtk_tree_path_free(d_path_selected);
                                }
                                if ( d_path_clicked )
                                {
                                    gtk_tree_path_free(d_path_clicked);
                                }
                            }

        void	*operator   new(size_t size)	{  return g_new0(ctx_menu_data, 1);  }
        void	operator	delete(void *p)		{  g_free(p);  }
    };

    //  =======================================================================
    //  HeadBand
    //  =======================================================================
    public:
    struct HeadBand
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        View        *       a_view;

        GtkWidget   *   d_hbox_main;
        GtkWidget   *       d_button_connection_image;
        GtkWidget   *       d_entry_path;
        GtkWidget   *       d_alignement_padder;
        GtkWidget   *       d_button_refresh;
        GtkWidget   *       d_button_sort;
        GtkWidget   *       d_button_show_hide;
        GtkWidget   *       d_button_close;

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        public:
        inline View         *   view()      { return a_view; }
        inline GtkWidget    *   widget()    { return d_hbox_main; }

        void            show();
        void            hide();
        void            update_style();

        void            set_image_closed();
        void            set_image_opened();

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        void    *   operator new    (size_t);
        void        operator delete (void*);

        void        raz_pointers();
        void        init_instance(GnomeCmdConnectionTreeview::View*);

        public:
         HeadBand(GnomeCmdConnectionTreeview::View*);
        ~HeadBand();

        void        dispose();
        void        finalize();
    };
    //  =======================================================================
    //  Treeview
    //  =======================================================================
    public:
    struct Treeview
    {
        friend class View;
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        View            *   a_view;

        GtkTreeModel    *   a_treemodel;

        GtkWidget       *   d_scrolled_window;
        GtkWidget       *   d_treeview;

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        public:
        inline View         *   view()      { return a_view;                    }
        private:
        inline GtkTreeView  *   treeview()  { return GTK_TREE_VIEW(d_treeview); }
        inline GtkTreeModel *   treemodel() { return a_treemodel;               }
        //.....................................................................
        public:
        inline GtkWidget    *   widget()    { return d_scrolled_window; }

        void            show();
        void            hide();
        void            update_style();
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        void    *   operator new    (size_t);
        void        operator delete (void*);

        void        raz_pointers();
        void        init_instance(GnomeCmdConnectionTreeview::View*, GtkTreeModel*);

        public:
                    Treeview(GnomeCmdConnectionTreeview::View*, GtkTreeModel*);
                   ~Treeview();
        void        dispose();
        void        finalize();
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		private:
		static  void	CellRendererPixbuf(
							GtkTreeViewColumn *col,
							GtkCellRenderer   *renderer,
							GtkTreeModel      *model,
							GtkTreeIter       *iter,
							gpointer           user_data);

		static void		CellRendererTxt(
							GtkTreeViewColumn *col,
							GtkCellRenderer   *renderer,
							GtkTreeModel      *model,
							GtkTreeIter       *iter,
							gpointer           user_data);
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        eRowState           row_state(GtkTreePath* _path);
        gboolean	        row_collapse(GtkTreePath* _path);
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		static  void		signal_cursor_changed   (GtkTreeView* tree_view, gpointer);
		static  gboolean	signal_test_expand_row  (GtkTreeView*, GtkTreeIter*, GtkTreePath*, gpointer);
		static  void		signal_row_expanded     (GtkTreeView*, GtkTreeIter*, GtkTreePath*, gpointer);
		static 	void		signal_row_collapsed    (GtkTreeView*, GtkTreeIter*, GtkTreePath*, gpointer);
        //.....................................................................
		static  gboolean	signal_button_press_event(GtkWidget*, GdkEventButton*, gpointer);
        static  gboolean    click_left_single(ctx_menu_data*);
        static  gboolean    click_left_double(ctx_menu_data*);
        //.....................................................................
        static  gboolean    context_menu_pop(ctx_menu_data*);

        private:
        GtkTreeSelection	*   selection()
                                {
                                    return gtk_tree_view_get_selection(treeview());
                                }
        gint				    selection_count()
                                {
                                    // Note: gtk_tree_selection_count_selected_rows() does not
                                    //   exist in gtk+-2.0, only in gtk+ >= v2.2 !
                                    return 	gtk_tree_selection_count_selected_rows(selection());
                                }
    };
    //  ***********************************************************************
	//  *																	  *
	//  *							Members     							  *
	//  *																	  *
	//  ***********************************************************************
    private:
    Control                 *   a_control;

    GtkWidget               *   d_vbox_main;
    GcmdStruct<HeadBand>    *   d_headband;
    GcmdStruct<Treeview>    *   d_treeview;

    gboolean                a_treeview_showed;

    //  =======================================================================
	// divers
    //  =======================================================================


    //  ***********************************************************************
	//  *																	  *
	//  *							Methods     							  *
	//  *																	  *
	//  ***********************************************************************

    //  =======================================================================
	//  accessors, ...
    //  =======================================================================
    public:
    inline  Control     *       control()           { return a_control;         }
    inline  HeadBand    *       headband()          { return d_headband;        }
    inline  Treeview    *       treeview()          { return d_treeview;        }
    inline  GtkWidget   *       widget()            { return d_vbox_main;       }
    inline  gboolean            treeview_showed()   { return a_treeview_showed; }

    void            show();
    void            hide();
    void            update_style();

    void            show_treeview();
    void            hide_treeview();
    //  =======================================================================
	//  new, ...
    //  =======================================================================
    private:
    void    *   operator new    (size_t);
    void        operator delete (void*);

    void        raz_pointers();
    void        init_instance(GnomeCmdConnectionTreeview::Control*, GtkTreeModel*);

    public:
                View(GnomeCmdConnectionTreeview::Control*, GtkTreeModel*);
               ~View();
    void        dispose();
    void        finalize();
    //  =======================================================================
	//  wrappers
    //  =======================================================================
    public:
    inline  GtkTreeSelection    *   selection()                         { return treeview()->selection();           }
    inline  gint				    selection_count()                   { return treeview()->selection_count();     }
    inline  eRowState               row_state(GtkTreePath* _path)       { return treeview()->row_state(_path);      }
    inline  gboolean	            row_collapse(GtkTreePath* _path)    { return treeview()->row_collapse(_path);   }
    //  =======================================================================
	//  dives
    //  =======================================================================
    public:
    static eIcon	Icon_from_type_access(eFileType, eFileAccess);

    ///////////////////////////////////////////////////////////////////////////


    // For menu editing simplicity
    private:
    gboolean		context_menu_pop(ctx_menu_data*);
    gboolean		click_left_single(ctx_menu_data *ctxdata);
    gboolean		click_left_double(ctx_menu_data *ctxdata);
};














