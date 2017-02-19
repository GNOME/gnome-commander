/** 
 * @file gnome-cmd-tags-file.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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

#include <config.h>
#include <time.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags-file.h"
#include "utils.h"

using namespace std;


void gcmd_tags_file_load_metadata(GnomeCmdFile *f)
{
    g_return_if_fail (f != NULL);
    g_return_if_fail (f->info != NULL);

    if (f->metadata && f->metadata->is_accessed(TAG_FILE))  return;

    if (!f->metadata)
        f->metadata = new GnomeCmdFileMetadata;

    if (!f->metadata)  return;

    f->metadata->mark_as_accessed(TAG_FILE);

    // if (!f->is_local())  return;

    gchar *dpath = f->get_dirname();

    static char buff[32];

    f->metadata->add(TAG_FILE_NAME, f->info->name);
    f->metadata->add(TAG_FILE_PATH, dpath);

    g_free (dpath);

    gchar *uri_str = f->get_uri_str(GNOME_VFS_URI_HIDE_PASSWORD);
    f->metadata->add(TAG_FILE_LINK, uri_str);
    g_free (uri_str);

    f->metadata->add(TAG_FILE_SIZE, f->info->size);

    strftime(buff,sizeof(buff),"%Y-%m-%d %T",localtime(&f->info->atime));
    f->metadata->add(TAG_FILE_ACCESSED, buff);
    strftime(buff,sizeof(buff),"%Y-%m-%d %T",localtime(&f->info->mtime));
    f->metadata->add(TAG_FILE_MODIFIED, buff);

    f->metadata->add(TAG_FILE_PERMISSIONS, perm2textstring(f->info->permissions,buff,sizeof(buff)));

    f->metadata->add(TAG_FILE_FORMAT, f->info->type==GNOME_VFS_FILE_TYPE_DIRECTORY ? "Folder" : f->info->mime_type);
}
