/**
 * @file gnome-cmd-gkeyfile-utils.cc
 * @brief Utility functions for working with GKeyFile data structures from GLib
 * @note Originally taken from the GnuCash project
 * (gnc-gkeyfile-utils.h, David Hampton <hampton@employees.org>)
 * @copyright (C) 2013-2015 Uwe Scholz\n
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
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "gnome-cmd-gkeyfile-utils.h"


/** Open and read a key/value file from disk into memory.
 *
 *  @param file The name of the file to load. This should be a fully
 *  qualified path.
 *
 *  @param ignore_error If true this function will ignore any problems
 *  reading the an existing file from disk and will return a GKeyFile
 *  structure. Set to TRUE if performing a read/modify/write on a file
 *  that may or may not already exist.
 *
 *  @returns A pointer to a GKeyFile data structure, or NULL if a
 *  (non-ignored) error occurred.
 */
GKeyFile *gcmd_key_file_load_from_file (const gchar *filename,
					gboolean ignore_error)
{
    GKeyFile *key_file;
    GKeyFileFlags flags;
    GError *error = NULL;

    flags = static_cast <GKeyFileFlags> (G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS);
    
    g_return_val_if_fail(filename != NULL, NULL);
    
    if (!g_file_test(filename, G_FILE_TEST_EXISTS))
	return NULL;
    
    key_file = g_key_file_new();
    if (!key_file)
	return NULL;
    
    if (g_key_file_load_from_file(key_file, filename, flags, &error))
	return key_file;
    
    /* An error occurred */
    if (!ignore_error)
	g_warning("Unable to read file %s: %s\n", filename, error->message);
    return key_file;
}


/** Write a key/value file from memory to disk. If there is no data to
 *  be written, this function will not create a file and will remove any
 *  exiting file.
 *
 *  @param file The name of the file to write. This should be a fully
 *  qualified path.
 *
 *  @param key_file The data to be written.
 *
 *  @returns A TRUE if the data was successfully written to disk.
 *  FALSE if there was an error.
 */
gboolean gcmd_key_file_save_to_file (const gchar *filename,
				     GKeyFile *key_file)
{
    gchar *contents;
    FILE *fd;
    extern int errno;
    gint length;
    ssize_t written;
    gboolean success = TRUE;
    
    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(key_file != NULL, FALSE);
    
    contents = g_key_file_to_data(key_file, NULL, NULL);
    length = strlen(contents);

    fd = fopen(filename, "w");
    if (fd == NULL)
    {
	g_critical("Cannot open file %s: %s\n", filename, strerror(errno));
	g_free(contents);
	return FALSE;
    }
    
    written = fprintf(fd, contents);
    if (written < 0)
    {
	success = FALSE;
	g_critical("Cannot write to file %s: %s\n", filename, strerror(errno));
	fclose(fd);
    }
    else if (written != length)
    {
	success = FALSE;
	g_critical("File %s truncated (provided %d, written %d)",
		   filename, length, (int)written);
	/* Ignore any error */
	fclose(fd);
    }
    else if (fclose(fd) == -1)
    {
	g_warning("Close failed for file %s: %s", filename, strerror(errno));
    }
    g_free(contents);
    return success;
}
