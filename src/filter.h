/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2003 Marcus Bjurman

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

#ifndef __FILTER_H__
#define __FILTER_H__

#include <regex.h>

typedef enum {
	FILTER_TYPE_REGEX,
	FILTER_TYPE_FNMATCH
} FilterType;


typedef struct
{
	/* common stuff */
	FilterType type;
	
	/* regex filtering stuff */
	regex_t *re_exp;
	
	/* fnmatch filtering stuff */
	char *fn_exp;
	int fn_flags;
	
} Filter;


Filter  *filter_new   (const gchar *exp,
					   gboolean case_sens);

void     filter_free  (Filter *filter);

gboolean filter_match (Filter *filter,
					   gchar *text);


#endif //__FILTER_H__
