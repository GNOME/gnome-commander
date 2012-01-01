/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyleft      2010-2012 Guillaume Wardavoir

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

#ifndef __GNOME_CMD_FOLDVIEW_QUICKSTACK_H__
#define __GNOME_CMD_FOLDVIEW_QUICKSTACK_H__

#include <glib.h>

template <typename ElementType>
struct GnomeCmdQuickStack
{
	private:
	guint32					a_el_core_size;	//= sizeof(ElementType);
	guint32					a_ix_core_size;	//= sizeof(IndexType);

	guint32					a_step;// increment	= 10;
	guint32					a_size;//

	ElementType*			d_el;//		= NULL;
	guint32*				d_ix;//		= NULL;
	guint32					a_ff;//		= 0;

	private:
	gboolean				realloc();
	guint32					index_pop();
	void					index_push(guint32 index);

	public:
							GnomeCmdQuickStack<ElementType>(gint increment);
							~GnomeCmdQuickStack();

	static guint32			s_invalid_index;	// = 0xFFFFFFFF

	guint32					push(ElementType);
	void					pop (guint32);

	ElementType				get		(guint32 index);
	ElementType		*		get_ptr (guint32 index);

	guint32					stat_size()			{   return a_size;					}
	guint32					stat_first_free()	{   return a_ff;					}
	guint32					stat_used()			{   return a_size - ( a_ff + 1 );	}
};

//  ***************************************************************************
//
//  						Ctor, Dtor
//
//  ***************************************************************************
template <typename ElementType> guint32 GnomeCmdQuickStack<ElementType>::s_invalid_index = 0xFFFFFFFF;

template <typename ElementType>
GnomeCmdQuickStack<ElementType>::GnomeCmdQuickStack(gint increment)
{
	guint32 i = 0;
	//.........................................................................

	a_el_core_size  = sizeof(ElementType);
	a_ix_core_size  = sizeof(guint32);

	a_step  = increment;
	a_size  = 0;

	d_el	= NULL;
	d_ix	= NULL;
	a_ff	= 0;

	// alloc for pointers
	d_el = (ElementType*)(g_malloc0(a_el_core_size * a_step));
	if ( !d_el )
	{
		g_printerr("GVFS_qstack_init:g_malloc0 failed for d_el");
		return;
	}

	// alloc for indexes
	d_ix = (guint32*)(g_malloc0(a_ix_core_size * a_step));
	if ( !d_ix )
	{
		g_printerr("GVFS_qstack_init:g_malloc0 failed for d_ix");
		return;
	}

	// init indexes stack
	for ( i = 0 ; i != a_step ; i++ )
	{
		d_ix[i] = a_step - i - 1;
	}
	a_ff = a_step - 1;

	// size
	a_size = a_step;
}
template <typename ElementType>
GnomeCmdQuickStack<ElementType>::~GnomeCmdQuickStack()
{
	//GCMD_INF("gstack_destroy");
	g_free(d_el);
	g_free(d_ix);
}

//  ***************************************************************************
//
//  						private
//
//  ***************************************************************************
template <typename ElementType>
gboolean
GnomeCmdQuickStack<ElementType>::realloc()
{
	gpointer temp = NULL;
	guint32	 i	  = 0;

	//GCMD_WNG("GVFS_qstack_realloc at size:%04i", a_size);

	//.........................................................................
	// free indexes stack ; since there is no more index free,
	// we could just realloc - but there is no g_realloc0 in Glib
    //
    // We should use g_memdup for emulate g_realloc0                            // _GWR_TODO_

	// free the old indexes stack
	g_free(d_ix);

	// alloc a new indexes stack
	d_ix = (guint32*)(g_malloc0(a_ix_core_size * ( a_size + a_step ) ));
	if ( !d_ix )
	{
		g_printerr("GVFS_qstack_realloc:g_malloc0 failed for d_ix");
		return FALSE;
	}

	// store new free indexes
	for ( i = 0 ; i != a_step ; i++ )
	{
		d_ix[i] = a_size + a_step - i - 1;
	}

	//.........................................................................
	// realloc pointers array
	temp	= (gpointer)d_el;
	d_el	= (ElementType*)(g_malloc0(a_el_core_size * ( a_size + a_step ) ));
	if ( !d_el )
	{
		g_printerr("GVFS_qstack_realloc:g_malloc0 failed for d_el");
		return FALSE;
	}

	// copy used pointers
	memcpy( d_el, temp, a_el_core_size * a_size);

	// free temp
	g_free(temp);

	// update stack size
	a_size += a_step;

	// update first free index
	a_ff = a_step - 1;

	return TRUE;
}
template <typename ElementType>
guint32
GnomeCmdQuickStack<ElementType>::index_pop()
{
	if ( a_ff == 0 )
	{
		//guint32 temp = a_ff;
		// index on indexes is zero ; we are missing storage places
		// -> realloc
		guint32 index = d_ix[0];
		realloc();

		//GCMD_INF("GVFS_qstack_pop (realloc):index:%04i ff:%04i->%04i of %04i", index, temp, a_ff, a_size-1);
		return index;
	}

	//GCMD_INF("GVFS_qstack_pop:ff:%04i->%04i of %04i", a_ff, a_ff-1, a_size-1);
	return d_ix[a_ff--];
}
template <typename ElementType>
void
GnomeCmdQuickStack<ElementType>::index_push(guint32 index)
{
	if ( a_ff >= a_size )
	{
		g_printerr("GnomeCmdQuickStack::push:a_ff too high");
		return;
	}

	//GCMD_INF("GVFS_qstack_push:index:%04i ff:%04i->%04i of %04i", index, a_ff, a_ff+1, a_size-1);
	d_ix[++a_ff] = index;
}




//  ***************************************************************************
//
//  						public
//
//  ***************************************************************************
template <typename ElementType>
guint32
GnomeCmdQuickStack<ElementType>::push(ElementType element)
{
	guint32 index = index_pop();

	d_el[index] = element;

	return index;
}
template <typename ElementType>
void
GnomeCmdQuickStack<ElementType>::pop(guint32 index)
{
	index_push(index);
}

template <typename ElementType>
ElementType
GnomeCmdQuickStack<ElementType>::get(guint32 _index)
{
	return d_el[_index];
}







#endif //__GNOME_CMD_FOLDVIEW_QUICKSTACK_H__
