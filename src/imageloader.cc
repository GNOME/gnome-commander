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
    GdkPixbuf *pixbuf;
    GdkPixbuf *lnk_pixbuf;
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


static const gchar *pixmap_files[NUM_PIXBUFS] = {"",
                                                 "gnome_cmd_arrow_up.xpm",
                                                 "gnome_cmd_arrow_down.xpm",
                                                 "overlay_symlink.xpm",
                                                 "overlay_umount.xpm"};


static const gchar *categories[][2] = {{"text", "gnome-text-plain.png"},
                                       {"video", "gnome-video-plain.png"},
                                       {"image", "gnome-image-plain.png"},
                                       {"audio", "gnome-audio-plain.png"},
                                       {"pack", "gnome-pack-plain.png"},
                                       {"font", "gnome-font-plain.png"}};

static GdkPixbuf *pixbufs[NUM_PIXBUFS];
static CacheEntry file_type_pixmaps[NUM_FILE_TYPE_PIXMAPS];

static GHashTable *mime_cache = nullptr;
static GdkPixbuf *symlink_pixbuf = nullptr;

static const gint ICON_SIZE = 16;

static gboolean load_icon (const gchar *icon_path, GdkPixbuf **pb, GdkPixbuf **lpb);


/*
 * Load application pixmaps
 */
void IMAGE_init ()
{
    mime_cache = g_hash_table_new (g_str_hash, g_str_equal);

     // Load misc icons

    for (gint i=1; i<NUM_PIXBUFS; i++)
    {
        gchar *path = g_build_filename (PIXMAPS_DIR, pixmap_files[i], nullptr);

        DEBUG ('i', "imageloader: loading pixmap: %s\n", path);

        pixbufs[i] = pixbuf_from_file (path);
        if (!pixbufs[i])
        {
            gchar *path2 = g_build_filename ("../pixmaps", pixmap_files[i], nullptr);

            g_warning (_("Couldn’t load installed file type pixmap, trying to load %s instead"), path2);

            pixbufs[i] = pixbuf_from_file (path2);
            if (!pixbufs[i])
                g_warning (_("Can’t find the pixmap anywhere. Make sure you have installed the program or is executing gnome-commander from the gnome-commander-%s/src directory"), PACKAGE_VERSION);

            g_free (path2);
        }
        g_free (path);
    }


     // Load file type icons

     for (size_t i=0; i<NUM_FILE_TYPE_PIXMAPS; i++)
    {
        CacheEntry *e = &file_type_pixmaps[i];
        gchar *path = g_build_filename (PIXMAPS_DIR, file_type_pixmap_files[i], nullptr);

        DEBUG ('i', "imageloader: loading pixmap: %s\n", path);

        if (!load_icon (path, &e->pixbuf, &e->lnk_pixbuf))
        {
            gchar *path2 = g_build_filename ("../pixmaps", pixmap_files[i], nullptr);

            g_warning (_("Couldn’t load installed pixmap, trying to load %s instead"), path2);

            if (!load_icon (path2, &e->pixbuf, &e->lnk_pixbuf))
                g_warning (_("Can’t find the pixmap anywhere. Make sure you have installed the program or is executing gnome-commander from the gnome-commander-%s/src directory"), PACKAGE_VERSION);
            g_free (path2);
        }

        g_free (path);
    }

    register_gnome_commander_stock_icons ();
}


GdkPixbuf *IMAGE_get_pixbuf (Pixmap pixmap_id)
{
    return pixmap_id > 0 && pixmap_id < NUM_PIXBUFS ? pixbufs[pixmap_id] : nullptr;
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
 * Tries to load an image from the specified path
 */
static gboolean load_icon (const gchar *icon_path, GdkPixbuf **pb, GdkPixbuf **lpb)
{
    GdkPixbuf *pixbuf;
    GdkPixbuf *lnk_pixbuf;
    gint x, y, w, h, sym_w, sym_h;
    gfloat scale;

    DEBUG ('i', "Trying to load \"%s\"\n\n", icon_path);

    pixbuf = gdk_pixbuf_new_from_file (icon_path, nullptr);
    if (!pixbuf) return FALSE;


    // Load the symlink overlay pixmap
    if (!symlink_pixbuf)
    {
        symlink_pixbuf = pixbufs[PIXMAP_OVERLAY_SYMLINK];
    }
    sym_w = gdk_pixbuf_get_width (symlink_pixbuf);
    sym_h = gdk_pixbuf_get_height (symlink_pixbuf);


    // Scale the pixmap if needed
    h = gnome_cmd_data.options.icon_size;
    if (h != gdk_pixbuf_get_height (pixbuf))
    {
        scale = (gfloat)h/(gfloat)gdk_pixbuf_get_height (pixbuf);
        w = (gint)(scale*(gfloat)gdk_pixbuf_get_width (pixbuf));

        GdkPixbuf *tmp = gdk_pixbuf_scale_simple (pixbuf, w, h, gnome_cmd_data.options.icon_scale_quality);
        g_object_unref (pixbuf);
        pixbuf = tmp;
    }


    // Create a copy with a symlink overlay
    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);
    x = w - sym_w;
    y = h - sym_h;
    if (x < 0) { sym_w -= x; x = 0; }
    if (y < 0) { sym_h -= y; y = 0; }
    lnk_pixbuf = gdk_pixbuf_copy (pixbuf);

    gdk_pixbuf_copy_area (symlink_pixbuf, 0, 0, sym_w, sym_h, lnk_pixbuf, x, y);

    *pb = pixbuf;
    *lpb = lnk_pixbuf;

    return TRUE;
}


/**
 * Tries to load an image for the specified mime-type in the specified directory.
 * If symlink is true a smaller symlink image is painted over the image to indicate this.
 */
static GdkPixbuf *get_mime_icon_in_dir (const gchar *icon_dir,
                                        guint32 type,
                                        const gchar *mime_type,
                                        gboolean symlink)
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
        GdkPixbuf *pb = nullptr;
        GdkPixbuf *lpb = nullptr;

        DEBUG ('y', "Looking up pixmap for: %s\n", mime_type);

        icon_path = get_mime_document_type_icon_path (mime_type, icon_dir);
        DEBUG('z', "\nSearching for icon for %s\n", mime_type);
        DEBUG('z', "Trying %s\n", icon_path);
        if (icon_path)
            load_icon (icon_path, &pb, &lpb);

        if (!pb)
        {
            icon_path2 = get_category_icon_path (mime_type, icon_dir);
            DEBUG('z', "Trying %s\n", icon_path2);
            if (icon_path2)
                load_icon (icon_path2, &pb, &lpb);
        }

        if (!pb)
        {
            icon_path3 = get_mime_file_type_icon_path (type, icon_dir);
            DEBUG('z', "Trying %s\n", icon_path3);
            if (icon_path3)
                load_icon (icon_path3, &pb, &lpb);
        }

        g_free (icon_path);
        g_free (icon_path2);
        g_free (icon_path3);

        entry = g_new0 (CacheEntry, 1);
        entry->dead_end = (pb == nullptr);
        entry->pixbuf = pb;
        entry->lnk_pixbuf = lpb;

        DEBUG('z', "Icon found?: %s\n", entry->dead_end ? "No" : "Yes");

        g_hash_table_insert (mime_cache, g_strdup (mime_type), entry);
    }

    if (entry->dead_end)
        return nullptr;

    return symlink ? entry->lnk_pixbuf : entry->pixbuf;
}


static GdkPixbuf *get_mime_icon (guint32 type,
                                 const gchar *mime_type,
                                 gboolean symlink)
{
    return get_mime_icon_in_dir (gnome_cmd_data.options.theme_icon_dir, type, mime_type, symlink);
}


inline GdkPixbuf *get_type_icon (guint32 type, gboolean symlink)
{
    if (type >= NUM_FILE_TYPE_PIXMAPS) return nullptr;

    return symlink ? file_type_pixmaps[type].lnk_pixbuf : file_type_pixmaps[type].pixbuf;
}


GdkPixbuf *IMAGE_get_pixmap_and_mask (guint32 type,
                                      const gchar *mime_type,
                                      gboolean symlink)
{
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (gnome_cmd_data.options.layout)
    {
        case GNOME_CMD_LAYOUT_TYPE_ICONS:
            return get_type_icon (type, symlink);

        case GNOME_CMD_LAYOUT_MIME_ICONS:
            {
                auto pixbuf = get_mime_icon (type, mime_type, symlink);
                if (pixbuf)
                    return pixbuf;
                return get_type_icon (type, symlink);
            }

        default:
            return nullptr;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    return nullptr;
}


static gboolean remove_entry (const gchar *key, CacheEntry *entry, gpointer user_data)
{
    g_return_val_if_fail (entry, TRUE);

    if (!entry->dead_end)
    {
        g_object_unref (entry->pixbuf);
    }

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
    for (int i=0; i<NUM_PIXBUFS; i++)
    {
        g_clear_object (&pixbufs[i]);
    }
}

static struct
{
    const gchar *filename;
    const gchar *stock_id;
} stock_icons[] =
{
    { PIXMAPS_DIR G_DIR_SEPARATOR_S "copy_file_names.xpm",    COPYFILENAMES_STOCKID },
    { PIXMAPS_DIR G_DIR_SEPARATOR_S "rotate-90-16.xpm",       ROTATE_90_STOCKID },
    { PIXMAPS_DIR G_DIR_SEPARATOR_S "rotate-270-16.xpm",      ROTATE_270_STOCKID },
    { PIXMAPS_DIR G_DIR_SEPARATOR_S "rotate-180-16.xpm",      ROTATE_180_STOCKID },
    { PIXMAPS_DIR G_DIR_SEPARATOR_S "flip-vertical-16.xpm",   FLIP_VERTICAL_STOCKID },
    { PIXMAPS_DIR G_DIR_SEPARATOR_S "flip-horizontal-16.xpm", FLIP_HORIZONTAL_STOCKID },
    { PIXMAPS_DIR G_DIR_SEPARATOR_S FILETYPEICONS_FOLDER G_DIR_SEPARATOR_S "file_type_dir.xpm",     FILETYPEDIR_STOCKID},
    { PIXMAPS_DIR G_DIR_SEPARATOR_S FILETYPEICONS_FOLDER G_DIR_SEPARATOR_S "file_type_regular.xpm", FILETYPEREGULARFILE_STOCKID},
};

static gint n_stock_icons = G_N_ELEMENTS (stock_icons);

void register_gnome_commander_stock_icons (void)
{
    GtkIconFactory *icon_factory;
    GtkIconSet *icon_set;
    GtkIconSource *icon_source;
    gint i;

    icon_factory = gtk_icon_factory_new ();

    for (i = 0; i < n_stock_icons; i++)
       {
           icon_set = gtk_icon_set_new ();
           icon_source = gtk_icon_source_new ();
           gtk_icon_source_set_filename (icon_source, stock_icons[i].filename);
           gtk_icon_set_add_source (icon_set, icon_source);
           gtk_icon_source_free (icon_source);
           gtk_icon_factory_add (icon_factory, stock_icons[i].stock_id, icon_set);
           gtk_icon_set_unref (icon_set);
       }

    gtk_icon_factory_add_default (icon_factory);

    g_object_unref (icon_factory);
}

/**
 * This method returns the path of the default application icon for a given GAppInfo object.
 * The returned char must not be free'ed.
 */
const gchar* get_default_application_icon_path(GAppInfo* appInfo)
{
    g_return_val_if_fail (appInfo, nullptr);

    auto appIconName = g_icon_to_string (g_app_info_get_icon(appInfo));
    auto gtkIconInfo = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default(), appIconName, ICON_SIZE, GTK_ICON_LOOKUP_FORCE_SIZE);

    g_free(appIconName);

    if (gtkIconInfo == nullptr)
    {
        return nullptr;
    }

    return gtk_icon_info_get_filename(gtkIconInfo);
}
