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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include <time.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags-file.h"
#include "utils.h"

using namespace std;


void gcmd_tags_file_init()
{
}


void gcmd_tags_file_shutdown()
{
}


void gcmd_tags_file_load_metadata(GnomeCmdFile *finfo)
{
    g_return_if_fail (finfo != NULL);
    g_return_if_fail (finfo->info != NULL);

    if (finfo->metadata && finfo->metadata->is_accessed(TAG_FILE))  return;

    if (!finfo->metadata)
        finfo->metadata = new GnomeCmdFileMetadata;

    if (!finfo->metadata)  return;

    finfo->metadata->mark_as_accessed(TAG_FILE);

    // if (!gnome_cmd_file_is_local(finfo))  return;

    gchar *dpath = gnome_cmd_file_get_dirname(finfo);

    static char buff[32];

    finfo->metadata->add(TAG_FILE_NAME, finfo->info->name);
    finfo->metadata->add(TAG_FILE_PATH, dpath);

    g_free (dpath);

    gchar *uri_str = gnome_cmd_file_get_uri_str (finfo, GNOME_VFS_URI_HIDE_PASSWORD);
    finfo->metadata->add(TAG_FILE_LINK, uri_str);
    g_free (uri_str);

    finfo->metadata->add(TAG_FILE_SIZE, finfo->info->size);

    strftime(buff,sizeof(buff),"%Y-%m-%d %T",localtime(&finfo->info->atime));
    finfo->metadata->add(TAG_FILE_ACCESSED, buff);
    strftime(buff,sizeof(buff),"%Y-%m-%d %T",localtime(&finfo->info->mtime));
    finfo->metadata->add(TAG_FILE_MODIFIED, buff);

    finfo->metadata->add(TAG_FILE_PERMISSIONS, perm2textstring(finfo->info->permissions,buff,sizeof(buff)));

    finfo->metadata->add(TAG_FILE_FORMAT, finfo->info->type==GNOME_VFS_FILE_TYPE_DIRECTORY ? "Folder" : finfo->info->mime_type);

}
