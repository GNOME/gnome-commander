/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyright (C) 2010-2010 Guillaume Wardavoir

	---------------------------------------------------------------------------

	C++

	Contain variadic macros

	---------------------------------------------------------------------------

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

#include	<config.h>

#include	<stdlib.h>

#include	<gtk/gtksignal.h>
#include	<gtk/gtkvbox.h>

#include	<libgnomevfs/gnome-vfs.h>
#include	<libgnomevfs/gnome-vfs-utils.h>

#include	"gnome-cmd-includes.h"
#include	"gnome-cmd-combo.h"
#include	"gnome-cmd-main-win.h"
#include	"gnome-cmd-style.h"
#include	"gnome-cmd-data.h"
#include	"gnome-cmd-con-home.h"
#include	"gnome-cmd-con-list.h"

#include	"gnome-cmd-foldview-private.h"
#include	"gnome-cmd-foldview-gvfs.h"

//  ***************************************************************************
//  *																		  *
//  *								Defines								      *
//  *																		  *
//  ***************************************************************************
GtkTreeIter GcmdGtkFoldview::Model::Iter_zero = { 0,0,0,0 };

//  ###########################################################################
//  ###																		###
//  ##																		 ##
//  #					struct GcmdGtkFoldview::Rowlike						  #
//  ##																		 ##
//  ###																		###
//  ###########################################################################

GcmdGtkFoldview::Model::Rowlike::Rowlike(gchar *_utf8_name, gint _icon)
{
	d_utf8_name		= g_strdup( _utf8_name );
	a_icon			= _icon;
	
	// collate key
	glong l = g_utf8_strlen(d_utf8_name, -1);
	d_utf8_collate_key = g_utf8_collate_key_for_filename(d_utf8_name, l);
}
GcmdGtkFoldview::Model::Rowlike::~Rowlike()
{
	g_free(d_utf8_name);
}

const   gchar*  
GcmdGtkFoldview::Model::Rowlike::utf8_collate_key()
{ 
	return d_utf8_collate_key;	
}
const   gchar*	
GcmdGtkFoldview::Model::Rowlike::utf8_name()
{ 
	return d_utf8_name;			
}
gint	
GcmdGtkFoldview::Model::Rowlike::icon()
{ 
	return a_icon;				
}



//  ***************************************************************************
//  *																		  *
//  *					Model ( gtkTreeStore )							      *
//  *																		  *
//  ***************************************************************************

//=============================================================================
//	Create, destroy,...
//=============================================================================
void GcmdGtkFoldview::Model::raz_pointers()
{
	m_treestore				= NULL;

	root.m_iter.stamp		= 0;
	root.m_iter.user_data   = NULL;
	root.m_iter.user_data2  = NULL;
	root.m_iter.user_data3  = NULL;
	root.m_uri				= NULL;
	root.m_info				= NULL;

	m_con			= NULL;
}
void GcmdGtkFoldview::Model::init_instance()
{
	raz_pointers();
	create();
}

gboolean GcmdGtkFoldview::Model::create()
{
#ifdef __GTS__

	m_treestore = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_sortable_set_sort_func( treesortable(), 0, GcmdGtkFoldview::Model::Compare, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id( treesortable(), 0, GTK_SORT_ASCENDING);

#else

	m_treestore = gnome_cmd_foldview_treestore_new();

#endif

	connection(get_home_con());

	return TRUE;
}
void GcmdGtkFoldview::Model::destroy()
{
}

//=============================================================================
//  alloc / free a GtkTreeIter
//=============================================================================
GtkTreeIter* GcmdGtkFoldview::Model::Iter_new(GtkTreeIter* iter)
{
	GtkTreeIter *new_iter = g_new0(GtkTreeIter,1);

	if ( iter != NULL )	
		*new_iter = *iter;

	return new_iter;
}
void GcmdGtkFoldview::Model::Iter_del(GtkTreeIter* iter)
{
	g_free(iter);
}




//=============================================================================
//  model for root element
//=============================================================================
gboolean GcmdGtkFoldview::Model::Root::set(GnomeVFSURI *uri)
{
	if ( ( m_uri != NULL) || ( m_info != NULL ) )
	{
		ERR_RET(FALSE, "model_root::set:root not empty - please first call unset");
	}

	m_info = gnome_vfs_file_info_new();
	m_uri  = GVFS_uri_new(uri->text);

	if ( ( !m_uri ) || ( !m_info ) )
	{
		gwr_err_vfs("model_root::set:uri <%s>:failed", m_uri->text);
		unset();
		return FALSE;
	}

	if ( !GVFS_info_from_uri(m_uri,m_info) )
	{
		gwr_err("model_root::set:uri <%s>:could not get info", m_uri->text);
		unset();
		return FALSE;
	}

	if ( !GVFS_vfsinfo_has_type_directory(m_info) )
	{
		gwr_err("model_root::set:uri <%s>:is not a folder", m_uri->text);
		unset();
		return FALSE;
	}

	return TRUE;
}

void	GcmdGtkFoldview::Model::Root::unset()
{
	if ( m_uri )
	{
		gwr_inf("model_root::unset:uri not null, destroying...");
		if ( m_uri->ref_count != 1 )
			ERR_FAIL("model_root::unset:uri refcount is not 1:%i", m_uri->ref_count);
		gnome_vfs_uri_unref(m_uri);
	}
	else
	{
		gwr_inf("model_root::unset:uri is null, destroying nothing");
	}

	if ( m_info )
	{
		gwr_inf("model_root::unset:info not null, destroying...");
		if ( m_info->refcount != 1 )
			ERR_FAIL("model_root::unset:infp refcount is not 1:%i", m_info->refcount);
		gnome_vfs_file_info_unref(m_info);
	}
	else
	{
		gwr_inf("model_root::unset:info is null, destroying nothing");
	}

	m_uri		= NULL; 
	m_info		= NULL;
}

//=============================================================================
//	GtkTreeStore comparefunc
//=============================================================================
gint GcmdGtkFoldview::Model::Compare(
	GtkTreeModel	*model,
	GtkTreeIter		*iter_a,
	GtkTreeIter		*iter_b,
	gpointer		user_data)
{
	gchar		*str_a, *str_b;
	gboolean	a_dotted		= FALSE;
	gboolean	b_dotted		= FALSE;
	gint		res				= 0;

	gtk_tree_model_get(model, iter_a, 0, &str_a, -1);
	gtk_tree_model_get(model, iter_b, 0, &str_b, -1);

	if ( str_a[0] == '.' )  a_dotted = TRUE;
	if ( str_b[0] == '.' )  b_dotted = TRUE;

	if ( a_dotted && !b_dotted ) 
	{
		g_free(str_a);
		g_free(str_b);
		return 1;
	}

	if ( b_dotted && !a_dotted )
	{
		g_free(str_a);
		g_free(str_b);
		return -1;
	}

	res = g_utf8_collate(str_a, str_b);
	g_free(str_a);
	g_free(str_b);
	return res;
}

//=============================================================================
//  given an iter, return a mallocated 
//  * GnomeVFSURI   representing the full path
//  * gchar*		representing the iter string
//  ( is GtkTreeModel sucking ??? )
//=============================================================================
GnomeVFSURI* GcmdGtkFoldview::Model::iter_get_uri_new(GtkTreeIter *final)
{
	GtkTreePath *path   = NULL;
	GnomeVFSURI *uri	= NULL;
	GtkTreeIter iter;
	gchar		strUri  [1024];
	gchar*		strTemp;

	strcpy(strUri,"");

#ifdef __GTS__

	// get the path from iter
	path	=   gtk_tree_model_get_path( treemodel(), final );

	while ( gtk_tree_path_get_depth(path) >= 1 )
	{
		if ( gtk_tree_model_get_iter( treemodel(), &iter, path) )
		{
			gtk_tree_model_get(treemodel(), &iter, 0, &strTemp, -1);
			//gwr_inf("Model::iter_get_uri_new:temp:%s", strTemp);
			g_strreverse(strTemp);
			g_strlcat(strUri, strTemp, 1024);
			g_strlcat(strUri, "/", 1024);
			g_free(strTemp);
		}
		gtk_tree_path_up(path);
	}


	// free the path
	gtk_tree_path_free(path);

	// reverse
	g_strreverse(strUri);
	//gwr_inf("uri_from_iter:%s", strUri);

	// new uri
	uri = GVFS_uri_new(strUri);
	return uri;

#else

	// get the path from iter
	path	=   GnomeCmdFoldviewTreestore::get_path( treemodel(), final );

	while ( gtk_tree_path_get_depth(path) >= 1 )
	{
		if ( GnomeCmdFoldviewTreestore::get_iter( treemodel(), &iter, path) )
		{
				GValue v = {0};
				GnomeCmdFoldviewTreestore::get_value(treemodel(), &iter, 0, &v);
				Rowlike *r = (Rowlike*)g_value_get_pointer(&v);
				strTemp = g_strdup(r->utf8_name());

			//gwr_inf("Model::iter_get_uri_new:temp:%s", strTemp);
			g_strreverse(strTemp);
			g_strlcat(strUri, strTemp, 1024);
			g_strlcat(strUri, "/", 1024);
			g_free(strTemp);
		}
		gtk_tree_path_up(path);
	}


	// free the path
	gtk_tree_path_free(path);

	// reverse
	g_strreverse(strUri);
	//gwr_inf("Model::uri_from_iter:%s", strUri);

	// new uri
	uri = GVFS_uri_new(strUri);
	return uri;
	
#endif
}

gchar* GcmdGtkFoldview::Model::iter_get_string_new(GtkTreeIter *iter)
{
	gchar*		str			= NULL;

	GValue v = {0};
	gtk_tree_model_get_value(treemodel(), iter, 0, &v);

#ifdef __GTS__

	const gchar* str_const = g_value_get_string(&v);
	str = (char*)str_const;

#else

	Rowlike *r = (Rowlike*)g_value_get_pointer(&v);
	str = g_strdup( r->utf8_name() );

#endif
	
	return str;
}

/*
#ifdef __GTS__

	gtk_tree_model_get(treemodel(), iter, 0, &str_const, -1);

#else

	GValue v = {0};
	gtk_tree_model_get_value(treemodel(), iter, 0, &v);
	str_const = g_value_get_string(&v);
	
#endif

	str = (char*)str_const;
	return str;
}
*/
//=============================================================================
//  get_iter
//=============================================================================
gboolean GcmdGtkFoldview::Model::get_iter(
	GtkTreePath *path,
	GtkTreeIter *iter)
{
#ifdef __GTS__

	return gtk_tree_model_get_iter(
		treemodel(),
		iter,
		path);

#else

	return GnomeCmdFoldviewTreestore::get_iter(
		treemodel(),
		iter,
		path);

#endif
}

//=============================================================================
//  subfolders of an item
//=============================================================================
gboolean GcmdGtkFoldview::Model::iter_add_child(
	GtkTreeIter						*parent,
	GtkTreeIter						*child,
	const gchar						*name,
	gint							icon)
{
#ifdef __GTS__

	gtk_tree_store_append( treestore(), child, parent );
	gtk_tree_store_set( treestore(), child, 0, name, 1, icon, -1 );

#else

	Rowlike* r = new Rowlike((gchar*)name, icon);
	treestore()->add_child(parent, child, r);

#endif
	return TRUE;
}

//=============================================================================
//  set value for iter
//=============================================================================
void GcmdGtkFoldview::Model::set_value(GtkTreeIter *iter, gchar* text, gint icon)
{
#ifdef __GTS__

	gtk_tree_store_set( treestore(), iter, 0, text, 1, icon, -1);

#else

	
	Rowlike* r = new Rowlike(text, icon);

	GValue v = {0};
	g_value_init(&v, G_TYPE_POINTER);
	g_value_set_pointer(&v, r);

	treestore()->set_value(iter, 0, &v);

#endif
}

//=============================================================================
//  set value for first child -> control
//=============================================================================
void GcmdGtkFoldview::Model::iter_replace_first_child(
	GtkTreeIter				*parent, 
	GtkTreeIter				*child, 
	gchar*					text, 
	gint					icon)
{
#ifdef __GTS__

	if ( !gtk_tree_model_iter_children( treemodel(), child, parent) )
	{
		gwr_err("Model:iter_replace_first_child:could not get first child");
		return;
	}

#else

	if ( !GnomeCmdFoldviewTreestore::iter_children(treemodel(), child, parent) )
	{
		gwr_err("Model:iter_replace_first_child:could not get first child");
		return;
	}

#endif

	// set the value
	set_value( child, text, icon);
}

//=============================================================================
//  set value for [DUMMY] child -> control
//  It seems that we can append rows during text_expand_row signal, but not		// _GWR_GTK_01_
//  remove rows. So instead of removing [DUMMY], we replace its value...and
//  we create the model_iter_set_first_child above just for fun.
//=============================================================================
void GcmdGtkFoldview::Model::iter_replace_dummy_child(
	GtkTreeIter				*parent, 
	GtkTreeIter				*child,
	gchar*					text, 
	gint					icon)
{
	gint	n = 0;

	n = iter_n_children(parent);

	if ( n !=1 )
	{
		gwr_err("iter_replace_dummy_child:parent has %03i children", n);
		return;
	}

	iter_replace_first_child(parent, child, text, icon);
}

//=============================================================================
//  Count childs of an iter
//=============================================================================
gint GcmdGtkFoldview::Model::iter_n_children(GtkTreeIter *parent)
{
#ifdef __GTS__

	return gtk_tree_model_iter_n_children( treemodel(), parent );

#else

	return GnomeCmdFoldviewTreestore::iter_n_children(treemodel(), parent);

#endif
}


//=============================================================================
//  Remove one iter
//  Removes iter from tree_store. After being removed, iter is set to the next
//  valid row at that level, or invalidated if it previously pointed
//  to the last one.
//=============================================================================
/*
gboolean GcmdGtkFoldview::Model::iter_remove(GtkTreeIter *iter)
{
	gwr_inf("iter_remove");

#ifdef __GTS__
	return gtk_tree_store_remove( treestore(), iter);
#else
	gwr_inf("Model:iter_remove %04i", treestore()->remove(iter,TRUE));
	return TRUE;
#endif
}
*/
//=============================================================================
//  Remove all children of an iter
//=============================================================================
gint GcmdGtkFoldview::Model::iter_remove_children(GtkTreeIter *parent)
{
#ifdef __GTS__ 

	gint				count   = 0;
	//gchar				*str	= NULL;
	GtkTreeIter			child;

	//  Code with iter_remove, that is not recursive, but does not
	//  return the number of nodes removed
	//	
	while ( gtk_tree_model_iter_children( treemodel(), &child, parent) )
	{
		gtk_tree_store_remove(treestore(), &child);
	}

	//  Code with iter_remove, that returns the number of nodes removed
	//  but is unuselessly recursive
	//	
	//	while ( gtk_tree_model_iter_children( treemodel(), &child, parent) )
	//	{
	//		//str = iter_get_string_new(&child);
	//		//gwr_inf("Model::iter_remove_children:child %s", str);
	//		//g_free(str);
	//
	//		count += iter_remove_children(&child);
	//
	//		iter_remove(&child);
	//		count ++;
	//	}
	//

	return count;

#else

	return treestore()->remove_children(parent);

#endif

}

//=============================================================================
//  Remove all
//=============================================================================
void GcmdGtkFoldview::Model::iter_remove_all()
{
#ifdef __GTS__

	gtk_tree_store_clear(treestore());

#else

	treestore()->clear();

#endif
}
