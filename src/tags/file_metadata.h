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


struct GnomeCmdFileMetadataService;
struct GnomeCmdFileMetadata;


extern "C" gchar *gnome_cmd_file_metadata_service_get_name (GnomeCmdFileMetadataService *fms, const gchar *tag_id);
extern "C" gchar *gnome_cmd_file_metadata_service_get_description (GnomeCmdFileMetadataService *fms, const gchar *tag_id);
extern "C" GMenu *gnome_cmd_file_metadata_service_create_menu (GnomeCmdFileMetadataService *fms, const gchar *action_name);
extern "C" GnomeCmdFileMetadata *gnome_cmd_file_metadata_service_extract_metadata (GnomeCmdFileMetadataService *fms, GnomeCmdFile *f);
extern "C" gchar *gnome_cmd_file_metadata_service_to_tsv (GnomeCmdFileMetadataService *fms, GnomeCmdFileMetadata *fm);
extern "C" GStrv gnome_cmd_file_metadata_service_file_summary (GnomeCmdFileMetadataService *fms, GnomeCmdFileMetadata *fm);


extern "C" GnomeCmdFileMetadata *gnome_cmd_file_metadata_new ();
extern "C" void gnome_cmd_file_metadata_free (GnomeCmdFileMetadata *fm);

extern "C" void gnome_cmd_file_metadata_add (GnomeCmdFileMetadata *fm, const gchar *tag_id, const gchar *value);

extern "C" gboolean gnome_cmd_file_metadata_has_tag (GnomeCmdFileMetadata *fm, const gchar *tag_id);
extern "C" gchar *gnome_cmd_file_metadata_get (GnomeCmdFileMetadata *fm, const gchar *tag_id);
