/**
 * @file gnome-cmd-settings-migrate.cc
 * @brief Here are functions stored for migrating xml settings to Gnomes GSettings
 * @copyright (C) 2013-2016 Uwe Scholz\n
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
#include <stdio.h>
#include "gnome-cmd-settings-migrate.h"

/**
 * Hier sollte folgendes passieren:
 * - Prüfen, ob ~/.gnome-commander/gnome-commander.xml existiert
 * - Wenn ja
 *   - dortige Einstellungen laden und in GSettings speichern
 *   - umbenennen der xml-Datei in *.bak
 *   - Rückgabewert TRUE
 * - wenn nein:
 *   - Rückgabewert: FALSE
 */

/**
 * @note This function returns FALSE if ~/.gnome-commander/gnome-commander.xml
 * does not exist. If it exists, the function loads most of the settings
 * into the GSettings path org.gnome.gnome-commander.preferences.
 * (See gnome-cmd-settings.cc for more info.) When the migration of the
 * old data is completed, the xml file is renamed into
 * ~/.gnome-commander/gnome-commander.xml.backup and TRUE is returned.
 *
 * @note Beginning with gcmd-v1.6 GSettings is used for storing and
 * loading gcmd settings. For compatibility reasons, this
 * functions tries to load settings from the 'old' xml file.
 *
 * @note In later versions of gcmd (later than v1.6), this function
 * might be removed, because when saving the settings, GSettings is used.
 *
 * @returns FALSE if ~/.gnome-commander/gnome-commander.xml is not existing
 * and TRUE if it is existing and most of the settings inside have been moved
 * into the GSettings path org.gnome.gnome-commander.preferences.
 */
gboolean load_xml_into_gsettings (const gchar *fname)
{
    gboolean xml_was_there;
    gchar *xml_cfg_path = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);

    FILE *fd = fopen (xml_cfg_path, "r");

    if (fd)
    {
        // ToDo: Data migration

        fclose (fd);
        xml_was_there = TRUE;
        // ToDo: Move old xml-file to ~/.gnome-commander/gnome-commander.xml.backup
        //       à la save_devices_old ("devices.backup");
    }
    else
    {
        g_warning ("Failed to open the file %s for reading, skipping data migration", xml_cfg_path);
        xml_was_there = FALSE;
    }

    g_free (xml_cfg_path);

    return xml_was_there;
}
