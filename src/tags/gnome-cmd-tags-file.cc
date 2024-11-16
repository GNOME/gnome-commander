/** 
 * @file gnome-cmd-tags-file.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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
    g_return_if_fail (f->get_file_info() != NULL);

    if (f->metadata && f->metadata->is_accessed(TAG_FILE))  return;

    if (!f->metadata)
        f->metadata = new GnomeCmdFileMetadata;

    if (!f->metadata)  return;

    f->metadata->mark_as_accessed(TAG_FILE);

    // if (!f->is_local())  return;

    gchar *dpath = f->get_dirname();

    static char buff[32];

    auto fileName = f->GetGfileAttributeString(G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

    f->metadata->add(TAG_FILE_NAME, fileName);
    f->metadata->add(TAG_FILE_PATH, dpath);

    g_free (dpath);

    gchar *uri_str = f->get_uri_str();
    f->metadata->add(TAG_FILE_LINK, uri_str);
    g_free (uri_str);

    f->metadata->add(TAG_FILE_SIZE, f->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_STANDARD_SIZE));

    auto gFileInfo = g_file_query_info(f->get_file(), "time::*" "," , G_FILE_QUERY_INFO_NONE, nullptr, nullptr);

    auto accessTime = g_file_info_get_access_date_time (gFileInfo);
    auto accessTimeString = g_date_time_format (accessTime, "%Y-%m-%d %T");
    f->metadata->add(TAG_FILE_ACCESSED, accessTimeString);
    g_free(accessTimeString);

    auto modificationTime = g_file_info_get_modification_date_time (gFileInfo);
    auto modificationTimeString = g_date_time_format (modificationTime, "%Y-%m-%d %T");
    f->metadata->add(TAG_FILE_MODIFIED, modificationTimeString);

    f->metadata->add(TAG_FILE_PERMISSIONS, perm2textstring(f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_MODE),buff,sizeof(buff)));

    f->metadata->add(TAG_FILE_FORMAT, f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY
                                        ? "Folder"
                                        : f->GetGfileAttributeString(G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE));

    g_free(fileName);
    g_free(modificationTimeString);
    g_object_unref(gFileInfo);
}
