/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyleft      2010-2010 Guillaume Wardavoir
							Tim-Philipp Müller

	***************************************************************************
	Tim-Philipp Müller wrote the excellent "GTK+ 2.0 Tree View Tutorial" whose
	section 11 'writing custom models' is the base of the
	GnomeCmdFoldviewTreestore code.
	***************************************************************************

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

#ifndef _GNOME_CMD_FOLDVIEW_TREESTORE_H_
#define _GNOME_CMD_FOLDVIEW_TREESTORE_H_

#include	<gtk/gtk.h>
#include	"gnome-cmd-foldview-utils.h"
#include	"gnome-cmd-foldview-logger.h"

//*****************************************************************************
//
//					Object definition
//
//*****************************************************************************
#define GNOME_CMD_FOLDVIEW_TREESTORE_TYPE            (gnome_cmd_foldview_treestore_get_type())
#define GNOME_CMD_FOLDVIEW_TREESTORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_FOLDVIEW_TREESTORE_TYPE, GnomeCmdFoldviewTreestore))
#define IS_GNOME_CMD_FOLDVIEW_TREESTORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_CMD_FOLDVIEW_TREESTORE_TYPE))

extern Logger sLogger;

struct GnomeCmdFoldviewTreestore
{
	//=========================================================================
	//  Some common things
	//=========================================================================
	public:
	enum eSortType
	{
		eSortNone			= 0x00,
		eSortAscending		= 0x01,
		eSortDescending		= 0x02,
	};

	//=========================================================================
	//  Al structs, for crossed refs in this header
	//=========================================================================
	public:
	struct  Path;
	struct  DataInterface;

	private:
    struct  Node;
    struct  Branch;
    struct  NodeBlock;

	//=========================================================================
	//
	//								Data
	//
	//  Instead of storing boring G_TYPES vars in the store, we can store
	//  any struct that inherits from GnomeCmdFoldviewTreestore::Data
	//  when using GnomeCmdFoldviewTreestore :)
	//
	//  It allows :
	//
	//  - calling delete on this store will directly call g_free on the
	//    users's data, that inherits from this struct.
	//
    //  - convention :
    //
    //      DataInterface_A.compare_xxx(DataInterface_B)
    //
    //      = 0 if DataInterface_A & DataInterface_B cant be ordered
    //
    //      < 0 if A is before B
    //
    //      > 0 if A is after B
	//
	//=========================================================================
	public:
	struct DataInterface
	{
		public:
		inline virtual ~DataInterface() {}

		public:
        virtual gint            compare                     (DataInterface*)            = 0;
        virtual gint            compare_str                 (const gchar*)              = 0;

		public:
		virtual	void			set_path_from_treestore		(Path*)						= 0;
		virtual Path		*	path						()							= 0;
	};

	//=========================================================================
	//
	//							    Node
	//
	//=========================================================================
	private:
	struct Node
	{
        friend struct GnomeCmdFoldviewTreestore;
        friend struct NodeBlock;
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		private:
		enum
		{
			e_UID_SHIFT			= 0												,
			e_UID_BITS			= GCMD_B32(00000000,00001111,11111111,11111111) ,
			e_UID_MASK			= ~e_UID_BITS									,

			e_VISIBLE_SHIFT		= 20											,
			e_VISIBLE_BITS		= GCMD_B32(00000000,00010000,00000000,00000000) ,
			e_VISIBLE_MASK		= ~e_VISIBLE_BITS
		};
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		private:
		static  gint Count;
		public:
		static  gint Remaining()	{ return Count; }
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		private:
		guint32								a_bits;
		gint								a_pos;
		Node							*   a_parent;
		Node							*   a_next;
		NodeBlock						*   a_children;
		DataInterface					*	d_data;
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		public:
		void*		operator new	(size_t size);
		void		operator delete (void *p);

		 Node(guint32 _uid, gint _depth, gint _pos, Node *_parent, DataInterface* _data);
		~Node();
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
                gint				remove_child(gint _pos);
				gint				remove_children();
        //.....................................................................
		public:
		const gchar*				log         ();
        void                        dump_tree   (gint _level = 0);

		inline  guint32				uid()		        { return ( a_bits & e_UID_BITS		) >> e_UID_SHIFT;					}
		inline  gboolean			visible()	        { return ( a_bits & e_VISIBLE_BITS  ) != (guint32)0 ? TRUE  : FALSE;	}
		inline  gboolean			hidden()	        { return ( a_bits & e_VISIBLE_BITS  ) == (guint32)0 ? FALSE : TRUE;		}
		inline  gint				pos()		        { return a_pos;			                                                }
		inline  void				inc_pos()		    { a_pos += 1;    		                                                }
		inline  void				dec_pos()		    { a_pos -= 1;    		                                                }
		inline  void				set_pos(gint _p)    { a_pos = _p;			                                                }
		inline  gint				depth()             { return parent() ? parent()->children()->depth() : 0;                  }
		inline  Node			*   parent()	        { return a_parent;		                                                }
		inline	Node			*   next()		        { return a_next;		                                                }
		inline	void		        set_next(Node *_n)	{ a_next = _n;  		                                                }
		inline	NodeBlock		*   children()	        { return a_children;	                                                }
		inline	gboolean		    is_sterile()	    { return a_children->empty();	                                        }
		inline	DataInterface   *&	data()		        { return d_data;		                                                }
                gint				row();
	};

	//=========================================================================
	//
	//							    NodeBlock
	//
	//=========================================================================
	struct  NodeBlock
	{
        friend struct GnomeCmdFoldviewTreestore;
        friend struct Branch;
        friend struct Node;
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		private:
		static  gint Count;
		public:
		static  gint	Remaining()	{ return Count; }
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		private:
		gint				a_card;
		guint				a_depth;
		Node			*   a_parent;
		// d_nodes will be a GArray of node* ; we dont store entire node structs
		// for speeding the sort : we dont move struct, but only pointers. The
		// counterpart is that we are forced to malloc each node
        // We should use something lighter than a GArray                        // _GWR_TODO_

		GArray			*   d_nodes;
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		public:
		void*		operator new	(size_t size);
		void		operator delete (void *p);

		 NodeBlock  (guint _depth, Node *_parent);
		~NodeBlock  ();
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
		gint		remove_node			(gint	pos);
		gint		remove_nodes		();
        //.....................................................................
		public:
		inline		gint				card()		{ return a_card;	}
		inline		gint				empty()		{ return ! a_card; 	}
		inline		guint				depth()		{ return a_depth;   }
		inline		GArray*				array()		{ return d_nodes;   }

		void        reset();
		Node	*   parent()			{ return a_parent; }
		Node	*   node_get			(gint	pos);
		Node	*	node_add			(guint32 _uid, eSortType, gint _collate_key_to_use, DataInterface*);

	};

	//=========================================================================
	//
	//						        Branch
	//
	//=========================================================================
    private:
    struct Branch
    {
		public:
		void*		operator new	(size_t size);
		void		operator delete (void *p);

         Branch();
         Branch(Node*);
        ~Branch();
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        private:
        Node    *   a_root;
        Node    *   a_root_parent;
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        public:
        inline  gboolean            is_valid()          { return (a_root && a_root_parent);     }
        inline  Node        *       root()              { return a_root;                        }
        inline  Node        *       parent()            { return a_root_parent;                 }

                Branch      *       dup();

                gint                cut();
                gint                cut_but_keep_root();

                gboolean            sort();
    };

	//=========================================================================
	//
	//							    Path
	//
	//=========================================================================
	public:
	struct Path
	{
		private:
		gchar   *   d_ascii_dump;
		gint		a_card;
		guint32	*   d_uid;

		public:
		void	*   operator new	(size_t size);
		void		operator delete (void *p);
					Path			(gint card);
					~Path			();
		public:
		gint				card			();
		guint32				uid_get			(gint _pos);
		void				uid_set			(gint _pos, guint32 _uid);
		Path*				dup				();
		const gchar		*	dump			();
	};

	//=========================================================================
	//
	//					    GnomeCmdFoldviewTreestore
	//
	//=========================================================================
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//  GObject inheritance
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:
    GObject         parent;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//  This one is THE root node, hidden, and father
	//  of all nodes
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	private:
	gboolean			a_node_root_created;
	Node			*   d_node_root;

	public:
	inline  Node    *	node_root()			{ return d_node_root; }
			void		node_root_init();
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//  uids
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	private:
	guint32				a_uid;

	public:
	void				uid_init()	{ a_uid = 0; }
	guint32				uid_new()   { return ++a_uid; }
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//  stamp
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	private:
	gint        		a_stamp;

	public:
	inline  gint		stamp()			{ return a_stamp;			}
	void				stamp_init()	{ a_stamp = 0x00;			}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//  sorting
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	eSortType	a_sort_type;
	gint		a_sort_collate_key_to_use;

	public:
	gboolean	must_sort_ascending()		{ return a_sort_type == eSortAscending;		}
	gboolean	must_sort_descending()		{ return a_sort_type == eSortDescending;	}

	public:
	void		set_sort_type(eSortType sort_type)
                {
                    a_sort_type	= sort_type;
                }
	void		set_sort_collate_key_to_use(gint sort_collate_key_to_use)
				{
					a_sort_collate_key_to_use	= sort_collate_key_to_use;
				}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// some helpers
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//
	// iter validation
	//
	private:
	gboolean    iter_is_valid(GtkTreeIter*);
	gboolean    iter_is_valid_but_may_be_null(GtkTreeIter*);

	//
	// private helpers for sending signals
	//
	private:
	void		emit_row_inserted			(Node*);
	void		emit_row_changed			(Node*);
	void		emit_row_deleted			(Node*);
	void		emit_row_has_child_toggled	(Node*);

	//
	// private
	//
	private:
	guint32		iter_get_uid						(GtkTreeIter*);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// public extensions : useful for foldview
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	public:
    void        ext_dump_tree                       (GtkTreeIter *);
	void		ext_add_child						(GtkTreeIter *_in_parent,	GtkTreeIter *_out_child, DataInterface*);
	void		ext_set_data						(GtkTreeIter *in,			DataInterface*);
	void		ext_data_changed					(GtkTreeIter *in);
	gboolean	ext_get_data						(GtkTreeIter *in,			DataInterface**);
	gint		ext_clear();
	gboolean	ext_get_root						(GtkTreeIter *in,			GtkTreeIter *out_root);
	gboolean	ext_is_root							(GtkTreeIter *in);
	gint		ext_iter_depth						(GtkTreeIter *in);
	gchar*		ext_get_gtk_path_str_new			(GtkTreeIter *in);
	gint		ext_iter_get_row					(GtkTreeIter *in);
	gint		ext_iter_remove_children_no_signal_row_deleted  (GtkTreeIter *in);
	gint	    ext_iter_sterile_remove				(GtkTreeIter *in);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// public extension : paths of uids
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	public:
	gboolean	ext_iter_from_path(			Path *path, GtkTreeIter *_iter_out);
	gboolean	ext_iter_from_path(const	Path *path, GtkTreeIter *_iter_out);
	Path*	    ext_path_from_iter(GtkTreeIter *_iter_in);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// public extension : perform action when matching
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	public:
	gboolean	ext_match_child_gint		(GtkTreeIter *in_parent, GtkTreeIter *out_child, gboolean(*)(DataInterface*, gint),  gint the_gint);
	gboolean	ext_match_child_str         (GtkTreeIter *in_parent, GtkTreeIter *out_child,							 const gchar * the_str);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// GtkTreeModelIface implementation
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	public:
	static  GtkTreeModelFlags   get_flags		(GtkTreeModel*);
	static  gint				get_n_columns   (GtkTreeModel*);
	static  GType				get_column_type (GtkTreeModel*, gint index);

	static  gboolean			get_iter		(GtkTreeModel*, GtkTreeIter *out, GtkTreePath  *path);
	static  GtkTreePath *		get_path		(GtkTreeModel*, GtkTreeIter *in);
	static  void				get_value		(GtkTreeModel*, GtkTreeIter *in,	gint column, GValue *value);
	static  gboolean			iter_next		(GtkTreeModel*, GtkTreeIter *inout);
	static  gboolean			iter_children   (GtkTreeModel*, GtkTreeIter *out, GtkTreeIter *parent);
	static  gboolean			iter_has_child  (GtkTreeModel*,	GtkTreeIter *in);
	static  gint				iter_n_children (GtkTreeModel*,	GtkTreeIter *in_parent);
	static  gboolean			iter_nth_child  (GtkTreeModel*,	GtkTreeIter *out_child,	GtkTreeIter *in_parent,	gint n);
	static  gboolean			iter_parent		(GtkTreeModel*,	GtkTreeIter *out_parent, GtkTreeIter  *in_child);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Divers
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	void	    resort();

	public:
	gint		refcount() { return ((GInitiallyUnowned*)this)->ref_count; }

};

GType         					gnome_cmd_foldview_treestore_get_type   (void);
GnomeCmdFoldviewTreestore*		gnome_cmd_foldview_treestore_new		(void);

//*****************************************************************************
//
//					Class Definition
//
//*****************************************************************************
#define GNOME_CMD_FOLDVIEW_TREESTORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_CMD_FOLDVIEW_TREESTORE_TYPE, GnomeCmdFoldviewTreestoreClass))
#define IS_GNOME_CMD_FOLDVIEW_TREESTORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_CMD_FOLDVIEW_TREESTORE_TYPE))
#define GNOME_CMD_FOLDVIEW_TREESTORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_CMD_FOLDVIEW_TREESTORE_TYPE, GnomeCmdFoldviewTreestoreClass))


struct _GnomeCmdFoldviewTreestoreClass
{
  GObjectClass parent_class;
};

typedef struct _GnomeCmdFoldviewTreestoreClass  GnomeCmdFoldviewTreestoreClass;


#endif
