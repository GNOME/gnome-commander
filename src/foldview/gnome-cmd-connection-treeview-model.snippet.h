/*
    ###########################################################################

    gnome-cmd-connection-treeview-model.snippet.h

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

    Struct  : Model

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
//                  GnomeCmdConnectionTreeview::Model
//
//  ###########################################################################
struct Model
{
    //  ***********************************************************************
	//  *																	  *
	//  *							Enums, ...    							  *
	//  *																	  *
	//  ***********************************************************************
    private: enum eCollateKeys
    {
        eCollateKeyRaw					= 0x00,
        eCollateKeyCaseInsensitive		= 0x01,

        eCollateKeyCard					= 0x02
    };

    public: enum eRowType
    {
        eRowStd						= 0x00  ,
        eRowRoot					= 0x01  ,
        eRowDummy					= 0x02
    };

    public: enum eRowStatus
    {
        eStatusOK       = 0x00  ,
        eStatusPB1      = 0x01  ,
        eStatusPB2      = 0x02
    };
    //  ***********************************************************************
	//  *																	  *
	//  *							Structs     							  *
	//  *																	  *
	//  ***********************************************************************

    //  =======================================================================
    //  All structs declarations, in the good order, for crossed references
    //  in this header ( see UML clas diagram for infos )
    //  =======================================================================
    private:
    // Template class to add GnomeVFS common fields to a struct
    template<typename T> struct GnomeVFSAsyncTpl;

    private:
    struct  SortModule;
    struct  SortModuleInsertion;

    public:
    struct  IterInfo;

    public:
    struct  File;
    struct	    Directory;
    struct	    Symlink;

    private:
    struct  Refresh;
    struct  RefreshList;

    private:
    struct                                              AsyncCallerData;
    struct                                                  AsyncCore;
    struct	                                                    AsyncGetFileInfo;
    struct			                                                GioAsyncGetFileInfo;
    typedef GnomeVFSAsyncTpl<AsyncGetFileInfo>                      GnomeVFSAsyncGetFileInfo;
    struct		                                                AsyncEnumerateChildren;
    struct			                                                GioAsyncEnumerateChildren;
    typedef GnomeVFSAsyncTpl<AsyncEnumerateChildren>                GnomeVFSAsyncEnumerateChildren;

    typedef void (*AsyncCallerCallback)(AsyncCore*);

    private:
    struct  TreeRowInterface;
    struct	  TreeRowStd;
    struct	  TreeRowRoot;
    struct	  TreeRowDummy;

    private:
    struct  MonitorData;
    struct  MonitorInterface;
    struct		Monitor;
    struct			GioMonitor;
    struct			GnomeVFSMonitor;
    struct	GioMonitorHelper;

    public: // cos of View::Treeview
    struct  Row;

    private:
    struct  GnomeVFS;
    struct  GIO;

    private:
    typedef GnomeCmdFoldviewTreestore::Path				TreestorePath;
    typedef GnomeCmdFoldviewTreestore::DataInterface	TreestoreData;

    typedef File		FoldviewFile;
    typedef Directory   FoldviewDir;
    typedef Symlink		FoldviewLink;

    //  =======================================================================
    //  All structs definitions are in this file
    //                      |
    //                      v
    //  =======================================================================
    #include    "gnome-cmd-connection-treeview-model-struct.snippet.h"

    //  ***********************************************************************
	//  *																	  *
	//  *							Members     							  *
	//  *																	  *
	//  ***********************************************************************
    //  =======================================================================
	//  static
    //  =======================================================================
    public:
    static          GtkTreeIter Iter_zero;
    static const    gchar*      Msg_names[];
    static const    gchar*      Msg_name(eMsgType);

    //  =======================================================================
	//  divers
    //  =======================================================================
    private:
    gchar			*   collate_key_new_from_utf8(const gchar* utf8_str);

    //  =======================================================================
	//  core members
    //  =======================================================================
    private:
    Control                         *   a_control;

    GnomeCmdFoldviewTreestore		*   d_treestore;
    struct GIO                          a_GIO;
    struct GnomeVFS                     a_GnomeVFS;
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
    void        init_instance(Control*);

    public:
     Model(GnomeCmdConnectionTreeview::Control*);
    ~Model();

    void        dispose();
    void        finalize();
    //  =======================================================================
	//  accessors
    //  =======================================================================
    public:
    inline  Control                     *   control()       { return a_control;                     }
    inline  GnomeCmdFoldviewTreestore	*   treestore()		{ return d_treestore;					}
    inline  GtkTreeModel				*   treemodel()		{ return GTK_TREE_MODEL(d_treestore);	}
    inline  GtkTreeSortable				*   treesortable()	{ return GTK_TREE_SORTABLE(d_treestore);}
    //  =======================================================================
	//  divers
    //  =======================================================================

    //  =======================================================================
    //  NO LOCK : GtkTreeModelIface impl
    //  =======================================================================
    public:
    gboolean		        get_iter		        (GtkTreePath *path /*in*/, GtkTreeIter *iter /*out*/);
    void			        get_value		        (GtkTreeIter *in, gint column, GValue *value);
    gint			        iter_n_children         (GtkTreeIter *parent);
    //  =======================================================================
    //  NO LOCK : GtkTreeModelIface extensions
    //  =======================================================================
    public:
    Row					*	iter_get_treerow		(GtkTreeIter*);
    gboolean				iter_get_treerow		(GtkTreeIter*, Row**);
    gboolean				iter_set_treerow		(GtkTreeIter*, Row*);

    gboolean				iter_is_expanded		(GtkTreeIter*);
    gboolean				iter_is_collapsed		(GtkTreeIter*);
    gboolean				iter_collapse			(GtkTreeIter*);

    const gchar			*	iter_get_display_string (GtkTreeIter*);
    const Uri   			iter_get_uri			(GtkTreeIter *iter);
    gboolean				iter_get_root			(GtkTreeIter* in_any,   GtkTreeIter *out_root);
    gchar				*	iter_get_path_str_new	(GtkTreeIter* in);
    gboolean				iter_has_uid			(GtkTreeIter* in,		gint uid);
    gboolean				iter_same				(GtkTreeIter*,			GtkTreeIter*);
    gboolean				iter_is_root			(GtkTreeIter*);
    gint					iter_depth				(GtkTreeIter*);

    gboolean				iter_add_tree           (GtkTreeIter *iter_in		,GtkTreeIter *iter_tree_out);
    gboolean                iter_add_tree           (   Uri _utf8_uri, const gchar* _utf8_display_name, Uri _utf8_symlink_target_uri,
                                                        eFileAccess _access, gboolean _is_symlink,
                                                        gboolean _is_samba, gboolean _is_local, gboolean _host_redmond,
                                                        GtkTreeIter* _iter_tree_out);

    gboolean				iter_add_child	        (GtkTreeIter *parent /*in*/, GtkTreeIter *child /*out*/, Row*);

    //  The dummy child of an iter :
    //	  - indicates that there are subdirectories under the iter
    //	  - let GtkTreeView show a little arrow on collapsed iters,
    //		allowing expanding by user-click
    //	  - preserve from keeping in memory iters that are not
    //		displayed. ( Ex : /usr/lib )
    gboolean				iter_dummy_child_add	(GtkTreeIter *parent /*in*/, GtkTreeIter *child /*out*/);
    gboolean				iter_dummy_child_remove (GtkTreeIter *parent);
    void					iter_dummy_child_replace(GtkTreeIter *_iter_parent /*in*/, GtkTreeIter *_iter_child /*out*/, Row* _rw_child_new);
    gboolean				iter_dummy_child_check  (GtkTreeIter *_iter_parent);

    gboolean				iter_remove(GtkTreeIter*);
    gint					iter_collapsed_remove_children(GtkTreeIter *parent);
    void					iter_remove_all();
    //  =======================================================================
    //  NO LOCK : Model methods using files
    //  =======================================================================
    public:
    //gboolean		iter_files_file_already_present		(GtkTreeIter *iter, File *file);
    gboolean		iter_files_file_filter				(File * _file);
    gboolean		iter_files_add_at_least_one			(GList*);

    gboolean	    iter_files_add_file(
                        GtkTreeIter		*   iter_parent,    /// in
                        GtkTreeIter     *   iter_child,     /// out
                        File			*   file,
                        gboolean			check_for_doubloon,
                        gboolean			replace_first_child);
    gboolean	    iter_files_replace(
                        GtkTreeIter		*   iter,    /// in
                        File			*   file);
    //  =======================================================================
    //  Refresh
    //  =======================================================================
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  NO LOCK : Refresh main call
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:
                void		iter_refresh            (GtkTreeIter*);
    //			void		iter_refresh_children   (GtkTreeIter*);
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  LOCKER  : Refresh callbacks
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    private:
    //			GList	*	iter_refresh_store_R            (GList*, GtkTreeIter *iter);
                void        iter_refresh_action             (GtkTreeIter*, TreestorePath*, gboolean, gboolean, gboolean);
    static		void		Iter_refresh_callback           (AsyncCore*);
    static		void		Iter_refresh_children_callback  (AsyncCore*);
    //static	gboolean	Iter_refresh_launch_async_ops   (gpointer);
    //  =======================================================================
    //  Sort
    //  =======================================================================
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  NO LOCK : Sort main call
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:
                void		iter_sort               (GtkTreeIter*);
    //  =======================================================================
	//  Check if empty
    //  =======================================================================
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  NO LOCK : check if empty main call
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:
                void		iter_check_if_empty             (GtkTreeIter*);
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  LOCKER  : check if empty callback
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    private:
    static		void		Iter_check_if_empty_callback    (AsyncCore*);
    //  =======================================================================
	//  Expanded from ui
    //  =======================================================================
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  NO LOCK : expanded from ui main call
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:
            void		    iter_expanded_from_ui           (GtkTreeIter *iter, gboolean _replace_dummy);
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  LOCKER  : expanded from ui callback
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    private:
    static  void		    Iter_expanded_from_ui_callback  (AsyncCore*);
    //  =======================================================================
	//  Enumerate children
    //  =======================================================================
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  NO LOCK : enumerate children main call
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	private:
            void            iter_enumerate_children(GtkTreeIter* , AsyncCallerCallback);
    //  =======================================================================
	//  Collapsed from ui
    //  =======================================================================
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  NO LOCK : collapsed from ui main call
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:
            void		    iter_collapsed_from_ui(GtkTreeIter *iter);
    //  =======================================================================
	//  iter_new_tree
    //  =======================================================================
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  NO LOCK : iter_new_tree
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:
            void		    add_first_tree(const Uri, const gchar*);
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //  LOCKER : iter_new_tree
    //  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    private:
    static  void            Add_first_tree_callback(AsyncCore*);
    //  =======================================================================
	//  Monitoring
    //  =======================================================================
    public:
            gboolean		iter_monitor_start		(GtkTreeIter* _iter);
            gboolean		iter_monitor_stop		(GtkTreeIter* _iter);

    private:
    static  void			Iter_monitor_callback_del	   (MonitorData*);
    static  void			Iter_monitor_callback_child_del(MonitorData*, const gchar* _name_debug);
    static  void			Iter_monitor_callback_child_new(MonitorData*, File*);
    //static  void			Iter_monitor_callback_acc	   (MonitorData*, eFileAccess, const gchar *_name_debug);
    static  void			Iter_monitor_callback_child_acc(MonitorData*, eFileAccess, const gchar *_name_debug);
    //  =======================================================================
	//  Lock
    //  =======================================================================
    public:
	static  gboolean	Lock();
	static  gboolean	Unlock();



};














