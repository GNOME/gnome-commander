/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
