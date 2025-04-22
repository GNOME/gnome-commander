/*
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * For more details see the file COPYING.
 */

#include "gnome-cmd-file-metadata-extractor.h"

G_DEFINE_INTERFACE (GnomeCmdFileMetadataExtractor, gnome_cmd_file_metadata_extractor, G_TYPE_OBJECT);

static void gnome_cmd_file_metadata_extractor_default_init (GnomeCmdFileMetadataExtractorInterface *iface)
{
}

GStrv gnome_cmd_file_metadata_extractor_supported_tags (GnomeCmdFileMetadataExtractor *fme)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_METADATA_EXTRACTOR (fme), NULL);
    GnomeCmdFileMetadataExtractorInterface *iface = GNOME_CMD_FILE_METADATA_EXTRACTOR_GET_IFACE (fme);
    return iface->supported_tags ? (* iface->supported_tags) (fme) : NULL;
}

GStrv gnome_cmd_file_metadata_extractor_summary_tags (GnomeCmdFileMetadataExtractor *fme)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_METADATA_EXTRACTOR (fme), NULL);
    GnomeCmdFileMetadataExtractorInterface *iface = GNOME_CMD_FILE_METADATA_EXTRACTOR_GET_IFACE (fme);
    return iface->summary_tags ? (* iface->summary_tags) (fme) : NULL;
}

gchar *gnome_cmd_file_metadata_extractor_class_name (GnomeCmdFileMetadataExtractor *fme, const gchar *tag)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_METADATA_EXTRACTOR (fme), NULL);
    GnomeCmdFileMetadataExtractorInterface *iface = GNOME_CMD_FILE_METADATA_EXTRACTOR_GET_IFACE (fme);
    return iface->class_name ? (* iface->class_name) (fme, tag) : NULL;
}

gchar *gnome_cmd_file_metadata_extractor_tag_name (GnomeCmdFileMetadataExtractor *fme, const gchar *tag)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_METADATA_EXTRACTOR (fme), NULL);
    GnomeCmdFileMetadataExtractorInterface *iface = GNOME_CMD_FILE_METADATA_EXTRACTOR_GET_IFACE (fme);
    return iface->tag_name ? (* iface->tag_name) (fme, tag) : NULL;
}

gchar *gnome_cmd_file_metadata_extractor_tag_description (GnomeCmdFileMetadataExtractor *fme, const gchar *tag)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_METADATA_EXTRACTOR (fme), NULL);
    GnomeCmdFileMetadataExtractorInterface *iface = GNOME_CMD_FILE_METADATA_EXTRACTOR_GET_IFACE (fme);
    return iface->tag_description ? (* iface->tag_description) (fme, tag) : NULL;
}

void gnome_cmd_file_metadata_extractor_extract_metadata (GnomeCmdFileMetadataExtractor *fme,
                                                         GnomeCmdFileDescriptor *fd,
                                                         GnomeCmdFileMetadataExtractorAddTag add,
                                                         gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_METADATA_EXTRACTOR (fme));
    GnomeCmdFileMetadataExtractorInterface *iface = GNOME_CMD_FILE_METADATA_EXTRACTOR_GET_IFACE (fme);
    if (iface->extract_metadata)
        (* iface->extract_metadata) (fme, fd, add, user_data);
}
