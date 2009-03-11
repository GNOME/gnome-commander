/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
        GNOME Commander - A GNOME based file manager
        Copyright (C) 2001-2006 Marcus Bjurman
        Copyright (C) 2007-2009 Piotr Eljasiak

    Partly based on "view.c" module from "Midnight Commander" Project.
    Copyright (C) 1994, 1995, 1996 The Free Software Foundation
    Written by: 1994, 1995, 1998 Miguel de Icaza
        1994, 1995 Janne Kukonlehto
        1995 Jakub Jelinek
        1996 Joseph M. Hinkle
        1997 Norbert Warmuth
        1998 Pavel Machek

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

#include <config.h>
#include <glib.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#ifdef HAVE_MMAP
#include <sys/mman.h>

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#endif

#include "gvtypes.h"

#include "fileops.h"

#define BUF_MEDIUM 256
#define BUF_LARGE 1024

#define VIEW_PAGE_SIZE 8192

using namespace std;


struct _ViewerFileOps
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


ViewerFileOps *gv_fileops_new()
{
    ViewerFileOps *fops = g_new0(ViewerFileOps, 1);
    g_return_val_if_fail (fops!=NULL, NULL);

    fops->file = -1;
    return fops;
}


/*
  TODO: Make this thread-safe!
*/
inline const char *unix_error_string (int error_num)
{
    static char buffer [BUF_LARGE];
    gchar *strerror_currentlocale;

    strerror_currentlocale = g_locale_from_utf8(g_strerror (error_num), -1, NULL, NULL, NULL);
    g_snprintf (buffer, sizeof (buffer), "%s (%d)", strerror_currentlocale, error_num);
    g_free (strerror_currentlocale);

    return buffer;
}


static int gv_file_internal_open(ViewerFileOps *ops, int fd)
{
    g_return_val_if_fail (ops!=NULL, -1);
    g_return_val_if_fail (fd>2, -1);

    const gchar *error;
    int cntlflags;

    // Make sure we are working with a regular file
    if (fstat (fd, &ops->s) == -1)
    {
        close (fd);
        g_warning("Cannot stat fileno(%d): %s ",
            fd, unix_error_string (errno));
        return -1;
    }

    if (!S_ISREG (ops->s.st_mode))
    {
        close (fd);
        g_warning("Cannot view: not a regular file ");
        return -1;
    }

    // We don't need O_NONBLOCK after opening the file, unset it
    cntlflags = fcntl (fd, F_GETFL, 0);
    if (cntlflags != -1)
    {
        cntlflags &= ~O_NONBLOCK;
        fcntl (fd, F_SETFL, cntlflags);
    }

    error = gv_file_load(ops, fd);
    if (error)
    {
        close(fd);
        g_warning("Failed to open file: %s", error);
        return -1;
    }

    ops->last_byte = ops->first + ops->s.st_size;

    return 0;
}


int gv_file_open_fd(ViewerFileOps *ops, int filedesc)
{
    g_free (ops->filename);

    g_return_val_if_fail (filedesc>2, -1);

    int fd = dup(filedesc);

    if (fd==-1)
    {
        g_warning("file_open_fd failed, 'dup' returned -1");
        return -1;
    }

    return gv_file_internal_open(ops, fd);
}


int gv_file_open(ViewerFileOps *ops, const gchar* _file)
{
    g_free (ops->filename);

    g_return_val_if_fail (_file!=NULL, -1);
    g_return_val_if_fail (_file[0]!=0, -1);

    ops->filename = g_strdup (_file);

    int fd;

    // Open the file
    if ((fd = open (_file, O_RDONLY | O_NONBLOCK)) == -1)
    {
        g_warning("Cannot open \"%s\"\n %s ", _file, unix_error_string (errno));
        return -1;
    }

    return gv_file_internal_open(ops, fd);
}


offset_type gv_file_get_max_offset(ViewerFileOps *ops)
{
    g_return_val_if_fail (ops!=NULL, 0);

    return ops->s.st_size;
}


void gv_file_close (ViewerFileOps *ops)
{
    g_return_if_fail (ops!=NULL);

    if (ops->file != -1)
    {
        close (ops->file);
        ops->file = -1;
    }
    memset(&ops->s, 0, sizeof(ops->s));
}


// return values: NULL for success, else points to error message
const char *gv_file_init_growing_view (ViewerFileOps *ops, const char *filename)
{
    ops->growing_buffer = 1;

    if ((ops->file = open (filename, O_RDONLY)) == -1)
        return "init_growing_view: cannot open file";

    return NULL;
}


/*
    returns  NULL on success
*/
const char *gv_file_load(ViewerFileOps *ops, int fd)
{
    g_return_val_if_fail (ops!=NULL, "invalid ops paramter");

    ops->file = fd;

    if (ops->s.st_size == 0)
    {
        // Must be one of those nice files that grow (/proc)
        gv_file_close (ops);
        return gv_file_init_growing_view (ops, ops->filename);
    }
#ifdef HAVE_MMAP
    if ((size_t) ops->s.st_size == ops->s.st_size)
        ops->data = (unsigned char *) mmap (0, ops->s.st_size, PROT_READ, MAP_FILE | MAP_SHARED, ops->file, 0);
    else
        ops->data = (unsigned char *) MAP_FAILED;

    if (ops->data != MAP_FAILED)
    {
        // mmap worked
        ops->first = 0;
        ops->bytes_read = ops->s.st_size;
        ops->mmapping = 1;
        return NULL;
    }
#endif                // HAVE_MMAP

    /* For the OSes that don't provide mmap call, try to load all the
    * file into memory (alex@bcs.zaporizhzhe.ua).  Also, mmap can fail
    * for any reason, so we use this as fallback (pavel@ucw.cz) */

    // Make sure view->s.st_size is not truncated when passed to g_malloc
    if ((gulong) ops->s.st_size == ops->s.st_size)
        ops->data = (unsigned char *) g_try_malloc ((gulong) ops->s.st_size);
    else
        ops->data = NULL;

    if (ops->data == NULL ||
        lseek (ops->file, 0, SEEK_SET) != 0 ||
        read (ops->file, (char *) ops->data, ops->s.st_size) != ops->s.st_size)
    {
        g_free (ops->data);
        gv_file_close (ops);
        return gv_file_init_growing_view (ops, ops->filename);
    }

    ops->first = 0;
    ops->bytes_read = ops->s.st_size;
    return NULL;
}


/*
    returns: -1 on failure
        0->255 value on success
*/
int gv_file_get_byte (ViewerFileOps *ops, offset_type byte_index)
{
    g_return_val_if_fail (ops!=NULL, -1);

    int page = byte_index / VIEW_PAGE_SIZE + 1;
    int offset = byte_index % VIEW_PAGE_SIZE;

    if (ops->growing_buffer)
    {
        if (page > ops->blocks)
        {
            ops->block_ptr = (char **) g_realloc (ops->block_ptr, page*sizeof (char *));
            for (int i = ops->blocks; i < page; i++)
            {
                char *p = (char *) g_try_malloc (VIEW_PAGE_SIZE);
                ops->block_ptr[i] = p;
                if (!p)
                    return '\n';
                int n = read (ops->file, p, VIEW_PAGE_SIZE);
/*
         * FIXME: Errors are ignored at this point
         * Also should report preliminary EOF
         */
                if (n != -1)
                    ops->bytes_read += n;
                if (ops->s.st_size < ops->bytes_read)
                {
                    ops->bottom_first = INVALID_OFFSET; // Invalidate cache
                    ops->s.st_size = ops->bytes_read;
                    ops->last_byte = ops->bytes_read;
                }
            }
            ops->blocks = page;
        }

        return byte_index >= ops->bytes_read ? -1 : ops->block_ptr[page - 1][offset];
    }
    else
        return byte_index >= ops->last_byte ? -1 : ops->data[byte_index];
}


// based on MC's view.c "free_file"
void gv_file_free(ViewerFileOps *ops)
{
    g_return_if_fail (ops!=NULL);

#ifdef HAVE_MMAP
    if (ops->mmapping)
        munmap ((char *) ops->data, ops->s.st_size);
#endif                // HAVE_MMAP
    gv_file_close (ops);

    // Block_ptr may be zero if the file was a file with 0 bytes
    if (ops->growing_buffer && ops->block_ptr)
    {
        for (int i = 0; i < ops->blocks; i++)
            g_free (ops->block_ptr[i]);
        g_free (ops->block_ptr);
    }
}
