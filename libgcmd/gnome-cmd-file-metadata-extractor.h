/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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


#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gnome-cmd-state.h>
#include <gnome-cmd-file-descriptor.h>

G_BEGIN_DECLS

#define GNOME_CMD_TYPE_FILE_METADATA_EXTRACTOR (gnome_cmd_file_metadata_extractor_get_type ())

G_DECLARE_INTERFACE (GnomeCmdFileMetadataExtractor,
                     gnome_cmd_file_metadata_extractor,
                     GNOME_CMD,
                     FILE_METADATA_EXTRACTOR,
                     GObject)

typedef void (* GnomeCmdFileMetadataExtractorAddTag) (const gchar *tag, const gchar *value, gpointer user_data);

struct _GnomeCmdFileMetadataExtractorInterface
{
    GTypeInterface g_iface;

    GStrv (* supported_tags) (GnomeCmdFileMetadataExtractor *fme);
    GStrv (* summary_tags) (GnomeCmdFileMetadataExtractor *fme);
    gchar *(* class_name) (GnomeCmdFileMetadataExtractor *fme, const gchar *cls);
    gchar *(* tag_name) (GnomeCmdFileMetadataExtractor *fme, const gchar *tag);
    gchar *(* tag_description) (GnomeCmdFileMetadataExtractor *fme, const gchar *tag);
    void (* extract_metadata) (GnomeCmdFileMetadataExtractor *fme,
                               GnomeCmdFileDescriptor *f,
                               GnomeCmdFileMetadataExtractorAddTag add,
                               gpointer user_data);
};

GStrv gnome_cmd_file_metadata_extractor_supported_tags (GnomeCmdFileMetadataExtractor *fme);
GStrv gnome_cmd_file_metadata_extractor_summary_tags (GnomeCmdFileMetadataExtractor *fme);
gchar *gnome_cmd_file_metadata_extractor_tag_name (GnomeCmdFileMetadataExtractor *fme, const gchar *tag);
gchar *gnome_cmd_file_metadata_extractor_class_name (GnomeCmdFileMetadataExtractor *fme, const gchar *cls);
gchar *gnome_cmd_file_metadata_extractor_tag_description (GnomeCmdFileMetadataExtractor *fme, const gchar *tag);
void gnome_cmd_file_metadata_extractor_extract_metadata (GnomeCmdFileMetadataExtractor *fme,
                                                         GnomeCmdFileDescriptor *f,
                                                         GnomeCmdFileMetadataExtractorAddTag add,
                                                         gpointer user_data);

G_END_DECLS
