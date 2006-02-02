/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2006 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 

#ifndef __HANDLE_H__
#define __HANDLE_H__

typedef struct
{
	gint ref_count;
	gpointer ref;
} Handle;


Handle  *handle_new (gpointer ref);
void     handle_free (Handle *h);
void     handle_ref (Handle *h);
void     handle_unref (Handle *h);
gpointer handle_get_ref (Handle *h);


#endif //__HANDLE_H__
