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

#ifndef __GCMDGTKFOLDVIEW_H__
#define __GCMDGTKFOLDVIEW_H__

//=============================================================================
//
//						public header file
//
//=============================================================================

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>

struct  IGnomeCmdFoldview
{
	
};

// This function because of the creation process of GtkWidgets. Annoying.
GtkWidget*			gnome_cmd_foldview_new();
void				gnome_cmd_foldview_update_style(GtkWidget *widget);



#endif //__GCMDGTKFOLDVIEW_H__
