/** 
 * @file gnome-cmd-advrename-profile-component.h
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
#pragma once

#include "gnome-cmd-data.h"
#include "tags/file_metadata.h"

#define GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT              (gnome_cmd_advrename_profile_component_get_type ())
#define GNOME_CMD_ADVRENAME_PROFILE_COMPONENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT, GnomeCmdAdvrenameProfileComponent))
#define GNOME_CMD_ADVRENAME_PROFILE_COMPONENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT, GnomeCmdAdvrenameProfileComponentClass))
#define GNOME_CMD_IS_ADVRENAME_PROFILE_COMPONENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT))
#define GNOME_CMD_IS_ADVRENAME_PROFILE_COMPONENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT))
#define GNOME_CMD_ADVRENAME_PROFILE_COMPONENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_ADVRENAME_PROFILE_COMPONENT, GnomeCmdAdvrenameProfileComponentClass))


extern "C" GType gnome_cmd_advrename_profile_component_get_type ();


struct GnomeCmdAdvrenameProfileComponent;


extern "C" GnomeCmdAdvrenameProfileComponent *gnome_cmd_advrename_profile_component_new (AdvancedRenameProfile *profile,
                                                                                         GnomeCmdFileMetadataService *file_metadata_service);
extern "C" void gnome_cmd_advrename_profile_component_update (GnomeCmdAdvrenameProfileComponent *component);
extern "C" void gnome_cmd_advrename_profile_component_copy (GnomeCmdAdvrenameProfileComponent *component);

gchar *gnome_cmd_advrename_profile_component_convert_case (GnomeCmdAdvrenameProfileComponent *component, gchar *string);
gchar *gnome_cmd_advrename_profile_component_trim_blanks (GnomeCmdAdvrenameProfileComponent *component, gchar *string);

const gchar *gnome_cmd_advrename_profile_component_get_template_entry (GnomeCmdAdvrenameProfileComponent *component);
void gnome_cmd_advrename_profile_component_set_template_history (GnomeCmdAdvrenameProfileComponent *component, GList *history);
void gnome_cmd_advrename_profile_component_set_sample_fname (GnomeCmdAdvrenameProfileComponent *component, const gchar *fname);

std::vector<GnomeCmd::RegexReplace> gnome_cmd_advrename_profile_component_get_valid_regexes (GnomeCmdAdvrenameProfileComponent *component);
