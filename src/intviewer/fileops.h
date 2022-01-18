/**
 * @file fileops.h
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2022 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <sys/stat.h>

#pragma once

/*
    File Handling functions (based on Midnight Commander's view.c)

    'open' & 'close' : just open and close the file handle
    'load' & 'free' : allocate & free buffer memory, call mmap/munmap

    calling order should be: open->load->[use file with "get_byte"]->free (which calls close)
*/

struct ViewerFileOps
{
    // File handling (based on 'Midnight Commander'-'s view.c)
    char *filename;        // Name of the file
    unsigned char *data;    // Memory area for the file to be viewed
    int file;        // File descriptor (for mmap and munmap)
    int mmapping;        // Did we use mmap on the file?

    // Growing buffers information
    int growing_buffer;    // Use the growing buffers?
    char **block_ptr;    // Pointer to the block pointers
    int   blocks;        // The number of blocks in *block_ptr
    struct stat s;        // stat for file

    offset_type last;           // Last byte shown
    offset_type last_byte;      // Last byte of file
    offset_type first;        // First byte in file
    offset_type bottom_first;    // First byte shown when very last page is displayed
                    // For the case of WINCH we should reset it to -1
    offset_type bytes_read;     // How much of file is read
};

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
