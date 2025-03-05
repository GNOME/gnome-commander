/**
 * @file gnome-cmd-file-list.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 * @copyright (C) 2024 Andrey Kutejko\n
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
#include <glib-object.h>
#include <algorithm>
#include <numeric>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-list-actions.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-plain-path.h"
#include "utils.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-xfer.h"
#include "imageloader.h"
#include "gnome-cmd-file-collection.h"
#include "dialogs/gnome-cmd-delete-dialog.h"
#include "dialogs/gnome-cmd-rename-dialog.h"
#include "dialogs/gnome-cmd-file-props-dialog.h"
#include "text-utils.h"
#include "widget-factory.h"

using namespace std;


/* Controls if file-uris should be escaped for local files when drag-N-dropping
 * Setting this seems be more portable when dropping on old file-managers as gmc etc.
 */
#define UNESCAPE_LOCAL_FILES

/* The time (in ms) it takes from that the right mouse button is clicked on a file until
 * the popup menu appears when the right btn is used to select files.
 */
#define POPUP_TIMEOUT 750


#define FL_PBAR_MAX 50


enum
{
    FILE_CLICKED,        // A file in the list was clicked
    FILE_RELEASED,       // A file in the list has been clicked and mouse button has been released
    LIST_CLICKED,        // The file list widget was clicked
    FILES_CHANGED,       // The visible content of the file list has changed (files have been: selected, created, deleted or modified)
    DIR_CHANGED,         // The current directory has been changed
    CON_CHANGED,         // The current connection has been changed
    RESIZE_COLUMN,       // Column's width was changed
    LAST_SIGNAL
};


static const char *drag_types [] =
{
    TARGET_URI_LIST_TYPE,
    TARGET_TEXT_PLAIN_TYPE,
    TARGET_URL_TYPE
};

static const char *drop_types [] =
{
    TARGET_URI_LIST_TYPE,
    TARGET_URL_TYPE
};


static guint signals[LAST_SIGNAL] = { 0 };


typedef GtkSorter * (*GnomeCmdSorterFactory) (gboolean case_sensitive,
                                              gboolean symbolic_links_as_regular_files,
                                              GtkSortType sort_type);


struct GnomeCmdFileListColumn
{
    guint id;
    const gchar *title;
    float alignment;
    GtkSortType default_sort_direction;
    GnomeCmdSorterFactory sorter_factory;
};


struct GnomeCmdColorTheme;
enum GnomeCmdColorThemeItem {
    SELECTION_FOREGROUND = 0,
    SELECTION_BACKGROUND,
    NORMAL_FOREGROUND,
    NORMAL_BACKGROUND,
    CURSOR_FOREGROUND,
    CURSOR_RBACKGROUND,
    ALTERNATE_FOREGROUND,
    ALTERNATE_BACKGROUND,
};
extern "C" GnomeCmdColorTheme *gnome_cmd_get_current_theme();
extern "C" void gnome_cmd_theme_free(GnomeCmdColorTheme *);
extern "C" GdkRGBA *gnome_cmd_color_get_color(GnomeCmdColorTheme *, GnomeCmdColorThemeItem);


struct LsColorsPalette;
extern "C" LsColorsPalette *gnome_cmd_get_palette();
extern "C" void gnome_cmd_palette_free(LsColorsPalette *palette);
extern "C" GdkRGBA *gnome_cmd_palette_get_color(LsColorsPalette *palette, gint plane, gint palette_color);
extern "C" void ls_colors_get_r(GFileInfo *file_info, gint *fg, gint *bg);


static void cell_data (GtkTreeViewColumn *tree_column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *tree_model,
                       GtkTreeIter *iter,
                       gpointer data);
extern "C" GtkSorter *gnome_cmd_sort_by_name (gboolean case_sensitive, gboolean symbolic_links_as_regular_files, GtkSortType sort_type);
extern "C" GtkSorter *gnome_cmd_sort_by_ext (gboolean case_sensitive, gboolean symbolic_links_as_regular_files, GtkSortType sort_type);
extern "C" GtkSorter *gnome_cmd_sort_by_dir (gboolean case_sensitive, gboolean symbolic_links_as_regular_files, GtkSortType sort_type);
extern "C" GtkSorter *gnome_cmd_sort_by_size (gboolean case_sensitive, gboolean symbolic_links_as_regular_files, GtkSortType sort_type);
extern "C" GtkSorter *gnome_cmd_sort_by_date (gboolean case_sensitive, gboolean symbolic_links_as_regular_files, GtkSortType sort_type);
extern "C" GtkSorter *gnome_cmd_sort_by_perm (gboolean case_sensitive, gboolean symbolic_links_as_regular_files, GtkSortType sort_type);
extern "C" GtkSorter *gnome_cmd_sort_by_owner (gboolean case_sensitive, gboolean symbolic_links_as_regular_files, GtkSortType sort_type);
extern "C" GtkSorter *gnome_cmd_sort_by_group (gboolean case_sensitive, gboolean symbolic_links_as_regular_files, GtkSortType sort_type);
static void on_column_clicked (GtkTreeViewColumn *column, GnomeCmdFileList *fl);
static void on_column_resized (GtkTreeViewColumn *column, GParamSpec *pspec, GnomeCmdFileList *fl);
static void on_cursor_change(GtkTreeView *tree, GnomeCmdFileList *fl);
static gboolean gnome_cmd_file_list_key_pressed (GtkEventControllerKey* self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);


static GnomeCmdFileListColumn file_list_column[GnomeCmdFileList::NUM_COLUMNS] =
{{GnomeCmdFileList::COLUMN_ICON, nullptr, 0.5, GTK_SORT_ASCENDING, nullptr},
 {GnomeCmdFileList::COLUMN_NAME, N_("name"), 0.0, GTK_SORT_ASCENDING, gnome_cmd_sort_by_name},
 {GnomeCmdFileList::COLUMN_EXT, N_("ext"), 0.0, GTK_SORT_ASCENDING, gnome_cmd_sort_by_ext},
 {GnomeCmdFileList::COLUMN_DIR, N_("dir"), 0.0, GTK_SORT_ASCENDING, gnome_cmd_sort_by_dir},
 {GnomeCmdFileList::COLUMN_SIZE, N_("size"), 1.0, GTK_SORT_DESCENDING, gnome_cmd_sort_by_size},
 {GnomeCmdFileList::COLUMN_DATE, N_("date"), 0.0, GTK_SORT_DESCENDING, gnome_cmd_sort_by_date},
 {GnomeCmdFileList::COLUMN_PERM, N_("perm"), 0.0, GTK_SORT_ASCENDING, gnome_cmd_sort_by_perm},
 {GnomeCmdFileList::COLUMN_OWNER, N_("uid"), 0.0, GTK_SORT_ASCENDING, gnome_cmd_sort_by_owner},
 {GnomeCmdFileList::COLUMN_GROUP, N_("gid"), 0.0, GTK_SORT_ASCENDING, gnome_cmd_sort_by_group}};


enum DataColumns {
    DATA_COLUMN_FILE = GnomeCmdFileList::NUM_COLUMNS,
    DATA_COLUMN_ICON_NAME,
    DATA_COLUMN_SELECTED,
    NUM_DATA_COLUMNS,
};

const gint FILE_COLUMN = GnomeCmdFileList::NUM_COLUMNS;


struct GnomeCmdFileListClass
{
    GtkWidgetClass parent_class;

    void (* file_clicked)        (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event);
    void (* file_released)       (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event);
    void (* list_clicked)        (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event);
    void (* files_changed)       (GnomeCmdFileList *fl);
    void (* dir_changed)         (GnomeCmdFileList *fl, GnomeCmdDir *dir);
    void (* con_changed)         (GnomeCmdFileList *fl, GnomeCmdCon *con);
    void (* resize_column)       (GnomeCmdFileList *fl, guint index, GtkTreeViewColumn *col);
};


struct GnomeCmdFileList::Private
{
    GtkTreeView *view;
    GtkListStore *store;

    GtkTreeViewColumn *columns[NUM_COLUMNS];

    gchar *base_dir;

    GtkSorter *sorter;
    gint current_col;
    GtkSortType sort_raising[NUM_COLUMNS];
    gint column_resizing;
    GnomeCmdColorTheme *color_theme;
    LsColorsPalette *ls_palette;

    gboolean shift_down;
    GtkTreeIterPtr shift_down_row;
    gint shift_down_key;
    gchar *focus_later;

    // dropping files
    GList *dropping_files;
    GnomeCmdDir *dropping_to;

    explicit Private(GnomeCmdFileList *fl);
    ~Private();

    static void on_dnd_popup_menu_copy(GtkButton *item, GnomeCmdFileList *fl);
    static void on_dnd_popup_menu_move(GtkButton *item, GnomeCmdFileList *fl);
    static void on_dnd_popup_menu_link(GtkButton *item, GnomeCmdFileList *fl);
};


struct CellDataClosure
{
    gsize column_index;
    GnomeCmdFileList *fl;
};


GnomeCmdFileList::Private::Private(GnomeCmdFileList *fl)
    : shift_down_row (nullptr, &gtk_tree_iter_free)
{
    view = GTK_TREE_VIEW (gtk_tree_view_new ());
    gtk_tree_view_set_enable_search (view, FALSE);

    GtkWidget *scrolled_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand (scrolled_window, TRUE);
    gtk_widget_set_vexpand (scrolled_window, TRUE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), GTK_WIDGET (view));
    gtk_widget_set_parent (scrolled_window, *fl);

    base_dir = nullptr;

    column_resizing = 0;
    color_theme = nullptr;
    ls_palette = nullptr;

    focus_later = nullptr;
    shift_down = FALSE;
    shift_down_row = nullptr;
    shift_down_key = 0;

    dropping_files = nullptr;
    dropping_to = nullptr;

    memset(sort_raising, GTK_SORT_ASCENDING, sizeof(sort_raising));

    store = gtk_list_store_new (NUM_DATA_COLUMNS,
        G_TYPE_ICON,                   // COLUMN_ICON
        G_TYPE_STRING,                 // COLUMN_NAME
        G_TYPE_STRING,                 // COLUMN_EXT
        G_TYPE_STRING,                 // COLUMN_DIR
        G_TYPE_STRING,                 // COLUMN_SIZE
        G_TYPE_STRING,                 // COLUMN_DATE
        G_TYPE_STRING,                 // COLUMN_PERM
        G_TYPE_STRING,                 // COLUMN_OWNER
        G_TYPE_STRING,                 // COLUMN_GROUP
        GNOME_CMD_TYPE_FILE,           // DATA_COLUMN_FILE
        G_TYPE_STRING,                 // DATA_COLUMN_ICON_NAME
        G_TYPE_BOOLEAN                 // DATA_COLUMN_SELECTED
    );

    for (gsize i=0; i<NUM_COLUMNS; i++)
    {
        columns[i] = gtk_tree_view_column_new ();
        if (file_list_column[i].title != nullptr)
            gtk_tree_view_column_set_title (columns[i], _(file_list_column[i].title));
        gtk_tree_view_column_set_sizing (columns[i], GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_resizable (columns[i], TRUE);
        gtk_tree_view_column_set_fixed_width (columns[i], gnome_cmd_data.fs_col_width[i]);

        gtk_tree_view_insert_column (view, columns[i], i);

        CellDataClosure *cell_data_col = g_new0 (CellDataClosure, 1);
        cell_data_col->column_index = i;
        cell_data_col->fl = fl;

        GtkCellRenderer *renderer;
        if (i == GnomeCmdFileList::COLUMN_ICON)
        {
            gtk_tree_view_column_set_clickable (columns[i], FALSE);

            renderer = gtk_cell_renderer_pixbuf_new();
            gtk_tree_view_column_pack_start (columns[i], renderer, TRUE);

            gtk_tree_view_column_set_cell_data_func (columns[i], renderer, cell_data, cell_data_col, g_free);

            renderer = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start (columns[i], renderer, TRUE);

            CellDataClosure *cell_data_icon = g_new0 (CellDataClosure, 1);
            cell_data_icon->column_index = DATA_COLUMN_ICON_NAME;
            cell_data_icon->fl = fl;
            gtk_tree_view_column_set_cell_data_func (columns[i], renderer, cell_data, cell_data_icon, g_free);
        }
        else
        {
            gtk_tree_view_column_set_clickable (columns[i], TRUE);
            // gtk_tree_view_column_set_sort_column_id (columns[i], i);
            gtk_tree_view_column_set_sort_indicator (columns[i], TRUE);

            renderer = gtk_cell_renderer_text_new ();
            g_object_set (renderer, "xalign", file_list_column[i].alignment, NULL);
            gtk_tree_view_column_pack_start (columns[i], renderer, TRUE);

            gtk_tree_view_column_set_cell_data_func (columns[i], renderer, cell_data, cell_data_col, g_free);
        }
    }

    for (gsize i=0; i<NUM_COLUMNS; i++)
    {
        g_signal_connect (columns[i], "clicked", G_CALLBACK (on_column_clicked), fl);
        g_signal_connect (columns[i], "notify::width", G_CALLBACK (on_column_resized), fl);
    }
}


static void paint_cell_with_ls_colors (GtkCellRenderer *cell, GnomeCmdFile *file, bool has_foreground, LsColorsPalette *ls_palette)
{
    if (!gnome_cmd_data.options.use_ls_colors)
        return;

    gint fg;
    gint bg;
    ls_colors_get_r(file->get_file_info(), &fg, &bg);

    if (has_foreground && fg >= 0)
        if (auto color = gnome_cmd_palette_get_color(ls_palette, 0 /*FG*/, fg))
            g_object_set (G_OBJECT (cell),
                "foreground-rgba", color,
                "foreground-set", TRUE,
                nullptr);

    if (bg >= 0)
        if (auto color = gnome_cmd_palette_get_color(ls_palette, 1 /*BG*/, bg))
            g_object_set (G_OBJECT (cell),
                "cell-background-rgba", color,
                "cell-background-set", TRUE,
                nullptr);
}


static void cell_data (GtkTreeViewColumn *column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       gpointer data)
{
    CellDataClosure *cell_data = static_cast<CellDataClosure *>(data);

    gboolean selected = false;
    GnomeCmdFile *file = nullptr;
    gpointer value;
    gtk_tree_model_get (model, iter,
        DATA_COLUMN_FILE, &file,
        DATA_COLUMN_SELECTED, &selected,
        cell_data->column_index, &value,
        -1);

    bool has_foreground;
    if (cell_data->column_index == GnomeCmdFileList::COLUMN_ICON)
    {
        if (gnome_cmd_data.options.layout == GNOME_CMD_LAYOUT_TEXT)
            g_object_set (G_OBJECT (cell), "gicon", NULL, nullptr);
        else
            g_object_set (G_OBJECT (cell), "gicon", value, nullptr);
        has_foreground = false;
    }
    else if (cell_data->column_index == DATA_COLUMN_ICON_NAME)
    {
        if (gnome_cmd_data.options.layout == GNOME_CMD_LAYOUT_TEXT)
            g_object_set (G_OBJECT (cell), "text", value, nullptr);
        else
            g_object_set (G_OBJECT (cell), "text", NULL, nullptr);
        has_foreground = false;
    }
    else
    {
        g_object_set (G_OBJECT (cell), "text", value, nullptr);
        has_foreground = true;
    }

    auto color_theme = cell_data->fl->priv->color_theme;
    if (selected)
    {
        if (color_theme != nullptr)
        {
            if (has_foreground)
                g_object_set (G_OBJECT (cell),
                    "foreground-rgba", gnome_cmd_color_get_color (color_theme, SELECTION_FOREGROUND),
                    "foreground-set", TRUE,
                    nullptr);
            g_object_set (G_OBJECT (cell),
                "cell-background-rgba", gnome_cmd_color_get_color (color_theme, SELECTION_BACKGROUND),
                "cell-background-set", TRUE,
                nullptr);
        }
        else
        {
            // TODO: Consider better ways to highlight selected files
            g_object_set (G_OBJECT (cell),
                "weight-rgba", PANGO_WEIGHT_BOLD,
                "weight-set", TRUE,
                nullptr);
        }
    }
    else
    {
        if (color_theme != nullptr)
        {
            if (has_foreground)
                g_object_set (G_OBJECT (cell),
                    "foreground-rgba", gnome_cmd_color_get_color (color_theme, NORMAL_FOREGROUND),
                    "foreground-set", TRUE,
                    nullptr);

            g_object_set (G_OBJECT (cell),
                "cell-background-rgba", gnome_cmd_color_get_color (color_theme, NORMAL_BACKGROUND),
                "cell-background-set", TRUE,
                nullptr);
        }

        paint_cell_with_ls_colors (cell, file, has_foreground, cell_data->fl->priv->ls_palette);
    }
}


static GtkPopover *create_dnd_popup (GnomeCmdFileList *fl, GList *gFileGlist, GnomeCmdDir* to)
{
    g_clear_list (&fl->priv->dropping_files, g_object_unref);
    fl->priv->dropping_files = gFileGlist;

    fl->priv->dropping_to = to;

    GMenu *menu = g_menu_new ();

    GMenu *section = g_menu_new ();
    {
        GMenuItem *item = g_menu_item_new (_("_Copy here"), NULL);
        g_menu_item_set_action_and_target (item, "fl.drop-files", "i", GnomeCmdFileList::DndMode::COPY);
        g_menu_append_item (section, item);
    }
    {
        GMenuItem *item = g_menu_item_new (_("_Move here"), NULL);
        g_menu_item_set_action_and_target (item, "fl.drop-files", "i", GnomeCmdFileList::DndMode::MOVE);
        g_menu_append_item (section, item);
    }
    {
        GMenuItem *item = g_menu_item_new (_("_Link here"), NULL);
        g_menu_item_set_action_and_target (item, "fl.drop-files", "i", GnomeCmdFileList::DndMode::LINK);
        g_menu_append_item (section, item);
    }
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (section));
    g_menu_append (menu,  _("C_ancel"), "fl.drop-files-cancel");

    GtkWidget *dnd_popup = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (dnd_popup, GTK_WIDGET (fl));

    return GTK_POPOVER (dnd_popup);
}


GnomeCmdFileList::Private::~Private()
{
    gnome_cmd_theme_free (color_theme);
    gnome_cmd_palette_free(ls_palette);
    g_clear_list (&dropping_files, g_object_unref);
    dropping_to = nullptr;
}


static void gnome_cmd_file_list_drop_files (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto fl = GNOME_CMD_FILE_LIST (user_data);

    auto mode = static_cast<GnomeCmdFileList::DndMode>(g_variant_get_int32 (parameter));

    auto gFileGlist = fl->priv->dropping_files;
    fl->priv->dropping_files = nullptr;

    auto to = fl->priv->dropping_to;
    fl->priv->dropping_to = nullptr;

    fl->drop_files(mode, G_FILE_COPY_NONE, gFileGlist, to);
}


static void gnome_cmd_file_list_drop_files_cancel (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto fl = GNOME_CMD_FILE_LIST (user_data);

    g_clear_list (&fl->priv->dropping_files, g_object_unref);
    fl->priv->dropping_to = nullptr;
}


GnomeCmdFileList::GnomeCmdFileList(ColumnID sort_col, GtkSortType sort_order)
{
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
    priv->current_col = sort_col;
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    priv->sort_raising[sort_col] = sort_order;
    priv->sorter = file_list_column[sort_col].sorter_factory (
        gnome_cmd_data.options.case_sens_sort,
        gnome_cmd_data.options.symbolic_links_as_regular_files,
        sort_order);
    priv->color_theme = gnome_cmd_get_current_theme();
    priv->ls_palette = gnome_cmd_get_palette();

    gtk_widget_add_css_class (GTK_WIDGET (priv->view), "gnome-cmd-file-list");

    GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    gtk_tree_view_set_model (priv->view, GTK_TREE_MODEL (priv->store));
}


inline gchar *strip_extension (const gchar *fname)
{
    gchar *s = g_strdup (fname);
    gchar *p = strrchr(s,'.');
    if (p && p != s)
        *p = '\0';
    return s;
}


static void set_model_row(GnomeCmdFileList *fl, GtkTreeIter *iter, GnomeCmdFile *f, bool tree_size)
{
    gchar *dpath;
    gchar *fname;
    gchar *fext;

    GtkListStore *model = fl->priv->store;

    // If the user wants a character instead of icon for filetype set it now
    if (gnome_cmd_data.options.layout == GNOME_CMD_LAYOUT_TEXT)
        gtk_list_store_set (model, iter, DATA_COLUMN_ICON_NAME, (gchar *) f->get_type_string(), -1);

    // Prepare the strings to show
    gchar *t1 = f->GetPathStringThroughParent();
    gchar *t2 = g_path_get_dirname (t1);
    dpath = get_utf8 (t2);
    g_free (t1);
    g_free (t2);

    if (gnome_cmd_data.options.ext_disp_mode == GNOME_CMD_EXT_DISP_STRIPPED
        && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_REGULAR)
    {
        fname = strip_extension (f->get_name());
    }
    else
    {
        fname = g_strdup(f->get_name());
    }

    if (fl->priv->base_dir != nullptr)
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_DIR, g_strconcat(get_utf8("."), dpath + strlen(fl->priv->base_dir), nullptr), -1);
    else
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_DIR, dpath, -1);

    if (gnome_cmd_data.options.ext_disp_mode != GNOME_CMD_EXT_DISP_WITH_FNAME)
        fext = get_utf8 (f->get_extension());
    else
        fext = nullptr;

    //Set other file information
    gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_NAME, fname, -1);
    gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_EXT, fext, -1);

    gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_SIZE, tree_size ? (gchar *) f->get_tree_size_as_str() : (gchar *) f->get_size(), -1);

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY || !f->is_dotdot)
    {
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_DATE, (gchar *) f->get_mdate(FALSE), -1);
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_PERM, (gchar *) f->get_perm(), -1);
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_OWNER, (gchar *) f->get_owner(), -1);
        gtk_list_store_set (model, iter, GnomeCmdFileList::COLUMN_GROUP, (gchar *) f->get_group(), -1);
    }

    gtk_list_store_set (model, iter,
        DATA_COLUMN_FILE, f,
        DATA_COLUMN_SELECTED, FALSE,
        -1);

    g_free (dpath);
    g_free (fname);
    g_free (fext);
}


G_DEFINE_TYPE (GnomeCmdFileList, gnome_cmd_file_list, GTK_TYPE_WIDGET)


// given a GnomeFileList, returns the upper-left corner of the selected file
static void get_focus_row_coordinates (GnomeCmdFileList *fl, gint &x, gint &y, gint &width, gint &height)
{
    GtkTreePath *path;
    GdkRectangle rect_name;
    GdkRectangle rect_ext;

    gtk_tree_view_get_cursor (fl->priv->view, &path, nullptr);
    gtk_tree_view_get_cell_area (fl->priv->view, path, fl->priv->columns[GnomeCmdFileList::COLUMN_NAME], &rect_name);
    gtk_tree_view_get_cell_area (fl->priv->view, path, fl->priv->columns[GnomeCmdFileList::COLUMN_EXT], &rect_ext);
    gtk_tree_path_free (path);

    gtk_tree_view_convert_bin_window_to_widget_coords (fl->priv->view, rect_name.x, rect_name.y, &x, &y);

    width = rect_name.width;
    height = rect_name.height;
    if (gnome_cmd_data.options.ext_disp_mode != GNOME_CMD_EXT_DISP_BOTH)
        width += rect_ext.width;
}


void GnomeCmdFileList::focus_file_at_row (GtkTreeIter *row)
{
    GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), row);
    gtk_tree_view_set_cursor (priv->view, path, nullptr, false);
    gtk_tree_path_free (path);
}


guint GnomeCmdFileList::size()
{
    return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), nullptr);
}


void GnomeCmdFileList::toggle_file(GtkTreeIter *iter)
{
    gboolean selected = FALSE;
    GnomeCmdFile *file = nullptr;
    gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter,
        DATA_COLUMN_FILE, &file,
        DATA_COLUMN_SELECTED, &selected,
        -1);

    if (file->is_dotdot)
        return;

    selected = !selected;
    gtk_list_store_set (priv->store, iter, DATA_COLUMN_SELECTED, selected, -1);

    g_signal_emit (this, signals[FILES_CHANGED], 0);
}


static gint iter_compare(GtkListStore *store, GtkTreeIter *iter1, GtkTreeIter *iter2)
{
    GtkTreePath *path1 = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter1);
    GtkTreePath *path2 = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter2);
    gint result = gtk_tree_path_compare (path1, path2);
    gtk_tree_path_free (path1);
    gtk_tree_path_free (path2);
    return result;
}


inline void select_file_range (GnomeCmdFileList *fl, GtkTreeIter *start_row, GtkTreeIter *end_row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    int state = 0;
    fl->traverse_files ([fl, start_row, end_row, &state](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        bool matches = iter_compare(store, iter, start_row) == 0 || iter_compare(store, iter, end_row) == 0;
        if (state == 0)
        {
            if (matches)
            {
                state = 1;
                if (!file->is_dotdot)
                    fl->select_iter(iter);
            }
        }
        else
        {
            if (!file->is_dotdot)
                fl->select_iter(iter);
            if (matches)
                return GnomeCmdFileList::TRAVERSE_BREAK;
        }
        return GnomeCmdFileList::TRAVERSE_CONTINUE;
    });

    g_signal_emit (fl, signals[FILES_CHANGED], 0);
}


inline void toggle_file_range (GnomeCmdFileList *fl, GtkTreeIter *start_row, GtkTreeIter *end_row, bool closed_end)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    int state = 0;
    fl->traverse_files ([fl, start_row, end_row, &state, closed_end](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        bool matches = iter_compare(store, iter, start_row) == 0 || iter_compare(store, iter, end_row) == 0;
        if (state == 0)
        {
            if (matches)
            {
                fl->toggle_file (iter);
                state = 1;
            }
        }
        else if (state == 1)
        {
            if (matches)
            {
                if (closed_end)
                {
                    fl->toggle_file (iter);
                }
                return GnomeCmdFileList::TRAVERSE_BREAK;
            }
            else
            {
                fl->toggle_file (iter);
            }
        }
        return GnomeCmdFileList::TRAVERSE_CONTINUE;
    });
}


extern "C" void toggle_files_with_same_extension (GnomeCmdFileList *fl, gboolean select);


static void update_column_sort_arrows (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    for (gint i=GnomeCmdFileList::COLUMN_NAME; i<GnomeCmdFileList::NUM_COLUMNS; i++)
    {
        if (i != fl->priv->current_col)
        {
            gtk_tree_view_column_set_sort_indicator (fl->priv->columns[i], FALSE);
        }
        else if (fl->priv->sort_raising[i])
        {
            gtk_tree_view_column_set_sort_indicator (fl->priv->columns[i], TRUE);
            gtk_tree_view_column_set_sort_order (fl->priv->columns[i], GTK_SORT_ASCENDING);
        }
        else
        {
            gtk_tree_view_column_set_sort_indicator (fl->priv->columns[i], TRUE);
            gtk_tree_view_column_set_sort_order (fl->priv->columns[i], GTK_SORT_DESCENDING);
        }
    }
}


/******************************************************
 * DnD functions
 **/

static GString *build_selected_file_list (GnomeCmdFileList *fl)
{
    GString *list = g_string_new(nullptr);
    fl->traverse_files ([fl, list](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (fl->is_selected_iter(iter))
        {
            auto uriString = file->get_uri_str();
            g_string_append (list, uriString);
            g_string_append (list, "\r\n");
        }
        return GnomeCmdFileList::TRAVERSE_CONTINUE;
    });
    if (list->len == 0)
    {
        auto file = fl->get_selected_file();
        auto uriString = file->get_uri_str();
        g_string_append (list, uriString);
    }
    return list;
}


extern "C" GMenu *gnome_cmd_file_popmenu_new (GnomeCmdFileList *fl);

static void show_file_popup (GnomeCmdFileList *fl, GdkRectangle *point_to)
{
    // create the popup menu
    GMenu *menu_model = gnome_cmd_file_popmenu_new (fl);
    if (!menu_model) return;

    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu_model));
    gtk_widget_set_parent (popover, GTK_WIDGET (fl));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

    GdkRectangle rect;
    if (!point_to)
    {
        get_focus_row_coordinates (fl, rect.x, rect.y, rect.width, rect.height);
        point_to = &rect;
    }

    gtk_popover_set_pointing_to (GTK_POPOVER (popover), point_to);

    gtk_popover_popup (GTK_POPOVER (popover));
}


struct PopupClosure
{
    GnomeCmdFileList *fl;
    GdkRectangle point_to;
};


static gboolean on_right_mb (PopupClosure *closure)
{
    show_file_popup (closure->fl, &closure->point_to);
    g_free (closure);
    return G_SOURCE_REMOVE;
}


/*******************************
 * Callbacks
 *******************************/


static gint column_index(GtkTreeViewColumn *column, GnomeCmdFileList *fl)
{
    auto col_iter = std::find(std::begin(fl->priv->columns), std::end(fl->priv->columns), column);
    if (col_iter == std::end(fl->priv->columns))
        return -1;
    return std::distance(fl->priv->columns, col_iter);
}


static void on_column_clicked (GtkTreeViewColumn *column, GnomeCmdFileList *fl)
{
    g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gint col = column_index(column, fl);
    if (col < 0)
        return;

    fl->priv->sort_raising[col] = fl->priv->current_col == col
        ? (GtkSortType) !fl->priv->sort_raising[col]
        : file_list_column[col].default_sort_direction;

    g_set_object(&fl->priv->sorter,
        file_list_column[col].sorter_factory (
            gnome_cmd_data.options.case_sens_sort,
            gnome_cmd_data.options.symbolic_links_as_regular_files,
            fl->priv->sort_raising[col]));

    fl->priv->current_col = col;
    update_column_sort_arrows (fl);

    fl->sort();
}


static void on_column_resized (GtkTreeViewColumn *column, GParamSpec *pspec, GnomeCmdFileList *fl)
{
    g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->priv->column_resizing)
        return;

    gint index = column_index(column, fl);
    if (index < 0)
        return;

    g_signal_emit (fl, signals[RESIZE_COLUMN], 0, index, column);
}


void GnomeCmdFileList::focus_prev()
{
    GtkTreePath *path;

    gtk_tree_view_get_cursor (priv->view, &path, nullptr);
    gtk_tree_path_prev (path);
    gtk_tree_view_set_cursor (priv->view, path, nullptr, false);
    gtk_tree_path_free (path);
}


void GnomeCmdFileList::focus_next()
{
    GtkTreePath *path;

    gtk_tree_view_get_cursor (priv->view, &path, nullptr);
    gtk_tree_path_next (path);
    gtk_tree_view_set_cursor (priv->view, path, nullptr, false);
    gtk_tree_path_free (path);
}


extern "C" GMenu *gnome_cmd_list_popmenu_new ();

inline void show_list_popup (GnomeCmdFileList *fl, gint x, gint y)
{
    auto menu = gnome_cmd_list_popmenu_new ();
    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (popover, GTK_WIDGET (fl));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
    GdkRectangle rect = { x, y, 0, 0 };
    gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
    gtk_popover_popup (GTK_POPOVER (popover));
}


static void on_button_press (GtkGestureClick *gesture, int n_press, double x, double y, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    auto row = fl->get_dest_row_at_coords (x, y);

    GnomeCmdFileListButtonEvent event = {
        .iter = row.get(),
        .file = nullptr,
        .button = button,
        .n_press = n_press,
        .x = x,
        .y = y,
        .state = get_modifiers_state (),
    };

    if (row)
    {
        event.file = fl->get_file_at_row(row.get());
        g_signal_emit (fl, signals[LIST_CLICKED], 0, &event);
        g_signal_emit (fl, signals[FILE_CLICKED], 0, &event);
    }
    else
    {
        g_signal_emit (fl, signals[LIST_CLICKED], 0, &event);
        if (n_press == 1 && button == 3) {
            show_list_popup (fl, x, y);
        }
    }
}


extern "C" gboolean mime_exec_file (GtkWindow *parent_window, GnomeCmdFile *f);


static void on_file_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (event != nullptr);
    g_return_if_fail (GNOME_CMD_IS_FILE (event->file));

    fl->modifier_click = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK);

    if (event->n_press == 2 && event->button == 1 && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
    {
        fl->do_file_specific_action (event->file);
    }
    else if (event->n_press == 1 && event->button == 1)
    {
        if (event->state & GDK_SHIFT_MASK)
        {
            select_file_range (fl, fl->priv->shift_down_row.get(), event->iter);
            fl->priv->shift_down = FALSE;
            fl->priv->shift_down_key = 0;
        }
        else if (event->state & GDK_CONTROL_MASK)
        {
            fl->toggle_file (event->iter);
        }
    }
    else if (event->n_press == 1 && event->button == 3)
    {
        if (!event->file->is_dotdot)
        {
            if (gnome_cmd_data.options.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
            {
                auto focus_iter = fl->get_focused_file_iter();
                if (iter_compare(fl->priv->store, focus_iter.get(), event->iter) == 0)
                {
                    fl->select_iter(event->iter);
                    g_signal_emit(fl, signals[FILES_CHANGED], 0);
                    show_file_popup (fl, nullptr);
                }
                else
                {
                    if (!fl->is_selected_iter(event->iter))
                        fl->select_iter(event->iter);
                    else
                        fl->unselect_iter(event->iter);
                    g_signal_emit(fl, signals[FILES_CHANGED], 0);
                }
            }
            else
            {
                PopupClosure *closure = g_new0 (PopupClosure, 1);
                closure->fl = fl;
                closure->point_to.x = event->x;
                closure->point_to.y = event->y;
                closure->point_to.width = 0;
                closure->point_to.height = 0;
                g_timeout_add (1, (GSourceFunc) on_right_mb, closure);
            }
        }
    }
}


static void on_file_released (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (event != nullptr);
    g_return_if_fail (GNOME_CMD_IS_FILE (event->file));

    if (event->n_press == 1 && event->button == 1 && !fl->modifier_click && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        fl->do_file_specific_action (event->file);
}


static void on_button_release (GtkGestureClick *gesture, int n_press, double x, double y, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    auto row = fl->get_dest_row_at_coords (x, y);
    if (!row)
        return;

    GnomeCmdFile *f = fl->get_file_at_row(row.get());

    GnomeCmdFileListButtonEvent event = {
        .iter = row.get(),
        .file = f,
        .button = button,
        .n_press = n_press,
        .x = x,
        .y = y,
        .state = get_modifiers_state (),
    };
    g_signal_emit (fl, signals[FILE_RELEASED], 0, &event);

    if (button == 1 && state_is_blank (event.state))
    {
        if (f && !fl->is_selected_iter(row.get()) && gnome_cmd_data.options.left_mouse_button_unselects)
            fl->unselect_all();
    }
}


static void on_realize (GnomeCmdFileList *fl, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    update_column_sort_arrows (fl);
    fl->realized = TRUE;
}


static void on_dir_file_created (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->insert_file(f))
        g_signal_emit (fl, signals[FILES_CHANGED], 0);
}


static void on_dir_file_deleted (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->cwd == dir)
        if (fl->remove_file(f))
            g_signal_emit (fl, signals[FILES_CHANGED], 0);
}


static void on_dir_file_changed (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->has_file(f))
    {
        fl->update_file(f);
        g_signal_emit (fl, signals[FILES_CHANGED], 0);
    }
}


static void on_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->has_file(f))
    {
        // f->invalidate_metadata(TAG_FILE);    // FIXME: should be handled in GnomeCmdDir, not here
        fl->update_file(f);

        GnomeCmdFileList::ColumnID sort_col = fl->get_sort_column();

        if (sort_col==GnomeCmdFileList::COLUMN_NAME || sort_col==GnomeCmdFileList::COLUMN_EXT)
            fl->sort();
    }
}


static void on_dir_list_ok (GnomeCmdDir *dir, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_ok\n");

    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->realized)
    {
        gtk_widget_set_sensitive (*fl, TRUE);
        gtk_widget_set_cursor (*fl, nullptr);
        gtk_widget_grab_focus (*fl);
    }

    if (fl->connected_dir!=dir)
    {
        if (fl->connected_dir)
        {
            g_signal_handlers_disconnect_by_func (fl->connected_dir, (gpointer) on_dir_file_created, fl);
            g_signal_handlers_disconnect_by_func (fl->connected_dir, (gpointer) on_dir_file_deleted, fl);
            g_signal_handlers_disconnect_by_func (fl->connected_dir, (gpointer) on_dir_file_changed, fl);
            g_signal_handlers_disconnect_by_func (fl->connected_dir, (gpointer) on_dir_file_renamed, fl);
        }

        g_signal_connect (dir, "file-created", G_CALLBACK (on_dir_file_created), fl);
        g_signal_connect (dir, "file-deleted", G_CALLBACK (on_dir_file_deleted), fl);
        g_signal_connect (dir, "file-changed", G_CALLBACK (on_dir_file_changed), fl);
        g_signal_connect (dir, "file-renamed", G_CALLBACK (on_dir_file_renamed), fl);

        fl->connected_dir = dir;
    }

    g_signal_emit (fl, signals[DIR_CHANGED], 0, dir);

    DEBUG('l', "returning from on_dir_list_ok\n");
}


static gboolean set_home_connection (GnomeCmdFileList *fl)
{
    g_printerr ("Setting home connection\n");
    fl->set_connection(get_home_con ());

    return FALSE;
}


/**
 * Handles an error which occured when directory listing failed.
 * We expect that the error which should be reported is stored in the GnomeCmdDir object.
 * The error location is freed afterwards.
 */
static void on_dir_list_failed (GnomeCmdDir *dir, GError *error, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_failed\n");

    if (error)
        gnome_cmd_show_message (GTK_WINDOW (gtk_widget_get_root (*fl)), _("Directory listing failed."), error->message);

    g_signal_handlers_disconnect_matched (fl->cwd, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, fl);
    fl->connected_dir = nullptr;
    gnome_cmd_dir_unref (fl->cwd);
    gtk_widget_set_cursor (*fl, nullptr);
    gtk_widget_set_sensitive (*fl, TRUE);

    if (fl->lwd && fl->con == gnome_cmd_file_get_connection (GNOME_CMD_FILE (fl->lwd)))
    {
        fl->cwd = fl->lwd;
        g_signal_connect (fl->cwd, "list-ok", G_CALLBACK (on_dir_list_ok), fl);
        g_signal_connect (fl->cwd, "list-failed", G_CALLBACK (on_dir_list_failed), fl);
        fl->lwd = nullptr;
    }
    else
        g_timeout_add (1, (GSourceFunc) set_home_connection, fl);
}


static gboolean grab_focus(GtkWidget *widget)
{
    GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (widget);
    return gtk_widget_grab_focus (GTK_WIDGET (fl->priv->view));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_file_list_dispose (GObject *object)
{
    GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (object);

    while (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (fl)))
        gtk_widget_unparent (child);

    G_OBJECT_CLASS (gnome_cmd_file_list_parent_class)->dispose (object);
}


static void gnome_cmd_file_list_finalize (GObject *object)
{
    GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (object);

    g_clear_object (&fl->priv->sorter);

    delete fl->priv;

    G_OBJECT_CLASS (gnome_cmd_file_list_parent_class)->finalize (object);
}


static void gnome_cmd_file_list_class_init (GnomeCmdFileListClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = gnome_cmd_file_list_dispose;
    object_class->finalize = gnome_cmd_file_list_finalize;

    GTK_WIDGET_CLASS (klass)->grab_focus = grab_focus;

    signals[FILE_CLICKED] =
        g_signal_new ("file-clicked",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, file_clicked),
            nullptr, nullptr, nullptr,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILE_RELEASED] =
        g_signal_new ("file-released",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, file_released),
            nullptr, nullptr, nullptr,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[LIST_CLICKED] =
        g_signal_new ("list-clicked",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, list_clicked),
            nullptr, nullptr, nullptr,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILES_CHANGED] =
        g_signal_new ("files-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, files_changed),
            nullptr, nullptr, nullptr,
            G_TYPE_NONE,
            0);

    signals[DIR_CHANGED] =
        g_signal_new ("dir-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, dir_changed),
            nullptr, nullptr, nullptr,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[CON_CHANGED] =
        g_signal_new ("con-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, con_changed),
            nullptr, nullptr, nullptr,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[RESIZE_COLUMN] =
        g_signal_new ("resize-column",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, resize_column),
            nullptr, nullptr, nullptr,
            G_TYPE_NONE,
            2, G_TYPE_UINT, G_TYPE_POINTER);

}


static void on_refresh (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GNOME_CMD_FILE_LIST (user_data)->reload();
}


static void gnome_cmd_file_list_init (GnomeCmdFileList *fl)
{
    gtk_widget_set_layout_manager (*fl, gtk_bin_layout_new ());

    fl->priv = new GnomeCmdFileList::Private(fl);

    auto action_group = g_simple_action_group_new ();
    static const GActionEntry action_entries[] = {
        { "refresh",            on_refresh,                                         nullptr, nullptr, nullptr },

        { "file-view",          gnome_cmd_file_list_action_file_view,               "mb",    nullptr, nullptr },
        { "file-edit",          gnome_cmd_file_list_action_file_edit,               nullptr, nullptr, nullptr },

        { "open-with-default",  gnome_cmd_file_selector_action_open_with_default,   nullptr, nullptr, nullptr },
        { "open-with-other",    gnome_cmd_file_selector_action_open_with_other,     nullptr, nullptr, nullptr },
        { "open-with",          gnome_cmd_file_selector_action_open_with,           "(sv)",  nullptr, nullptr },
        { "execute",            gnome_cmd_file_selector_action_execute,             nullptr, nullptr, nullptr },
        { "execute-script",     gnome_cmd_file_selector_action_execute_script,      "(sb)",  nullptr, nullptr },

        { "drop-files",         gnome_cmd_file_list_drop_files,                     "i",     nullptr, nullptr },
        { "drop-files-cancel",  gnome_cmd_file_list_drop_files_cancel,              nullptr, nullptr, nullptr },
    };
    g_action_map_add_action_entries (G_ACTION_MAP (action_group), action_entries, G_N_ELEMENTS (action_entries), fl);
    gtk_widget_insert_action_group (GTK_WIDGET (fl), "fl", G_ACTION_GROUP (action_group));

    fl->init_dnd();

    GtkGesture *click_controller = gtk_gesture_click_new ();
    gtk_widget_add_controller (GTK_WIDGET (fl->priv->view), GTK_EVENT_CONTROLLER (click_controller));
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_controller), 0);

    g_signal_connect (click_controller, "pressed", G_CALLBACK (on_button_press), fl);
    g_signal_connect (click_controller, "released", G_CALLBACK (on_button_release), fl);
    g_signal_connect (fl->priv->view, "cursor-changed", G_CALLBACK (on_cursor_change), fl);

    g_signal_connect_after (fl, "realize", G_CALLBACK (on_realize), fl);
    g_signal_connect (fl, "file-clicked", G_CALLBACK (on_file_clicked), fl);
    g_signal_connect (fl, "file-released", G_CALLBACK (on_file_released), fl);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller (GTK_WIDGET (fl), GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (gnome_cmd_file_list_key_pressed), fl);
}


/***********************************
 * Public functions
 ***********************************/

GnomeCmdFileList::ColumnID GnomeCmdFileList::get_sort_column() const
{
    return (ColumnID) priv->current_col;
}


GtkSortType GnomeCmdFileList::get_sort_order() const
{
    return (GtkSortType) priv->sort_raising[priv->current_col];
}


inline void add_file_to_clist (GnomeCmdFileList *fl, GnomeCmdFile *f, GtkTreeIter *next)
{
    GtkTreeIter iter;
    if (next != nullptr)
        gtk_list_store_insert_before (fl->priv->store, &iter, next);
    else
        gtk_list_store_append (fl->priv->store, &iter);
    set_model_row (fl, &iter, f, false);

    // If the use wants icons to show file types set it now
    if (gnome_cmd_data.options.layout != GNOME_CMD_LAYOUT_TEXT)
    {
        GIcon *icon = f->get_type_icon();
        gtk_list_store_set (fl->priv->store, &iter, GnomeCmdFileList::COLUMN_ICON, icon, -1);
    }

    // If we have been waiting for this file to show up, focus it
    if (fl->priv->focus_later && strcmp (f->get_name(), fl->priv->focus_later)==0)
        fl->focus_file_at_row (&iter);
}


GnomeCmdFileList::TraverseControl GnomeCmdFileList::traverse_files(
    std::function<GnomeCmdFileList::TraverseControl (GnomeCmdFile *f, GtkTreeIter *, GtkListStore *)> visitor)
{
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter))
    {
        do
        {
            GnomeCmdFile *file;
            gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, FILE_COLUMN, &file, -1);

            if (visitor (file, &iter, priv->store) == GnomeCmdFileList::TRAVERSE_BREAK)
            {
                return GnomeCmdFileList::TRAVERSE_BREAK;
            }
        }
        while (gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &iter));
    }
    return GnomeCmdFileList::TRAVERSE_CONTINUE;
}


/******************************************************************************
*
*   Function: GnomeCmdFileList::append_file
*
*   Purpose:  Add a file to the list
*
*   Params:   @f: The file to add
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/
void GnomeCmdFileList::append_file (GnomeCmdFile *f)
{
    add_file_to_clist (this, f, nullptr);
}


gboolean GnomeCmdFileList::insert_file(GnomeCmdFile *f)
{
    if (!file_is_wanted(f))
        return FALSE;

    GtkTreeIter next_iter;
    bool next_iter_found = false;
    traverse_files ([&next_iter, &next_iter_found, this, f](GnomeCmdFile *f2, GtkTreeIter *iter, GtkListStore *store) {
        if (gtk_sorter_compare (priv->sorter, f2, f) == GTK_ORDERING_LARGER)
        {
            memmove (&next_iter, iter, sizeof (next_iter));
            next_iter_found = true;
            return TRAVERSE_BREAK;
        }
        return TRAVERSE_CONTINUE;
    });

    if (next_iter_found)
        add_file_to_clist (this, f, &next_iter);
    else
        append_file(f);

    return TRUE;
}


static gint compare_with_gtk_sorter (gconstpointer itema, gconstpointer itemb, gpointer user_data)
{
    return gtk_sorter_compare (GTK_SORTER (user_data), G_OBJECT (itema), G_OBJECT (itemb));
}


void GnomeCmdFileList::show_files(GnomeCmdDir *dir)
{
    clear();

    GList *files = nullptr;

    // select the files to show
    for (auto i = gnome_cmd_dir_get_files (dir); i; i = i->next)
    {
        GnomeCmdFile *f = GNOME_CMD_FILE (i->data);

        if (file_is_wanted (f))
            files = g_list_append (files, f);
    }

    // Create a parent dir file (..) if appropriate
    GError *error = nullptr;
    auto uriString = GNOME_CMD_FILE (dir)->get_uri_str();
    auto gUri = g_uri_parse(uriString, G_URI_FLAGS_NONE, &error);
    if (error)
    {
        g_warning("show_files: g_uri_parse error of %s: %s", uriString, error->message);
        g_error_free(error);
    }
    auto path = g_uri_get_path(gUri);
    if (path && strcmp (path, G_DIR_SEPARATOR_S) != 0)
        files = g_list_append (files, gnome_cmd_dir_new_parent_dir_file (dir));
    g_free (uriString);

    if (files)
    {
        files = g_list_sort_with_data (files, compare_with_gtk_sorter, priv->sorter);

        for (auto i = files; i; i = i->next)
            append_file(GNOME_CMD_FILE (i->data));

        g_list_free (files);
    }

    if (gtk_widget_get_realized (GTK_WIDGET (priv->view)))
        gtk_tree_view_scroll_to_point (priv->view, 0, 0);
}


void GnomeCmdFileList::update_file(GnomeCmdFile *f)
{
    if (!f->needs_update())
        return;

    auto row = get_row_from_file(f);
    if (!row)
        return;

    set_model_row (this, row.get(), f, false);

    if (gnome_cmd_data.options.layout != GNOME_CMD_LAYOUT_TEXT)
    {
        GIcon *icon = f->get_type_icon();
        gtk_list_store_set (priv->store, row.get(), GnomeCmdFileList::COLUMN_ICON, icon, -1);
    }
}


void GnomeCmdFileList::show_dir_tree_size(GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));

    auto row = get_row_from_file(f);
    if (!row)
        return;

    set_model_row (this, row.get(), f, true);
}


void GnomeCmdFileList::show_visible_tree_sizes()
{
    invalidate_tree_size();

    traverse_files([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        set_model_row (this, iter, file, true);
        return TraverseControl::TRAVERSE_CONTINUE;
    });

    g_signal_emit (this, signals[FILES_CHANGED], 0);
}


void GnomeCmdFileList::show_column(ColumnID col, gboolean value)
{
    gtk_tree_view_column_set_visible (priv->columns[col], value);
}


void GnomeCmdFileList::resize_column(ColumnID col, gint width)
{
    if (priv == nullptr)
        return;

    gint current_width = gtk_tree_view_column_get_width (priv->columns[col]);
    if (current_width == width)
        return;

    priv->column_resizing += 1;
    gtk_tree_view_column_set_fixed_width (priv->columns[col], width);
    priv->column_resizing -= 1;
}


gboolean GnomeCmdFileList::remove_file(GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, FALSE);

    auto row = get_row_from_file(f);

    if (!row)                               // f not found in the shown file list...
        return FALSE;

    if (gtk_list_store_remove (priv->store, row.get()))
        focus_file_at_row (row.get());

    return TRUE;
}


void GnomeCmdFileList::clear()
{
    gtk_list_store_clear (this->priv->store);
}


void GnomeCmdFileList::reload()
{
    g_return_if_fail (GNOME_CMD_IS_DIR (cwd));

    unselect_all();
    gnome_cmd_dir_relist_files (GTK_WINDOW (gtk_widget_get_root (*this)), cwd, gnome_cmd_con_needs_list_visprog (con));
}


GList *GnomeCmdFileList::get_selected_files()
{
    GList *files = nullptr;
    traverse_files([this, &files](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (is_selected_iter(iter))
            files = g_list_append (files, file);
        return TraverseControl::TRAVERSE_CONTINUE;
    });
    if (files == nullptr)
    {
        GnomeCmdFile *f = get_selected_file();
        if (f != nullptr)
            files = g_list_append (nullptr, g_object_ref (f));
    }
    return files;
}


GnomeCmdFile *GnomeCmdFileList::get_first_selected_file()
{
    GnomeCmdFile *selected_file = nullptr;
    traverse_files([this, &selected_file](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (is_selected_iter(iter))
        {
            selected_file = file;
            return TraverseControl::TRAVERSE_BREAK;
        }
        return TraverseControl::TRAVERSE_CONTINUE;
    });
    return selected_file;
}


GtkTreeIterPtr GnomeCmdFileList::get_focused_file_iter()
{
    GtkTreePath *path = nullptr;
    gtk_tree_view_get_cursor (priv->view, &path, NULL);
    GtkTreeIter iter;
    bool found = false;
    if (path != nullptr)
        found = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
    gtk_tree_path_free (path);
    if (found)
        return GtkTreeIterPtr(gtk_tree_iter_copy(&iter), &gtk_tree_iter_free);
    else
        return GtkTreeIterPtr(nullptr, &gtk_tree_iter_free);
}


GnomeCmdFile *GnomeCmdFileList::get_focused_file()
{
    auto iter = get_focused_file_iter();
    if (iter)
        return get_file_at_row(iter.get());
    return nullptr;
}


void GnomeCmdFileList::set_selected_at_iter(GtkTreeIter *iter, gboolean selected)
{
    gtk_list_store_set (priv->store, iter, DATA_COLUMN_SELECTED, selected, -1);
}


void GnomeCmdFileList::select_iter(GtkTreeIter *iter)
{
    set_selected_at_iter (iter, TRUE);
}


void GnomeCmdFileList::unselect_iter(GtkTreeIter *iter)
{
    set_selected_at_iter (iter, FALSE);
}


bool GnomeCmdFileList::is_selected_iter(GtkTreeIter *iter)
{
    gboolean selected = FALSE;
    gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter, DATA_COLUMN_SELECTED, &selected, -1);
    return selected;
}


gboolean GnomeCmdFileList::has_file(const GnomeCmdFile *f)
{
    return TRAVERSE_BREAK == traverse_files ([f](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        return file == f ? TRAVERSE_BREAK : TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::select_all_files()
{
    traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (file && !file->is_dotdot)
            set_selected_at_iter (iter, !GNOME_CMD_IS_DIR(file));
        return TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::unselect_all_files()
{
    traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (!GNOME_CMD_IS_DIR(file))
           unselect_iter(iter);
        return TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::select_all()
{
    if (gnome_cmd_data.options.select_dirs)
    {
        traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
            if (file && !file->is_dotdot)
                select_iter(iter);
            return TRAVERSE_CONTINUE;
        });
    }
    else
    {
        traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
            if (file && !file->is_dotdot && !GNOME_CMD_IS_DIR (file))
                select_iter(iter);
            return TRAVERSE_CONTINUE;
        });
    }
    g_signal_emit(this, signals[FILES_CHANGED], 0);
}


void GnomeCmdFileList::unselect_all()
{
    traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        unselect_iter (iter);
        return TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::toggle()
{
    auto iter = get_focused_file_iter();
    if (iter)
        toggle_file(iter.get());
}


void GnomeCmdFileList::toggle_and_step()
{
    auto iter = get_focused_file_iter();
    if (iter)
        toggle_file(iter.get());
    if (gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), iter.get()))
        focus_file_at_row (iter.get());
}


void GnomeCmdFileList::focus_file(const gchar *fileToFocus, gboolean scrollToFile)
{
    g_return_if_fail(fileToFocus != nullptr);

    auto fileToFocusNormalized = g_utf8_normalize(fileToFocus, -1, G_NORMALIZE_DEFAULT);

    auto result = traverse_files ([this, &fileToFocusNormalized, scrollToFile](GnomeCmdFile *f, GtkTreeIter *iter, GtkListStore *store) {
        auto currentFilename = g_utf8_normalize(f->get_name(), -1, G_NORMALIZE_DEFAULT);
        if (g_strcmp0 (currentFilename, fileToFocusNormalized) == 0)
        {
            focus_file_at_row (iter);
            if (scrollToFile)
            {
                GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), iter);
                gtk_tree_view_scroll_to_cell (priv->view, path, nullptr, false, 0.0, 0.0);
                gtk_tree_path_free (path);
            }
            g_free(currentFilename);
            return TRAVERSE_BREAK;
        }
        g_free(currentFilename);
        return TRAVERSE_CONTINUE;
    });
    g_free(fileToFocusNormalized);

    /* The file was not found, remember the filename in case the file gets
       added to the list in the future (after a FAM event etc). */
    if (result == TRAVERSE_CONTINUE)
    {
        g_free (priv->focus_later);
        priv->focus_later = g_strdup (fileToFocus);
    }
}


void GnomeCmdFileList::select_row(GtkTreeIter* row)
{
    GtkTreeIter iter;
    if (row == nullptr)
    {
        if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter))
        {
            row = &iter;
        }
    }
    focus_file_at_row (row);
}


GnomeCmdFile *GnomeCmdFileList::get_file_at_row (GtkTreeIter *iter)
{
    GnomeCmdFile *file = nullptr;
    if (iter != nullptr)
        gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter, FILE_COLUMN, &file, -1);
    return file;
}


GtkTreeIterPtr GnomeCmdFileList::get_row_from_file(GnomeCmdFile *f)
{
    GtkTreeIterPtr result(nullptr, &gtk_tree_iter_free);
    traverse_files ([f, &result](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (file == f)
        {
            result.reset (gtk_tree_iter_copy (iter));
            return TRAVERSE_BREAK;
        }
        return TRAVERSE_CONTINUE;
    });
	return result;
}


extern "C" void gnome_cmd_file_list_invert_selection(GnomeCmdFileList *fl);
extern "C" void gnome_cmd_file_list_restore_selection(GnomeCmdFileList *fl);


void GnomeCmdFileList::sort()
{
    GnomeCmdFile *selfile = get_selected_file();

    std::vector<gint> indexes(size());
    std::iota(indexes.begin(), indexes.end(), 0);

    std::sort(indexes.begin(), indexes.end(), [this] (const gint a, const gint b) {
        GtkTreeIter itera, iterb;
        g_assert_true (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (priv->store), &itera, nullptr, a));
        g_assert_true (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (priv->store), &iterb, nullptr, b));

        GnomeCmdFile *filea = nullptr;
        GnomeCmdFile *fileb = nullptr;
        gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &itera, DATA_COLUMN_FILE, &filea, -1);
        gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iterb, DATA_COLUMN_FILE, &fileb, -1);

        return gtk_sorter_compare (priv->sorter, filea, fileb) == GTK_ORDERING_SMALLER;
    });
    gtk_list_store_reorder (priv->store, &indexes[0]);

    // refocus the previously selected file if this file list has the focus
    if (selfile && gtk_widget_has_focus (GTK_WIDGET (this)))
    {
        auto selrow = get_row_from_file(selfile);
        focus_file_at_row(selrow.get());
    }
}


void gnome_cmd_file_list_show_rename_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (GNOME_CMD_IS_FILE (f))
    {
        gint x, y, w, h;

        get_focus_row_coordinates (fl, x, y, w, h);

        GtkWidget *popover = gnome_cmd_rename_dialog_new (f, *fl, x, y, w, h);
        gtk_popover_popup (GTK_POPOVER (popover));
    }
}


void gnome_cmd_file_list_show_delete_dialog (GnomeCmdFileList *fl, gboolean forceDelete)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();

    if (files)
    {
        gnome_cmd_delete_dialog_show (GTK_WINDOW (gtk_widget_get_root (*fl)), files, forceDelete);
    }
}


void gnome_cmd_file_list_show_properties_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (f)
    {
        GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (fl)));
        gnome_cmd_file_props_dialog_show (parent_window, f);
    }
}


extern "C" void show_pattern_selection_dialog_r(GnomeCmdFileList *fl, gboolean mode, GnomeCmdData::SearchConfig *search_config);

void gnome_cmd_file_list_show_selpat_dialog (GnomeCmdFileList *fl, gboolean mode)
{
    show_pattern_selection_dialog_r(fl, mode, &gnome_cmd_data.search_defaults);
}


static bool is_quicksearch_starting_character (guint keyval)
{
    return (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z) ||
            (keyval >= GDK_KEY_a && keyval <= GDK_KEY_z) ||
            (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9) ||
            keyval == GDK_KEY_period ||
            keyval == GDK_KEY_question ||
            keyval == GDK_KEY_asterisk ||
            keyval == GDK_KEY_bracketleft;
}


static bool is_quicksearch_starting_modifier (guint state)
{
    switch (gnome_cmd_data.options.quick_search)
    {
        case GNOME_CMD_QUICK_SEARCH_CTRL_ALT:
            return (state & GDK_CONTROL_MASK) && (state & GDK_ALT_MASK);

        case GNOME_CMD_QUICK_SEARCH_ALT:
            return !(state & GDK_CONTROL_MASK) && (state & GDK_ALT_MASK);

        case GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER:
            return !(state & GDK_CONTROL_MASK) && !(state & GDK_ALT_MASK);

        default:
            return false;
    }
}

static gboolean gnome_cmd_file_list_key_pressed (GtkEventControllerKey* self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    auto fl = GNOME_CMD_FILE_LIST (user_data);

    if (state_is_alt (state))
    {
        switch (keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                gnome_cmd_file_list_show_properties_dialog (fl);
                return TRUE;

            case GDK_KEY_KP_Add:
                toggle_files_with_same_extension (fl, TRUE);
                break;

            case GDK_KEY_KP_Subtract:
                toggle_files_with_same_extension (fl, FALSE);
                break;

            default:
                break;
        }
    }
    else if (state_is_shift (state))
    {
        switch (keyval)
        {
            case GDK_KEY_F6:
                gnome_cmd_file_list_show_rename_dialog (fl);
                return TRUE;

            case GDK_KEY_F10:
                show_file_popup (fl, nullptr);
                return TRUE;

            case GDK_KEY_Left:
            case GDK_KEY_KP_Left:
            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                return FALSE;

            case GDK_KEY_Page_Up:
            case GDK_KEY_KP_Page_Up:
            case GDK_KEY_KP_9:
                fl->priv->shift_down_key = 3;
                fl->priv->shift_down_row.reset (gtk_tree_iter_copy (fl->get_focused_file_iter().get()));
                return FALSE;

            case GDK_KEY_Page_Down:
            case GDK_KEY_KP_Page_Down:
            case GDK_KEY_KP_3:
                fl->priv->shift_down_key = 4;
                fl->priv->shift_down_row.reset (gtk_tree_iter_copy (fl->get_focused_file_iter().get()));
                return FALSE;

            case GDK_KEY_Up:
            case GDK_KEY_KP_Up:
            case GDK_KEY_KP_8:
            case GDK_KEY_Down:
            case GDK_KEY_KP_Down:
            case GDK_KEY_KP_2:
                fl->priv->shift_down_key = 1; // 2
                if (auto focused = fl->get_focused_file_iter())
                    fl->toggle_file(focused.get());
                return FALSE;

            case GDK_KEY_Home:
            case GDK_KEY_KP_Home:
            case GDK_KEY_KP_7:
                fl->priv->shift_down_key = 5;
                fl->priv->shift_down_row.reset (gtk_tree_iter_copy (fl->get_focused_file_iter().get()));
                return FALSE;

            case GDK_KEY_End:
            case GDK_KEY_KP_End:
            case GDK_KEY_KP_1:
                fl->priv->shift_down_key = 6;
                fl->priv->shift_down_row.reset (gtk_tree_iter_copy (fl->get_focused_file_iter().get()));
                return FALSE;

            case GDK_KEY_Delete:
            case GDK_KEY_KP_Delete:
                gnome_cmd_file_list_show_delete_dialog (fl, TRUE);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_alt_shift (state))
    {
        switch (keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                fl->show_visible_tree_sizes();
                return TRUE;
            default:
                break;
        }
    }
    else if (state_is_ctrl (state))
    {
        switch (keyval)
        {
            case GDK_KEY_F3:
                on_column_clicked (fl->priv->columns[GnomeCmdFileList::COLUMN_NAME], fl);
                return TRUE;

            case GDK_KEY_F4:
                on_column_clicked (fl->priv->columns[GnomeCmdFileList::COLUMN_EXT], fl);
                return TRUE;

            case GDK_KEY_F5:
                on_column_clicked (fl->priv->columns[GnomeCmdFileList::COLUMN_DATE], fl);
                return TRUE;

            case GDK_KEY_F6:
                on_column_clicked (fl->priv->columns[GnomeCmdFileList::COLUMN_SIZE], fl);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_blank (state))
    {
        switch (keyval)
        {
            case GDK_KEY_space:
                set_cursor_busy_for_widget (*fl);
                fl->toggle();
                if (GnomeCmdFile *selfile = fl->get_selected_file())
                    fl->show_dir_tree_size(selfile);
                g_signal_emit (fl, signals[FILES_CHANGED], 0);
                gtk_widget_set_cursor (*fl, nullptr);
                return TRUE;

            case GDK_KEY_KP_Add:
            case GDK_KEY_plus:
            case GDK_KEY_equal:
                gnome_cmd_file_list_show_selpat_dialog (fl, TRUE);
                return TRUE;

            case GDK_KEY_KP_Subtract:
            case GDK_KEY_minus:
                gnome_cmd_file_list_show_selpat_dialog (fl, FALSE);
                return TRUE;

            case GDK_KEY_KP_Multiply:
                gnome_cmd_file_list_invert_selection(fl);
                return TRUE;

            case GDK_KEY_KP_Divide:
                gnome_cmd_file_list_restore_selection(fl);
                return TRUE;

            case GDK_KEY_Insert:
            case GDK_KEY_KP_Insert:
                fl->toggle();
                {
                    auto iter = fl->get_focused_file_iter();
                    if (iter && gtk_tree_model_iter_next (GTK_TREE_MODEL (fl->priv->store), iter.get()))
                        fl->focus_file_at_row (iter.get());
                }
                return TRUE;

            case GDK_KEY_Delete:
            case GDK_KEY_KP_Delete:
                gnome_cmd_file_list_show_delete_dialog (fl);
                return TRUE;

            case GDK_KEY_Shift_L:
            case GDK_KEY_Shift_R:
                if (!fl->priv->shift_down)
                    fl->priv->shift_down_row = fl->get_focused_file_iter();
                return TRUE;

            case GDK_KEY_Menu:
                show_file_popup (fl, nullptr);
                return TRUE;

            case GDK_KEY_F3:
                gtk_widget_activate_action (GTK_WIDGET (fl), "fl.file-view", "mb", nullptr, nullptr);
                return TRUE;

            case GDK_KEY_F4:
                gtk_widget_activate_action (GTK_WIDGET (fl), "fl.file-edit", nullptr);
                return TRUE;

            default:
                break;
        }
    }

    if (is_quicksearch_starting_character (keyval))
    {
        if (is_quicksearch_starting_modifier (state))
            gnome_cmd_file_list_show_quicksearch (fl, keyval);
        else if (gnome_cmd_data.cmdline_visibility)
        {
            gchar text[2];
            text[0] = keyval;
            text[1] = '\0';
            gnome_cmd_cmdline_append_text (main_win->get_cmdline(), text);
            gnome_cmd_cmdline_focus (main_win->get_cmdline());
        }
        return TRUE;
    }

    return FALSE;
}


static void on_cursor_change(GtkTreeView *tree, GnomeCmdFileList *fl)
{
    gint shift_down_key = fl->priv->shift_down_key;
    fl->priv->shift_down_key = 0;

    auto cursor = fl->get_focused_file_iter();

    if (shift_down_key == 3) // page up
        toggle_file_range (fl, fl->priv->shift_down_row.get(), cursor.get(), false);
    else if (shift_down_key == 4) // page down
        toggle_file_range (fl, fl->priv->shift_down_row.get(), cursor.get(), false);
    else if (shift_down_key == 5) // home
        toggle_file_range (fl, fl->priv->shift_down_row.get(), cursor.get(), true);
    else if (shift_down_key == 6) // end
        toggle_file_range (fl, fl->priv->shift_down_row.get(), cursor.get(), true);
}


void GnomeCmdFileList::set_base_dir (gchar *dir)
{
    g_return_if_fail (dir != nullptr);
    if (priv->base_dir) { g_free (priv->base_dir); }
    priv->base_dir = dir;
}


extern "C" void open_connection_r (GnomeCmdFileList *fl, GtkWindow *parent_window, GnomeCmdCon *con);

void GnomeCmdFileList::set_connection (GnomeCmdCon *new_con, GnomeCmdDir *start_dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (new_con));

    if (con==new_con)
    {
        if (!gnome_cmd_con_should_remember_dir (new_con))
            set_directory (gnome_cmd_con_get_default_dir (new_con));
        else
            if (start_dir)
                set_directory (start_dir);
        return;
    }

    if (!gnome_cmd_con_is_open (new_con))
    {
        GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (this)));
        open_connection_r (this, parent_window, new_con);
        return;
    }

    con = new_con;

    if (lwd)
    {
        gnome_cmd_dir_cancel_monitoring (lwd);
        gnome_cmd_dir_unref (lwd);
        lwd = nullptr;
    }
    if (cwd)
    {
        gnome_cmd_dir_cancel_monitoring (cwd);
        gnome_cmd_dir_unref (cwd);
        cwd = nullptr;
    }

    if (!start_dir)
        start_dir = gnome_cmd_con_get_default_dir (con);

    g_signal_emit (this, signals[CON_CHANGED], 0, con);

    set_directory(start_dir);
}


void GnomeCmdFileList::set_directory(GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    if (cwd==dir)
        return;

    if (realized && dir->state!=GnomeCmdDir::STATE_LISTED)
    {
        gtk_widget_set_sensitive (*this, FALSE);
        set_cursor_busy_for_widget (*this);
    }

    gnome_cmd_dir_ref (dir);

    if (lwd && lwd!=dir)
        gnome_cmd_dir_unref (lwd);

    if (cwd)
    {
        lwd = cwd;
        gnome_cmd_dir_cancel_monitoring (lwd);
        g_signal_handlers_disconnect_matched (lwd, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        if (gnome_cmd_file_is_local (GNOME_CMD_FILE (lwd)) && !gnome_cmd_dir_is_monitored (lwd) && gnome_cmd_dir_needs_mtime_update (lwd))
            gnome_cmd_dir_update_mtime (lwd);
    }

    cwd = dir;

    switch (dir->state)
    {
        case GnomeCmdDir::STATE_EMPTY:
            g_signal_connect (dir, "list-ok", G_CALLBACK (on_dir_list_ok), this);
            g_signal_connect (dir, "list-failed", G_CALLBACK (on_dir_list_failed), this);
            gnome_cmd_dir_list_files (GTK_WINDOW (gtk_widget_get_root (*this)), dir, gnome_cmd_con_needs_list_visprog (con));
            break;

        case GnomeCmdDir::STATE_LISTING:
        case GnomeCmdDir::STATE_CANCELING:
            g_signal_connect (dir, "list-ok", G_CALLBACK (on_dir_list_ok), this);
            g_signal_connect (dir, "list-failed", G_CALLBACK (on_dir_list_failed), this);
            break;

        case GnomeCmdDir::STATE_LISTED:
            g_signal_connect (dir, "list-ok", G_CALLBACK (on_dir_list_ok), this);
            g_signal_connect (dir, "list-failed", G_CALLBACK (on_dir_list_failed), this);

            // check if the dir has up-to-date file list; if not and it's a local dir - relist it
            if (gnome_cmd_file_is_local (GNOME_CMD_FILE (dir)) && !gnome_cmd_dir_is_monitored (dir) && gnome_cmd_dir_update_mtime (dir))
                gnome_cmd_dir_relist_files (GTK_WINDOW (gtk_widget_get_root (*this)), dir, gnome_cmd_con_needs_list_visprog (con));
            else
                on_dir_list_ok (dir, this);
            break;

        default:
            break;
    }

    gnome_cmd_dir_start_monitoring (dir);
}


void GnomeCmdFileList::update_style()
{
    // TODO: Maybe??? gtk_cell_renderer_set_fixed_size (priv->columns[1], )
    // gtk_clist_set_row_height (*this, gnome_cmd_data.options.list_row_height);

    gnome_cmd_theme_free (priv->color_theme);
    priv->color_theme = gnome_cmd_get_current_theme();

    gnome_cmd_palette_free(priv->ls_palette);
    priv->ls_palette = gnome_cmd_get_palette();

    auto col = priv->current_col;
    g_set_object(&priv->sorter,
        file_list_column[col].sorter_factory (
            gnome_cmd_data.options.case_sens_sort,
            gnome_cmd_data.options.symbolic_links_as_regular_files,
            priv->sort_raising[col]));

    PangoFontDescription *font_desc = pango_font_description_from_string (gnome_cmd_data.options.list_font);
    // gtk_widget_override_font (*this, font_desc);
    pango_font_description_free (font_desc);

    gtk_widget_queue_draw (*this);
}


gboolean GnomeCmdFileList::file_is_wanted(GnomeCmdFile *gnomeCmdFile)
{
    g_return_val_if_fail (gnomeCmdFile != nullptr, FALSE);
    g_return_val_if_fail (gnomeCmdFile->get_file() != nullptr, FALSE);

    GError *error = nullptr;

    auto gFileInfo = g_file_query_info(gnomeCmdFile->get_file(),
                                   "standard::*",
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   nullptr,
                                   &error);
    if (error)
    {
        auto uri = g_file_get_uri(gnomeCmdFile->get_file());
        g_message ("file_is_wanted: retrieving file info for %s failed: %s", uri, error->message);
        g_free(uri);
        g_error_free (error);
        return TRUE;
    }

    auto gFileType       = g_file_info_get_attribute_uint32(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_TYPE);
    auto gFileIsHidden   = g_file_info_get_attribute_boolean(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN);
    auto gFileIsSymLink  = g_file_info_get_attribute_boolean(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);
    auto gFileIsVirtual  = g_file_info_get_attribute_boolean(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL);
    auto gFileIsVolatile = g_file_info_get_attribute_boolean(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_IS_VOLATILE);

    auto returnValue = TRUE;

    auto fileNameString = g_file_get_basename(gnomeCmdFile->get_file());

    if (strcmp ((const char*) fileNameString, ".") == 0)
        returnValue = FALSE;
    if (gnomeCmdFile->is_dotdot)
        returnValue = FALSE;
    if (gnome_cmd_data.options.filter.file_types[gFileType])
        returnValue = FALSE;
    if (gFileIsSymLink && gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_SYMLINK])
        returnValue = FALSE;
    if (gFileIsHidden && gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_HIDDEN])
        returnValue = FALSE;
    if (gFileIsVirtual && gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_VIRTUAL])
        returnValue = FALSE;
    if (gFileIsVolatile && gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_VOLATILE])
        returnValue = FALSE;
    if (gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_BACKUP]
        && patlist_matches (gnome_cmd_data.options.backup_pattern_list, fileNameString))
        returnValue = FALSE;

    g_free(fileNameString);

    return returnValue;
}


void GnomeCmdFileList::goto_directory(const gchar *in_dir)
{
    g_return_if_fail (in_dir != nullptr);

    GnomeCmdDir *new_dir = nullptr;
    gchar *focus_dir = nullptr;
    gchar *dir;

    if (g_str_has_prefix (in_dir, "~"))
    {
        strlen(in_dir) > 1
            ? dir = g_strdup_printf("%s%s" , g_get_home_dir(), in_dir+1)
            : dir = g_strdup_printf("%s" , g_get_home_dir());
    }
    else
        dir = unquote_if_needed (in_dir);

    if (strcmp (dir, "..") == 0)
    {
        // let's get the parent directory
        new_dir = gnome_cmd_dir_get_parent (cwd);
        if (!new_dir)
        {
            g_free (dir);
            return;
        }
        focus_dir = strdup(GNOME_CMD_FILE (cwd)->get_name());
    }
    else
    {
        // check if it's an absolute address or not
        if (dir[0] == '/')
            new_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dir));
        else
            if (g_str_has_prefix (dir, "\\\\"))
            {
                GnomeCmdPath *path = gnome_cmd_con_create_path (get_smb_con (), dir);
                if (path)
                    new_dir = gnome_cmd_dir_new (get_smb_con (), path);
            }
            else
                new_dir = gnome_cmd_dir_get_child (cwd, dir);
    }

    if (new_dir)
        set_directory(new_dir);

    // focus the current dir when going back to the parent dir
    if (focus_dir)
    {
        focus_file(focus_dir, FALSE);
        g_free(focus_dir);
    }

    g_free (dir);
}


void GnomeCmdFileList::invalidate_tree_size()
{
    traverse_files ([](GnomeCmdFile *f, GtkTreeIter *iter, GtkListStore *store) {
        if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
            f->invalidate_tree_size();
        return TRAVERSE_CONTINUE;
    });
}


/******************************************************
 * DnD functions
 **/
/*
static void drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data, guint info, guint32 time, GnomeCmdFileList *fl)
{
    GList *files = nullptr;

    GString *data = build_selected_file_list (fl);

    if (data->len == 0)
    {
        g_string_free (data, TRUE);
        return;
    }

    gchar **uriCharList, **uriCharListTmp = nullptr;

    switch (info)
    {
        case TARGET_URI_LIST:
        case TARGET_TEXT_PLAIN:
            gtk_selection_data_set (selection_data, gtk_selection_data_get_target (selection_data), 8, (const guchar *) data->str, data->len);
            break;

        case TARGET_URL:
            uriCharList = uriCharListTmp = g_uri_list_extract_uris (data->str);
            for (auto uriChar = *uriCharListTmp; uriChar; uriChar=*++uriCharListTmp)
            {
                auto gFile = g_file_new_for_uri(uriChar);
                files = g_list_append(files, gFile);
            }
            if (files)
                gtk_selection_data_set (selection_data, gtk_selection_data_get_target (selection_data), 8, (const guchar *) files->data, strlen ((const char *) files->data));
            g_list_foreach (files, (GFunc) g_object_unref, nullptr);
            g_strfreev(uriCharList);
            break;

        default:
            g_assert_not_reached ();
    }

    g_string_free (data, TRUE);
}


static void drag_data_received (GtkWidget *widget, GdkDragContext *context,
                                gint x, gint y, GtkSelectionData *selection_data,
                                guint info, guint32 time, GnomeCmdFileList *fl)
{
    auto row = fl->get_dest_row_at_pos (x, y);
    GnomeCmdFile *f = fl->get_file_at_row (row.get());
    GnomeCmdDir *to = fl->cwd;

    // the drop was over a directory in the list, which means that the
    // xfer should be done to that directory instead of the current one in the list
    if (f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        to = f->is_dotdot
            ? gnome_cmd_dir_get_parent (to)
            : gnome_cmd_dir_get_child (to, g_file_info_get_display_name(f->get_file_info()));

    // transform the drag data to a list with GFiles
    GList *gFileGlist = uri_strings_to_gfiles ((gchar *) gtk_selection_data_get_data (selection_data));

    GdkModifierType mask = get_modifiers_state ();

    if (!(mask & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)))
    {
        switch (gnome_cmd_data.options.mouse_dnd_default)
        {
            case GNOME_CMD_DEFAULT_DND_QUERY:
                {
                    auto dnd_popover = create_dnd_popup (fl, gFileGlist, to);

                    GdkRectangle rect = { x, y, 1, 1 };
                    gtk_popover_set_pointing_to (dnd_popover, &rect);

                    gtk_popover_popup (dnd_popover);
                }
                break;
            case GNOME_CMD_DEFAULT_DND_MOVE:
                fl->drop_files(GnomeCmdFileList::DndMode::MOVE, G_FILE_COPY_NONE, gFileGlist, to);
                break;
            case GNOME_CMD_DEFAULT_DND_COPY:
                fl->drop_files(GnomeCmdFileList::DndMode::COPY, G_FILE_COPY_NONE, gFileGlist, to);
                break;
            default:
                break;
        }
        return;
    }

    // find out what operation to perform if Shift or Control was pressed while DnD
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (gdk_drag_context_get_selected_action (context))
    {
        case GDK_ACTION_MOVE:
            fl->drop_files(GnomeCmdFileList::DndMode::MOVE, G_FILE_COPY_NONE, gFileGlist, to);
            break;

        case GDK_ACTION_COPY:
            fl->drop_files(GnomeCmdFileList::DndMode::COPY, G_FILE_COPY_NONE, gFileGlist, to);
            break;

        case GDK_ACTION_LINK:
            fl->drop_files(GnomeCmdFileList::DndMode::LINK, G_FILE_COPY_NONE, gFileGlist, to);
            break;

        default:
            g_warning ("Unknown context->action in drag_data_received");
            return;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


static gboolean drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, GnomeCmdFileList *fl)
{
    gdk_drag_status (context, gdk_drag_context_get_suggested_action (context), time);

    auto row = fl->get_dest_row_at_pos (x, y);

    if (row)
    {
        GnomeCmdFile *f = fl->get_file_at_row(row.get());

        if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY)
            row.reset ();
    }

    return FALSE;
}


static void drag_data_delete (GtkWidget *widget, GdkDragContext *drag_context, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();
    fl->remove_files(files);
    g_list_free (files);
}
*/

void GnomeCmdFileList::init_dnd()
{
    // set up drag source
    GdkContentFormats* drag_formats = gdk_content_formats_new (drag_types, G_N_ELEMENTS (drag_types));
    gtk_tree_view_enable_model_drag_source (priv->view,
                                            GDK_BUTTON1_MASK,
                                            drag_formats,
                                            (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK));

    // g_signal_connect (this, "drag-data-get", G_CALLBACK (drag_data_get), this);
    // g_signal_connect (this, "drag-data-delete", G_CALLBACK (drag_data_delete), this);

    // set up drag destination
    GdkContentFormats* drop_formats = gdk_content_formats_new (drop_types, G_N_ELEMENTS (drop_types));
    gtk_tree_view_enable_model_drag_dest (priv->view,
                                          drop_formats,
                                          (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK));

    // g_signal_connect (this, "drag-motion", G_CALLBACK (drag_motion), this);
    // g_signal_connect (this, "drag-data-received", G_CALLBACK (drag_data_received), this);
}


struct DropDoneClosure
{
    GnomeCmdDir *dir;
};


static void on_drop_done (gboolean success, gpointer user_data)
{
    DropDoneClosure *drop_done = (DropDoneClosure *) user_data;

    gnome_cmd_dir_relist_files (GTK_WINDOW (main_win), drop_done->dir, FALSE);
    main_win->focus_file_lists ();

    g_free (drop_done);
}


void GnomeCmdFileList::drop_files(DndMode dndMode, GFileCopyFlags gFileCopyFlags, GList *gFileGlist, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    DropDoneClosure *drop_done;

    switch (dndMode)
    {
        case COPY:
            drop_done = g_new0 (DropDoneClosure, 1);
            drop_done->dir = dir;
            gnome_cmd_copy_gfiles_start (GTK_WINDOW (main_win),
                                         gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         on_drop_done,
                                         drop_done);
            break;
        case MOVE:
            drop_done = g_new0 (DropDoneClosure, 1);
            drop_done->dir = dir;
            gnome_cmd_move_gfiles_start (GTK_WINDOW (main_win),
                                         gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         on_drop_done,
                                         drop_done);
            break;
        case LINK:
            drop_done = g_new0 (DropDoneClosure, 1);
            drop_done->dir = dir;
            gnome_cmd_link_gfiles_start (GTK_WINDOW (main_win),
                                         gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         on_drop_done,
                                         drop_done);
            break;
        default:
            return;
    }
}

GtkTreeIterPtr GnomeCmdFileList::get_dest_row_at_pos (gint drag_x, gint drag_y)
{
    gint wx, wy;
    gtk_tree_view_convert_bin_window_to_widget_coords (priv->view, drag_x, drag_y, &wx, &wy);
    return get_dest_row_at_coords (wx, wy);
}

GtkTreeIterPtr GnomeCmdFileList::get_dest_row_at_coords (gdouble x, gdouble y)
{
    GtkTreePath *path;
    if (!gtk_tree_view_get_dest_row_at_pos (priv->view, x, y, &path, nullptr))
        return GtkTreeIterPtr(nullptr, &gtk_tree_iter_free);

    GtkTreeIter iter;
    bool found = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
    gtk_tree_path_free (path);
    if (found)
        return GtkTreeIterPtr(gtk_tree_iter_copy(&iter), &gtk_tree_iter_free);
    else
        return GtkTreeIterPtr(nullptr, &gtk_tree_iter_free);
}

bool GnomeCmdFileList::do_file_specific_action (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, FALSE);

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
    {
        if (!locked)
        {
            invalidate_tree_size();

            if (f->is_dotdot)
                goto_directory("..");
            else
                set_directory(GNOME_CMD_DIR (f));

            return TRUE;
        }
        return FALSE;
    }

    return mime_exec_file (GTK_WINDOW (gtk_widget_get_root (*this)), get_focused_file());
}


// FFI
extern "C" GtkTreeView *gnome_cmd_file_list_get_tree_view (GnomeCmdFileList *fl)
{
    return fl->priv->view;
}

GList *gnome_cmd_file_list_get_selected_files (GnomeCmdFileList *fl)
{
    return fl->get_selected_files();
}

GnomeCmdFile *gnome_cmd_file_list_get_focused_file(GnomeCmdFileList *fl)
{
    return fl->get_focused_file();
}

GnomeCmdDir *gnome_cmd_file_list_get_cwd(GnomeCmdFileList *fl)
{
    return fl->cwd;
}

GnomeCmdCon *gnome_cmd_file_list_get_connection(GnomeCmdFileList *fl)
{
    return fl->con;
}

GnomeCmdDir *gnome_cmd_file_list_get_directory(GnomeCmdFileList *fl)
{
    return fl->cwd;
}

gboolean gnome_cmd_file_list_is_locked (GnomeCmdFileList *fl)
{
    return fl->locked;
}

void gnome_cmd_file_list_reload (GnomeCmdFileList *fl)
{
    fl->reload();
}

void gnome_cmd_file_list_set_connection(GnomeCmdFileList *fl, GnomeCmdCon *con, GnomeCmdDir *start_dir)
{
    fl->set_connection(con, start_dir);
}

void gnome_cmd_file_list_focus_file(GnomeCmdFileList *fl, const gchar *focus_file, gboolean scroll_to_file)
{
    fl->focus_file(focus_file, scroll_to_file);
}

void gnome_cmd_file_list_goto_directory(GnomeCmdFileList *fl, const gchar *dir)
{
    fl->goto_directory(dir);
}
