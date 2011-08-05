/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak
    Copyleft      2010-2011 Guillaume Wardavoir
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

#include	<glib.h>
#include	<glib/gprintf.h>
#include	<string.h>
#include	"gnome-cmd-foldview-treestore.h"
#include	"gnome-cmd-foldview-utils.h"



//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #								DATA									  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #								PATH									  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
void*
GnomeCmdFoldviewTreestore::Path::operator new(size_t t)
{
	Path * path = g_try_new0(Path, 1);

	if ( ! path )
		GCMD_ERR("GnomeCmdFoldviewTreestore::Path::operator new:g_try_new0 failed");

	return path;
}
void
GnomeCmdFoldviewTreestore::Path::operator delete(void *p)
{
	g_free(p);
}

GnomeCmdFoldviewTreestore::Path::Path(gint card)
{
	d_ascii_dump	= (gchar*)g_try_malloc0(64);
	a_card			= card;
	d_uid			= (guint32*) g_try_malloc0(a_card * sizeof(guint32) );
}
GnomeCmdFoldviewTreestore::Path::~Path()
{
	g_free(d_ascii_dump);
	g_free(d_uid);
}

gint
GnomeCmdFoldviewTreestore::Path::card()
{
	return a_card;
}

guint32
GnomeCmdFoldviewTreestore::Path::uid_get(gint _pos)
{
	return d_uid[_pos];
}

void
GnomeCmdFoldviewTreestore::Path::uid_set(gint _pos, guint32 _uid)
{
	d_uid[_pos] = _uid;
}

GnomeCmdFoldviewTreestore::Path*
GnomeCmdFoldviewTreestore::Path::dup()
{
	Path	*   dup = NULL;
	gint		i   = 0;
	//.........................................................................

	dup = new Path(card());

	for ( i = 0 ; i != card() ; i++ )
		dup->uid_set(i, uid_get(i));

	return dup;
}

const gchar*
GnomeCmdFoldviewTreestore::Path::dump()
{
	gchar   temp[128];
	gint	i ;
	//.........................................................................
	if ( a_card == 0 )
	{
		sprintf(d_ascii_dump, "~ Empty ~");
		return (const gchar*)d_ascii_dump;
	}

	sprintf(d_ascii_dump, "%03i", uid_get(0));

	for ( i = 1 ; i < a_card ; i++ )
	{
		sprintf(temp, " %03i ", uid_get(i));
		strcat(d_ascii_dump, temp);
	}

	return (const gchar*)d_ascii_dump;
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #								NODES 							 	      #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

gint GnomeCmdFoldviewTreestore::Node::Count = 0;

#define NODE_FROM_ITER(_node, _iter)										\
	_node = (GnomeCmdFoldviewTreestore::Node*)_iter->user_data

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
	*   GnomeCmdFoldviewTreestore::Node::new()
	*
	**/

//=============================================================================
void*	GnomeCmdFoldviewTreestore::Node::operator new(
	size_t  size)
{
	GnomeCmdFoldviewTreestore::Node	*n = g_try_new0(GnomeCmdFoldviewTreestore::Node, 1);

	return n;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Node::delete()
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::Node::operator delete(
void *p)
{
	g_free(p);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Node::node()
	*
	*   @_depth  : Depth of the node ( = same as depth in GtkTreePath )
	*   @_pos    : Pos ( starting from 0 ) of the node in the block
	*			   it belongs to
	*   @_parent : Parent node of this node
	*   @_data   : User's data
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::Node::Node(
	guint32					_uid,
	gint					_depth, // only for the new node_block
	gint					_pos,
	Node				*   _parent,
	DataInterface		*   _data)
{
	a_bits			=   0;
	a_bits			+=  ( (guint32)( _uid                           << e_UID_SHIFT)      ) & e_UID_BITS;
	a_pos			=   _pos;
	a_parent	    =   _parent;
	a_next			=   NULL;

	a_children		=   new NodeBlock(_depth + 1, this);

	d_data			=   _data;

	Count++;
	//NODE_INF("NOD+(%04i):[%s]",Count, log());
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Node::~node()
	*
	*   Here we only delete the memory allowed for the struct itself.
    *   We do _NOT_ free the node_block containing the children.
	*   For recursive delete, use remove()
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::Node::~Node()
{
	//NODE_INF("NOD-(%04i):[%s]", (Count - 1), log());

    delete data();

	Count--;
}



//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Node:remove_children()
	*
	*   __RECURSIVE__ remove the descendance of the node.
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::Node::remove_children()
{
    return children()->remove_nodes();
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Node:remove_child()
	*
	*   __RECURSIVE__ remove a child and all its descendance.
    *
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::Node::remove_child(
    gint    _pos)
{
	gint									count   = 0;
	//.........................................................................

    // remove children nodes
	count += children()->remove_node(_pos);

	return count;
}
//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Node::row
	*
	*   __RECURSIVE__
	*   Get a node's row index in the tree
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::Node::row()
{
	if ( depth() != 1 )
	{
		return ( pos() + 1 ) + parent()->row();
	}

	return pos();
}

//=============================================================================
//
//  node : Logging
//
//=============================================================================
const gchar* GnomeCmdFoldviewTreestore::Node::log()
{
	static gchar Node_str_01[1024];

	//node : ref_count pos parent next children data
	sprintf(Node_str_01, "this:%08x dep:%03i pos:%03i nxt:%s chd:%03i dat:%s",
        this,
		a_parent ? a_parent->children()->depth() : 1,
		a_pos,
		a_next ? "Y" : "N",
		a_children->card(),
		d_data ? "Y" : "N" );

	return Node_str_01;
}

void GnomeCmdFoldviewTreestore::Node::dump_tree(
    gint    _level)
{
    GnomeCmdFoldviewTreestore::NodeBlock * block   = NULL;
    GnomeCmdFoldviewTreestore::Node *       child   = NULL;
    gint                                    card    = 0;
    gint                                    depth   = 0;

    gint                                i       = 0;
    gchar                               sp      [256];
    gchar                               s1      [256];
    gchar                               s2      [256];
    //.........................................................................
    //
    //  0123456789
    //  [xxxxxxxx]                      0   8
    //      |                           4   1
    //      +-----[xxxxxxxx]            4   10
    //                |
    //                +-----[xxxxxxxx]
    //

    //.........................................................................
    //
    //  some vars
    //
    block   = children();
    card    = block->card();
    depth   = block->depth();

    //.........................................................................
    //
    //  this
    //
    if ( _level  == 0 )
    {
        strcpy(s1, "     ");
        strcpy(s2, "     ");
    }
    if ( _level  == 1 )
    {
        strcpy(s1, "         |");
        strcpy(s2, "         +-----");
    }
    if ( _level >= 2 )
    {
        strcpy(sp, "     ");
        for ( i = 2 ; i <= _level ; i++ )
        {
            //          0123456789
            strcat(sp, "          ");
        }

        strcpy(s1, sp);
        strcpy(s2, sp);
        strcat(s1, "    |");
        strcat(s2, "    +-----");
    }

    NODE_TKI("%s", s1);
    NODE_TKI("%s[%08x]", s2, this);

    //.........................................................................
    //
    //  children
    //
    for ( i = 0 ; i != card ; i++ )
    {
        child = g_array_index(block->array(), GnomeCmdFoldviewTreestore::Node*, i);

        child->dump_tree(1 + _level);
    }
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							 NODE BLOCKS								  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
gint GnomeCmdFoldviewTreestore::NodeBlock::Count   = 0;

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::new:
	*
	**/

//=============================================================================
void*   GnomeCmdFoldviewTreestore::NodeBlock::operator new(
	size_t								size)
{
	GnomeCmdFoldviewTreestore::NodeBlock	*nb = g_try_new0(GnomeCmdFoldviewTreestore::NodeBlock, 1);

	return nb;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::delete:
	*
	**/

//=============================================================================
void	GnomeCmdFoldviewTreestore::NodeBlock::operator delete (void *p)
{
	g_free(p);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::NodeBlock()
	*
	*   @_depth  : The depth of the block ( = same as depth in GtkTreePath )
	*   @_parent : The parent node
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::NodeBlock::NodeBlock(
	guint									_depth,
	GnomeCmdFoldviewTreestore::Node		*   _parent)
{
	a_card		= 0;
	a_depth		= _depth;
	a_parent	= _parent;

	d_nodes		= g_array_sized_new(
		FALSE,					// zero_terminated element appended at the end
		TRUE,					// all bits set to zero
		sizeof(Node*),			// element_size,
		10);					//reserved_size);

	// Fuck, Fuck, Fuck !!!
	// Spended hours on this, g_array_sized_new( ...set bits to 0 )
	// doesnt fucking work !!!
	// printf("GArray 0x%08x d:%03i p:0x%08x [0]=0x%08x\n", nb->d_nodes, nb->a_depth, nb->a_parent, g_array_index(nb->d_nodes, GnomeCmdFoldviewTreestore::Node*, 0));

	Count++;
	//BLOCK_INF("BLK+(%04i):d %03i", Count, a_depth);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::~node_block()
	*
	*   Here we only delete the array of children ; it does _NOT_ affect the
	*   children objects.
	*   For recursive delete, use purge()
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::NodeBlock::~NodeBlock()
{
	//BLOCK_INF("BLK-(%04i):d %03i c:%03i", Count - 1, a_depth, a_card);

	g_array_free( d_nodes, TRUE );

	Count--;
}



//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::reset()
	*
	*   Set the number of children to zero, redim the GArray
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::NodeBlock::reset()
{
    if ( ! a_card )
        return;

	g_array_remove_range(d_nodes, 0, a_card);
	a_card = 0;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::node_get:
	*
	*   @pos : Position ( starting from zero ) of the node to get.
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::Node*
GnomeCmdFoldviewTreestore::NodeBlock::node_get(
	gint pos)
{
	GnomeCmdFoldviewTreestore::Node			*node   = NULL;
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

	node = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, pos);
	//BLOCK_INF("BLK(%-20s) d:%03i p:%03i c:%03i [%s]", "node_get", a_depth, pos, a_card, node->log());
	return node;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::node_append:
	*
	*   @data : user's data
	*
	*   Insert a node in the block. This method respect the ordering settings
	*   of the store.
	*
    **/

//=============================================================================
GnomeCmdFoldviewTreestore::Node*
GnomeCmdFoldviewTreestore::NodeBlock::node_add(
	guint32				_uid,
	eSortType			_sort_type,
	gint				_collate_key_to_use,
	DataInterface	*   _data)
{
	GnomeCmdFoldviewTreestore::Node *node   = NULL;
	GnomeCmdFoldviewTreestore::Node *temp   = NULL;
	gint							i		= 0;
	//.........................................................................

	// create a new node with pos = 0 :
	// we cant set the position now, because we dont know at which position
	// we will be stored
	node = new GnomeCmdFoldviewTreestore::Node(_uid, a_depth, 0, a_parent, _data);

	if ( ! _sort_type  ) // eSortNone is 0x00
	{
		goto generic_append;
	}

	if ( _sort_type == GnomeCmdFoldviewTreestore::eSortDescending )
	{
		temp = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, 0);
loop_acy:
		// Fuck, Fuck, Fuck !!!
		// Spended hours on this, g_array_sized_new( ...set bits to 0 )
		// doesnt fucking work ( in glib 2.16.6 ) !!!
		if ( i >= a_card )
			goto generic_append;

        //  Previous method, with direct access to collate keys
        //  from DataInterface
        //
        //		if ( strcmp (
        //				node->data()->utf8_collate_key(_collate_key_to_use),
        //				temp->data()->utf8_collate_key(_collate_key_to_use)
        //					) >= 0 )
        //
        if ( node->data()->compare(temp->data()) >= 0 )
            goto generic_insert;

		temp = temp->next();
		i++;
		goto loop_acy;
	}

	if ( _sort_type == GnomeCmdFoldviewTreestore::eSortAscending )
	{
		temp = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, 0);
loop_dcy:
		// Fuck, Fuck, Fuck !!!
		// Spended hours on this, g_array_sized_new( ...set bits to 0 )
		// doesnt fucking work ( in glib 2.16.6 ) !!!
		if ( i >= a_card )
			goto generic_append;

        //  Previous method, with direct access to collate keys
        //  from DataInterface
        //
		//if ( strcmp (
		//		node->data()->utf8_collate_key(_collate_key_to_use),
		//		temp->data()->utf8_collate_key(_collate_key_to_use)
		//			) <= 0 )
        //
        if ( node->data()->compare(temp->data()) <= 0 )
			goto generic_insert;

		temp = temp->next(); i++;
		goto loop_dcy;
	}

	// Bad sort type
	g_assert(FALSE);

	//.........................................................................
	//
	// 'Append' back-end : We have to append 'node' at the end of the array
	//
	// append to the end of the array
generic_append:
	// Now we know the node's pos, it is a_card - 1 + 1 = a_card
	node->set_pos(a_card);

	// Note : I have never seen d_nodes change in append case
	// but I still reaffect the GArray, cf GLib documentation
	d_nodes = g_array_append_val(d_nodes, node);

	// modify the previous node so its ->next field points to the newly
	// created node ; do that only if we didnt create the first node.
	if ( a_card != 0 )
	{
		temp = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, a_card - 1);
		temp->set_next(node);
	}

	a_card++;
	return node;

	//.........................................................................
	//
	// 'Insert' back-end : We have to insert 'node' at pos i, instead of 'temp'
	//
generic_insert:
	// Now we know our pos, it is i
	node->set_pos(i);

	// I reaffect the GArray, cf GLib documentation, but I didnt dig this case
	d_nodes = g_array_insert_val(d_nodes, i, node);

	// modify the previous node so its ->next field points to 'node' ( at pos i )
	// do that only if we didnt create the first node.
	if ( i != 0 )
		g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, i - 1)->set_next(node);

	// modify 'node' so its ->next field points to the node at pos i + 1
	// here we have collated, so we are sure that we have taken the place
	// of a node, that is now just after us
	node->set_next( g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, i + 1) );

	// increase by 1 all ->pos fields of nodes following 'node'
	temp = node->next();
	g_assert(temp);
	while ( temp )
	{
		temp->inc_pos();
		temp = temp->next();
	}

	a_card++;
	return node;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::remove_node()
	*
	*   @param node : the node to remove
	*
	*   __RECURSIVE__
	*   Remove the node and all its children. Update block info.
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::NodeBlock::remove_node(
	gint pos)
{
	GnomeCmdFoldviewTreestore::Node*		node	= NULL;
	GnomeCmdFoldviewTreestore::Node*		follow	= NULL;
	gint									i		= 0;
    gint                                    count   = 0;
	//.........................................................................
	node = node_get(pos);
	g_return_val_if_fail( node, 0 );

    //BLOCK_INF("remove_node():[%s]", node->log());

	// remove the node from the array
	g_array_remove_index(d_nodes, pos);
	a_card--;

	// modify eventually the previous node's "next" field
	// ( dont call node_get, we do inline-verifications
    //  - pos = 0 => no previous node to modify
    //  - pos !== a_card <=> we remove the last node ; in this case next = NULL
	if ( pos != 0 )
			g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, pos - 1)->set_next
            (
				( pos != a_card														?
					g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, pos)   :
					NULL )
            );

	// update all nodes "pos" field, starting from node->pos() since it just
	// has been replaced by the g_array_index call
	// ( dont call node_get, we dont need verifications )
	for ( i = pos ; i < a_card ; i ++ )
	{
		follow = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, i);
		follow->dec_pos();
	}

    //
    // delete the node and all its children
    //

    // remove children nodes
	count += node->remove_children();

    // delete the NodeBlock struct
	delete node->children();

    // delete the Node struct
    delete node;

    // return count
	return ( 1 + count);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::NodeBlock::remove_nodes()
	*
	*   __RECURSIVE__
	*   Remove all the nodes and all their children. Update block info.
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::NodeBlock::remove_nodes()
{
	GnomeCmdFoldviewTreestore::Node*		node	= NULL;
	GnomeCmdFoldviewTreestore::Node*		next	= NULL;
    gint                                    count   = 0;
	//.........................................................................

    //BLOCK_INF("remove_nodes():[%03i]", a_card);

    node = node_get(0);

    while ( node )
    {
        next    =   node->next();
        count   +=  remove_node(node->pos());
        node    =   next;
    }
	//for ( i = 0 ; i < a_card ; i ++ )
	//{
	//	node = g_array_index(d_nodes, GnomeCmdFoldviewTreestore::Node*, i);
	//	count += remove_node(i);
	//}

    reset();

    return count;
}


//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							    BRANCH     								  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::operator new()
	*
	**/

//=============================================================================
void*   GnomeCmdFoldviewTreestore::Branch::operator new(size_t size)
{
	return (void*)g_try_new0(GnomeCmdFoldviewTreestore::Branch, 1);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::operator delete()
	*
	**/

//=============================================================================
void	GnomeCmdFoldviewTreestore::Branch::operator delete(void *p)
{
	g_free(p);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::Branch()
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::Branch::Branch(
	Node    *   _node)
{
    Node    *   _parent;
    //.........................................................................
    Branch();

    g_return_if_fail( _node );

    _parent = _node->parent();
    g_return_if_fail( _parent );

    a_root          =   _node;
    a_root_parent   =   _parent;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::Branch()
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::Branch::Branch()
{
    a_root          =   NULL;
    a_root_parent   =   NULL;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::~Branch()
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::Branch::~Branch()
{
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::dup()
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::Branch*
GnomeCmdFoldviewTreestore::Branch::dup()
{
    return NULL;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::cut()
	*
    *   Delete a branch.
    *
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::Branch::cut()
{
    gint            count               = 0;
    NodeBlock   *   block_parent        = NULL;
    //.........................................................................

    // tell the parent to remove a_root child
    block_parent = a_root_parent->children();
    g_return_val_if_fail( block_parent, -1 );

    count = block_parent->remove_node( a_root->pos() );

    a_root = NULL;

    return count;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::cut_but_keep_root()
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::Branch::cut_but_keep_root()
{
	return a_root->remove_children();
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::Branch::sort()
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::Branch::sort()
{
    return FALSE;
}



//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #							NODE ROOT									  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################
void
GnomeCmdFoldviewTreestore::node_root_init()
{
	//g_return_if_fail( ! a_node_root_created );

	d_node_root = new GnomeCmdFoldviewTreestore::Node( 0, 0, 0, NULL, NULL) ;
	a_node_root_created = TRUE;
}

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #					TREESTORE  - CUSTOM METHODS							  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

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
	*   @iter : the iter inserted
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::emit_row_inserted(
GnomeCmdFoldviewTreestore::Node* node)
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
	*   @iter : the iter changed
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::emit_row_changed(
GnomeCmdFoldviewTreestore::Node* node)
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
	*   GnomeCmdFoldviewTreestore::emit_row_deleted:
	*
	*   @iter : the iter deleted
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::emit_row_deleted(
GnomeCmdFoldviewTreestore::Node* node)
{
	GtkTreePath					*path = NULL;
	GtkTreeIter					iter;
	//.........................................................................
	g_assert( node );

	ITER_FROM_NODE(this, &iter, node);

	path = get_path(GTK_TREE_MODEL(this), &iter);

	gtk_tree_model_row_deleted(
		GTK_TREE_MODEL(this),
		path);

	gtk_tree_path_free(path);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::emit_row_has_child_toggled:
	*
	*   @iter : the iter that has
	*			- its first child
	*			  or
	*			- no more children
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::emit_row_has_child_toggled(
GnomeCmdFoldviewTreestore::Node* node)
{
	GtkTreePath					*path = NULL;
	GtkTreeIter					iter;
	//.........................................................................
	g_assert( node );

	ITER_FROM_NODE(this, &iter, node);

	path = get_path(GTK_TREE_MODEL(this), &iter);

	gtk_tree_model_row_has_child_toggled(
		GTK_TREE_MODEL(this),
		path,
		&iter);

	gtk_tree_path_free(path);
}


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::iter_get_uid:
	*
	*   @iter : the iter to get the uid from
	*
	**/

//=============================================================================
guint32
GnomeCmdFoldviewTreestore::iter_get_uid(
	GtkTreeIter *_in_iter)
{
	GnomeCmdFoldviewTreestore::Node				*node	= NULL;
	//.........................................................................
	g_return_val_if_fail( iter_is_valid(_in_iter), 0 );

	NODE_FROM_ITER(node, _in_iter);

	return node->uid();
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_dump
	*
	*   Dump the treestore, for debugging purpose
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::ext_dump_tree(
    GtkTreeIter *   _iter)
{
    GnomeCmdFoldviewTreestore::Node *   node    = NULL;
    //.........................................................................
    if ( ! _iter )
    {
        node = node_root();
    }
    else
    {
        NODE_FROM_ITER(node, _iter);
    }
    NODE_TKI("  +-------------------------------------------+");
    //                                   [12345678]
    NODE_TKI("  | Dumping Tree from node [%08x]         |", node);
    NODE_TKI("  +-------------------------------------------+");

    node->dump_tree();

    NODE_TKI("   ");
    //NODE_INF("  +-------------------------------------------+");
    NODE_TKI("  Dumped !");
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_add_child:
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
GnomeCmdFoldviewTreestore::ext_add_child(
	GtkTreeIter									*   _in_parent,
	GtkTreeIter									*   _out_child,
	GnomeCmdFoldviewTreestore::DataInterface	*   _data_child)
{
	GnomeCmdFoldviewTreestore::Node				*n_parent	= NULL;
	GnomeCmdFoldviewTreestore::Node				*n_child	= NULL;
	//.........................................................................
	g_return_if_fail( iter_is_valid_but_may_be_null( _in_parent ) );

	// try to set node_root
	if ( ! _in_parent )
	{
		g_return_if_fail( node_root() );

		n_parent = node_root();
	}
	else
	{
		NODE_FROM_ITER(n_parent, _in_parent );
		g_return_if_fail( n_parent );
	}

	n_child = n_parent->children()->node_add(uid_new(), a_sort_type, a_sort_collate_key_to_use, _data_child);

	ITER_FROM_NODE(this, _out_child, n_child);

	_data_child->set_path_from_treestore(ext_path_from_iter(_out_child));

	emit_row_inserted( n_child );
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_set_data:
	*
	*   @iter : set Data for an iter.
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::ext_set_data(
	GtkTreeIter		*   _iter_in,
	DataInterface	*   _data)
{
	Node				*   node		= NULL;
	DataInterface		*	data_old	= NULL;
	DataInterface		*   data_new	= NULL;
	//.........................................................................
	g_return_if_fail ( iter_is_valid(_iter_in) );

	NODE_FROM_ITER(node, _iter_in);
	g_assert(node);

	data_old = node->data();
	g_return_if_fail( data_old );

	data_new = _data;
	g_return_if_fail( data_new );

	// We set the Path for the new Data with a copy of the old Data's Path
	data_new->set_path_from_treestore( data_old->path()->dup() );

	// Then we delete the old Data, that will delete old Path too
	delete data_old;

	// Then we set the new Data as iter Data
	node->data() = data_new;

	emit_row_changed( node );
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_data_changed:
	*
	*   @iter : set Data for an iter.
	*
	*   Tell the treestore that an iter's data has changed. So emit the
	*   row_changed signal.
	*
	**/

//=============================================================================
void
GnomeCmdFoldviewTreestore::ext_data_changed(
	GtkTreeIter		*   _iter_in)
{
	Node				*   node		= NULL;
	//.........................................................................
	g_return_if_fail ( iter_is_valid(_iter_in) );

	NODE_FROM_ITER(node, _iter_in);
	g_assert(node);

	emit_row_changed( node );
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_get_data:
	*
	*   @iter : get Data for an iter.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::ext_get_data(
	GtkTreeIter		*		_iter_in,
	DataInterface	**		_data_out)
{
	Node				*   node		= NULL;
	DataInterface		*	data_old	= NULL;
	//.........................................................................
	g_return_val_if_fail ( _data_out != NULL, FALSE );
	*_data_out = NULL;

	g_return_val_if_fail ( iter_is_valid(_iter_in), FALSE );

	NODE_FROM_ITER(node, _iter_in);
	g_assert(node);

	data_old = node->data();
	g_return_val_if_fail( data_old, FALSE );

	*_data_out = data_old;

	return ( data_old != NULL );
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_clear:
	*
	*   Clear * everything *
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::ext_clear()
{
	gint count = 0;
	//.........................................................................
	g_return_val_if_fail( d_node_root, 0 );

	// We should code a quicker mehod                                           // _GWR_TODO_
	count = d_node_root->remove_children();

	// delete node_root
	delete d_node_root->children();
	delete d_node_root;

	// Set pointer to NULL
	d_node_root = NULL;

	LEAKS_INF("GnomeCmdFoldviewTreestore::ext_clear()");
	LEAKS_INF("  remaining nodes  : %04i %s", Node::Remaining(),		Node::Remaining() ?         "- not empty -" : "EMPTY !");
	LEAKS_INF("  remaining blocks : %04i %s", NodeBlock::Remaining(),   NodeBlock::Remaining() ?    "- not empty -" : "EMPTY !");

	return count;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_get_root:
	*
	*   Retrives the root node of a given node
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::ext_get_root(
	GtkTreeIter *in,
	GtkTreeIter *out_root)
{
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid(in), FALSE );
	g_return_val_if_fail ( out_root, FALSE );

	NODE_FROM_ITER(node, in);
	g_return_val_if_fail( node, FALSE );

	// All this code for this simple loop :)
	while ( node->parent() != node_root() )
		node = node->parent();

	ITER_FROM_NODE(this, out_root, node);
	return TRUE;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_is_root:
	*
	*   Tell wether the iter is a root iter
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::ext_is_root(
	GtkTreeIter *in)
{
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid(in), FALSE );

	NODE_FROM_ITER(node, in);
	g_return_val_if_fail( node, FALSE );

	return ( node->parent() == node_root() ? TRUE : FALSE );
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_iter_depth:
	*
	*   Return the depth of an iter
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::ext_iter_depth(
	GtkTreeIter *in)
{
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid(in), FALSE );

	NODE_FROM_ITER(node, in);
	g_return_val_if_fail( node, FALSE );

	return node->depth();
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_get_gtk_path_str_new:
	*
	*   @in : a GtkTreeIter
	*
	*   Return the string path for an iter ( GtkTreePath style )
	*
	**/

//=============================================================================
gchar*
GnomeCmdFoldviewTreestore::ext_get_gtk_path_str_new(
	GtkTreeIter *in)
{
	gint										i			= 0;
	GArray										*a			= NULL;
	gint										pos			= 0;
	gchar										 ascii_tmp1 [8];
	gchar										 ascii_tmp2 [1024];
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid(in), NULL );

	NODE_FROM_ITER(node, in);
	g_return_val_if_fail( node, NULL );

	a = g_array_sized_new(FALSE, TRUE, sizeof(gint), 10);

	do
	{
		pos = node->pos();
		g_array_append_vals(a, &pos, 1);

		node = node->parent();
		i++;
	}
	while ( node != node_root() );

	ascii_tmp2[0] = 0;

	STORE_INF("get_path_str_new:depth %02i", i);

	if ( i > 1 )
		g_sprintf( ascii_tmp2, "%i:", g_array_index(a, gint, --i) );

	while ( i > 1 )
	{
		g_sprintf( ascii_tmp1, "%i:", g_array_index(a, gint, --i) );
		g_strlcat(ascii_tmp2, ascii_tmp1, 1024);
	}

	g_sprintf( ascii_tmp1, "%i", g_array_index(a, gint, 0) );
	g_strlcat(ascii_tmp2, ascii_tmp1, 1024);

	g_array_free(a, TRUE);

	//STORE_INF("get_path_str_new:%s", ascii_tmp2);

	return g_strdup(ascii_tmp2);
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_iter_get_row:
	*
	*   @in : a GtkTreeIter
	*
	*   Get the row of an iter
	*
	**/

//=============================================================================
gint GnomeCmdFoldviewTreestore::ext_iter_get_row(
	GtkTreeIter *in)
{
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid(in), 0 );

	NODE_FROM_ITER(node, in);
	g_return_val_if_fail( node, 0 );

	return node->row();
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_iter_remove_children_no_signal_row_deleted:
	*
	*   @in : a GtkTreeIter
	*
	*   Remove all children of iter, but dont send any row_deleted signal.
	*   Only send row_toggled if parent
	*   Call only this method if an iter is collapsed !
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::ext_iter_remove_children_no_signal_row_deleted(
	GtkTreeIter *in)
{
    gint            count       = 0;
	Node		*   node		= NULL;
	//.........................................................................
    g_return_val_if_fail ( iter_is_valid(in), 0 );

    NODE_FROM_ITER(node, in);
    g_return_val_if_fail( node, 0 );

    // Create branch from the node
    Branch branch(node);
    g_return_val_if_fail( branch.is_valid(), -1 );

    count = branch.cut_but_keep_root();

	// if a_root is now sterile, send child_toggled signal ( if parent != root )
	if ( branch.root()->is_sterile() )
		if ( branch.root() != node_root() )
			emit_row_has_child_toggled(branch.root());

    return count;

    //   Previous method, without the Branch concept
    //
    //	g_return_val_if_fail ( iter_is_valid(in), 0 );
    //
    //	NODE_FROM_ITER(node, in);
    //	g_return_val_if_fail( node, 0 );
    //
    //	gint count = node->remove_children();
    //
    //	// if parent is now sterile, send child_toggled signal ( if parent != root )
    //	if ( ! node->children()->card() )
    //		if ( node->parent() != node_root() )
    //			emit_row_has_child_toggled(node);
    //
    //	return count;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_iter_sterile_remove:
	*
	*   @in : a GtkTreeIter with no children
	*
	*   Remove the iter & send a row_deleted signal
	*
	**/

//=============================================================================
gint
GnomeCmdFoldviewTreestore::ext_iter_sterile_remove(
	GtkTreeIter *in)
{
    gint            count       = 0;
    Node		*   node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid(in), FALSE );

	NODE_FROM_ITER(node, in);
	g_return_val_if_fail( node, FALSE );
	g_return_val_if_fail( node->is_sterile(), FALSE );

    // Create branch from the node
    Branch branch(node);
    g_return_val_if_fail( branch.is_valid(), FALSE );

    // *IMPORTANT* send signal before removing, according to GtkTreeModel doc
	emit_row_deleted(node);

    // cut the branch
    count = branch.cut();

	// if parent is now sterile, send child_toggled signal ( if parent != root )
	if ( branch.parent()->is_sterile() )
		if ( branch.parent() != node_root() )
			emit_row_has_child_toggled(branch.parent());

    return count;

    //   Previous method, without the Branch concept
    //
    //	// children block where node resides
    //	block = node->parent()->children();
    //
    //	// *IMPORTANT* send signal before removing, according to GtkTreeModel doc
    //	emit_row_deleted(node);
    //
    //	// remove the node
    //	block->remove_node( node->pos() );
    //
    //	// if parent is now sterile, send child_toggled signal ( if parent != root )
    //	if ( ! block->card() )
    //		if ( block->parent() != node_root() )
    //			emit_row_has_child_toggled(block->parent());
    //
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_iter_from_path
	*
	*   @_path		: a GnomeCmdFoldviewTreestore::Path
	*   @_iter_out  : a GtkTreeIter
	*
	*   Retrieves an iter from a Path.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::ext_iter_from_path(
	const   Path			*   _path,
			GtkTreeIter		*   _iter_out)
{
	return ext_iter_from_path((Path*)_path, _iter_out);
}
gboolean
GnomeCmdFoldviewTreestore::ext_iter_from_path(
	Path			*   _path,
	GtkTreeIter		*   _iter_out)
{
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	gint										depth		= 0;
	//.........................................................................
	g_return_val_if_fail( _path, FALSE );

	depth	= 1;
	node	= node_root();

loop:
	node = node->children()->node_get(0);

	while ( node )
	{
		if ( node->uid() == _path->uid_get(depth-1) )
		{
			if ( ++depth > _path->card() )
				goto found;
			else
				goto loop;
		}
		node = node->next();
	}

	return FALSE;

found:
	//REFRESH_INF("Model::iter_find_from_base_path:Retrieved iter %s", path->dump());
	ITER_FROM_NODE(this, _iter_out, node);
	return TRUE;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_path_from_iter
	*
	*   @_path		: a GnomeCmdFoldviewTreestore::Path
	*   @_iter_out  : a GtkTreeIter
	*
	*   Return a mallocated Path from an iter
	*
	**/

//=============================================================================
GnomeCmdFoldviewTreestore::Path*
GnomeCmdFoldviewTreestore::ext_path_from_iter(
	GtkTreeIter		*   _iter_in)
{
	GnomeCmdFoldviewTreestore::Node			*   node		= NULL;
	gint										depth		= 0;
	gint										i			= 1;
	Path									*   path		= NULL;
	//.........................................................................
	g_return_val_if_fail( _iter_in,					FALSE );
	g_return_val_if_fail( iter_is_valid(_iter_in),  FALSE );

	NODE_FROM_ITER(node, _iter_in);
	g_return_val_if_fail( node,	FALSE );

	// build & fill a reverse path
	depth		= node->depth();
	path		= new Path(depth);

	do
	{
		if ( node == node_root() )
		{
			NODE_ERR("GnomeCmdFoldviewTreestore::ext_path_from_iter():node root reached !");
			delete path;
			return NULL;
		}

		path->uid_set( depth - i, node->uid() );

		node = node->parent();
	}
	while ( (i++) != depth );

	return path;
}


//*****************************************************************************
//*																			  *
//*							Match functions									  *
//*																			  *
//*****************************************************************************


//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_match_child_gint:
	*
	*   in_parent								: a GtkTreeIter
	*   out_child								: a GtkTreeIter
	*   gboolean(*match_function)(Data*, gint)  : matching function
	*   the_gint								: a gint
	*
	*   For all childs of in_parent, call match_function(child->data, the_gint).
	*
	*   If parent is NULL, test root nodes
	*
	*   If it returns TRUE for a child, the function fills in out_child with
	*   this child and return TRUE.
	*
	*   If matching_function return FALSE for all childs, return FALSE.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::ext_match_child_gint(
	GtkTreeIter								*   in_parent   ,
	GtkTreeIter								*   out_child   ,
	gboolean(*match_function)(DataInterface*, gint)					,
	gint										the_gint)
{
	gint				i			= 0;
	Node			*   node		= NULL;
	NodeBlock		*   block		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid_but_may_be_null(in_parent), FALSE );

	if ( !in_parent )
		node = node_root();
	else
		NODE_FROM_ITER(node, in_parent);

	g_return_val_if_fail( node, FALSE );

	block   = node->children();
	g_return_val_if_fail( block->card() != 0, FALSE );

	while ( i < block->card() )
	{
		node = block->node_get(i++);
		if ( match_function(node->data(), the_gint ) )
		{
			ITER_FROM_NODE(this, out_child, node);
			return TRUE;
		}
	}

	return FALSE;
}

//=============================================================================

  /**
	*   GnomeCmdFoldviewTreestore::ext_match_child_collate_key:
	*
	*   in_parent								: a GtkTreeIter
	*   out_child								: a GtkTreeIter
	*   collate_key_id							: a gint identifying
	*											  the collate key to use
	*											  for the childs
	*   utf8_collate_key						: a collate key
	*
	*   For all childs of in_parent, compare child->collatekey &
	*   collate_key_to_use.
	*
	*   If parent is NULL, test root nodes
	*
	*   If it collate for a child, the function fills in out_child with
	*   this child and return TRUE.
	*
	*   If no collate for all childs, return FALSE.
	*
	**/

//=============================================================================
gboolean
GnomeCmdFoldviewTreestore::ext_match_child_str(
	GtkTreeIter								*   in_parent   ,
	GtkTreeIter								*   out_child   ,
	const gchar								*   _str)
{
	gint										i			= 0;
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	GnomeCmdFoldviewTreestore::NodeBlock		*block		= NULL;
	//.........................................................................
	g_return_val_if_fail ( iter_is_valid_but_may_be_null(in_parent), FALSE );

	if ( !in_parent )
		node = node_root();
	else
		NODE_FROM_ITER(node, in_parent);

	g_return_val_if_fail( node, FALSE );

	block   = node->children();
	g_return_val_if_fail( block->card() != 0, FALSE );

	while ( i < block->card() )
	{
		node = block->node_get(i++);
        //  Previous method, with direct access to collate keys
        //  from DataInterface
        //
		//if (	strcmp
        //        (
		//			node->data()->utf8_collate_key(collate_key_id),
		//			utf8_collate_key
        //       ) == 0 )
        if ( node->data()->compare_str(_str) == 0 )
		{
			ITER_FROM_NODE(this, out_child, node);
			return TRUE;
		}
	}

	return FALSE;
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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	GnomeCmdFoldviewTreestore::NodeBlock		*block		= NULL;
	gint          *indices = NULL, pos = 0, depth =0;
	gint		i = 0;
	//.........................................................................
	g_assert( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel) );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_assert( path );

	indices = gtk_tree_path_get_indices(path);
	depth   = gtk_tree_path_get_depth(path);

	// get the position
	pos		= indices[i++];

	// ok, get first root node ( first child of the ~root~ node )
	node	=   treestore->node_root()->children()->node_get(pos);

	while ( node )
	{
		// path is done ?
		if ( i == depth )
		{
			ITER_FROM_NODE(treestore, iter, node);
			return TRUE;
		}

		// node = node->children()->get_pos( indices[i++] );
		block   = node->children();
		pos		= indices[i++];
		node	= block->node_get(pos);
	}

	if ( treestore->node_root()->children()->card() != 0 )
		GCMD_ERR("GnomeCmdFoldviewTreestore::get_iter::failed to get iter from path");

	return FALSE;
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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), NULL );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid(iter), NULL);

	path = gtk_tree_path_new();

	NODE_FROM_ITER(node, iter);
	g_return_val_if_fail( node, NULL );

	do
	{
		gtk_tree_path_prepend_index(path, node->pos());
		node = node->parent();
	}
	while ( node != treestore->node_root() );

	//STORE_INF("get_path:%s", gtk_tree_path_to_string(path));

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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	if ( ! IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel) )		goto fail;
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	if ( ! treestore->iter_is_valid(iter) )					goto fail;
	//if ( column != 0 )									goto fail;
	//if ( !value )											goto fail;

	NODE_FROM_ITER(node, iter);
	if ( ! node )											goto fail;

	g_value_init(value, G_TYPE_POINTER);
	g_value_set_pointer(value, node->data());
	return;

fail:
	g_value_init(value, G_TYPE_POINTER);
	g_value_set_pointer(value, NULL);
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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), FALSE );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);

	g_return_val_if_fail ( treestore->iter_is_valid_but_may_be_null(iter_parent), FALSE );

	if ( ! iter_parent )
	{
		node = treestore->node_root();
			g_return_val_if_fail( node, FALSE );

        if ( node->is_sterile() )
            return FALSE;

		ITER_FROM_NODE(treestore, iter_child, node->children()->node_get(0));
		return TRUE;
	}

	NODE_FROM_ITER(node, iter_parent);
	g_assert(node);

	if ( ! node->is_sterile() )
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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), 0 );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid_but_may_be_null(iter), 0 );

	if ( ! iter )
		node = treestore->node_root();
	else
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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	GnomeCmdFoldviewTreestore::NodeBlock		*block		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), FALSE );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid_but_may_be_null(iter_parent), FALSE );

	if ( ! iter_parent )
		node = treestore->node_root();
	else
	{
		NODE_FROM_ITER(node, iter_parent);
		g_assert( node );
	}

	block = node->children();

	g_assert( n < block->card() );
	//if ( n >= block->card() )
	//return FALSE;

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
	GnomeCmdFoldviewTreestore::Node				*node		= NULL;
	//.........................................................................
	g_return_val_if_fail ( IS_GNOME_CMD_FOLDVIEW_TREESTORE(treemodel), FALSE );
	treestore = GNOME_CMD_FOLDVIEW_TREESTORE(treemodel);
	g_return_val_if_fail ( treestore->iter_is_valid(iter_child), FALSE );

	NODE_FROM_ITER(node, iter_child);
	g_assert( node );
	g_assert( node->parent() );

	node = node->parent();
	g_assert( node );
	//g_assert( node != treestore->node_root() );
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
	// default sort settings
	treestore->set_sort_type(GnomeCmdFoldviewTreestore::eSortAscending);
	treestore->set_sort_collate_key_to_use(0);

    //treestore->d_node_root          = NULL;
    //treestore->a_node_root_created  = FALSE;

	// others
	treestore->stamp_init();
	treestore->node_root_init();
	treestore->uid_init();
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

	GCMD_INF("GnomeCmdFoldviewTreestore::finalize()");

	// free all records and free all memory used by the list
	treestore->ext_clear();

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





