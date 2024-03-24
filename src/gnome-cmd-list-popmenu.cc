/** 
 * @file gnome-cmd-list-popmenu.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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
#include "gnome-cmd-list-popmenu.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-user-actions.h"
#include "utils.h"
#include "cap.h"
#include "imageloader.h"

using namespace std;


G_DEFINE_TYPE (GnomeCmdListPopmenu, gnome_cmd_list_popmenu, GTK_TYPE_MENU)


static void on_new_directory (GtkMenuItem *item, GnomeCmdFileSelector *fs)
{
    file_mkdir (item);
}


static void on_new_textfile (GtkMenuItem *item, GnomeCmdFileSelector *fs)
{
    gnome_cmd_file_selector_show_new_textfile_dialog (fs);
}


static void on_refresh (GtkMenuItem *item, GnomeCmdFileSelector *fs)
{
    fs->file_list()->reload();
}


static void on_paste (GtkMenuItem *item, GnomeCmdFileSelector *fs)
{
    gnome_cmd_file_selector_cap_paste (fs);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GTK_OBJECT_CLASS (gnome_cmd_list_popmenu_parent_class)->destroy (object);
}


static void map (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (gnome_cmd_list_popmenu_parent_class)->map (widget);
}


static void gnome_cmd_list_popmenu_class_init (GnomeCmdListPopmenuClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void gnome_cmd_list_popmenu_init (GnomeCmdListPopmenu *menu)
{
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_list_popmenu_new (GnomeCmdFileSelector *fs)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), nullptr);

    static const GtkActionEntry entries[] =
    {
        { "New",          nullptr,                     _("_New") },
        { "Paste",        GTK_STOCK_PASTE,             _("_Paste"),              nullptr, nullptr, (GCallback) on_paste },
        { "Terminal",     TERMINAL_STOCKID,            _("Open _terminal here"), nullptr, nullptr, (GCallback) command_open_terminal__internal },
        { "Refresh",      GTK_STOCK_REFRESH,           _("_Refresh"),            nullptr, nullptr, (GCallback) on_refresh },
        { "NewDirectory", FILETYPEDIR_STOCKID,         _("_Directory"),          nullptr, nullptr, (GCallback) on_new_directory },
        { "NewTextFile",  FILETYPEREGULARFILE_STOCKID, _("_Text File"),          nullptr, nullptr, (GCallback) on_new_textfile }
    };

    static const char *uiDescription =
    "<ui>"
    "  <popup action='EmptyFileListPopup'>"
    "    <menu action='New'>"
    "      <menuitem action='NewDirectory'/>"
    "      <menuitem action='NewTextFile'/>"
    "    </menu>"
    "    <menuitem action='Paste'/>"
    "    <separator/>"
    "    <menuitem action='Terminal'/>"
    "    <separator/>"
    "    <menuitem action='Refresh'/>"
    "  </popup>"
    "</ui>";

    GtkActionGroup *action_group;
    GtkUIManager *uiManager;
    GError *error;

    action_group = gtk_action_group_new ("PopupActions");
    gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), fs);

    uiManager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (uiManager, action_group, 0);

    error = nullptr;
    if (!gtk_ui_manager_add_ui_from_string (uiManager, uiDescription, -1, &error))
    {
        g_message ("building menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
    }

    GtkWidget *menu = gtk_ui_manager_get_widget (uiManager, "/EmptyFileListPopup");

    return GTK_WIDGET (menu);
}

