/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
        GNOME Commander - A GNOME based file manager
        Copyright (C) 2001-2006 Marcus Bjurman
        Copyright (C) 2007-2009 Piotr Eljasiak

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

#ifndef __LIBGVIEWER_FILEOPS_H__
#define __LIBGVIEWER_FILEOPS_H__

/*
    File Handling functions (based on Midnight Commander's view.c)

    'open' & 'close' : just open and close the file handle
    'load' & 'free' : allocate & free buffer memory, call mmap/munmap

    calling order should be: open->load->[use file with "get_byte"]->free (which calls close)
*/


typedef struct _ViewerFileOps ViewerFileOps;

ViewerFileOps *gv_fileops_new();

/*
    returns -1 on failure
*/
int gv_file_open(ViewerFileOps *ops, const gchar* _file);

int gv_file_open_fd(ViewerFileOps *ops, int filedesc);


/*
     returns: NULL on success
*/
const char *gv_file_load (ViewerFileOps *ops, int fd);

/*
    return values: NULL for success, else points to error message
*/
const char *gv_file_init_growing_view (ViewerFileOps *ops, const char *filename);

/*
    returns: -1 on failure
        0->255 value on success
*/
int gv_file_get_byte (ViewerFileOps *ops, offset_type byte_index);

offset_type gv_file_get_max_offset(ViewerFileOps *ops);

void gv_file_close (ViewerFileOps *ops);

void gv_file_free (ViewerFileOps *ops);

#endif // __LIBGVIEWER_FILEOPS_H__
