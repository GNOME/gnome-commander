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


struct GnomeCmdMainMenuPrivate
{
    GtkWidget *mainMenuBar;
    GtkWidget *connections_menu;
    GtkWidget *bookmarks_menu;
    GtkWidget *plugins_menu;

    GtkWidget *menu_view_back;
    GtkWidget *menu_view_forward;

    GList *connections_menuitems;
    GList *bookmark_menuitems;
    GList *group_menuitems;
};


G_DEFINE_TYPE (GnomeCmdMainMenu, gnome_cmd_main_menu, GTK_TYPE_MENU_BAR)


/*******************************
 * Private functions
 *******************************/

static void on_bookmark_selected (GtkMenuItem *menuitem, GnomeCmdBookmark *bookmark)
{
    g_return_if_fail (bookmark != nullptr);

    gnome_cmd_bookmark_goto (bookmark);
}


static void on_con_list_list_changed (GnomeCmdConList *con_list, GnomeCmdMainMenu *main_menu)
{
    gnome_cmd_main_menu_update_connections (main_menu);
}


static GtkWidget *
add_menu_item (GnomeCmdMainMenu *main_menu,
               GtkMenuShell *menu,
               const gchar *text,
               const gchar *tooltip,
               GdkPixbuf *pixbuf,
               GCallback callback,
               gpointer user_data)
{
    g_return_val_if_fail (GTK_IS_MENU_SHELL (menu), nullptr);

    GtkWidget *item, *label;
    GtkWidget *image_widget = nullptr;

    item = gtk_image_menu_item_new ();

    gtk_widget_set_tooltip_text (item, tooltip);

    if (pixbuf)
        image_widget = gtk_image_new_from_pixbuf (pixbuf);

    if (image_widget)
    {
        gtk_widget_show (image_widget);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image_widget);
    }

    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);


    // Create the contents of the menu item
    label = gtk_label_new (text);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (item), label);


    // Connect to the signal and set user data
    if (callback)
    {
        g_object_set_data (G_OBJECT (item), "uidata", user_data);
        g_signal_connect (item, "activate", callback, user_data);
    }

    return item;
}


static GtkWidget *add_separator (GtkMenuShell *menu)
{
    GtkWidget *child = gtk_menu_item_new ();
    gtk_widget_set_sensitive (child, FALSE);
    gtk_widget_show (child);
    gtk_menu_shell_append (menu, child);
    return child;
}


static void add_bookmark_menu_item (GnomeCmdMainMenu *main_menu, GtkMenuShell *menu, GnomeCmdBookmark *bookmark)
{
    GtkWidget *item;

    item = add_menu_item (main_menu, menu, bookmark->name, nullptr,
                          IMAGE_get_pixbuf (PIXMAP_BOOKMARK),
                          G_CALLBACK (on_bookmark_selected), bookmark);

    // Remember this bookmarks item-widget so that we can remove it later
    main_menu->priv->bookmark_menuitems = g_list_append (main_menu->priv->bookmark_menuitems, item);
}


static void add_bookmark_group (GnomeCmdMainMenu *main_menu, GtkMenuShell *menu, GnomeCmdBookmarkGroup *group)
{
    g_return_if_fail (GTK_IS_MENU_SHELL (menu));
    g_return_if_fail (group != nullptr);

    GdkPixbuf *pixbuf = gnome_cmd_con_get_go_pixbuf (group->con);
    GtkWidget *item = add_menu_item (main_menu, menu, gnome_cmd_con_get_alias (group->con), nullptr,
                                     pixbuf,
                                     nullptr, nullptr);

    // Remember this bookmarks item-widget so that we can remove it later
    main_menu->priv->group_menuitems = g_list_append (main_menu->priv->group_menuitems, item);


    // Add bookmarks for this group
    GtkWidget *submenu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

    for (GList *bookmarks = group->bookmarks; bookmarks; bookmarks = bookmarks->next)
    {
        auto bookmark = static_cast<GnomeCmdBookmark*> (bookmarks->data);
        add_bookmark_menu_item (main_menu, GTK_MENU_SHELL (submenu), bookmark);
    }
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkWidget *object)
{
    GnomeCmdMainMenu *menu = GNOME_CMD_MAIN_MENU (object);

    g_free (menu->priv);

    GTK_WIDGET_CLASS (gnome_cmd_main_menu_parent_class)->destroy (object);
}


static void map (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (gnome_cmd_main_menu_parent_class)->map (widget);
}


static void gnome_cmd_main_menu_class_init (GnomeCmdMainMenuClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    widget_class->destroy = destroy;
    widget_class->map = ::map;
}

static GtkUIManager *get_file_menu_ui_manager()
{
    static const char *uiMenuBarDescription =
    "<ui>"
    "  <menubar name='MainMenuBar'>"
    "    <menu action='FileMenu'>"
    "      <menuitem action='ChangeOwnerOrGroup'/>"
    "      <menuitem action='ChangeMod'/>"
    "      <menuitem action='AdvRenameTool'/>"
    "      <menuitem action='CreateSymLink'/>"
    "      <menuitem action='OpenFileProperties'/>"
    "      <separator/>"
    "      <menuitem action='Search'/>"
    "      <menuitem action='QuickSearch'/>"
    "      <menuitem action='EnableFilter'/>"
    "      <separator/>"
    "      <menuitem action='Diff'/>"
    "      <menuitem action='SyncDirs'/>"
    "      <separator/>"
    "      <menuitem action='StartAsRoot'/>"
    "      <separator/>"
    "      <menuitem action='Quit'/>"
    "    </menu>"
    "    <menu action='EditMenu'>"
    "      <menuitem action='Cut'/>"
    "      <menuitem action='Copy'/>"
    "      <menuitem action='Paste'/>"
    "      <menuitem action='Delete'/>"
    "      <separator/>"
    "      <menuitem action='CopyFileNames'/>"
    "    </menu>"
    "    <menu action='MarkMenu'>"
    "      <menuitem action='SelectAll'/>"
    "      <menuitem action='UnselectAll'/>"
    "      <menuitem action='SelectAllFiles'/>"
    "      <menuitem action='UnSelectAllFiles'/>"
    "      <menuitem action='SelectWPattern'/>"
    "      <menuitem action='UnSelectWPattern'/>"
    "      <menuitem action='SelectWExtension'/>"
    "      <menuitem action='UnselectWExtension'/>"
    "      <menuitem action='InvertSelection'/>"
    "      <menuitem action='RestoreSelection'/>"
    "      <separator/>"
    "      <menuitem action='CompareDirectories'/>"
    "    </menu>"
    "    <menu action='ViewMenu'>"
    "      <menuitem action='Back'/>"
    "      <menuitem action='Forward'/>"
    "      <menuitem action='Refresh'/>"
    "      <separator/>"
    "      <menuitem action='NewTab'/>"
    "      <menuitem action='CloseTab'/>"
    "      <menuitem action='CloseAllTabs'/>"
    "      <separator/>"
    "      <menuitem action='ShowToolbar'/>"
    "      <menuitem action='ShowDeviceButtons'/>"
    "      <menuitem action='ShowDeviceList'/>"
    "      <menuitem action='ShowCommandLine'/>"
    "      <menuitem action='ShowButtonbar'/>"
    "      <separator/>"
    "      <menuitem action='ShowHiddenFiles'/>"
    "      <menuitem action='ShowBackupFiles'/>"
    "      <separator/>"
    "      <menuitem action='EqualPanes'/>"
    "      <menuitem action='MaximizePane'/>"
    "      <separator/>"
    "      <menuitem action='HorizontalOrientation'/>"
    "    </menu>"
    "    <menu action='Settings'>"
    "      <menuitem action='Options'/>"
    "      <menuitem action='Shortcuts'/>"
    "    </menu>"
    "    <menu action='Connections'>"
    "      <menuitem action='RemoteServer'/>"
    "      <menuitem action='NewConnection'/>"
    "    </menu>"
    "    <menu action='Bookmarks'>"
    "      <menuitem action='BookmarkDir'/>"
    "      <menuitem action='ManageBookmarks'/>"
    "    </menu>"
    "    <menu action='Plugins'>"
    "      <menuitem action='ConfigurePlugins'/>"
    "    </menu>"
    "    <menu action='Help'>"
    "      <menuitem action='Documentation'/>"
    "      <menuitem action='KeyboardShortcuts'/>"
    "      <menuitem action='Web'/>"
    "      <menuitem action='Problem'/>"
    "      <separator/>"
    "      <menuitem action='About'/>"
    "    </menu>"
    "  </menubar>"
    "</ui>";

    static const GtkActionEntry fileMenuEntries[] =
    {
        { "FileMenu", nullptr, _("_File") },
        { "ChangeOwnerOrGroup", nullptr, _("Change _Owner/Group"),        nullptr, nullptr, (GCallback) file_chown },
        { "ChangeMod",          nullptr, _("Change Per_missions"),        nullptr, nullptr, (GCallback) file_chmod },
        { "AdvRenameTool",      nullptr, _("Advanced _Rename Tool"),      "<Control>M", nullptr, (GCallback) file_advrename },
        { "CreateSymLink",      nullptr, _("Create _Symbolic Link"),      "<Control><shift>F5", nullptr, (GCallback) file_create_symlink },
        { "OpenFileProperties", GTK_STOCK_PROPERTIES, _("_Properties…"),  "<Alt>KP_Enter", nullptr, (GCallback) file_properties },
        { "Search",             GTK_STOCK_FIND, _("_Search…"),            "<Alt>F7", nullptr, (GCallback) file_search },
        { "QuickSearch",        GTK_STOCK_FIND, _("_Quick Search…"),      nullptr, nullptr, (GCallback) file_quick_search },
        { "EnableFilter",       GTK_STOCK_CLEAR, _("_Enable Filter…"),    nullptr, nullptr, (GCallback) edit_filter },
        { "Diff",               nullptr, _("_Diff"),                      nullptr, nullptr, (GCallback) file_diff },
        { "SyncDirs",           nullptr, _("S_ynchronize Directories"),   nullptr, nullptr, (GCallback) file_sync_dirs },
        { "StartAsRoot",        GTK_STOCK_DIALOG_AUTHENTICATION,          _("Start _GNOME Commander as root"), nullptr, nullptr, (GCallback) command_root_mode },
        { "Quit",               GTK_STOCK_QUIT, _("_Quit"), "<Control>Q", nullptr, (GCallback) file_exit }
    };

    static const GtkActionEntry editMenuEntries[] =
    {
        { "EditMenu",      nullptr,          _("_Edit") },
        { "Cut",           GTK_STOCK_CUT,    _("Cu_t"),            "<Control>X", nullptr, (GCallback) edit_cap_cut },
        { "Copy",          GTK_STOCK_COPY,   _("_Copy"),           "<Control>C", nullptr, (GCallback) edit_cap_copy },
        { "Paste",         GTK_STOCK_PASTE,  _("_Paste"),          "<Control>V", nullptr, (GCallback) edit_cap_paste },
        { "Delete",        GTK_STOCK_DELETE, _("_Delete"),         "Delete",     nullptr, (GCallback) file_delete },
        { "CopyFileNames", nullptr,          _("Copy _File Names"), nullptr,     nullptr, (GCallback) edit_copy_fnames }
    };

    static const GtkActionEntry markMenuEntries[] =
    {
        { "MarkMenu",           nullptr,              _("_Mark") },
        { "SelectAll",          GTK_STOCK_SELECT_ALL, _("_Select All"),                   "<Control>KP_Add", nullptr, (GCallback) mark_select_all },
        { "UnselectAll",        nullptr,              _("_Unselect All"),                 "<Control>KP_Subtract", nullptr, (GCallback) mark_unselect_all },
        { "SelectAllFiles",     nullptr,              _("Select all _Files"),              nullptr, nullptr, (GCallback) mark_select_all_files },
        { "UnSelectAllFiles",   nullptr,              _("Unselect all Fi_les"),            nullptr, nullptr, (GCallback) mark_unselect_all_files },
        { "SelectWPattern",     nullptr,              _("Select with _Pattern"),          "KP_Add", nullptr, (GCallback) mark_select_with_pattern },
        { "UnSelectWPattern",   nullptr,              _("Unselect with P_attern"),        "KP_Subtract", nullptr, (GCallback) mark_unselect_with_pattern },
        { "SelectWExtension",   nullptr,              _("Select with same _Extension"),   nullptr, nullptr, (GCallback) mark_select_all_with_same_extension },
        { "UnselectWExtension", nullptr,              _("Unselect with same E_xtension"), nullptr, nullptr, (GCallback) mark_unselect_all_with_same_extension },
        { "InvertSelection",    nullptr,              _("_Invert Selection"),             "KP_Multiply", nullptr, (GCallback) mark_invert_selection },
        { "RestoreSelection",   nullptr,              _("_Restore Selection"),            nullptr, nullptr, (GCallback)  mark_restore_selection},
        { "CompareDirectories", nullptr,              _("_Compare Directories"),          "<Shift>F2", nullptr, (GCallback) mark_compare_directories }
    };

    static const GtkActionEntry viewMenuEntries[] =
    {
        { "ViewMenu",     nullptr,              _("_View") },
        { "Back",         GTK_STOCK_GO_BACK,    _("_Back"),               "<Alt>Pointer_Left",        nullptr, (GCallback) view_back },
        { "Forward",      GTK_STOCK_GO_FORWARD, _("_Forward"),            "<Alt>Pointer_Right",       nullptr, (GCallback) view_forward },
        { "Refresh",      GTK_STOCK_REFRESH,    _("_Refresh"),            "<Control>R",               nullptr, (GCallback) view_refresh },
        { "NewTab",       GTK_STOCK_OPEN,       _("Open in New _Tab"),    "<Control>T",               nullptr, (GCallback) view_new_tab },
        { "CloseTab",     GTK_STOCK_CLOSE,      _("_Close Tab"),          "<Control>W",               nullptr, (GCallback) view_close_tab },
        { "CloseAllTabs", GTK_STOCK_CLOSE,      _("Close _All Tabs"),     "<Control><Shift>W",        nullptr, (GCallback) view_close_all_tabs },
        { "EqualPanes",   GTK_STOCK_ZOOM_100,   _("_Equal Panel Size"),   "<Control><Shift>KP_Equal", nullptr, (GCallback) view_equal_panes },
        { "MaximizePane", GTK_STOCK_ZOOM_FIT,   _("Maximize Panel Size"), nullptr,                    nullptr, (GCallback) view_maximize_pane }
    };

    static const GtkToggleActionEntry viewMenuToggleEntries[] =
    {
        { "ShowToolbar",           nullptr, _("Show Toolbar"),           nullptr,             nullptr, (GCallback) view_toolbar,                true },
        { "ShowDeviceButtons",     nullptr, _("Show Device Buttons"),    nullptr,             nullptr, (GCallback) view_conbuttons,             true },
        { "ShowDeviceList",        nullptr, _("Show Device List"),       nullptr,             nullptr, (GCallback) view_devlist,                true },
        { "ShowCommandLine",       nullptr, _("Show Command Line"),      nullptr,             nullptr, (GCallback) view_cmdline,                true },
        { "ShowButtonbar",         nullptr, _("Show Buttonbar"),         nullptr,             nullptr, (GCallback) view_buttonbar,              true },
        { "ShowHiddenFiles",       nullptr, _("Show Hidden Files"),      "<Control><Shift>H", nullptr, (GCallback) view_hidden_files,           true },
        { "ShowBackupFiles",       nullptr, _("Show Backup Files"),      nullptr,             nullptr, (GCallback) view_backup_files,           true },
        { "HorizontalOrientation", nullptr, _("Horizontal Orientation"), nullptr,             nullptr, (GCallback) view_horizontal_orientation, true }
    };

    static const GtkActionEntry settingsMenuEntries[] =
    {
        { "Settings",  nullptr,               _("_Settings") },
        { "Options",   GTK_STOCK_PREFERENCES, _("_Options…"),            "<Control>O", nullptr, (GCallback) options_edit },
        { "Shortcuts", GTK_STOCK_ITALIC,      _("_Keyboard Shortcuts…"), nullptr,      nullptr, (GCallback) options_edit_shortcuts }
    };

    static const GtkActionEntry connectionsMenuEntries[] =
    {
        { "Connections",   nullptr,           _("_Connections") },
        { "RemoteServer",  GTK_STOCK_CONNECT, _("_Remote Server…"), "<Control>F", nullptr, (GCallback) connections_open },
        { "NewConnection", GTK_STOCK_CONNECT, _("New Connection…"), "<Control>N", nullptr, (GCallback) connections_new }
    };

    static const GtkActionEntry bookmarksMenuEntries[] =
    {
        { "Bookmarks",       nullptr,              _("_Bookmarks") },
        { "BookmarkDir",     GTK_STOCK_ADD,        _("_Bookmark this Directory…"), nullptr,      nullptr, (GCallback) bookmarks_add_current },
        { "ManageBookmarks", GTK_STOCK_PROPERTIES, _("_Manage Bookmarks…"),        "<Control>D", nullptr, (GCallback) bookmarks_edit }
    };

    static const GtkActionEntry pluginsMenuEntries[] =
    {
        { "Plugins",          nullptr,            _("_Plugins") },
        { "ConfigurePlugins", EXEC_WHEEL_STOCKID, _("_Configure Plugins…"), nullptr, nullptr, (GCallback) plugins_configure },
    };

    static const GtkActionEntry helpMenuEntries[] =
    {
        { "Help",              nullptr,          _("_Help") },
        { "Documentation",     GTK_STOCK_HELP,   _("_Documentation"),              "F1",    nullptr, (GCallback) help_help },
        { "KeyboardShortcuts", GTK_STOCK_ITALIC, _("_Keyboard Shortcuts"),         nullptr, nullptr, (GCallback) help_keyboard },
        { "Web",               GTK_STOCK_HOME,   _("GNOME Commander on the _Web"), nullptr, nullptr, (GCallback) help_web },
        { "Problem",           GTK_STOCK_CAPS_LOCK_WARNING, _("Report a _Problem"),           nullptr, nullptr, (GCallback) help_problem },
        { "About",             GTK_STOCK_ABOUT,  _("_About"),                      nullptr, nullptr, (GCallback) help_about },
    };

    GtkActionGroup *actionGroup;
    GtkUIManager *uiManager;
    GError *error;

    actionGroup = gtk_action_group_new ("MenuBarActions");
    gtk_action_group_add_actions (actionGroup, fileMenuEntries, G_N_ELEMENTS (fileMenuEntries), nullptr);
    gtk_action_group_add_actions (actionGroup, editMenuEntries, G_N_ELEMENTS (editMenuEntries), nullptr);
    gtk_action_group_add_actions (actionGroup, markMenuEntries, G_N_ELEMENTS (markMenuEntries), nullptr);
    gtk_action_group_add_actions (actionGroup, viewMenuEntries, G_N_ELEMENTS (viewMenuEntries), nullptr);
    gtk_action_group_add_actions (actionGroup, settingsMenuEntries, G_N_ELEMENTS (settingsMenuEntries), nullptr);
    gtk_action_group_add_actions (actionGroup, connectionsMenuEntries, G_N_ELEMENTS (connectionsMenuEntries), nullptr);
    gtk_action_group_add_actions (actionGroup, bookmarksMenuEntries, G_N_ELEMENTS (bookmarksMenuEntries), nullptr);
    gtk_action_group_add_actions (actionGroup, pluginsMenuEntries, G_N_ELEMENTS (pluginsMenuEntries), nullptr);
    gtk_action_group_add_actions (actionGroup, helpMenuEntries, G_N_ELEMENTS (helpMenuEntries), nullptr);
    gtk_action_group_add_toggle_actions (actionGroup, viewMenuToggleEntries, G_N_ELEMENTS (viewMenuToggleEntries), nullptr);

    uiManager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (uiManager, actionGroup, 0);

    error = nullptr;
    if (!gtk_ui_manager_add_ui_from_string (uiManager, uiMenuBarDescription, -1, &error))
    {
        g_message ("building main menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
    }

    return uiManager;
}

static void gnome_cmd_main_menu_init (GnomeCmdMainMenu *main_menu)
{

    GtkUIManager *uiManager = get_file_menu_ui_manager();

    main_menu->priv = g_new (GnomeCmdMainMenuPrivate, 1);
    main_menu->priv->bookmark_menuitems = nullptr;
    main_menu->priv->connections_menuitems = nullptr;
    main_menu->priv->group_menuitems = nullptr;
    //main_menu->priv->view_menuitems = nullptr;

    //gtk_menu_bar_set_shadow_type (GTK_MENU_BAR (main_menu), GTK_SHADOW_NONE);

    main_menu->priv->mainMenuBar = gtk_ui_manager_get_widget (uiManager, "/MainMenuBar");

    main_menu->priv->menu_view_back    = gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/Back");
    main_menu->priv->menu_view_forward = gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/Forward");
    main_menu->priv->connections_menu  = gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/Connections");
    main_menu->priv->bookmarks_menu    = gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/Bookmarks");
    main_menu->priv->plugins_menu      = gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/Plugins");

    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/ShowToolbar")), gnome_cmd_data.show_toolbar);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/ShowDeviceButtons")), gnome_cmd_data.show_devbuttons);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/ShowDeviceList")), gnome_cmd_data.show_devlist);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/ShowCommandLine")), gnome_cmd_data.cmdline_visibility);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/ShowButtonbar")), gnome_cmd_data.buttonbar_visibility);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/ShowHiddenFiles")), !gnome_cmd_data.options.filter.file_types[GnomeCmdData::G_FILE_IS_HIDDEN]);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/ShowBackupFiles")), !gnome_cmd_data.options.filter.file_types[GnomeCmdData::G_FILE_IS_BACKUP]);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_ui_manager_get_widget (uiManager, "/MainMenuBar/ViewMenu/HorizontalOrientation")), gnome_cmd_data.horizontal_orientation);

    g_signal_connect (gnome_cmd_con_list_get (), "list-changed", G_CALLBACK (on_con_list_list_changed), main_menu);

    gnome_cmd_main_menu_update_bookmarks (main_menu);
    gnome_cmd_main_menu_update_connections (main_menu);
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_main_menu_new ()
{
    return (GtkWidget *) g_object_new (GNOME_CMD_TYPE_MAIN_MENU, nullptr);
}


static void add_connection (GnomeCmdMainMenu *main_menu, GnomeCmdCon *con, const gchar *text, GdkPixbuf *pixbuf, GCallback func)
{
    GtkMenuShell *connections_menu = GTK_MENU_SHELL (gtk_menu_item_get_submenu (GTK_MENU_ITEM (main_menu->priv->connections_menu)));
    GtkWidget *item;

    item = add_menu_item (main_menu, connections_menu, text, nullptr, pixbuf, func, con);

    main_menu->priv->connections_menuitems = g_list_append (main_menu->priv->connections_menuitems, item);
}


void gnome_cmd_main_menu_update_connections (GnomeCmdMainMenu *main_menu)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_MENU (main_menu));

    GtkMenuShell *connections_menu = GTK_MENU_SHELL (gtk_menu_item_get_submenu (GTK_MENU_ITEM (main_menu->priv->connections_menu)));
    GnomeCmdConList *con_list = gnome_cmd_con_list_get ();
    GList *all_cons = gnome_cmd_con_list_get_all (con_list);
    // GList *remote_cons = gnome_cmd_con_list_get_all_remote (con_list);
    // GList *dev_cons = gnome_cmd_con_list_get_all_dev (con_list);

    // Remove all old items
    g_list_foreach (main_menu->priv->connections_menuitems, (GFunc) gtk_widget_destroy, nullptr);
    g_list_free (main_menu->priv->connections_menuitems);
    main_menu->priv->connections_menuitems = nullptr;

    // separator
    main_menu->priv->connections_menuitems = g_list_append (main_menu->priv->connections_menuitems, add_separator (connections_menu));

    // Add all open connections
    gint match_count = 0;

    for (GList *i = all_cons; i; i = i->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (i->data);
        if (!GNOME_CMD_IS_CON_REMOTE (con) || gnome_cmd_con_is_open (con))
        {
            add_connection (main_menu, con,
                            gnome_cmd_con_get_go_text (con),
                            gnome_cmd_con_get_go_pixbuf (con),
                            G_CALLBACK (connections_change));
            match_count++;
        }
    }

    // separator
    if (match_count)
        main_menu->priv->connections_menuitems = g_list_append (
            main_menu->priv->connections_menuitems,
            add_separator (connections_menu));

    // Add all open connections that are not permanent
    for (GList *i = all_cons; i; i = i->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (i->data);
        if (gnome_cmd_con_is_closeable (con) && gnome_cmd_con_is_open (con))
            add_connection (main_menu, con,
                            gnome_cmd_con_get_close_text (con),
                            gnome_cmd_con_get_close_pixbuf (con),
                            G_CALLBACK (connections_close));
    }
}


GtkWidget *get_gnome_cmd_main_menu_bar (GnomeCmdMainMenu *main_menu)
{
    g_return_val_if_fail (GNOME_CMD_IS_MAIN_MENU (main_menu), nullptr);

    return (main_menu->priv->mainMenuBar);
}

void gnome_cmd_main_menu_update_bookmarks (GnomeCmdMainMenu *main_menu)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_MENU (main_menu));

    // Remove all old bookmark menu items
    g_list_foreach (main_menu->priv->bookmark_menuitems, (GFunc) gtk_widget_destroy, nullptr);
    g_list_free (main_menu->priv->bookmark_menuitems);
    main_menu->priv->bookmark_menuitems = nullptr;

    // Remove all old group menu items
    g_list_foreach (main_menu->priv->group_menuitems, (GFunc) gtk_widget_destroy, nullptr);
    g_list_free (main_menu->priv->group_menuitems);
    main_menu->priv->group_menuitems = nullptr;

    // Add bookmark groups
    GList *cons = gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ());
    for (; cons; cons = cons->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (cons->data);
        GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (con);
        GtkMenuShell *bookmarks_menu = GTK_MENU_SHELL (gtk_menu_item_get_submenu (GTK_MENU_ITEM (main_menu->priv->bookmarks_menu)));
        if (group && group->bookmarks)
        {
            // separator
            main_menu->priv->bookmark_menuitems = g_list_append (main_menu->priv->bookmark_menuitems, add_separator (bookmarks_menu));
            // bookmark
            add_bookmark_group (main_menu, bookmarks_menu, group);
        }
    }
}


void gnome_cmd_main_menu_update_sens (GnomeCmdMainMenu *main_menu)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_MENU (main_menu));

    GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);

    gtk_widget_set_sensitive (main_menu->priv->menu_view_back, fs->can_back());
    gtk_widget_set_sensitive (main_menu->priv->menu_view_forward, fs->can_forward());
}


static void on_plugin_menu_activate (GtkMenuItem *item, PluginData *data)
{
    g_return_if_fail (data != nullptr);

    GnomeCmdState *state = main_win->get_state();
    gnome_cmd_plugin_update_main_menu_state (data->plugin, state);
}


void gnome_cmd_main_menu_add_plugin_menu (GnomeCmdMainMenu *main_menu, PluginData *pluginData)
{
    gtk_menu_shell_append (GTK_MENU_SHELL (gtk_menu_item_get_submenu (GTK_MENU_ITEM (main_menu->priv->plugins_menu))), pluginData->menu);

    g_signal_connect (pluginData->menu, "activate", G_CALLBACK (on_plugin_menu_activate), pluginData);
}
