#ifndef _GNOME_CMD_FOLDVIEW_TREESTORE_H_
#define _GNOME_CMD_FOLDVIEW_TREESTORE_H_

#include <gtk/gtk.h>

//***************************************************************************** 
//
//  Docs
//
//***************************************************************************** 
/*

	GtkTreeIter usage:
	{
		gint		stamp		: ginu
		gpointer	user_data   : treestore data : pointer on node ( GnomeCmdFoldviewTreestore::node* )
		gpointer	user_data2  : NULL ( for instant : this is the place for the collate key )
		gpointer	user_data3  : treestore's user data
	}

	So for each valid GtkTreeIter, we have iter->node->data() == iter->user_data3 ; 
	and a treestore's user can just do : get_iter(path), then (user's_type*)iter->user_data3
	instead of treestore->get_value(g_value_stuff...)
	Note that treestore code does that as well...
	
	
*/

//***************************************************************************** 
//
//					Object definition
//
//***************************************************************************** 
#define GNOME_CMD_FOLDVIEW_TREESTORE_TYPE            (gnome_cmd_foldview_treestore_get_type())

#define GNOME_CMD_FOLDVIEW_TREESTORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_FOLDVIEW_TREESTORE_TYPE, GnomeCmdFoldviewTreestore))

#define IS_GNOME_CMD_FOLDVIEW_TREESTORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_CMD_FOLDVIEW_TREESTORE_TYPE))

struct GnomeCmdFoldviewTreestore
{
	//=========================================================================
	//
	//								Data
	//
	//  This structure is for :
	//
	//  - calling delete from the store, so indirectly calling destructor
	//    of user's subclass automaically
	//  - ensure we have a collate key
	//  - debugging purpose
	//
	//=========================================================================
	struct Data
	{
		private:
		static  gint	Count;
		//.................................................................
		public:
		virtual const gchar* utf8_collate_key() = 0;
		//.................................................................
		public:
		Data();
		virtual ~Data() = 0;	// forbid instantiation
	};
	
	//=========================================================================
	//
	//								Nodes 
	//
	//=========================================================================
	struct node_block;

	struct node
	{
		friend struct node_block;

		private:
		static  gint Count;

		private:
		gint								a_pos;
		node							*   a_parent;
		node							*   a_next;
		node_block						*   a_children;
		GnomeCmdFoldviewTreestore::Data *	d_data;
		//.................................................................
		public:
		void*		operator new	(size_t size, gint _depth, gint _pos, node *_parent, GnomeCmdFoldviewTreestore::Data* _data);
		public:
		void		operator delete (void *p);	
		//.................................................................
		public:
		const gchar*		log();
		//.................................................................
		public:
		gint				pos()		{ return a_pos;			}
		node			*   parent()	{ return a_parent;		}
		node			*   next()		{ return a_next;		}
		node_block		*   children()	{ return a_children;	}
		Data*&				data()		{ return d_data;		}				// only this can be modified by all

		private:
		gint		purge();
	};

	//=========================================================================
	//
	//							Node blocks
	//
	//=========================================================================
	struct node_block
	{
		private:
		static  gint Count;

		private:
		gint				a_card;
		guint				a_depth;
		node			*   a_parent;
		// d_nodes will be a GArray of node* ; we dont store entire node structs
		// for speeding the sort ; the counterpart is that we will be forced
		// to malloc each node
		GArray			*   d_nodes;
		//.................................................................
		public:
		void*		operator new	(size_t size, guint _depth, node *_parent);
		void		operator delete (void *p);	
		//.................................................................
		public:
		gint				card()		{ return a_card;	}
		guint				depth()		{ return a_depth;   }
		GArray*				array()		{ return d_nodes;   }
		//.................................................................
		private:
		// useless for instant
		//void		node_cut			(node		*node);
		public:
		node	*   node_get			(gint		index);
		node	*	node_append			(Data		*data);
		gint		purge();
		gint		purge_and_update();
	};

	//=========================================================================
	//
	//						Object
	//
	//=========================================================================
	//
	//  GObject inheritance
	//
	public:
	GObject				parent;

	//.........................................................................
	//  root node
	//.........................................................................
	private:
	node				*d_node_root;
	public:
	node*				node_root()			{ return d_node_root;   }
	void				node_root_init()	{ d_node_root = NULL;   }
	//.........................................................................
	//  stamp
	//.........................................................................
	private:
	gint        		a_stamp;
	public:
	inline  gint		stamp()			{ return a_stamp;			}
	void				stamp_init()	{ a_stamp = 0x00;		}

	//.........................................................................
	//  rendering
	//.........................................................................
	public:
	enum eRenderFlags
	{
		eSortAscending		= 0x01,
		eSortDescending		= 0x02,
		eSortCaseSensitive	= 0x04
	};
	static  eRenderFlags	Render_flags;

	public:
	static  gboolean	Render_sort()					{ return Render_flags != 0;						}
	static  gboolean	Render_sort_case_sensitive()	{ return Render_flags & eSortCaseSensitive;		}
	static  gboolean	Render_sort_ascending()			{ return Render_flags & eSortAscending;			}
	static  gboolean	Render_sort_descending()		{ return Render_flags & eSortDescending;		}
	
	public:
	gboolean	set_render_flags(eRenderFlags flags)
				{
					eRenderFlags old = Render_flags;
					Render_flags = flags;
					
					if ( Render_sort_case_sensitive() )
					{
						if ( !Render_sort_ascending() && !Render_sort_descending() )
						{
							Render_flags = old;
							return FALSE;
						}
					}

					return TRUE;
				}

	public:
	gint		refcount()
				{
					return ((GInitiallyUnowned*)this)->ref_count;
				}

	//.........................................................................
	// GtkTreeModel interface
	//.........................................................................
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

	//.........................................................................
	// our treestore methods
	//.........................................................................
	public:
	// iter validation
	inline gboolean iter_is_valid(GtkTreeIter*);	
	inline gboolean	iter_is_valid_but_may_be_null(GtkTreeIter*);	
	// private helpers for sending signals
	private:
	void	emit_row_inserted   (node*);
	void	emit_row_changed	(node*);
	// useful for foldview
	public:
	void	add_child			(GtkTreeIter *in_parent,	GtkTreeIter *out_child, GnomeCmdFoldviewTreestore::Data*);
	void	set_value			(GtkTreeIter *in,			gint column, GValue *value);
	gint	remove_children		(GtkTreeIter *in);	
	gint	clear();
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

typedef struct _GnomeCmdFoldviewTreestoreClass  GnomeCmdFoldviewTreestoreClass;
 
struct _GnomeCmdFoldviewTreestoreClass
{
  GObjectClass parent_class;
};
 

 
#endif
