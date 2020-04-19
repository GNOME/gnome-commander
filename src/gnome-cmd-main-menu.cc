/**
 * @file gnome-cmd-main-menu.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2020 Uwe Scholz\n
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

#include "../pixmaps/exec_wheel.xpm"

using namespace std;


/* These following types are slightly changed from the originals in the GnomeUI library
   We need special types because we neeed to place non-changeable shortcuts in the
   menus. Another difference is that we want only mouse-clicks in the menu to generate an
   action, keyboard shortcuts are caught by the different components by them self */

enum MenuType
{
    MENU_TYPE_END,        // No more items, use it at the end of an array
    MENU_TYPE_ITEM,       // Normal item, or radio item if it is inside a radioitems group
    MENU_TYPE_BASIC,
    MENU_TYPE_TOGGLEITEM, // Toggle (check box) item
    MENU_TYPE_RADIOITEMS, // Radio item group
    MENU_TYPE_SUBTREE,    // Item that defines a subtree/submenu
    MENU_TYPE_SEPARATOR   // Separator line (menus) or blank space (toolbars)
};


struct MenuData
{
    MenuType type;          // Type of item
    const gchar *label;     // The text to use for this menu-item
    const gchar *shortcut;  // The shortcut for this menu-item
    const gchar *tooltip;   // The tooltip of this menu-item
    gpointer moreinfo;      // For an item, toggleitem, this is a pointer to the function to call when the item is activated.
    gpointer user_data;     // Data pointer to pass to callbacks
    GnomeUIPixmapType pixmap_type;    // Type of pixmap for the item
    gconstpointer pixmap_info;      /* Pointer to the pixmap information:
                                     *
                                     * For GNOME_APP_PIXMAP_STOCK, a
                                     * pointer to the stock icon name.
                                     *
                                     * For GNOME_APP_PIXMAP_DATA, a
                                     * pointer to the inline xpm data.
                                     *
                                     * For GNOME_APP_PIXMAP_FILENAME, a
                                     * pointer to the filename string.
                                     */

    GtkWidget *widget;      // Filled in by gnome_app_create*, you can use this to tweak the widgets once they have been created
};

#define MENUTYPE_END { \
    MENU_TYPE_END, \
    nullptr, nullptr, nullptr, \
    nullptr, nullptr, \
    (GnomeUIPixmapType) 0, nullptr, \
    nullptr }

#define MENUTYPE_SEPARATOR { \
    MENU_TYPE_SEPARATOR, \
    nullptr, nullptr, nullptr, \
    nullptr, nullptr, \
    (GnomeUIPixmapType) 0, nullptr, \
    nullptr }


struct GnomeCmdMainMenuPrivate
{
    GtkWidget *file_menu;
    GtkWidget *edit_menu;
    GtkWidget *mark_menu;
    GtkWidget *view_menu;
    GtkWidget *options_menu;
    GtkWidget *connections_menu;
    GtkWidget *bookmarks_menu;
    GtkWidget *plugins_menu;
    GtkWidget *help_menu;

    GtkWidget *menu_view_back;
    GtkWidget *menu_view_forward;

    GList *connections_menuitems;

    GList *bookmark_menuitems;
    GList *group_menuitems;

    GList *view_menuitems;
};


static GtkMenuBarClass *parent_class = nullptr;


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


static GtkWidget *create_menu_item (GnomeCmdMainMenu *main_menu, GtkMenu *parent, MenuData *spec)
{
    GtkWidget *item=nullptr;
    GtkWidget *desc=nullptr;
    GtkWidget *shortcut=nullptr;
    GtkWidget *content = nullptr;

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (spec->type)
    {
        case MENU_TYPE_BASIC:
            item = gtk_menu_item_new ();
            desc = gtk_label_new_with_mnemonic (spec->label);
            g_object_ref (desc);
            gtk_widget_show (desc);

            gtk_container_add (GTK_CONTAINER (item), desc);
            break;

        case MENU_TYPE_ITEM:
            item = gtk_image_menu_item_new ();
            content = create_hbox (*main_win, FALSE, 12);

            desc = gtk_label_new_with_mnemonic (spec->label);
            g_object_ref (desc);
            gtk_widget_show (desc);
            gtk_box_pack_start (GTK_BOX (content), desc, FALSE, FALSE, 0);

            shortcut = create_label (*main_win, spec->shortcut);
            gtk_misc_set_alignment (GTK_MISC (shortcut), 1.0, 0.5);
            gtk_box_pack_start (GTK_BOX (content), shortcut, TRUE, TRUE, 0);

            if (spec->pixmap_type != 0 && spec->pixmap_info)
            {
                GtkWidget *pixmap = nullptr;
                pixmap = create_ui_pixmap (*main_win, spec->pixmap_type, spec->pixmap_info, GTK_ICON_SIZE_MENU);

                if (pixmap)
                {
                    gtk_widget_show (pixmap);
                    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), pixmap);
                }
            }
            if (spec->tooltip)
                gtk_widget_set_tooltip_text(item, spec->tooltip);
            gtk_container_add (GTK_CONTAINER (item), content);
            break;

        case MENU_TYPE_TOGGLEITEM:
            item = gtk_check_menu_item_new ();
            content = create_hbox (*main_win, FALSE, 12);

            desc = create_label (*main_win, spec->label);
            gtk_misc_set_alignment (GTK_MISC (desc), 0.0, 0.5);
            gtk_box_pack_start (GTK_BOX (content), desc, TRUE, TRUE, 0);

            shortcut = create_label (*main_win, spec->shortcut);
            gtk_misc_set_alignment (GTK_MISC (shortcut), 1.0, 0.5);
            gtk_box_pack_start (GTK_BOX (content), shortcut, TRUE, TRUE, 0);

            gtk_container_add (GTK_CONTAINER (item), content);
            g_signal_connect (item, "toggled", G_CALLBACK (spec->moreinfo), spec->user_data);
            break;

        case MENU_TYPE_SEPARATOR:
            item = gtk_menu_item_new ();
            gtk_widget_set_sensitive (item, FALSE);
            break;

        default:
            g_warning ("This MENU_TYPE is not implemented");
            return nullptr;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    gtk_widget_show (item);

    if (spec->type == MENU_TYPE_ITEM)
    {
        // Connect to the signal and set user data
        g_object_set_data (G_OBJECT (item), GNOMEUIINFO_KEY_UIDATA, spec->user_data);
        g_signal_connect (item, "activate", G_CALLBACK (spec->moreinfo), spec->user_data);
    }

    spec->widget = item;

    return item;
}


static GtkWidget *create_menu (GnomeCmdMainMenu *main_menu, MenuData *spec, MenuData *childs)
{
    GtkWidget *submenu = gtk_menu_new ();
    GtkWidget *menu_item = create_menu_item (main_menu, nullptr, spec);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), submenu);

    g_object_ref (menu_item);
    gtk_widget_show (menu_item);

    for (gint i=0; childs[i].type != MENU_TYPE_END; ++i)
    {
        GtkWidget *child = create_menu_item (main_menu, GTK_MENU (submenu), &childs[i]);
        gtk_menu_shell_append (GTK_MENU_SHELL (submenu), child);
    }

    return menu_item;
}


static GtkWidget *
add_menu_item (GnomeCmdMainMenu *main_menu,
               GtkMenuShell *menu,
               const gchar *text,
               const gchar *tooltip,
               GdkPixmap *pixmap,
               GdkBitmap *mask,
               GtkSignalFunc callback,
               gpointer user_data)
{
    g_return_val_if_fail (GTK_IS_MENU_SHELL (menu), nullptr);

    GtkWidget *item, *label;
    GtkWidget *pixmap_widget = nullptr;

    item = gtk_image_menu_item_new ();

    gtk_widget_set_tooltip_text (item, tooltip);

    if (pixmap && mask)
        pixmap_widget = gtk_pixmap_new (pixmap, mask);

    if (pixmap_widget)
    {
        gtk_widget_show (pixmap_widget);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), pixmap_widget);
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
        g_object_set_data (G_OBJECT (item), GNOMEUIINFO_KEY_UIDATA, user_data);
        g_signal_connect (item, "activate", callback, user_data);
    }

    return item;
}


static GtkWidget *add_separator (GnomeCmdMainMenu *main_menu, GtkMenuShell *menu)
{
    MenuData t = MENUTYPE_SEPARATOR;

    GtkWidget *child = create_menu_item (main_menu, GTK_MENU (menu), &t);
    gtk_menu_shell_append (menu, child);

    return child;
}


static void add_bookmark_menu_item (GnomeCmdMainMenu *main_menu, GtkMenuShell *menu, GnomeCmdBookmark *bookmark)
{
    GtkWidget *item;

    item = add_menu_item (main_menu, menu, bookmark->name, nullptr,
                          IMAGE_get_pixmap (PIXMAP_BOOKMARK), IMAGE_get_mask (PIXMAP_BOOKMARK),
                          GTK_SIGNAL_FUNC (on_bookmark_selected), bookmark);

    // Remember this bookmarks item-widget so that we can remove it later
    main_menu->priv->bookmark_menuitems = g_list_append (main_menu->priv->bookmark_menuitems, item);
}


static void add_bookmark_group (GnomeCmdMainMenu *main_menu, GtkMenuShell *menu, GnomeCmdBookmarkGroup *group)
{
    g_return_if_fail (GTK_IS_MENU_SHELL (menu));
    g_return_if_fail (group != nullptr);

    GnomeCmdPixmap *pixmap = gnome_cmd_con_get_go_pixmap (group->con);
    GtkWidget *item = add_menu_item (main_menu, menu, gnome_cmd_con_get_alias (group->con), nullptr,
                                     pixmap?pixmap->pixmap:nullptr, pixmap?pixmap->mask:nullptr,
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

static void destroy (GtkObject *object)
{
    GnomeCmdMainMenu *menu = GNOME_CMD_MAIN_MENU (object);

    g_free (menu->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != nullptr)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdMainMenuClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkMenuBarClass *) gtk_type_class (gtk_menu_bar_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdMainMenu *main_menu)
{
    MenuData files_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("Change _Owner/Group"), "", nullptr,
            (gpointer) file_chown, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Change Per_missions"), "", nullptr,
            (gpointer) file_chmod, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Advanced _Rename Tool"), "Ctrl+M", nullptr,
            (gpointer) file_advrename, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Create _Symbolic Link"), "Ctrl+Shift+F5", nullptr,
            (gpointer) file_create_symlink, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Properties…"), "Alt+ENTER", nullptr,
            (gpointer) file_properties, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PROPERTIES,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("_Search…"), "Alt+F7", nullptr,
            (gpointer) edit_search, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_FIND,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Quick Search…"), "", nullptr,
            (gpointer) edit_quick_search, nullptr,
            GNOME_APP_PIXMAP_NONE, 0,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Enable Filter…"), "", nullptr,
            (gpointer) edit_filter, nullptr,
            GNOME_APP_PIXMAP_NONE, 0,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("_Diff"), "", nullptr,
            (gpointer) file_diff, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("S_ynchronize Directories"), "", nullptr,
            (gpointer) file_sync_dirs, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("Start _GNOME Commander as root"), "", nullptr,
            (gpointer) command_root_mode, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_DIALOG_AUTHENTICATION,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("_Quit"), "Ctrl+Q", nullptr,
            (gpointer) file_exit, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_QUIT,
            nullptr
        },
        MENUTYPE_END
    };

    MenuData mark_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("_Select All"), "Ctrl++", nullptr,
            (gpointer) mark_select_all, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Unselect All"), "Ctrl+-", nullptr,
            (gpointer) mark_unselect_all, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Select with _Pattern"), "+", nullptr,
            (gpointer) mark_select_with_pattern, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Unselect with P_attern"), "-", nullptr,
            (gpointer) mark_unselect_with_pattern, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Select with same _Extension"), "", nullptr,
            (gpointer) mark_select_all_with_same_extension, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Unselect with same E_xtension"), "", nullptr,
            (gpointer) mark_unselect_all_with_same_extension, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Invert Selection"), "*", nullptr,
            (gpointer) mark_invert_selection, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Restore Selection"), "", nullptr,
            (gpointer) mark_restore_selection, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("_Compare Directories"), "Shift+F2", nullptr,
            (gpointer) mark_compare_directories, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        MENUTYPE_END
    };

    MenuData edit_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("Cu_t"), "Ctrl+X", nullptr,
            (gpointer) edit_cap_cut, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_CUT,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Copy"), "Ctrl+C", nullptr,
            (gpointer) edit_cap_copy, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_COPY,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Paste"), "Ctrl+V", nullptr,
            (gpointer) edit_cap_paste, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PASTE,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Delete"), "Delete", nullptr,
            (gpointer) file_delete, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_DELETE,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("Copy _File Names"), "", nullptr,
            (gpointer) edit_copy_fnames, nullptr,
            GNOME_APP_PIXMAP_NONE, 0,
            nullptr
        },
        MENUTYPE_END
    };

    MenuData view_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("_Back"), "Alt+Left", nullptr,
            (gpointer) view_back, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_GO_BACK,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Forward"), "Alt+Right", nullptr,
            (gpointer) view_forward, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_GO_FORWARD,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Refresh"), "Ctrl+R", nullptr,
            (gpointer) view_refresh, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_REFRESH,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("Open in New _Tab"), "Ctrl+T", nullptr,
            (gpointer) view_new_tab, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_OPEN,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Close Tab"), "Ctrl+W", nullptr,
            (gpointer) view_close_tab, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_CLOSE,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Close _All Tabs"), "Ctrl+Shift+W", nullptr,
            (gpointer) view_close_all_tabs, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_CLOSE,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_TOGGLEITEM, _("Show Toolbar"), "", nullptr,
            (gpointer) view_toolbar, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_TOGGLEITEM, _("Show Device Buttons"), "", nullptr,
            (gpointer) view_conbuttons, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_TOGGLEITEM, _("Show Device List"), "", nullptr,
            (gpointer) view_devlist, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_TOGGLEITEM, _("Show Command Line"), "", nullptr,
            (gpointer) view_cmdline, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_TOGGLEITEM, _("Show Buttonbar"), "", nullptr,
            (gpointer) view_buttonbar, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_TOGGLEITEM, _("Show Hidden Files"), "Ctrl+Shift+H", nullptr,
            (gpointer) view_hidden_files, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_TOGGLEITEM, _("Show Backup Files"), "", nullptr,
            (gpointer) view_backup_files, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("_Equal Panel Size"), "Ctrl+Shift+=", nullptr,
            (gpointer) view_equal_panes, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Maximize Panel Size"), "", nullptr,
            (gpointer) view_maximize_pane, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        {
            MENU_TYPE_TOGGLEITEM, _("Horizontal Orientation"), "", nullptr,
            (gpointer) view_horizontal_orientation, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        MENUTYPE_END
    };

    MenuData bookmarks_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("_Bookmark this Directory…"), "", nullptr,
            (gpointer) bookmarks_add_current, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ADD,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Manage Bookmarks…"), "Ctrl+D", nullptr,
            (gpointer) bookmarks_edit, nullptr,
            GNOME_APP_PIXMAP_NONE, nullptr,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        MENUTYPE_END
    };

    MenuData plugins_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("_Configure Plugins…"), "", nullptr,
            (gpointer) plugins_configure, nullptr,
            GNOME_APP_PIXMAP_DATA, exec_wheel_xpm,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        MENUTYPE_END
    };

    MenuData options_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("_Options…"), "Ctrl+O", nullptr,
            (gpointer) options_edit, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PREFERENCES,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Keyboard Shortcuts…"), "", nullptr,
            (gpointer) options_edit_shortcuts, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ITALIC,
            nullptr
        },
        MENUTYPE_END
    };

    MenuData connections_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("_Remote Server…"), "Ctrl+F", nullptr,
            (gpointer) connections_open, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_CONNECT,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("New Connection…"), "Ctrl+N", nullptr,
            (gpointer) connections_new, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_CONNECT,
            nullptr
        },
        MENUTYPE_END
    };

    MenuData help_menu_uiinfo[] =
    {
        {
            MENU_TYPE_ITEM, _("_Documentation"), "F1", nullptr,
            (gpointer) help_help, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_HELP,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("_Keyboard Shortcuts"), "", nullptr,
            (gpointer) help_keyboard, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ITALIC,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("GNOME Commander on the _Web"), "", nullptr,
            (gpointer) help_web, nullptr,
            GNOME_APP_PIXMAP_NONE, 0,
            nullptr
        },
        {
            MENU_TYPE_ITEM, _("Report a _Problem"), "", nullptr,
            (gpointer) help_problem, nullptr,
            GNOME_APP_PIXMAP_NONE, 0,
            nullptr
        },
        MENUTYPE_SEPARATOR,
        {
            MENU_TYPE_ITEM, _("_About"), "", nullptr,
            (gpointer) help_about, nullptr,
            GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ABOUT,
            nullptr
        },
        MENUTYPE_END
    };

    MenuData spec = { MENU_TYPE_BASIC, "", "", "",
                      nullptr, nullptr,
                      GNOME_APP_PIXMAP_NONE, nullptr,
                      nullptr };

    main_menu->priv = g_new (GnomeCmdMainMenuPrivate, 1);
    main_menu->priv->bookmark_menuitems = nullptr;
    main_menu->priv->connections_menuitems = nullptr;
    main_menu->priv->group_menuitems = nullptr;
    main_menu->priv->view_menuitems = nullptr;

    //gtk_menu_bar_set_shadow_type (GTK_MENU_BAR (main_menu), GTK_SHADOW_NONE);

    spec.label = _("_File");
    main_menu->priv->file_menu = create_menu (main_menu, &spec, files_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->file_menu);

    spec.label = _("_Edit");
    main_menu->priv->edit_menu = create_menu (main_menu, &spec, edit_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->edit_menu);

    spec.label = _("_Mark");
    main_menu->priv->mark_menu = create_menu (main_menu, &spec, mark_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->mark_menu);

    spec.label = _("_View");
    main_menu->priv->view_menu = create_menu (main_menu, &spec, view_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->view_menu);

    spec.label = _("_Settings");
    main_menu->priv->options_menu = create_menu (main_menu, &spec, options_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->options_menu);

    spec.label = _("_Connections");
    main_menu->priv->connections_menu = create_menu (main_menu, &spec, connections_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->connections_menu);

    spec.label = _("_Bookmarks");
    main_menu->priv->bookmarks_menu = create_menu (main_menu, &spec, bookmarks_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->bookmarks_menu);

    spec.label = _("_Plugins");
    main_menu->priv->plugins_menu = create_menu (main_menu, &spec, plugins_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->plugins_menu);

    spec.label = _("_Help");
    main_menu->priv->help_menu = create_menu (main_menu, &spec, help_menu_uiinfo);
    gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), main_menu->priv->help_menu);

    main_menu->priv->menu_view_back = view_menu_uiinfo[0].widget;
    main_menu->priv->menu_view_forward = view_menu_uiinfo[1].widget;

    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (view_menu_uiinfo[8].widget), gnome_cmd_data.show_toolbar);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (view_menu_uiinfo[9].widget), gnome_cmd_data.show_devbuttons);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (view_menu_uiinfo[10].widget), gnome_cmd_data.show_devlist);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (view_menu_uiinfo[11].widget), gnome_cmd_data.cmdline_visibility);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (view_menu_uiinfo[12].widget), gnome_cmd_data.buttonbar_visibility);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (view_menu_uiinfo[14].widget), !gnome_cmd_data.options.filter.hidden);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (view_menu_uiinfo[15].widget), !gnome_cmd_data.options.filter.backup);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (view_menu_uiinfo[19].widget), gnome_cmd_data.horizontal_orientation);

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


GtkType gnome_cmd_main_menu_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            (gchar*) "GnomeCmdMainMenu",
            sizeof (GnomeCmdMainMenu),
            sizeof (GnomeCmdMainMenuClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ nullptr,
            /* reserved_2 */ nullptr,
            (GtkClassInitFunc) nullptr
        };

        dlg_type = gtk_type_unique (gtk_menu_bar_get_type (), &dlg_info);
    }
    return dlg_type;
}


static void add_connection (GnomeCmdMainMenu *main_menu, GnomeCmdCon *con, const gchar *text, GnomeCmdPixmap *pixmap, GtkSignalFunc func)
{
    GtkMenuShell *connections_menu =GTK_MENU_SHELL (GTK_MENU_ITEM (main_menu->priv->connections_menu)->submenu);
    GtkWidget *item;

    item = add_menu_item (main_menu, connections_menu, text, nullptr, pixmap?pixmap->pixmap:nullptr, pixmap?pixmap->mask:nullptr, func, con);

    main_menu->priv->connections_menuitems = g_list_append (main_menu->priv->connections_menuitems, item);
}


void gnome_cmd_main_menu_update_connections (GnomeCmdMainMenu *main_menu)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_MENU (main_menu));

    GtkMenuShell *connections_menu = GTK_MENU_SHELL (GTK_MENU_ITEM (main_menu->priv->connections_menu)->submenu);
    GnomeCmdConList *con_list = gnome_cmd_con_list_get ();
    GList *all_cons = gnome_cmd_con_list_get_all (con_list);
    // GList *remote_cons = gnome_cmd_con_list_get_all_remote (con_list);
    // GList *dev_cons = gnome_cmd_con_list_get_all_dev (con_list);

    // Remove all old items
    g_list_foreach (main_menu->priv->connections_menuitems, (GFunc) gtk_widget_destroy, nullptr);
    g_list_free (main_menu->priv->connections_menuitems);
    main_menu->priv->connections_menuitems = nullptr;

    // separator
    main_menu->priv->connections_menuitems = g_list_append (main_menu->priv->connections_menuitems, add_separator (main_menu, connections_menu));

    // Add all open connections
    gint match_count = 0;

    for (GList *i = all_cons; i; i = i->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (i->data);
        if (!GNOME_CMD_IS_CON_REMOTE (con) || gnome_cmd_con_is_open (con))
        {
            add_connection (main_menu, con,
                            gnome_cmd_con_get_go_text (con),
                            gnome_cmd_con_get_go_pixmap (con),
                            GTK_SIGNAL_FUNC (connections_change));
            match_count++;
        }
    }

    // separator
    if (match_count)
        main_menu->priv->connections_menuitems = g_list_append (
            main_menu->priv->connections_menuitems,
            add_separator (main_menu, connections_menu));

    // Add all open connections that are not permanent
    for (GList *i = all_cons; i; i = i->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (i->data);
        if (gnome_cmd_con_is_closeable (con) && gnome_cmd_con_is_open (con))
            add_connection (main_menu, con,
                            gnome_cmd_con_get_close_text (con),
                            gnome_cmd_con_get_close_pixmap (con),
                            GTK_SIGNAL_FUNC (connections_close));
    }
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
        GtkMenuShell *bookmarks_menu = GTK_MENU_SHELL (GTK_MENU_ITEM (main_menu->priv->bookmarks_menu)->submenu);
        if (group && group->bookmarks)
            add_bookmark_group (main_menu, bookmarks_menu, group);
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


void gnome_cmd_main_menu_add_plugin_menu (GnomeCmdMainMenu *main_menu, PluginData *data)
{
    gtk_menu_shell_append (GTK_MENU_SHELL (GTK_MENU_ITEM (main_menu->priv->plugins_menu)->submenu), data->menu);

    g_signal_connect (data->menu, "activate", G_CALLBACK (on_plugin_menu_activate), data);
}
