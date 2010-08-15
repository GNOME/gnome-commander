#include <string.h>
#include "gnome-cmd-foldview-treestore.h"

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #								LOGGER									  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

void gwr_inf(const char* fmt, ...);
void gwr_wng(const char* fmt, ...);
void gwr_err(const char* fmt, ...);

//#define DEBUG_STORE
//#define DEBUG_NODES
//#define DEBUG_BLOCKS


// Logging for GnomeCmdFoldviewTreestore
#ifdef DEBUG_STORE

	#define STORE_INF(...)													\
	{																		\
		gwr_inf(__VA_ARGS__);												\
	}
	#define STORE_WNG(...)													\
	{																		\
		gwr_wng(__VA_ARGS__);												\
	}
	#define STORE_ERR(...)													\
	{																		\
		gwr_err( __VA_ARGS__);												\
	}

#else

	#define STORE_INF(...)
	#define STORE_WNG(...)
	#define STORE_ERR(...)

#endif

// Logging for nodes
#ifdef DEBUG_NODES

	#define NODE_INF(...)													\
	{																		\
		gwr_inf(__VA_ARGS__);												\
	}
	#define NODE_WNG(...)													\
	{																		\
		gwr_wng(__VA_ARGS__);												\
	}
	#define NODE_ERR(...)													\
	{																		\
		gwr_err( __VA_ARGS__);												\
	}

#else

	#define NODE_INF(...)
	#define NODE_WNG(...)
	#define NODE_ERR(...)

#endif

// Logging for blocks
#ifdef DEBUG_BLOCKS

	#define BLOCK_INF(...)													\
	{																		\
		gwr_inf(__VA_ARGS__);												\
	}
	#define BLOCK_WNG(...)													\
	{																		\
		gwr_wng(__VA_ARGS__);												\
	}
	#define BLOCK_ERR(...)													\
	{																		\
		gwr_err( __VA_ARGS__);												\
	}

#else

	#define BLOCK_INF(...)
	#define BLOCK_WNG(...)
	#define BLOCK_ERR(...)

#endif


//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #								DATA									  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
gint GnomeCmdFoldviewTreestore::Data::Count = 0;

GnomeCmdFoldviewTreestore::Data::Data()
{
	GnomeCmdFoldviewTreestore::Data::Count++;
	//STORE_INF("DAT(+ %04i)", Count);
}

GnomeCmdFoldviewTreestore::Data::~Data()
{
	Count--;
	//STORE_INF("DAT(- %04i)", Count);
}



//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #								NODES 							 	      #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

gint GnomeCmdFoldviewTreestore::node::Count = 0;

#define NODE_FROM_ITER(_node, _iter)										\
	_node = (GnomeCmdFoldviewTreestore::node*)_iter->user_data

#define ITER_FROM_NODE(_treestore, _iter, _node)							\
	(_iter)->stamp		= _treestore->stamp();								\
	(_iter)->user_data	= _node;											\
	(_iter)->user_data2	= NULL;												\
	(_iter)->user_data3	= _node->data();

#define ITER_RESET(_iter)													\
	(_iter)->stamp		= 0;												\
	(_iter)->user_data	= NULL;												\
	(_iter)->user_data2	= NULL;												\
	(_iter)->user_data3	= NULL;

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node::new:
	*
	*   @_depth  : Depth of the node ( = same as depth in GtkTreePath )
	*   @_pos    : Pos ( starting from 0 ) of the node in the block
	*			   it belongs to
	*   @_parent : Parent node of this node
	*   @_data   : User's data
	*
	**/

//=============================================================================
void*	GnomeCmdFoldviewTreestore::node::operator new(
	size_t								size,
	gint								_depth, // only for the new node_block
	gint								_pos,
	GnomeCmdFoldviewTreestore::node		*_parent,
	GnomeCmdFoldviewTreestore::Data*	_data)
{
	GnomeCmdFoldviewTreestore::node	*n = g_try_new0(GnomeCmdFoldviewTreestore::node, 1);

	if ( !n )
		return n;

	n->a_pos			= _pos;
	n->a_parent			= _parent;
	n->a_next			= NULL;

	n->a_children		= new (_depth + 1, n) node_block;

	n->d_data			= _data;

	Count++;
	NODE_INF("NOD(+%04i nodes) d:%03i p:%03i p:0x%08x,%03i", Count, _depth, _pos, _parent, _parent ? _parent->pos() : 0);

	return n;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node::delete:
	*
	*   This function does _NOT_ free the GArray containing the children.
	*   For recursive delete, use purge()
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::node::operator delete(
void *p)
{
	GnomeCmdFoldviewTreestore::node	*n = (GnomeCmdFoldviewTreestore::node*)p;

	#ifdef DEBUG_NODES
	NODE_INF("NOD(-%04i nodes) p:%03i p:0x%08x,%03i", Count - 1, n->pos(), n->parent(), n->parent() ? n->parent()->pos() : 0);
	#endif

	delete n->data();

	g_free(p);

	Count--;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node::purge:
	*
	*   @node : the node of this block to remove
	*
	*   _RECURSIVELY_ remove the node and all its descendance. The GArray
	*   containing the children of the node is deleted.
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::node::purge()
{
	gint									count   = 0;
	//.........................................................................

	count += children()->purge();
	delete children();
	return count;
}

//=============================================================================
//
//  node : Logging
//
//=============================================================================
#ifdef DEBUG_NODES
const gchar* GnomeCmdFoldviewTreestore::node::log()
{
	static gchar Node_str_01[1024];

	//node : pos parent next children data
	sprintf(Node_str_01, "dep:%03i pos:%03i nxt:%s chd:%03i dat:%s",
		a_parent ? a_parent->children()->depth() : 1,
		a_pos,
		a_next ? "Y" : "N",
		a_children->card(),
		d_data ? "Y" : "N" );

	return Node_str_01;
}
#else
const gchar* GnomeCmdFoldviewTreestore::node::log()
{
	return "XXX";
}
#endif

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							 NODE BLOCKS								  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
gint GnomeCmdFoldviewTreestore::node_block::Count   = 0;

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node_block::new:
	*
	*   @_depth  : The depth of the block ( = same as depth in GtkTreePath )
	*   @_parent : The parent node
	*
	**/

//=============================================================================
void*   GnomeCmdFoldviewTreestore::node_block::operator new(
	size_t								size,
	guint								_depth,
	GnomeCmdFoldviewTreestore::node		*_parent)
{
	GnomeCmdFoldviewTreestore::node_block	*nb = g_try_new0(GnomeCmdFoldviewTreestore::node_block, 1);

	if ( !nb )
		return nb;

	nb->a_card		= 0;
	nb->a_depth		= _depth;
	nb->a_parent	= _parent;

	nb->d_nodes		= g_array_sized_new(
		FALSE,					// zero_terminated element appended at the end
		TRUE,					// all bits set to zero
		sizeof(node*),			// element_size,
		10);					//reserved_size);

	// Fuck, Fuck, Fuck !!!
	// Spended hours on this, g_array_sized_new( ...set bits to 0 )
	// doesnt fucking work !!!
	// printf("GArray 0x%08x d:%03i p:0x%08x [0]=0x%08x\n", nb->d_nodes, nb->a_depth, nb->a_parent, g_array_index(nb->d_nodes, GnomeCmdFoldviewTreestore::node*, 0));

	Count++;
	BLOCK_INF("BLK(+%04i blocks) d:%03i", Count, nb->a_depth);

	return nb;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node_block::delete:
	*
	**/

//=============================================================================
void	GnomeCmdFoldviewTreestore::node_block::operator delete (void *p)
{
	GnomeCmdFoldviewTreestore::node_block *b = (GnomeCmdFoldviewTreestore::node_block*)p;
	#ifdef DEBUG_BLOCKS
	BLOCK_INF("BLK(-%04i blocks) d:%03i c:%03i", Count - 1, b->a_depth, b->a_card);
	#endif

	g_array_free( b->d_nodes, TRUE );

	g_free(p);

	Count--;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node_block::node_get:
	*
	*   @pos : Position ( starting from zero ) of the node to get.
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::node*
GnomeCmdFoldviewTreestore::node_block::node_get(
	gint pos)
{
	GnomeCmdFoldviewTreestore::node			*node   = NULL;
	//.........................................................................
	if ( a_card == 0 )
	{
		// gtk+ calls us with...
		//g_return_val_if_fail( index == 0, NULL );
		if ( pos != 0 )
			return NULL;

		//BLOCK_INF("BLK(%-20s) d:%03i p:%03i c:%03i [NULL]", "node_get", a_depth, index, a_card);
		// this is authorized, since gtk call us for 0th child !!!
		return NULL;
	}
	else
	{
		// gtk+ calls us with bad indexes !!! ( when scrooling liftbars )
		// g_return_val_if_fail( index < a_card, NULL );
		if ( pos >= a_card )
			return NULL;
	}

	node = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::node*, pos);
	//BLOCK_INF("BLK(%-20s) d:%03i p:%03i c:%03i [%s]", "node_get", a_depth, pos, a_card, node->log());
	return node;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node_block::node_append:
	*
	*   @data : user's data
	*
	*   Insert a node in the block. This method respect the ordering settings
	*   of the store.
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::node*
GnomeCmdFoldviewTreestore::node_block::node_append(
GnomeCmdFoldviewTreestore::Data *data)
{
	GnomeCmdFoldviewTreestore::node *node   = NULL;
	GnomeCmdFoldviewTreestore::node *temp   = NULL;
	gint							i		= 0;
	//.........................................................................

	// create a new node with pos = 0 :
	// we cant set the position now, because we dont know at which position
	// we will be stored
	node = new (a_depth, 0, a_parent, data) GnomeCmdFoldviewTreestore::node;

	if ( !GnomeCmdFoldviewTreestore::Render_sort() )
	{
		goto generic_append;
	}
	else
	{
		if ( GnomeCmdFoldviewTreestore::Render_sort_ascending() )
		{
			// ascending - case NO
			if ( !GnomeCmdFoldviewTreestore::Render_sort_case_sensitive() )
			{
				g_assert(FALSE);
			}
			//.................................................................
			// ascending - case YES
			else
			{
				temp = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::node*, 0);
loop_acy:
				// Fuck, Fuck, Fuck !!!
				// Spended hours on this, g_array_sized_new( ...set bits to 0 )
				// doesnt fucking work !!!
				//if ( !temp )
				if ( i >= a_card )
					goto generic_append;

				g_assert( node->parent() == temp->parent() );

				if ( strcmp( node->data()->utf8_collate_key(), temp->data()->utf8_collate_key() ) >= 0 )
					goto generic_insert;

				temp = temp->next(); i++;
				goto loop_acy;
			}
		}
		else
		{
			// descending - case NO
			if ( !GnomeCmdFoldviewTreestore::Render_sort_case_sensitive() )
			{
				g_assert(FALSE);
			}
			//.................................................................
			// descending - case YES
			else
			{
				temp = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::node*, 0);
loop_dcy:
				// Fuck, Fuck, Fuck !!!
				// Spended hours on this, g_array_sized_new( ...set bits to 0 )
				// doesnt fucking work !!!
				//if ( !temp )
				if ( i >= a_card )
					goto generic_append;

				g_assert( node->parent() == temp->parent() );

				if ( strcmp( node->data()->utf8_collate_key(), temp->data()->utf8_collate_key() ) <= 0 )
					goto generic_insert;

				temp = temp->next(); i++;
				goto loop_dcy;
			}
		}
	}


	//.........................................................................
	//
	// Generic back-end : We have to append 'node' at the end of the array
	//
	// append to the end of the array
generic_append:
	// Now we know our pos, it is a_card - 1 + 1 = a_card
	node->a_pos = a_card;

	// Note : I have never seen d_nodes change in append case
	d_nodes = g_array_append_val(d_nodes, node);

	// modify the previous node so its ->next field points to the newly
	// created node ; do that only if we didnt create the first node.
	if ( a_card != 0 )
	{
		temp = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::node*, a_card - 1);
		g_assert( node->parent() == temp->parent() );
		temp->a_next = node;
	}

	a_card++;
	return node;

	//.........................................................................
	//
	// Generic back-end : We have to insert 'node' at pos i, instead of 'temp'
	//
generic_insert:
	// Now we know our pos, it is i
	node->a_pos = i;

	// insert
	d_nodes = g_array_insert_val(d_nodes, i, node);

	// modify the previous node so its ->next field points to 'node' ( at pos i )
	// do that only if we didnt create the first node.
	if ( i != 0 )
		g_array_index(d_nodes, GnomeCmdFoldviewTreestore::node*, i - 1)->a_next = node;

	// modify 'node' so its ->next field points to the node at pos i + 1
	// here we have collated, so we are sure that we have taken the place
	// of a node, that is now just after us
	node->a_next = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::node*, i + 1);

	// increase by 1 all ->pos fields of nodes following 'node'
	temp = node->next();
	g_assert(temp);
	while ( temp )
	{
		temp->a_pos++;
		temp = temp->next();
	}

	a_card++;
	return node;
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node_block::node_cut:
	*
	*   @node : the node to cut
	*
	*   Cut the node, i.e. simply remove its pointer from the node_block
	*   it belongs to, and delete it;
	*
	**/

//=============================================================================
/*
void
GnomeCmdFoldviewTreestore::node_block::node_cut(
	GnomeCmdFoldviewTreestore::node*	node)
{
	GnomeCmdFoldviewTreestore::node*		follow  = NULL;
	gint									i		= 0;
	gint									pos		= 0;
	//.........................................................................

	pos = node->pos();

	// remove the node from the array
	g_array_remove_index(d_nodes, pos);
	a_card--;

	// now update all node->a_pos, starting from node->pos() since it just
	// has been replaced by GArray call
	for ( i = pos ; i < a_card ; i ++ )
	{
		follow = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::node*, i);
		follow->a_pos -= 1;
	}

	// delete the node, it will automatically call delete on the Data* member
	delete node;

	// modify the previous->a_next field
	if ( pos != 0 )
		g_array_index(d_nodes, GnomeCmdFoldviewTreestore::node*, pos - 1)->a_next = NULL;

}
*/

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::node_block::purge:
	*
	*   @node : the node of this block to remove
	*
	*   _RECURSIVELY_ remove all the nodes of the block.
	*   After this call, the block cannot be used, its info is invalid
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::node_block::purge()
{
	GnomeCmdFoldviewTreestore::node*		node	= NULL;
	GnomeCmdFoldviewTreestore::node*		next	= NULL;
	gint									count   = 0;
	//.........................................................................

	node = node_get(0);

	while ( node )
	{
		next = node->next();
		count += node->purge();

		delete node;
		count++;

		node = next;
	}

	// no update block, the "purge" process for node_blocks is separated
	// in two methods - this is the quick part
	return count;
}

gint
GnomeCmdFoldviewTreestore::node_block::purge_and_update()
{
	GnomeCmdFoldviewTreestore::node*		node	= NULL;
	GnomeCmdFoldviewTreestore::node*		next	= NULL;
	gint									count   = 0;
	//.........................................................................

	node = node_get(0);

	while ( node )
	{
		next = node->next();
		count += node->purge();

		delete node;
		count++;

		node = next;
	}

	// update block, the "purge" process for node_blocks is separated
	// in two methods - this is the first call
	g_array_remove_range(d_nodes, 0, a_card);
	a_card = 0;
	return count;
}


//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #					TREESTORE  - CUSTOM METHODS							  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

GnomeCmdFoldviewTreestore::eRenderFlags
	GnomeCmdFoldviewTreestore::Render_flags =
		(GnomeCmdFoldviewTreestore::eRenderFlags)
			(   GnomeCmdFoldviewTreestore::eSortDescending			|
				GnomeCmdFoldviewTreestore::eSortCaseSensitive
			);

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Iter_is_valid:
	*
	*   @iter : the iter to check
	*
	**/

//=============================================================================
inline gboolean
GnomeCmdFoldviewTreestore::iter_is_valid(
	GtkTreeIter *iter)
{
	g_return_val_if_fail( iter->stamp == stamp(), FALSE );

	return TRUE;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Iter_is_valid_but_may_be_null:
	*
	*   @iter : the iter to check
	*
	**/

//=============================================================================
inline gboolean
GnomeCmdFoldviewTreestore::iter_is_valid_but_may_be_null(
	GtkTreeIter *iter)
{
	if ( !iter )
		return TRUE;

	return iter_is_valid(iter);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::emit_row_inserted:
	*
	*   @iter : the iter to check
	*
	**/

//=============================================================================
void GnomeCmdFoldviewTreestore::emit_row_inserted(
GnomeCmdFoldviewTreestore::node* node)
{
	GtkTreePath					*path = NULL;
	GtkTreeIter					iter;
	//.........................................................................
	g_assert( node );

	ITER_FROM_NODE(this, &iter, node);

	path = get_path(GTK_TREE_MODEL(this), &iter);

	gtk_tree_model_row_inserted(
		GTK_TREE_MODEL(this),
		path,
		&iter);

	gtk_tree_path_free(path);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::emit_row_changed:
	*
	*   @iter : the iter to check
	*
	**/

//=============================================================================
void GnomeCmdFoldviewTreestore::emit_row_changed(
GnomeCmdFoldviewTreestore::node* node)
{
	GtkTreePath					*path = NULL;
	GtkTreeIter					iter;
	//.........................................................................
	g_assert( node );

	ITER_FROM_NODE(this, &iter, node);

	path = get_path(GTK_TREE_MODEL(this), &iter);

	gtk_tree_model_row_changed(
		GTK_TREE_MODEL(this),
		path,
		&iter);

	gtk_tree_path_free(path);
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::add_child:
	*
	*	@in_parent  : the iter that will have a new child
	*	@out_child  : the child that will be created
	*	@data		: user's data for the child
	*
	*   Add a child, respect to sorting
	*
	*   Special case : if in_parent is NULL, set the toplevel node
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::add_child(
	GtkTreeIter							*in_parent,
	GtkTreeIter							*out_child,
	GnomeCmdFoldviewTreestore::Data		*data)
{
	GnomeCmdFoldviewTreestore::node				*n_parent	= NULL;
	GnomeCmdFoldviewTreestore::node				*n_child	= NULL;
	//.........................................................................
	g_return_if_fail( iter_is_valid_but_may_be_null(in_parent) );

	// try to set node_root
	if ( ! in_parent )
	{
		g_return_if_fail( ! node_root() );

		d_node_root = new ( 1, 0, NULL, data) GnomeCmdFoldviewTreestore::node;
		n_child = d_node_root;
	}
	else
	{
		NODE_FROM_ITER(n_parent, in_parent );
		g_return_if_fail( n_parent );

		n_child = n_parent->children()->node_append(data);
	}

	ITER_FROM_NODE(this, out_child, n_child);
	emit_row_inserted( n_child );
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::set_value:
	*
	*   @iter : the iter to check
	*
	**/

//=============================================================================
void GnomeCmdFoldviewTreestore::set_value(
	GtkTreeIter *in,
	gint		column,
	GValue		*value)
{
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_if_fail ( iter_is_valid(in) );
	g_return_if_fail ( value );

	g_return_if_fail( G_IS_VALUE(value) );
	g_return_if_fail( G_VALUE_TYPE(value) == G_TYPE_POINTER );

	// specific to foldview
	g_return_if_fail( G_VALUE_HOLDS(value, G_TYPE_POINTER) );

	NODE_FROM_ITER(node, in);
	g_assert(node);

	GnomeCmdFoldviewTreestore::Data* data = node->data();

	if ( data )
		delete data;

	node->data() = (GnomeCmdFoldviewTreestore::Data*)g_value_get_pointer(value);

	emit_row_changed( node );
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::remove_children:
	*
	*   @iter : the iter to check
	*
	**/

//=============================================================================
gint GnomeCmdFoldviewTreestore::remove_children(
	GtkTreeIter *in)
{
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid(in), 0 );

	NODE_FROM_ITER(node, in);
	g_return_val_if_fail( node, 0 );

	gint count = node->children()->purge_and_update();

	return count;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::clear:
	*
	*   Clear * everything *
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::clear()
{
	gint count = 0;

	// store is empty !
	if ( !d_node_root )
		return 0;

	// Quickly purge all beyond node_root
	count = d_node_root->children()->purge();

	// delete node_root
	delete d_node_root->children();
	delete d_node_root;
	count++;

	// Set pointer to NULL
	d_node_root = NULL;

	gwr_inf("GnomeCmdFoldviewTreestore::clear:%03i nodes deleted", count);

	return count;
}




//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #				TREESTORE  - GtkTreeModelIface IMPLEMENTATION			  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::get_flags:
	*
	*   Tells the rest of the world whether our tree model has any special
	*   characteristics. In our case, tree iters are non persistent
	*
	**/

//=============================================================================
GtkTreeModelFlags
GnomeCmdFoldviewTreestore::get_flags(GtkTreeModel *tree_model)
{
  g_return_val_if_fail (IS_GNOME_CMD_FOLDVIEW_TREESTORE(tree_model), (GtkTreeModelFlags)0);

  return (GtkTreeModelFlags)0;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::get_n_columns:
	*
	*   We have only one column.
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::get_n_columns(GtkTreeModel *tree_model)
{
  //g_return_val_if_fail (IS_GNOME_CMD_FOLDVIEW_TREESTORE(tree_model), 0);

  return 1;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::get_column_type:
	*
	*   Our column is of type G_POINTER
	*
	**/

//=============================================================================
GType
GnomeCmdFoldviewTreestore::get_column_type(
	GtkTreeModel *treemodel,
	gint          index)
{
  g_return_val_if_fail (IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), G_TYPE_INVALID);
  g_return_val_if_fail (index == 0, G_TYPE_INVALID);

  return G_TYPE_POINTER;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::get_iter:
	*
	*   Converts a tree path (physical position) into a tree iter structure.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::get_iter(
	GtkTreeModel *treemodel,
	GtkTreeIter  *iter,
	GtkTreePath  *path)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	GnomeCmdFoldviewTreestore::node_block		*block		= NULL;
	gint          *indices = NULL, pos = 0, depth =0;
	gint		i = 0;
	//.........................................................................
	g_assert( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel) );
	g_assert( path!=NULL );

	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);

	indices = gtk_tree_path_get_indices(path);
	depth   = gtk_tree_path_get_depth(path);
	g_assert( indices );
	g_assert( depth > 0 );


	// foldview specific : we have only one toplevel node
	pos		= indices[i++];
	g_assert( pos == 0 );

	// ok, get root node
	node	=   treestore->node_root();

	// treestore is empty !
	if ( ! node )
		return FALSE;

	// loop
	while ( i < depth )
	{
		g_assert(node);

		// go further
		block   = node->children();
		pos		= indices[i++];
		node	= block->node_get(pos);
	}

	g_assert(node);

	ITER_FROM_NODE(treestore, iter, node);
	return TRUE;
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::get_path:
	*
	*  Converts a tree iter into a tree path
	*
	**/

//=============================================================================
GtkTreePath *
GnomeCmdFoldviewTreestore::get_path(
	GtkTreeModel *treemodel,
	GtkTreeIter  *iter)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GtkTreePath									*path		= NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), NULL );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid(iter), NULL);

	path = gtk_tree_path_new();

	NODE_FROM_ITER(node, iter);
	g_return_val_if_fail( node, NULL );

	while ( node )
	{
		gtk_tree_path_prepend_index(path, node->pos());
		node = node->parent();
	}

	STORE_INF("get_path:%s", gtk_tree_path_to_string(path));

	return path;
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::get_value:
	*
	*  Converts a tree iter into a tree path
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::get_value(
	GtkTreeModel *treemodel,
	GtkTreeIter  *iter,
	gint          column,
	GValue       *value)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel) );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_if_fail ( treestore->iter_is_valid(iter) );
	g_return_if_fail ( column == 0 );
	g_return_if_fail ( value );

	NODE_FROM_ITER(node, iter);
	g_assert(node);

	g_value_init(value, G_TYPE_POINTER);
	g_value_set_pointer(value, node->data());
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::iter_next:
	*
	*   Takes an iter structure and sets it to point to the next row.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::iter_next(
	GtkTreeModel  *treemodel,
	GtkTreeIter   *iter)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), FALSE );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid(iter), FALSE );

	NODE_FROM_ITER(node, iter);
	g_assert(node);

	node = node->next();
	if ( !node )
		return FALSE;

	ITER_FROM_NODE(treestore, iter, node);
	return TRUE;
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::iter_children:
	*
	*   Returns TRUE or FALSE depending on whether the row specified by
	*   'parent' has any children. If it has children, then 'iter' is set to
	*   point to the first child.
	*
	*   Special case: if 'parent' is NULL, then the first top-level row should
	*   be returned if it exists.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::iter_children(
	GtkTreeModel *treemodel,
	GtkTreeIter  *iter_child,
	GtkTreeIter  *iter_parent)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), FALSE );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid_but_may_be_null(iter_parent), FALSE );

	if ( ! iter_parent )
	{
		node = treestore->node_root();
			g_return_val_if_fail( node, FALSE );

		ITER_FROM_NODE(treestore, iter_child, node);
		return TRUE;
	}

	NODE_FROM_ITER(node, iter_parent);
	g_assert(node);

	if ( node->children()->card() )
	{
		ITER_FROM_NODE(treestore, iter_child, node->children()->node_get(0));
		return TRUE;
	}

	return FALSE;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::iter_has_child:
	*
	*   Returns TRUE or FALSE depending on whether the row specified by 'iter'
	*   has any children.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::iter_has_child(
	GtkTreeModel *treemodel,
	GtkTreeIter  *iter)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), FALSE );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid(iter), FALSE );

	NODE_FROM_ITER(node, iter);
	g_assert(iter);

	return node->children()->card() != 0 ? TRUE : FALSE;
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::iter_n_children:
	*
	*   Returns the number of children the row specified by 'iter' has.
	*
	*   Special case : if 'iter' is NULL, return the number of top-level nodes
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::iter_n_children(
	GtkTreeModel *treemodel,
	GtkTreeIter  *iter)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), 0 );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid_but_may_be_null(iter), 0 );

	// We have only one node_root for instant
	if ( ! iter )
		return 1;

	NODE_FROM_ITER(node, iter);
	g_assert(node);

	return node->children()->card();
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::iter_nth_child:
	*
	*   If the row specified by 'parent' has any children, set 'iter' to the
	*   n-th child and return TRUE if it exists, otherwise FALSE.
	*
	*   Special case : if 'parent' is NULL, we need to set 'iter' to the n-th
	*   toplevel node.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::iter_nth_child(
	GtkTreeModel *treemodel,
	GtkTreeIter  *iter_child,
	GtkTreeIter  *iter_parent,
	gint          n)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	GnomeCmdFoldviewTreestore::node_block		*block		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), FALSE );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid_but_may_be_null(iter_parent), FALSE );

	if ( ! iter_parent )
	{
		ITER_FROM_NODE(treestore, iter_child, treestore->node_root());
		return TRUE;
	}

	NODE_FROM_ITER(node, iter_parent);
	g_assert( node );

	block = node->children();

	if ( n >= block->card() )
		return FALSE;

	ITER_FROM_NODE(treestore, iter_child, block->node_get(n));
	return TRUE;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::iter_parent:
	*
	*   Point 'iter_parent' to the parent node of 'iter_child'.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::iter_parent(
	GtkTreeModel *treemodel,
	GtkTreeIter  *iter_parent,
	GtkTreeIter  *iter_child)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	GnomeCmdFoldviewTreestore::node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), FALSE );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid(iter_child), FALSE );

	NODE_FROM_ITER(node, iter_child);
	g_assert( node );
	g_assert( node != treestore->node_root() );

	node = node->parent();
	g_assert( node );
	ITER_FROM_NODE(treestore, iter_parent, node);

	return TRUE;
}



//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #					TREESTORE  - GOBJECT STUFF							  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

// GObject stuff - nothing to worry about
static GObjectClass *parent_class = NULL;

static void gnome_cmd_foldview_treestore_init(GnomeCmdFoldviewTreestore*);
static void gnome_cmd_foldview_treestore_class_init(GnomeCmdFoldviewTreestoreClass *klass);
static void gnome_cmd_foldview_treestore_tree_model_init (GtkTreeModelIface *iface);
static void gnome_cmd_foldview_treestore_finalize(GObject *object);

//=============================================================================

  /**
	*   gnome_cmd_foldview_treestore_init:
	*
	*   This is called everytime a new custom list object instance is created
	*   (we do that in gnome_cmd_foldview_treestore_new).
	*
	**/

//=============================================================================
static void
gnome_cmd_foldview_treestore_init(
GnomeCmdFoldviewTreestore *treestore)
{
	treestore->stamp_init();
	treestore->node_root_init();
}

//=============================================================================

  /**
	*   gnome_cmd_foldview_treestore_class_init:
	*
	*   More boilerplate GObject/GType stuff. Init callback for the type system,
	*   called once when our new class is created.
	*
	**/

//=============================================================================
static void
gnome_cmd_foldview_treestore_class_init(GnomeCmdFoldviewTreestoreClass *klass)
{
  GObjectClass *object_class;

  parent_class = (GObjectClass*) g_type_class_peek_parent (klass);
  object_class = (GObjectClass*) klass;

  object_class->finalize = gnome_cmd_foldview_treestore_finalize;
}

//=============================================================================

  /**
	*   gnome_cmd_foldview_treestore_tree_model_init:
	*
	*   Init callback for the interface registration in
	*   gnome_cmd_foldview_treestore_get_type. Here we override the
	*   GtkTreeModel interface functions that we implement.
	*
	**/

//=============================================================================
static void
gnome_cmd_foldview_treestore_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags       = GnomeCmdFoldviewTreestore::get_flags;
	iface->get_n_columns   = GnomeCmdFoldviewTreestore::get_n_columns;
	iface->get_column_type = GnomeCmdFoldviewTreestore::get_column_type;
	iface->get_iter        = GnomeCmdFoldviewTreestore::get_iter;
	iface->get_path        = GnomeCmdFoldviewTreestore::get_path;
	iface->get_value       = GnomeCmdFoldviewTreestore::get_value;
	iface->iter_next       = GnomeCmdFoldviewTreestore::iter_next;
	iface->iter_children   = GnomeCmdFoldviewTreestore::iter_children;
	iface->iter_has_child  = GnomeCmdFoldviewTreestore::iter_has_child;
	iface->iter_n_children = GnomeCmdFoldviewTreestore::iter_n_children;
	iface->iter_nth_child  = GnomeCmdFoldviewTreestore::iter_nth_child;
	iface->iter_parent     = GnomeCmdFoldviewTreestore::iter_parent;
}


//=============================================================================

  /**
	*   gnome_cmd_foldview_treestore_finalize:
	*
	*   This is called just before an instance is destroyed. Free dynamically
	*   allocated memory here.
	*
	**/

//=============================================================================
static void
gnome_cmd_foldview_treestore_finalize(GObject *object)
{
	GnomeCmdFoldviewTreestore					*treestore  = NULL;
	//.........................................................................
	g_return_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(object) );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(object);

	gwr_inf("GnomeCmdFoldviewTreestore::finalize()");

	// free all records and free all memory used by the list
	treestore->clear();

	// must chain up - finalize parent
	(* parent_class->finalize) (object);
}

//=============================================================================

  /**
	*   gnome_cmd_foldview_treestore_get_type:
	*
	*  Here we register our new type and its interfaces
	*  with the type system. If you want to implement
	*  additional interfaces like GtkTreeSortable, you
	*  will need to do it here.
	*
	**/

//=============================================================================
GType
gnome_cmd_foldview_treestore_get_type (void)
{
	static GType gnome_cmd_foldview_treestore_type = 0;

	/* Some boilerplate type registration stuff */
	if (gnome_cmd_foldview_treestore_type == 0)
	{
		static const GTypeInfo gnome_cmd_foldview_treestore_info =
		{
			sizeof (GnomeCmdFoldviewTreestoreClass),
			NULL,                                         /* base_init */
			NULL,                                         /* base_finalize */
			(GClassInitFunc) gnome_cmd_foldview_treestore_class_init,
			NULL,                                         /* class finalize */
			NULL,                                         /* class_data */
			sizeof (GnomeCmdFoldviewTreestore),
			0,                                           /* n_preallocs */
			(GInstanceInitFunc) gnome_cmd_foldview_treestore_init
		};
		static const GInterfaceInfo tree_model_info =
		{
			(GInterfaceInitFunc) gnome_cmd_foldview_treestore_tree_model_init,
			NULL,
			NULL
		};

		/* First register the new derived type with the GObject type system */
		gnome_cmd_foldview_treestore_type = g_type_register_static (G_TYPE_OBJECT, "CustomList",
		&gnome_cmd_foldview_treestore_info, (GTypeFlags)0);

		/* Now register our GtkTreeModel interface with the type system */
		g_type_add_interface_static (gnome_cmd_foldview_treestore_type, GTK_TYPE_TREE_MODEL, &tree_model_info);
	}

  return gnome_cmd_foldview_treestore_type;
}

//=============================================================================

  /**
	*   gnome_cmd_foldview_treestore_new:
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore*
gnome_cmd_foldview_treestore_new(void)
{
  GnomeCmdFoldviewTreestore *t;

  t = (GnomeCmdFoldviewTreestore*) g_object_new (GNOME_CMD_FOLDVIEW_TREESTORE_TYPE, NULL);

  g_assert( t != NULL );

  return t;
}
