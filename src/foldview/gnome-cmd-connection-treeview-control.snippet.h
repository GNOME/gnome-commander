/*
    ###########################################################################

    gnome-cmd-connection-treeview-control.snippet.h

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
struct Control
{
    //  ***********************************************************************
	//  *																	  *
	//  *							Enums, ...    							  *
	//  *																	  *
	//  ***********************************************************************
    typedef Model::IterInfo IterInfo;
    //  =======================================================================
    //
    //  =======================================================================
    //  ***********************************************************************
	//  *																	  *
	//  *							Structs     							  *
	//  *																	  *
	//  ***********************************************************************



    //  ***********************************************************************
	//  *																	  *
	//  *							Members     							  *
	//  *																	  *
	//  ***********************************************************************
    private:
    GnomeCmdConnectionTreeview  *   a_connection_treeview;

    //  =======================================================================
	//  divers
    //  =======================================================================


    //  ***********************************************************************
	//  *																	  *
	//  *							Methods     							  *
	//  *																	  *
	//  ***********************************************************************
    //  =======================================================================
	//  new, ...
    //  =======================================================================
    private:
    void    *   operator new    (size_t);
    void        operator delete (void*);

    void        raz_pointers();
    void        init_instance(GnomeCmdConnectionTreeview*);

    public:
     Control(GnomeCmdConnectionTreeview*);
    ~Control();

    void        dispose();
    void        finalize();
    //  =======================================================================
	//  Accessors, ...
    //  =======================================================================
    private:
    inline  Model                       *       model()                 { return a_connection_treeview->model();    }
    inline  View                        *       view()                  { return a_connection_treeview->view();     }
    inline  GnomeCmdConnectionTreeview  *       connection_treeview()   { return a_connection_treeview;             }
    public:
    inline  GtkWidget                   *       widget()                { return view()->widget(); }

    void            show();
    void            hide();
    void            update_style();
    //  =======================================================================
	//  wrappers on Model
    //  =======================================================================
    public:
    inline  void    iter_check_if_empty     (GtkTreeIter* _iter)                            { model()->iter_check_if_empty(_iter);                      }
    inline  void    iter_expanded_from_ui   (GtkTreeIter* _iter, gboolean _replace_dummy)   { model()->iter_expanded_from_ui(_iter, _replace_dummy);    }
    inline  void    iter_collapsed_from_ui  (GtkTreeIter* _iter)                            { model()->iter_collapsed_from_ui(_iter);                   }
    //  =======================================================================
	//  wrappers on View
    //  =======================================================================
    public:
    eRowState       row_state(GtkTreePath* _path)           { return view()->row_state(_path); }
    gboolean        row_collapse(GtkTreePath* _path)        { return view()->row_collapse(_path); }
    //  =======================================================================
	//  wrappers on GnomeCmdConnectionTreeview
    //  =======================================================================
    public:
    inline  gboolean		    is_con_device()         { return a_connection_treeview->is_con_device();        }
    inline  gboolean		    is_con_samba()          { return a_connection_treeview->is_con_samba();         }
    inline  gboolean		    is_con_local()          { return a_connection_treeview->is_con_local();         }
    inline  gboolean		    host_redmond()          { return a_connection_treeview->host_redmond();         }
    inline  GnomeCmdCon     *   gnome_cmd_connection()  { return a_connection_treeview->connection();           }
    inline  GnomeCmdConDevice*  con_device()            { return a_connection_treeview->con_device();           }

    inline  eAccessCheckMode    access_check_mode()     { return a_connection_treeview->access_check_mode();    }
    //  =======================================================================
	//  Widgets signals
    //  =======================================================================
    public:
    static  void    Signal_button_refresh_clicked   (GtkButton*, gpointer);
    static  void    Signal_button_sort_clicked      (GtkButton*, gpointer);
    static  void    Signal_button_show_hide_clicked (GtkButton*, gpointer);
    static  void    Signal_button_close_clicked     (GtkButton*, gpointer);
    //  =======================================================================
	//  Context menu
    //  =======================================================================
	//  -----------------------------------------------------------------------
	//  Context menu code
	//  -----------------------------------------------------------------------
	public:
	enum eContext
	{

		eNothing			= 0						,

		eSelected			= GCMD_B8(00000001)		,
		eClicked			= GCMD_B8(00000010)		,
		eBoth				= GCMD_B8(00000011)		,
		eBothEqual			= GCMD_B8(00000111)		,

		eSelectedIsRoot		= GCMD_B8(00001000)		,

		eAlways				= GCMD_B8(10000000)
	};

	private:
	struct  ctx_menu_desc
	{
		gint		a_context;		// context on which the entry has to be added
		gboolean	a_sensitive;	// wether the entry has to be sensitive
		gchar		*a_text;
		GCallback   a_callback;

		eContext	context()   { return (eContext)a_context; }
	};
	struct  ctx_menu_entry
	{
		ctx_menu_desc   a_desc;
		GtkWidget	   *a_widget;
		gulong			a_handle;
	};
	struct  ctx_menu_section
	{
		gchar			*a_title;
		gint			a_card;
		ctx_menu_entry  a_entry[5];
	};
	struct  ctx_menu
	{
		GtkWidget			*a_widget;
		gint				a_card;
		ctx_menu_section	a_section[5];
	};

	static		ctx_menu	Context_menu;

	void		context_menu_populate_add_separator(GtkWidget *widget);
	void		context_menu_populate_add_section(GtkWidget *widget, gint i, View::ctx_menu_data *ctxdata);
	void		context_menu_populate(GtkWidget*, View::ctx_menu_data*);
	public:
	void		context_menu_pop(View::ctx_menu_data*);

	//  -----------------------------------------------------------------------
	//  Context menu : User actions
	//  -----------------------------------------------------------------------
	private:
			void		set_active_tab	(GtkTreePath *path);
			void		open_new_tab	(GtkTreePath *path);
			void		tree_create		(View::ctx_menu_data*);
			void		tree_delete		(View::ctx_menu_data*);
			void		refresh			(View::ctx_menu_data*);
			void		sort			(View::ctx_menu_data*);
	public:
	static  void		Set_active_tab	(GtkMenuItem*, View::ctx_menu_data*);
	static  void		Open_new_tab	(GtkMenuItem*, View::ctx_menu_data*);
	static  void		Tree_create		(GtkMenuItem*, View::ctx_menu_data*);
	static  void		Tree_delete		(GtkMenuItem*, View::ctx_menu_data*);
	static  void		Refresh			(GtkMenuItem*, View::ctx_menu_data*);
	static  void		Sort			(GtkMenuItem*, View::ctx_menu_data*);

	private:
	enum eSyncState
	{
		SYNC_Y_LIST_Y,
		SYNC_Y_LIST_N,
		SYNC_N_LIST_Y,
		SYNC_N_LIST_N
	};
	eSyncState	sync_check		();
	private:
			void		sync_treeview   (View::ctx_menu_data*);
			void		unsync_treeview ();
			void		sync_update		(View::ctx_menu_data*);
	public:
	static  void		Sync_treeview   (GtkMenuItem*, View::ctx_menu_data*);
	static  void		Unsync_treeview (GtkMenuItem*, View::ctx_menu_data*);
	static  void		Sync_update		(GtkMenuItem*, View::ctx_menu_data*);

	private:
	GnomeCmdFileList		*   a_synced_list;
	gboolean				    a_synced;

    //  =======================================================================
	//                      Controller : Messages
    //  =======================================================================
    //
    //  Messages are implemented in the controller rather than in the model
    //  because :
    //
    //  - Model is getting bloated
    //
    //  - some actions need to act on the model, but on the view too (
    //  ex : remove an iter, implemented with a hack ( _GWR_HACK_ ) ). That
    //  is not allowed to a model.
    //

	//  -----------------------------------------------------------------------
    //  Message queue
	//  -----------------------------------------------------------------------
    private:
    GList               *		d_message_fifo_first;
    GList               *		d_message_fifo_last;
    guint32                     a_message_fifo_card;
    guint                       a_message_fifo_id;

    void              	        message_fifo_lock();
    void			            message_fifo_unlock();

    gboolean                    message_fifo_start();
    gboolean                    message_fifo_stop();

    Model::MsgCore	    *		message_fifo_pop();
    static  gboolean			Message_fifo_func_timeout(gpointer);

    public:
    gboolean			        message_fifo_push(Model::MsgCore*);
	//  -----------------------------------------------------------------------
    //  Messages actions
	//  -----------------------------------------------------------------------
    private:
    gboolean    iter_message_add_first_tree     (Model::MsgAddFirstTree    *);
    gboolean    iter_message_add_child          (Model::MsgAddChild        *);
    gboolean    iter_message_add_dummy_child    (Model::MsgAddDummyChild   *);
    gboolean    iter_message_del                (Model::MsgDel             *);
    gboolean    iter_message_set_readable       (Model::MsgSetReadable     *);
    gboolean    iter_message_set_not_readable   (Model::MsgSetNotReadable  *);
    gboolean    iter_message_AsyncMismatchIEFUC (Model::MsgAsyncMismatchIEFUC*);
    gboolean    iter_message_AsyncMismatchICIEC (Model::MsgAsyncMismatchICIEC*);
    //  =======================================================================
	//                      Controller : Sorting
    //  =======================================================================
    //
    //  Sorting is implemented in the controller rather than in the model
    //  because :
    //
    //  - Model is getting bloated
    //
    //  - some actions need to act on the model, but on the view too (
    //  ex : remove an iter, implemented with a hack ( _GWR_HACK_ ) ). That
    //  is not allowed to a model.
    //

	//  -----------------------------------------------------------------------
    //  Sorting list
	//  -----------------------------------------------------------------------
    public:
    GList               *		d_sorting_list_first;
    GList               *		d_sorting_list_last;
    guint32                     a_sorting_list_card;
    guint                       a_sorting_list_id;

    void              	        sorting_list_lock();
    void			            sorting_list_unlock();

    gboolean                    sorting_list_start();
    gboolean                    sorting_list_stop();

    gboolean			        sorting_list_add(Model::MsgCore*);

    static  gboolean			Sorting_list_func_timeout(gpointer);
    //static  gpointer			Sorting_list_func_thread(gpointer);

	//  -----------------------------------------------------------------------
    //  Sorting : done in specific messages
	//  -----------------------------------------------------------------------


};














