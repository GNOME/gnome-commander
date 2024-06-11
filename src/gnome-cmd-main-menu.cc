/**
 * @file gnome-cmd-main-menu.cc
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
#include "gnome-cmd-main-menu.h"
#include "gnome-cmd-types.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-data.h"
#include "utils.h"
#include "imageloader.h"
#include "gnome-cmd-user-actions.h"
#include "dialogs/gnome-cmd-manage-bookmarks-dialog.h"

using namespace std;


static GMenu* local_connections_menu ()
{
    GnomeCmdConList *con_list = gnome_cmd_con_list_get ();
    GList *all_cons = gnome_cmd_con_list_get_all (con_list);

    GMenu *menu = g_menu_new ();

    // Add all open connections
    for (GList *i = all_cons; i; i = i->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (i->data);
        if (!GNOME_CMD_IS_CON_REMOTE (con) || gnome_cmd_con_is_open (con))
        {
            gchar *label = gnome_cmd_con_get_go_text (con);
            GIcon *icon = gnome_cmd_con_get_go_icon (con);

            GMenuItem *item = g_menu_item_new (label, nullptr);
            g_menu_item_set_icon (item, icon);
            g_menu_item_set_action_and_target (item, "win.connections-set-current", "s", gnome_cmd_con_get_uuid (con));
            g_menu_append_item (menu, item);

            g_clear_pointer (&label, g_free);
            g_clear_object (&icon);
        }
    }

    return menu;
}


static GMenu* connections_menu ()
{
    GnomeCmdConList *con_list = gnome_cmd_con_list_get ();
    GList *all_cons = gnome_cmd_con_list_get_all (con_list);

    GMenu *menu = g_menu_new ();

    // Add all open connections that are not permanent
    for (GList *i = all_cons; i; i = i->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (i->data);
        if (gnome_cmd_con_is_closeable (con) && gnome_cmd_con_is_open (con))
        {
            gchar *label = gnome_cmd_con_get_close_text (con);
            GIcon *icon = gnome_cmd_con_get_close_icon (con);

            GMenuItem *item = g_menu_item_new (label, nullptr);
            g_menu_item_set_icon (item, icon);
            g_menu_item_set_action_and_target (item, "win.connections-close", "s", gnome_cmd_con_get_uuid (con));
            g_menu_append_item (menu, item);

            g_clear_pointer (&label, g_free);
            g_clear_object (&icon);
        }
    }

    return menu;
}


static GMenu *create_bookmarks_menu ()
{
    GMenu *menu = g_menu_new ();

    // Add bookmark groups
    GList *cons = gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ());
    for (; cons; cons = cons->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (cons->data);
        GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (con);

        if (group && group->bookmarks)
        {
            const gchar *con_uuid = gnome_cmd_con_get_uuid (con);

            // Add bookmarks for this group
            GMenu *group_items = g_menu_new();
            for (GList *bookmarks = group->bookmarks; bookmarks; bookmarks = bookmarks->next)
            {
                auto bookmark = static_cast<GnomeCmdBookmark*> (bookmarks->data);
                GMenuItem *item = g_menu_item_new (bookmark->name, nullptr);
                g_menu_item_set_action_and_target (item, "win.bookmarks-goto", "(ss)", con_uuid, bookmark->name);
                g_menu_append_item (group_items, item);
            }

            GMenuItem *group_item = g_menu_item_new_submenu (gnome_cmd_con_get_alias (group->con), G_MENU_MODEL (group_items));
            g_menu_item_set_icon (group_item, gnome_cmd_con_get_go_icon (group->con));

            g_menu_append_item (menu, group_item);
        }
    }

    return menu;
}


static GMenu *create_plugins_menu ()
{
    GMenu *menu = g_menu_new ();

    for (GList *plugins = plugin_manager_get_all (); plugins; plugins = plugins->next) {
        auto pluginData = static_cast<PluginData *>(plugins->data);
        if (pluginData->active && pluginData->menu)
            g_menu_append_section (menu, nullptr, G_MENU_MODEL (pluginData->menu));
    }

    return menu;
}


MenuBuilder::Result gnome_cmd_main_menu_new ()
{
    return MenuBuilder()
        .submenu(_("_File"))
            .section()
                .item(_("Change _Owner/Group"),             "win.file-chown")
                .item(_("Change Per_missions"),             "win.file-chmod")
                .item(_("Advanced _Rename Tool"),           "win.file-advrename",       "<Control>M")
                .item(_("Create _Symbolic Link"),           "win.file-create-symlink",  "<Control><shift>F5")
                .item(_("_Properties…"),                    "win.file-properties",      "<Alt>KP_Enter")
            .endsection()
            .section()
                .item(_("_Search…"),                        "win.file-search",          "<Alt>F7")
                .item(_("_Quick Search…"),                  "win.file-quick-search")
                .item(_("_Enable Filter…"),                 "win.edit-filter")
            .endsection()
            .section()
                .item(_("_Diff"),                           "win.file-diff")
                .item(_("S_ynchronize Directories"),        "win.file-sync-dirs")
            .endsection()
            .section()
                .item(_("Start _GNOME Commander as root"),  "win.command-root-mode")
            .endsection()
            .item(_("_Quit"),                               "win.file-exit",            "<Control>Q")
        .endsubmenu()
        .submenu(_("_Edit"))
            .section()
                .item(_("Cu_t"),                            "win.edit-cap-cut",         "<Control>X")
                .item(_("_Copy"),                           "win.edit-cap-copy",        "<Control>C")
                .item(_("_Paste"),                          "win.edit-cap-paste",       "<Control>V")
                .item(_("_Delete"),                         "win.file-delete",          "Delete")
            .endsection()
            .item(_("Copy _File Names"),                    "win.edit-copy-fnames")
        .endsubmenu()
        .submenu(_("_Mark"))
            .section()
                .item(_("_Select All"),                     "win.mark-select-all",                          "<Control>KP_Add")
                .item(_("_Unselect All"),                   "win.mark-unselect-all",                        "<Control>KP_Subtract")
                .item(_("Select all _Files"),               "win.mark-select-all-files")
                .item(_("Unselect all Fi_les"),             "win.mark-unselect-all-files")
                .item(_("Select with _Pattern"),            "win.mark-select-with-pattern",                 "KP_Add")
                .item(_("Unselect with P_attern"),          "win.mark-unselect-with-pattern",               "KP_Subtract")
                .item(_("Select with same _Extension"),     "win.mark-select-all-with-same-extension")
                .item(_("Unselect with same E_xtension"),   "win.mark-unselect-all-with-same-extension")
                .item(_("_Invert Selection"),               "win.mark-invert-selection",                    "KP_Multiply")
                .item(_("_Restore Selection"),              "win.mark-restore-selection")
            .endsection()
            .item(_("_Compare Directories"),                "win.mark-compare-directories",                 "<Shift>F2")
        .endsubmenu()
        .submenu(_("_View"))
            .section()
                .item(_("_Back"),                           "win.view-back",            "<Alt>Pointer_Left")
                .item(_("_Forward"),                        "win.view-forward",         "<Alt>Pointer_Right")
                .item(_("_Refresh"),                        "win.view-refresh",         "<Control>R")
            .endsection()
            .section()
                .item(_("Open in New _Tab"),                "win.view-new-tab",         "<Control>T")
                .item(_("_Close Tab"),                      "win.view-close-tab",       "<Control>W")
                .item(_("Close _All Tabs"),                 "win.view-close-all-tabs",  "<Control><Shift>W")
            .endsection()
            .section()
                .item(_("Show Toolbar"),                    "win.view-toolbar")
                .item(_("Show Device Buttons"),             "win.view-conbuttons")
                .item(_("Show Device List"),                "win.view-devlist")
                .item(_("Show Command Line"),               "win.view-cmdline")
                .item(_("Show Buttonbar"),                  "win.view-buttonbar")
            .endsection()
            .section()
                .item(_("Show Hidden Files"),               "win.view-hidden-files",    "<Control><Shift>H")
                .item(_("Show Backup Files"),               "win.view-backup-files")
            .endsection()
            .section()
                .item(_("_Equal Panel Size"),               "win.view-equal-panes",     "<Control><Shift>KP_Equal")
                .item(_("Maximize Panel Size"),             "win.view-maximize-pane")
            .endsection()
            .section()
                .item(_("Horizontal Orientation"),          "win.view-horizontal-orientation")
            .endsection()
        .endsubmenu()
        .submenu(_("_Settings"))
            .item(_("_Options…"),                           "win.options-edit",              "<Control>O")
            .item(_("_Keyboard Shortcuts…"),                "win.options-edit-shortcuts")
        .endsubmenu()
        .submenu(_("_Connections"))
            .item(_("_Remote Server…"),                     "win.connections-open",         "<Control>F")
            .item(_("New Connection…"),                     "win.connections-new",          "<Control>N")
            .section(local_connections_menu())
            .section(connections_menu())
        .endsubmenu()
        .submenu(_("_Bookmarks"))
            .item(_("_Bookmark this Directory…"),           "win.bookmarks-add-current")
            .item(_("_Manage Bookmarks…"),                  "win.bookmarks-edit",           "<Control>D")
            .section(create_bookmarks_menu())
        .endsubmenu()
        .submenu(_("_Plugins"))
            .item(_("_Configure Plugins…"),                 "win.plugins-configure")
            .section(create_plugins_menu())
        .endsubmenu()
        .submenu(_("_Help"))
            .section()
                .item(_("_Documentation"),                  "win.help-help",            "F1")
                .item(_("_Keyboard Shortcuts"),             "win.help-keyboard")
                .item(_("GNOME Commander on the _Web"),     "win.help-web")
                .item(_("Report a _Problem"),               "win.help-problem")
            .endsection()
            .item(_("_About"),  "win.help-about")
        .endsubmenu()
        .build();
}
