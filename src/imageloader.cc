/**
 * @file imageloader.cc
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

#include "gnome-cmd-includes.h"
#include "imageloader.h"
#include "utils.h"
#include "gnome-cmd-data.h"

using namespace std;


struct CacheEntry
{
    gboolean dead_end;
    GIcon *icon;
};


static const gchar *file_type_pixmap_files[] = {
    "file-type-icons/file_type_regular.xpm",
    "file-type-icons/file_type_regular.xpm",
    "file-type-icons/file_type_dir.xpm",
    "file-type-icons/file_type_fifo.xpm",
    "file-type-icons/file_type_socket.xpm",
    "file-type-icons/file_type_char_device.xpm",
    "file-type-icons/file_type_block_device.xpm",
    "file-type-icons/file_type_symlink.xpm"
};


#define NUM_FILE_TYPE_PIXMAPS G_N_ELEMENTS(file_type_pixmap_files)


static const gchar *categories[][2] = {{"text", "gnome-text-plain.png"},
                                       {"video", "gnome-video-plain.png"},
                                       {"image", "gnome-image-plain.png"},
                                       {"audio", "gnome-audio-plain.png"},
                                       {"pack", "gnome-pack-plain.png"},
                                       {"font", "gnome-font-plain.png"}};

static CacheEntry file_type_pixmaps[NUM_FILE_TYPE_PIXMAPS];

static GHashTable *mime_cache = nullptr;


static GIcon *load_icon (const gchar *path)
{
    if (path == nullptr)
        return nullptr;

    GFile *file = g_file_new_for_path (path);
    if (!file)
        return nullptr;
    if (!g_file_query_exists (file, nullptr))
    {
        g_object_unref (file);
        return nullptr;
    }

    GIcon *icon = g_file_icon_new (file);
    g_object_unref (file);
    return icon;
}


void IMAGE_init ()
{
    mime_cache = g_hash_table_new (g_str_hash, g_str_equal);

     // Load file type icons

    for (size_t i = 0; i<NUM_FILE_TYPE_PIXMAPS; i++)
    {
        CacheEntry *e = &file_type_pixmaps[i];
        gchar *path = g_build_filename (PIXMAPS_DIR, file_type_pixmap_files[i], nullptr);

        DEBUG ('i', "imageloader: loading pixmap: %s\n", path);

        e->icon = load_icon (path);
        if (!e->icon)
        {
            gchar *path2 = g_build_filename ("../pixmaps", file_type_pixmap_files[i], nullptr);

            g_warning (_("Couldn’t load installed pixmap, trying to load %s instead"), path2);

            e->icon = load_icon (path2);
            if (!e->icon)
                g_warning (_("Can’t find the pixmap anywhere. Make sure you have installed the program or is executing gnome-commander from the gnome-commander-%s/src directory"), PACKAGE_VERSION);
            g_free (path2);
        }

        g_free (path);
    }
}


/**
 * Takes a mime-type as argument and returns the filename
 * of the image representing it.
 */
inline char *get_mime_icon_name (const gchar *mime_type)
{
    gint i;
    gchar *icon_name;
    gchar *tmp = g_strdup (mime_type);
    gint l = strlen(tmp);

    // replace '/' with '-'
    for (i=0; i<l; i++)
        if (tmp[i] == '/')
            tmp[i] = '-';

    // add 'gnome-'
    icon_name = g_strdup_printf ("gnome-%s.png", tmp);
    g_free (tmp);

    return icon_name;
}


/**
 * Returns the filename that an image representing the given filetype
 * should have.
 */
static const gchar *get_type_icon_name (guint32 type)
{
    static const gchar *names[] = {
        "i-directory.png",
        "i-regular.png",
        "i-symlink.png"
    };

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (type)
    {
        case G_FILE_TYPE_DIRECTORY:
            return names[0];
        case G_FILE_TYPE_REGULAR:
            return names[1];
        case G_FILE_TYPE_SYMBOLIC_LINK:
            return names[2];
        //TODO: Add filetype names for G_FILE_TYPE_SHORTCUT and G_FILE_TYPE_MOUNTABLE

        default:
            return names[1];
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    return nullptr;
}


/**
 * Returns the full path to an image for the given filetype in the given directory.
 */
inline gchar *get_mime_file_type_icon_path (guint32 type, const gchar *icon_dir)
{
    return g_build_filename (icon_dir, get_type_icon_name (type), nullptr);
}


/**
 * Returns the full path the image representing the given mime-type in
 * the given directory. There is no guarantee that the image exists this
 * function just returns the name that the icon should have if it exists.
 */
inline gchar *get_mime_document_type_icon_path (const gchar *mime_type, const gchar *icon_dir)
{
    gchar *icon_name = get_mime_icon_name (mime_type);
    gchar *icon_path = g_build_filename (icon_dir, icon_name, nullptr);
    g_free (icon_name);

    return icon_path;
}


/**
 * Returns the full path to the image representing the category of the given
 * mime-type in the given directory. This is a hack to avoid having 20 equal
 * icons for 20 different video formats etc. in an icon-theme directory.
 */
inline gchar *get_category_icon_path (const gchar *mime_type, const gchar *icon_dir)
{
    for (size_t i=0; i<G_N_ELEMENTS(categories); i++)
        if (g_str_has_prefix (mime_type, categories[i][0]))
            return g_build_filename (icon_dir, categories[i][1], nullptr);

    return nullptr;
}


/**
 * Tries to load an image for the specified mime-type in the specified directory.
 * If symlink is true a smaller symlink image is painted over the image to indicate this.
 */
static GIcon *get_mime_icon_in_dir (const gchar *icon_dir,
                                    guint32 type,
                                    const gchar *mime_type)
{
    if (!mime_type)
        return nullptr;

    if (type == G_FILE_TYPE_SYMBOLIC_LINK)
        return nullptr;

    auto entry = static_cast<CacheEntry*> (g_hash_table_lookup (mime_cache, mime_type));
    if (!entry)
    {
        // We're looking up this mime-type for the first time

        gchar *icon_path = nullptr;
        gchar *icon_path2 = nullptr;
        gchar *icon_path3 = nullptr;
        GIcon *icon = nullptr;

        DEBUG ('y', "Looking up pixmap for: %s\n", mime_type);

        icon_path = get_mime_document_type_icon_path (mime_type, icon_dir);
        DEBUG('z', "\nSearching for icon for %s\n", mime_type);
        DEBUG('z', "Trying %s\n", icon_path);
        icon = load_icon (icon_path);

        if (!icon)
        {
            icon_path2 = get_category_icon_path (mime_type, icon_dir);
            DEBUG('z', "Trying %s\n", icon_path2);
            icon = load_icon (icon_path2);
        }

        if (!icon)
        {
            icon_path3 = get_mime_file_type_icon_path (type, icon_dir);
            DEBUG('z', "Trying %s\n", icon_path3);
            icon = load_icon (icon_path3);
        }

        g_free (icon_path);
        g_free (icon_path2);
        g_free (icon_path3);

        entry = g_new0 (CacheEntry, 1);
        entry->dead_end = (icon == nullptr);
        entry->icon = icon;

        DEBUG('z', "Icon found?: %s\n", entry->dead_end ? "No" : "Yes");

        g_hash_table_insert (mime_cache, g_strdup (mime_type), entry);
    }

    if (entry->dead_end)
        return nullptr;

    return entry->icon;
}


inline GIcon *get_type_icon (guint32 type)
{
    if (type >= NUM_FILE_TYPE_PIXMAPS) return nullptr;
    return file_type_pixmaps[type].icon;
}


GIcon *IMAGE_get_file_icon (guint32 type, const gchar *mime_type, gboolean symlink)
{
    GIcon *icon = nullptr;

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (gnome_cmd_data.options.layout)
    {
        case GNOME_CMD_LAYOUT_TYPE_ICONS:
            icon = get_type_icon (type);
            break;

        case GNOME_CMD_LAYOUT_MIME_ICONS:
            {
                icon = get_mime_icon_in_dir (gnome_cmd_data.options.theme_icon_dir, type, mime_type);
                if (!icon)
                    icon = get_type_icon (type);
            }
            break;

        default:
            return nullptr;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    if (icon == nullptr)
        return nullptr;

    if (symlink)
    {
        GIcon *unmount = g_themed_icon_new (OVERLAY_SYMLINK_ICON);
        GEmblem *emblem = g_emblem_new (unmount);

        return g_emblemed_icon_new (icon, emblem);
    }
    return icon;
}


static gboolean remove_entry (const gchar *key, CacheEntry *entry, gpointer user_data)
{
    g_return_val_if_fail (entry, TRUE);
    g_clear_object (&entry->icon);
    g_free (entry);

    return TRUE;
}


void IMAGE_clear_mime_cache ()
{
    g_return_if_fail (mime_cache != nullptr);

    g_hash_table_foreach_remove (mime_cache, (GHRFunc) remove_entry, nullptr);
}


void IMAGE_free ()
{
}
