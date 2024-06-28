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
#include "cap.h"
#include "gnome-cmd-file-popmenu.h"
#include "gnome-cmd-quicksearch-popup.h"
#include "gnome-cmd-file-collection.h"
#include "ls_colors.h"
#include "dialogs/gnome-cmd-delete-dialog.h"
#include "dialogs/gnome-cmd-patternsel-dialog.h"
#include "dialogs/gnome-cmd-rename-dialog.h"

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
    EMPTY_SPACE_CLICKED, // The file list was clicked but not on a file
    FILES_CHANGED,       // The visible content of the file list has changed (files have been: selected, created, deleted or modified)
    DIR_CHANGED,         // The current directory has been changed
    CON_CHANGED,         // The current connection has been changed
    RESIZE_COLUMN,       // Column's width was changed
    LAST_SIGNAL
};


static GtkTargetEntry drag_types [] =
{
    { TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
    { TARGET_TEXT_PLAIN_TYPE, 0, TARGET_TEXT_PLAIN },
    { TARGET_URL_TYPE, 0, TARGET_URL }
};

static GtkTargetEntry drop_types [] =
{
    { TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
    { TARGET_URL_TYPE, 0, TARGET_URL }
};


static guint signals[LAST_SIGNAL] = { 0 };


struct TmpDlData
{
    GnomeCmdFile *f;
    GtkWidget *dialog;
    gpointer *args;
};


struct GnomeCmdFileListColumn
{
    guint id;
    const gchar *title;
    PangoAlignment alignment;
    GtkSortType default_sort_direction;
    GCompareDataFunc sort_func;
};


static void cell_data (GtkTreeViewColumn *tree_column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *tree_model,
                       GtkTreeIter *iter,
                       gpointer data);
static gint sort_by_name (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_ext (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_dir (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_size (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_date (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_perm (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_owner (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_group (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static void on_column_clicked (GtkTreeViewColumn *column, GnomeCmdFileList *fl);
static void on_column_resized (GtkTreeViewColumn *column, GParamSpec *pspec, GnomeCmdFileList *fl);
static void on_cursor_change(GtkTreeView *tree, GnomeCmdFileList *fl);


static GnomeCmdFileListColumn file_list_column[GnomeCmdFileList::NUM_COLUMNS] =
{{GnomeCmdFileList::COLUMN_ICON, nullptr, PANGO_ALIGN_CENTER,GTK_SORT_ASCENDING, nullptr},
 {GnomeCmdFileList::COLUMN_NAME, N_("name"), PANGO_ALIGN_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_name},
 {GnomeCmdFileList::COLUMN_EXT, N_("ext"), PANGO_ALIGN_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_ext},
 {GnomeCmdFileList::COLUMN_DIR, N_("dir"), PANGO_ALIGN_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_dir},
 {GnomeCmdFileList::COLUMN_SIZE, N_("size"), PANGO_ALIGN_RIGHT, GTK_SORT_DESCENDING, (GCompareDataFunc) sort_by_size},
 {GnomeCmdFileList::COLUMN_DATE, N_("date"), PANGO_ALIGN_LEFT, GTK_SORT_DESCENDING, (GCompareDataFunc) sort_by_date},
 {GnomeCmdFileList::COLUMN_PERM, N_("perm"), PANGO_ALIGN_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_perm},
 {GnomeCmdFileList::COLUMN_OWNER, N_("uid"), PANGO_ALIGN_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_owner},
 {GnomeCmdFileList::COLUMN_GROUP, N_("gid"), PANGO_ALIGN_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_group}};


enum DataColumns {
    DATA_COLUMN_FILE = GnomeCmdFileList::NUM_COLUMNS,
    DATA_COLUMN_ICON_NAME,
    DATA_COLUMN_SELECTED,
    NUM_DATA_COLUMNS,
};

const gint FILE_COLUMN = GnomeCmdFileList::NUM_COLUMNS;


struct GnomeCmdFileListClass
{
    GtkTreeViewClass parent_class;

    void (* file_clicked)        (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event);
    void (* file_released)       (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event);
    void (* list_clicked)        (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event);
    void (* empty_space_clicked) (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event);
    void (* files_changed)       (GnomeCmdFileList *fl);
    void (* dir_changed)         (GnomeCmdFileList *fl, GnomeCmdDir *dir);
    void (* con_changed)         (GnomeCmdFileList *fl, GnomeCmdCon *con);
    void (* resize_column)       (GnomeCmdFileList *fl, guint index, GtkTreeViewColumn *col);
};


struct GnomeCmdFileList::Private
{
    GtkListStore *store;

    GtkTreeViewColumn *columns[NUM_COLUMNS];

    gchar *base_dir;

    GCompareDataFunc sort_func;
    gint current_col;
    gboolean sort_raising[NUM_COLUMNS];
    gint column_resizing;

    gboolean shift_down;
    GtkTreeIterPtr shift_down_row;
    gint shift_down_key;
    GnomeCmdFile *right_mb_down_file;
    gboolean right_mb_sel_state;
    guint right_mb_timeout_id;
    GtkWidget *selpat_dialog;
    GWeakRef quicksearch_popup;
    gchar *focus_later;

    GnomeCmdCon *con_opening;
    GtkWidget *con_open_dialog;
    GtkWidget *con_open_dialog_label;
    GtkWidget *con_open_dialog_pbar;

    explicit Private(GnomeCmdFileList *fl);
    ~Private();

    static void on_dnd_popup_menu_copy(GtkButton *item, GnomeCmdFileList *fl);
    static void on_dnd_popup_menu_move(GtkButton *item, GnomeCmdFileList *fl);
    static void on_dnd_popup_menu_link(GtkButton *item, GnomeCmdFileList *fl);
};


GnomeCmdFileList::Private::Private(GnomeCmdFileList *fl)
    : shift_down_row (nullptr, &gtk_tree_iter_free)
{
    base_dir = nullptr;

    column_resizing = 0;

    quicksearch_popup = { { nullptr } };
    selpat_dialog = nullptr;

    focus_later = nullptr;
    shift_down = FALSE;
    shift_down_row = nullptr;
    shift_down_key = 0;
    right_mb_sel_state = FALSE;
    right_mb_down_file = nullptr;
    right_mb_timeout_id = 0;

    con_opening = nullptr;
    con_open_dialog = nullptr;
    con_open_dialog_label = nullptr;
    con_open_dialog_pbar = nullptr;

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

        gtk_tree_view_insert_column (GTK_TREE_VIEW (fl), columns[i], i);

        GtkCellRenderer *renderer;
        if (i == GnomeCmdFileList::COLUMN_ICON)
        {
            gtk_tree_view_column_set_clickable (columns[i], FALSE);

            renderer = gtk_cell_renderer_pixbuf_new();
            gtk_tree_view_column_pack_start (columns[i], renderer, TRUE);

            gtk_tree_view_column_set_cell_data_func (columns[i], renderer, cell_data, reinterpret_cast<gpointer>(i), nullptr);

            renderer = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start (columns[i], renderer, TRUE);

            gtk_tree_view_column_set_cell_data_func (columns[i], renderer, cell_data, reinterpret_cast<gpointer>(DATA_COLUMN_ICON_NAME), nullptr);
        }
        else
        {
            gtk_tree_view_column_set_clickable (columns[i], TRUE);
            // gtk_tree_view_column_set_sort_column_id (columns[i], i);
            gtk_tree_view_column_set_sort_indicator (columns[i], TRUE);

            renderer = gtk_cell_renderer_text_new ();
            g_object_set (renderer, "alignment", file_list_column[i].alignment, NULL);
            gtk_tree_view_column_pack_start (columns[i], renderer, TRUE);

            gtk_tree_view_column_set_cell_data_func (columns[i], renderer, cell_data, reinterpret_cast<gpointer>(i), nullptr);
        }
    }

    for (gsize i=0; i<NUM_COLUMNS; i++)
    {
        g_signal_connect (columns[i], "clicked", G_CALLBACK (on_column_clicked), fl);
        g_signal_connect (columns[i], "notify::width", G_CALLBACK (on_column_resized), fl);
    }
}


static void paint_cell_with_ls_colors (GtkCellRenderer *cell, GnomeCmdFile *file, bool has_foreground)
{
    if (!gnome_cmd_data.options.use_ls_colors)
        return;

    if (LsColor *col = ls_colors_get (file))
    {
        if (has_foreground && col->fg != nullptr)
            g_object_set (G_OBJECT (cell),
                "foreground-rgba", col->fg,
                "foreground-set", TRUE,
                nullptr);
        if (col->bg != nullptr)
            g_object_set (G_OBJECT (cell),
                "cell-background-rgba", col->bg,
                "cell-background-set", TRUE,
                nullptr);
    }
}


static void cell_data (GtkTreeViewColumn *column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       gpointer data)
{
    gsize column_index = reinterpret_cast<gsize>(data);

    gboolean selected = false;
    GnomeCmdFile *file = nullptr;
    gpointer value;
    gtk_tree_model_get (model, iter,
        DATA_COLUMN_FILE, &file,
        DATA_COLUMN_SELECTED, &selected,
        column_index, &value,
        -1);

    bool has_foreground;
    if (column_index == GnomeCmdFileList::COLUMN_ICON)
    {
        if (gnome_cmd_data.options.layout == GNOME_CMD_LAYOUT_TEXT)
            g_object_set (G_OBJECT (cell), "gicon", NULL, nullptr);
        else
            g_object_set (G_OBJECT (cell), "gicon", value, nullptr);
        has_foreground = false;
    }
    else if (column_index == DATA_COLUMN_ICON_NAME)
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

    GnomeCmdColorTheme *colors = gnome_cmd_data.options.get_current_color_theme();
    if (selected)
    {
        if (has_foreground)
            g_object_set (G_OBJECT (cell),
                "foreground-rgba", &colors->sel_fg,
                "foreground-set", TRUE,
                nullptr);
        g_object_set (G_OBJECT (cell),
            "cell-background-rgba", &colors->sel_bg,
            "cell-background-set", TRUE,
            nullptr);
    }
    else
    {
        if (!colors->respect_theme)
        {
            if (has_foreground)
                g_object_set (G_OBJECT (cell),
                    "foreground-rgba", &colors->norm_fg,
                    "foreground-set", TRUE,
                    nullptr);

            g_object_set (G_OBJECT (cell),
                "cell-background-rgba", &colors->norm_bg,
                "cell-background-set", TRUE,
                nullptr);
        }

        paint_cell_with_ls_colors (cell, file, has_foreground);
    }
}


static void unref_gfile_list (GList *list)
{
    g_list_foreach (list, (GFunc) g_object_unref, nullptr);
}


static GtkPopover *create_dnd_popup (GnomeCmdFileList *fl, GList *gFileGlist, GnomeCmdDir* to)
{
    GtkWidget *dnd_popup = gtk_popover_new (GTK_WIDGET (fl));
    g_object_set_data_full (G_OBJECT (dnd_popup), "file-list", gFileGlist, (GDestroyNotify) unref_gfile_list);
    g_object_set_data (G_OBJECT (dnd_popup), "to", to);

    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    {
        GtkWidget *item = gtk_model_button_new ();
        g_object_set (item, "text", _("_Copy here"), NULL);
        g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (GnomeCmdFileList::Private::on_dnd_popup_menu_copy), fl);
        gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (item));
    }
    {
        GtkWidget *item = gtk_model_button_new ();
        g_object_set (item, "text", _("_Move here"), NULL);
        g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (GnomeCmdFileList::Private::on_dnd_popup_menu_move), fl);
        gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (item));
    }
    {
        GtkWidget *item = gtk_model_button_new ();
        g_object_set (item, "text", _("_Link here"), NULL);
        g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (GnomeCmdFileList::Private::on_dnd_popup_menu_link), fl);
        gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (item));
    }
    gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
    {
        GtkWidget *item = gtk_model_button_new ();
        g_object_set (item, "text", _("C_ancel"), NULL);
        gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (item));
    }
    gtk_container_add (GTK_CONTAINER (dnd_popup), vbox);

    gtk_widget_show_all (GTK_WIDGET (dnd_popup));
    return GTK_POPOVER (dnd_popup);
}


GnomeCmdFileList::Private::~Private()
{
}


void GnomeCmdFileList::Private::on_dnd_popup_menu_copy(GtkButton *item, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GtkWidget *dnd_popup = gtk_widget_get_ancestor (GTK_WIDGET (item), GTK_TYPE_POPOVER);
    auto gFileGlist = static_cast<GList *> (g_object_steal_data (G_OBJECT (dnd_popup), "file-list"));
    auto to = static_cast<GnomeCmdDir*> (g_object_steal_data (G_OBJECT (dnd_popup), "to"));

    fl->drop_files(GnomeCmdFileList::DndMode::COPY, G_FILE_COPY_NONE, gFileGlist, to);
}

void GnomeCmdFileList::Private::on_dnd_popup_menu_move(GtkButton *item, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GtkWidget *dnd_popup = gtk_widget_get_ancestor (GTK_WIDGET (item), GTK_TYPE_POPOVER);
    auto gFileGlist = static_cast<GList *> (g_object_steal_data (G_OBJECT (dnd_popup), "file-list"));
    auto to = static_cast<GnomeCmdDir*> (g_object_steal_data (G_OBJECT (dnd_popup), "to"));

    fl->drop_files(GnomeCmdFileList::DndMode::MOVE, G_FILE_COPY_NONE, gFileGlist, to);
}

void GnomeCmdFileList::Private::on_dnd_popup_menu_link(GtkButton *item, GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GtkWidget *dnd_popup = gtk_widget_get_ancestor (GTK_WIDGET (item), GTK_TYPE_POPOVER);
    auto gFileGlist = static_cast<GList *> (g_object_steal_data (G_OBJECT (dnd_popup), "file-list"));
    auto to = static_cast<GnomeCmdDir*> (g_object_steal_data (G_OBJECT (dnd_popup), "to"));

    fl->drop_files(GnomeCmdFileList::DndMode::LINK, G_FILE_COPY_NONE, gFileGlist, to);
}


static GtkCssProvider *create_css_provider()
{
    gchar *css;

    GnomeCmdColorTheme *colors = gnome_cmd_data.options.get_current_color_theme();

    if (!colors->respect_theme)
    {
        gchar *norm_bg = gdk_rgba_to_string (&colors->norm_bg);
        gchar *alt_bg = gdk_rgba_to_string (&colors->alt_bg);
        gchar *curs_bg = gdk_rgba_to_string (&colors->curs_bg);
        gchar *curs_fg = gdk_rgba_to_string (&colors->curs_fg);

        css = g_strdup_printf ("\
            treeview.view.gnome-cmd-file-list,                      \
            treeview.view.gnome-cmd-file-list.even {                \
                background-color: %s;                               \
            }                                                       \
            treeview.view.gnome-cmd-file-list.odd {                 \
                background-color: %s;                               \
            }                                                       \
            treeview.view.gnome-cmd-file-list:selected:focus {      \
                background-color: %s;                               \
                color: %s;                                          \
            }                                                       \
            ",
            // row
            norm_bg,
            alt_bg,

            // cursor
            curs_bg,
            curs_fg
        );

        g_free (norm_bg);
        g_free (alt_bg);
        g_free (curs_bg);
        g_free (curs_fg);
    }
    else
    {
        css = g_strdup("");
    }

    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data (css_provider, css, -1, NULL);

    g_free (css);

    return css_provider;
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
    priv->sort_func = file_list_column[sort_col].sort_func;

    create_column_titles();

    gtk_style_context_add_class (gtk_widget_get_style_context (*this), "gnome-cmd-file-list");
    gtk_style_context_add_provider_for_screen (gdk_screen_get_default(), GTK_STYLE_PROVIDER (create_css_provider()), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (*this);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    gtk_tree_view_set_model (*this, GTK_TREE_MODEL (priv->store));
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


G_DEFINE_TYPE (GnomeCmdFileList, gnome_cmd_file_list, GTK_TYPE_TREE_VIEW)


static void on_selpat_hide (GtkWidget *dialog, GnomeCmdFileList *fl)
{
    fl->priv->selpat_dialog = nullptr;
}


// given a GnomeFileList, returns the upper-left corner of the selected file
static void get_focus_row_coordinates (GnomeCmdFileList *fl, gint &x, gint &y, gint &width, gint &height)
{
    GtkTreePath *path;
    GdkRectangle rect_name;
    GdkRectangle rect_ext;

    gtk_tree_view_get_cursor (*fl, &path, nullptr);
    gtk_tree_view_get_cell_area (*fl, path, fl->priv->columns[GnomeCmdFileList::COLUMN_NAME], &rect_name);
    gtk_tree_view_get_cell_area (*fl, path, fl->priv->columns[GnomeCmdFileList::COLUMN_EXT], &rect_ext);
    gtk_tree_path_free (path);

    gtk_tree_view_convert_bin_window_to_widget_coords (*fl, rect_name.x, rect_name.y, &x, &y);

    width = rect_name.width;
    height = rect_name.height;
    if (gnome_cmd_data.options.ext_disp_mode != GNOME_CMD_EXT_DISP_BOTH)
        width += rect_ext.width;
}


void GnomeCmdFileList::focus_file_at_row (GtkTreeIter *row)
{
    GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), row);
    gtk_tree_view_set_cursor (*this, path, nullptr, false);
    gtk_tree_path_free (path);
}


guint GnomeCmdFileList::size()
{
    return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), nullptr);
}


void GnomeCmdFileList::select_file(GnomeCmdFile *f, GtkTreeIter *row)
{
    g_return_if_fail (f != nullptr);

    if (f->is_dotdot)
        return;

    GtkTreeIterPtr iter(nullptr, &gtk_tree_iter_free);
    if (row == nullptr)
    {
        iter = get_row_from_file (f);
        if (!iter)
            return;
        row = iter.get();
    }

    select_iter (row);
    g_signal_emit (this, signals[FILES_CHANGED], 0);
}


void GnomeCmdFileList::unselect_file(GnomeCmdFile *f, GtkTreeIter *row)
{
    g_return_if_fail (f != nullptr);

    GtkTreeIterPtr iter(nullptr, &gtk_tree_iter_free);
    if (row == nullptr)
    {
        iter = get_row_from_file (f);
        if (!iter)
            return;
        row = iter.get();
    }

    unselect_iter (row);
    g_signal_emit (this, signals[FILES_CHANGED], 0);
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


static void toggle_files_with_same_extension (GnomeCmdFileList *fl, gboolean select)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();
    if (!f) return;

    const gchar *ext1 = f->get_extension();
    if (!ext1) return;

    fl->traverse_files ([fl, ext1, select](GnomeCmdFile *ff, GtkTreeIter *iter, GtkListStore *store) {
        if (ff && ff->get_file_info())
        {
            const gchar *ext2 = ff->get_extension();

            if (ext2 && strcmp (ext1, ext2) == 0)
            {
                if (select)
                    fl->select_file(ff);
                else
                    fl->unselect_file(ff);
            }
        }
        return GnomeCmdFileList::TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::toggle_with_pattern(Filter &pattern, gboolean mode)
{
    if (gnome_cmd_data.options.select_dirs)
    {
        traverse_files ([this, &pattern, mode](GnomeCmdFile *f, GtkTreeIter *iter, GtkListStore *store) {
            if (f && f->get_file_info() && pattern.match(g_file_info_get_display_name(f->get_file_info())))
            {
                if (mode)
                    select_file(f);
                else
                    unselect_file(f);
            }
            return TRAVERSE_CONTINUE;
        });
    }
    else
    {
        traverse_files ([this, &pattern, mode](GnomeCmdFile *f, GtkTreeIter *iter, GtkListStore *store) {
            if (f && !GNOME_CMD_IS_DIR (f) && f->get_file_info() && pattern.match(g_file_info_get_display_name(f->get_file_info())))
            {
                if (mode)
                    select_file(f);
                else
                    unselect_file(f);
            }
            return TRAVERSE_CONTINUE;
        });
    }
}


void GnomeCmdFileList::create_column_titles()
{
    gtk_tree_view_set_headers_visible (*this, true);
}


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


static void show_file_popup (GnomeCmdFileList *fl, GdkPoint *point)
{
    // create the popup menu
    GMenu *menu_model = gnome_cmd_file_popmenu_new (fl);
    if (!menu_model) return;

    GtkWidget *popover = gtk_popover_new_from_model (GTK_WIDGET (fl), G_MENU_MODEL (menu_model));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

    GdkRectangle rect;
    if (point)
        rect = { point->x, point->y, 0, 0 };
    else
        get_focus_row_coordinates (fl, rect.x, rect.y, rect.width, rect.height);

    gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

    gtk_popover_popup (GTK_POPOVER (popover));
}


static gboolean on_right_mb_timeout (GnomeCmdFileList *fl)
{
    GnomeCmdFile *focus_file = fl->get_focused_file();

    if (fl->priv->right_mb_down_file == focus_file)
    {
        fl->select_file(focus_file);
        show_file_popup (fl, nullptr);
        return FALSE;
    }

    fl->priv->right_mb_down_file = focus_file;
    return TRUE;
}


struct PopupClosure
{
    GnomeCmdFileList *fl;
    GdkPoint point;
};


static gboolean on_right_mb (PopupClosure *closure)
{
    show_file_popup (closure->fl, &closure->point);
    g_free (closure);
    return G_SOURCE_REMOVE;
}


/******************************************************
 * File sorting functions
 **/

inline gint my_strcmp (const gchar *s1, const gchar *s2, gboolean raising)
{
    int ret = strcmp (s1, s2);

    if (ret > 0)
        return raising ? -1 : 1;

    if (ret < 0)
        return raising ? 1 : -1;

    return ret;
}


inline gint my_intcmp (gint i1, gint i2, gboolean raising)
{
    if (i1 > i2)
        return raising ? -1 : 1;

    if (i2 > i1)
        return raising ? 1 : -1;

    return 0;
}


inline gint my_filesizecmp (guint32 i1, guint32 i2, gboolean raising)
{
    if (i1 > i2)
        return raising ? -1 : 1;

    if (i2 > i1)
        return raising ? 1 : -1;

    return 0;
}


static gint sort_by_name (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    if(g_file_info_get_file_type(f1->get_file_info()) > g_file_info_get_file_type(f2->get_file_info()))
        return -1;

    if (g_file_info_get_file_type(f1->get_file_info()) < g_file_info_get_file_type(f2->get_file_info()))
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];

    return my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), raising);
}


static gint sort_by_ext (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    if(g_file_info_get_file_type(f1->get_file_info()) > g_file_info_get_file_type(f2->get_file_info()))
        return -1;

    if (g_file_info_get_file_type(f1->get_file_info()) < g_file_info_get_file_type(f2->get_file_info()))
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];

    if (!f1->get_extension() && !f2->get_extension())
        return my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), fl->priv->sort_raising[1]);

    if (!f1->get_extension())
        return raising?1:-1;
    if (!f2->get_extension())
        return raising?-1:1;

    gint ret = my_strcmp (f1->get_extension(), f2->get_extension(), raising);

    return ret ? ret : my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), fl->priv->sort_raising[1]);
}


static gint sort_by_dir (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    if (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE)
        > f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE))
        return -1;

    if (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE)
        < f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE))
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gchar *dirname1 = f1->get_dirname();
    gchar *dirname2 = f2->get_dirname();

    gint ret = my_strcmp (dirname1, dirname2, raising);

    g_free (dirname1);
    g_free (dirname2);

    if (!ret)
        ret = my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), raising);

    return ret;
}


static gint sort_by_size (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    // Check if both items are directories. In this case, just compare their names.
    if ((f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        && (f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY))
    {
        return my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), file_raising);
    }
    // If just one item is a directory, return a fixed value.
    if (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
    {
        return -1;
    }
    if (f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
    {
        return 1;
    }

    gint ret = my_intcmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE),
                          f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE), TRUE);

    if (!ret)
    {
        ret = my_filesizecmp (f1->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_STANDARD_SIZE),
                              f2->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_STANDARD_SIZE), raising);

        if (!ret)
            ret = my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), file_raising);
    }
    return ret;
}


static gint sort_by_perm (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE),
                          f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE), TRUE);
    if (!ret)
    {
        ret = my_intcmp (get_gfile_attribute_uint32(f1->get_file(), G_FILE_ATTRIBUTE_UNIX_MODE),
                         get_gfile_attribute_uint32(f2->get_file(), G_FILE_ATTRIBUTE_UNIX_MODE), raising);
        if (!ret)
            ret = my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), file_raising);
    }
    return ret;
}


static gint sort_by_date (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE),
                          f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE), TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_TIME_MODIFIED),
                         f2->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_TIME_MODIFIED), raising);
        if (!ret)
            ret = my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), file_raising);
    }
    return ret;
}


static gint sort_by_owner (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE),
                          f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE), TRUE);
    if (!ret)
    {
#ifdef linux
        ret = my_intcmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_UID),
                         f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_UID), raising);
#else
        ret = my_strcmp (f1->GetGfileAttributeString(G_FILE_ATTRIBUTE_OWNER_USER),
                         f2->GetGfileAttributeString(G_FILE_ATTRIBUTE_OWNER_USER), raising)
#endif
        if (!ret)
            ret = my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), file_raising);
    }
    return ret;
}


static gint sort_by_group (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE),
                          f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE), TRUE);
    if (!ret)
    {
#ifdef linux
        ret = my_intcmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_GID),
                         f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_UNIX_GID), raising);
#else
        ret = my_strcmp (f1->GetGfileAttributeString(G_FILE_ATTRIBUTE_OWNER_GROUP),
                         f2->GetGfileAttributeString(G_FILE_ATTRIBUTE_OWNER_GROUP), raising)
#endif
        if (!ret)
            ret = my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), file_raising);
    }
    return ret;
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

    fl->priv->sort_raising[col] = fl->priv->current_col == col ? !fl->priv->sort_raising[col] :
                                                                 (static_cast<bool>(file_list_column[col].default_sort_direction));

    fl->priv->sort_func = file_list_column[col].sort_func;
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

    gtk_tree_view_get_cursor (*this, &path, nullptr);
    gtk_tree_path_prev (path);
    gtk_tree_view_set_cursor (*this, path, nullptr, false);
    gtk_tree_path_free (path);
}


void GnomeCmdFileList::focus_next()
{
    GtkTreePath *path;

    gtk_tree_view_get_cursor (*this, &path, nullptr);
    gtk_tree_path_next (path);
    gtk_tree_view_set_cursor (*this, path, nullptr, false);
    gtk_tree_path_free (path);
}


static void on_button_press (GtkGestureMultiPress *gesture, int n_press, double x, double y, GnomeCmdFileList *fl)
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
        g_signal_emit (fl, signals[EMPTY_SPACE_CLICKED], 0, &event);
    }
}


static void do_mime_exec_single (gpointer *args)
{
    g_return_if_fail (args != nullptr);

    auto gnomeCmdApp = static_cast<GnomeCmdApp*> (args[0]);
    auto path = (gchar *) args[1];
    auto dpath = (gchar *) args[2];

    auto gnomeCmdFile = gnome_cmd_file_new(path);

    if (gnomeCmdFile == nullptr)
        return;

    gnome_cmd_file_ref(gnomeCmdFile);
    GList *gFileList = nullptr;
    gFileList = g_list_append(gFileList, gnomeCmdFile->get_file());
    g_app_info_launch (gnomeCmdFile->GetAppInfoForContentType(), gFileList, nullptr, nullptr);

    g_list_free(gFileList);
    gnome_cmd_file_unref(gnomeCmdFile);
    g_free (path);
    g_free (dpath);
    gnome_cmd_app_free (gnomeCmdApp);
    g_free (args);
}


static void on_tmp_download_response (GtkWidget *w, gint id, TmpDlData *dldata)
{
    if (id == GTK_RESPONSE_YES)
    {
        gchar *path_str = get_temp_download_filepath (dldata->f->get_name());

        if (!path_str) return;

        dldata->args[1] = (gpointer) path_str;

        auto sourceGFile = g_file_dup (dldata->f->get_gfile());
        GnomeCmdPlainPath path(path_str);
        auto destGFile = gnome_cmd_con_create_gfile (get_home_con (), &path);

        gnome_cmd_tmp_download (*main_win,
                                g_list_append (nullptr, sourceGFile),
                                g_list_append (nullptr, destGFile),
                                G_FILE_COPY_OVERWRITE,
                                G_CALLBACK (do_mime_exec_single),
                                dldata->args);
    }
    else
    {
        gnome_cmd_app_free (static_cast<GnomeCmdApp*> (dldata->args[0]));
        g_free (dldata->args);
    }

    gtk_window_destroy (GTK_WINDOW (dldata->dialog));
    g_free (dldata);
}


static void mime_exec_single (GtkWindow *parent_window, GnomeCmdFile *f)
{
    g_return_if_fail (f != nullptr);
    g_return_if_fail (f->get_file_info() != nullptr);

    gpointer *args;
    GnomeCmdApp *app;

    // Check if the file is a binary executable that lacks the executable bit

    if (!f->is_executable())
    {
        if (f->has_content_type("application/x-executable") || f->has_content_type("application/x-executable-binary"))
        {
            gchar *msg = g_strdup_printf (_("“%s” seems to be a binary executable file but it lacks the executable bit. Do you want to set it and then run the file?"), f->get_name());
            gint ret = run_simple_dialog (parent_window, FALSE, GTK_MESSAGE_QUESTION, msg,
                                          _("Make Executable?"),
                                          -1, _("Cancel"), _("OK"), nullptr);
            g_free (msg);

            if (ret != 1)
            {
                return;
            }

           if(!f->chmod(get_gfile_attribute_uint32(f->get_file(), G_FILE_ATTRIBUTE_UNIX_MODE) | GNOME_CMD_PERM_USER_EXEC, nullptr))
           {
               return;
           }
        }
    }

    // If the file is executable but not a binary file, check if the user wants to exec it or open it

    if (f->is_executable())
    {
        if (f->has_content_type("application/x-executable") || f->has_content_type("application/x-executable-binary"))
        {
            f->execute();
            return;
        }
        else
            if (f->content_type_begins_with("text/"))
            {
                gchar *msg = g_strdup_printf (_("“%s” is an executable text file. Do you want to run it, or display its contents?"), f->get_name());
                gint ret = run_simple_dialog (parent_window, FALSE, GTK_MESSAGE_QUESTION, msg, _("Run or Display"),
                                              -1, _("Cancel"), _("Display"), _("Run"), nullptr);
                g_free (msg);

                if (ret != 1)
                {
                    if (ret == 2)
                        f->execute();
                    return;
                }
            }
    }

    auto gAppInfo = f->GetAppInfoForContentType();
    if (gAppInfo == nullptr)
    {
        auto contentType = f->GetGfileAttributeString(G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
        gchar *msg = g_strdup_printf (_("No default application found for the file type %s."), contentType);
        gnome_cmd_show_message (parent_window, msg, _("Open the \"Applications\" page in the Control Center to add one."));
        g_free (contentType);
        g_free (msg);
        return;
    }

    app = gnome_cmd_app_new_from_app_info (gAppInfo);

    args = g_new0 (gpointer, 3);

    if (f->is_local())
    {
        GList *gFileList = nullptr;
        gFileList = g_list_append(gFileList, f->get_file());
        g_app_info_launch (gAppInfo, gFileList, nullptr, nullptr);
        g_list_free(gFileList);
    }
    else
    {
        if (gnome_cmd_app_get_handles_uris (app) && gnome_cmd_data.options.honor_expect_uris)
        {
            auto gFileFromUri = g_file_new_for_uri(f->get_uri_str());
            GList *gFileList = nullptr;
            gFileList = g_list_append(gFileList, gFileFromUri);
            g_app_info_launch (gAppInfo, gFileList, nullptr, nullptr);
            g_object_unref(gFileFromUri);
            g_list_free(gFileList);
            gnome_cmd_app_free(app);
        }
        else
        {
            gchar *msg = g_strdup_printf (_("%s does not know how to open remote file. Do you want to download the file to a temporary location and then open it?"), gnome_cmd_app_get_name (app));
            GtkWidget *dialog = gtk_message_dialog_new (parent_window, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", msg);
            TmpDlData *dldata = g_new0 (TmpDlData, 1);
            args[0] = (gpointer) app;
            // args[2] is NULL here (don't set exec dir for temporarily downloaded files)
            dldata->f = f;
            dldata->dialog = dialog;
            dldata->args = args;

            g_signal_connect (dialog, "response", G_CALLBACK (on_tmp_download_response), dldata);
            gtk_widget_show (dialog);
            g_free (msg);
        }
    }
}


inline gboolean mime_exec_file (GtkWindow *parent_window, GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, FALSE);

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_REGULAR)
    {
        mime_exec_single (parent_window, f);
        return TRUE;
    }
    return FALSE;
}


static void on_file_clicked (GnomeCmdFileList *fl, GnomeCmdFileListButtonEvent *event, gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (event != nullptr);
    g_return_if_fail (GNOME_CMD_IS_FILE (event->file));

    fl->modifier_click = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK);

    if (event->n_press == 2 && event->button == 1 && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
    {
        mime_exec_file (get_toplevel_window (*fl), event->file);
    }
    else
        if (event->n_press == 1 && (event->button == 1 || event->button == 3))
        {
            if (event->button == 1)
            {
                if (event->state & GDK_SHIFT_MASK)
                {
                    select_file_range (fl, fl->priv->shift_down_row.get(), event->iter);
                    fl->priv->shift_down = FALSE;
                    fl->priv->shift_down_key = 0;
                }
                else
                    if (event->state & GDK_CONTROL_MASK)
                    {
                        bool has_selection = fl->get_first_selected_file() != nullptr;
                        if (has_selection || gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
                            fl->toggle_file (event->iter);
                        else
                        {
                            auto prev_row = fl->get_focused_file_iter();
                            if (prev_row.get() != event->iter)
                                fl->select_file(fl->get_file_at_row(prev_row.get()), prev_row.get());
                            fl->select_file(fl->get_file_at_row(event->iter), event->iter);
                        }
                    }
            }
            else
                if (event->button == 3)
                    if (!event->file->is_dotdot)
                    {
                        if (gnome_cmd_data.options.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
                        {
                            if (!fl->is_selected_iter(event->iter))
                            {
                                fl->select_iter(event->iter);
                                fl->priv->right_mb_sel_state = 1;
                            }
                            else
                            {
                                fl->unselect_iter(event->iter);
                                fl->priv->right_mb_sel_state = 0;
                            }

                            fl->priv->right_mb_down_file = event->file;
                            fl->priv->right_mb_timeout_id =
                                g_timeout_add (POPUP_TIMEOUT, (GSourceFunc) on_right_mb_timeout, fl);
                        }
                        else
                        {
                            PopupClosure *closure = g_new0 (PopupClosure, 1);
                            closure->fl = fl;
                            closure->point.x = event->x;
                            closure->point.y = event->y;
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
        mime_exec_file (get_toplevel_window (*fl), event->file);
}


static void on_button_release (GtkGestureMultiPress *gesture, int n_press, double x, double y, GnomeCmdFileList *fl)
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
    else if (button == 3 && fl->priv->right_mb_timeout_id > 0)
    {
        g_source_remove (fl->priv->right_mb_timeout_id);
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


static void on_dir_list_ok (GnomeCmdDir *dir, GList *files, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_ok\n");

    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (fl->realized)
    {
        gtk_widget_set_sensitive (*fl, TRUE);
        set_cursor_default_for_widget (*fl);
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
        gnome_cmd_show_message (get_toplevel_window (*fl), _("Directory listing failed."), error->message);

    g_signal_handlers_disconnect_matched (fl->cwd, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, fl);
    fl->connected_dir = nullptr;
    gnome_cmd_dir_unref (fl->cwd);
    set_cursor_default_for_widget (*fl);
    gtk_widget_set_sensitive (*fl, TRUE);

    if (fl->lwd && fl->con == gnome_cmd_dir_get_connection (fl->lwd))
    {
        fl->cwd = fl->lwd;
        g_signal_connect (fl->cwd, "list-ok", G_CALLBACK (on_dir_list_ok), fl);
        g_signal_connect (fl->cwd, "list-failed", G_CALLBACK (on_dir_list_failed), fl);
        fl->lwd = nullptr;
    }
    else
        g_timeout_add (1, (GSourceFunc) set_home_connection, fl);
}


static void on_con_open_done (GnomeCmdCon *con, GnomeCmdFileList *fl)
{
    DEBUG('m', "on_con_open_done\n");

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (fl->priv->con_opening != nullptr);
    g_return_if_fail (fl->priv->con_opening == con);
    g_return_if_fail (fl->priv->con_open_dialog != nullptr);

    g_signal_handlers_disconnect_matched (con, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, fl);

    fl->set_connection (con);

    gtk_window_destroy (GTK_WINDOW (fl->priv->con_open_dialog));
    fl->priv->con_open_dialog = nullptr;
    fl->priv->con_opening = nullptr;
}


static void on_con_open_failed (GnomeCmdCon *con, GnomeCmdFileList *fl)
{
    DEBUG('m', "on_con_open_failed\n");

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (fl->priv->con_opening != nullptr);
    g_return_if_fail (fl->priv->con_opening == con);
    g_return_if_fail (fl->priv->con_open_dialog != nullptr);

    g_signal_handlers_disconnect_matched (con, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, fl);

    gnome_cmd_show_message (get_toplevel_window (*fl), _("Failed to open connection."), con->open_failed_msg);

    fl->priv->con_open_dialog = nullptr;
    fl->priv->con_opening = nullptr;
}


static void on_con_open_cancel (GtkButton *button, GnomeCmdFileList *fl)
{
    DEBUG('m', "on_con_open_cancel\n");

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (fl->priv->con_opening != nullptr);
    g_return_if_fail (fl->priv->con_opening->state == GnomeCmdCon::STATE_OPENING);

    gnome_cmd_con_cancel_open (fl->priv->con_opening);

    gtk_window_destroy (GTK_WINDOW (fl->priv->con_open_dialog));
    fl->priv->con_open_dialog = nullptr;
    fl->priv->con_opening = nullptr;
}


static gboolean update_con_open_progress (GnomeCmdFileList *fl)
{
    if (!fl->priv->con_open_dialog)
        return FALSE;

    const gchar *msg = gnome_cmd_con_get_open_msg (fl->priv->con_opening);
    gtk_label_set_text (GTK_LABEL (fl->priv->con_open_dialog_label), msg);
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (fl->priv->con_open_dialog_pbar));

    return TRUE;
}


static void create_con_open_progress_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    fl->priv->con_open_dialog = gnome_cmd_dialog_new (get_toplevel_window (*fl), nullptr);
    g_object_ref (fl->priv->con_open_dialog);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (fl->priv->con_open_dialog),
                                 _("_Cancel"),
                                 G_CALLBACK (on_con_open_cancel), fl);

    GtkWidget *vbox = create_vbox (fl->priv->con_open_dialog, FALSE, 12);

    fl->priv->con_open_dialog_label = create_label (fl->priv->con_open_dialog, "");

    fl->priv->con_open_dialog_pbar = create_progress_bar (fl->priv->con_open_dialog);
    gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (fl->priv->con_open_dialog_pbar), FALSE);
    gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (fl->priv->con_open_dialog_pbar), 1.0 / (gdouble) FL_PBAR_MAX);

    gtk_box_append (GTK_BOX (vbox), fl->priv->con_open_dialog_label);
    gtk_box_append (GTK_BOX (vbox), fl->priv->con_open_dialog_pbar);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (fl->priv->con_open_dialog), vbox);

    gtk_widget_show_all (fl->priv->con_open_dialog);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_file_list_finalize (GObject *object)
{
    GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (object);

    delete fl->priv;

    G_OBJECT_CLASS (gnome_cmd_file_list_parent_class)->finalize (object);
}


static void gnome_cmd_file_list_class_init (GnomeCmdFileListClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_file_list_finalize;

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

    signals[EMPTY_SPACE_CLICKED] =
        g_signal_new ("empty-space-clicked",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, empty_space_clicked),
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


static void gnome_cmd_file_list_init (GnomeCmdFileList *fl)
{
    fl->priv = new GnomeCmdFileList::Private(fl);

    auto action_group = g_simple_action_group_new ();
    static const GActionEntry action_entries[] = {
        { "open-with-default",  gnome_cmd_file_selector_action_open_with_default,   nullptr, nullptr, nullptr },
        { "open-with-other",    gnome_cmd_file_selector_action_open_with_other,     nullptr, nullptr, nullptr },
        { "open-with",          gnome_cmd_file_selector_action_open_with,           "s",     nullptr, nullptr },
        { "execute",            gnome_cmd_file_selector_action_execute,             nullptr, nullptr, nullptr },
        { "execute-script",     gnome_cmd_file_selector_action_execute_script,      "(sb)",  nullptr, nullptr }
    };
    g_action_map_add_action_entries (G_ACTION_MAP (action_group), action_entries, G_N_ELEMENTS (action_entries), fl);
    gtk_widget_insert_action_group (GTK_WIDGET (fl), "fl", G_ACTION_GROUP (action_group));

    fl->init_dnd();

    GtkGesture *click_controller = gtk_gesture_multi_press_new (GTK_WIDGET (fl));
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_controller), 0);

    g_signal_connect (click_controller, "pressed", G_CALLBACK (on_button_press), fl);
    g_signal_connect (click_controller, "released", G_CALLBACK (on_button_release), fl);
    g_signal_connect (fl, "cursor-changed", G_CALLBACK (on_cursor_change), fl);

    g_signal_connect_after (fl, "realize", G_CALLBACK (on_realize), fl);
    g_signal_connect (fl, "file-clicked", G_CALLBACK (on_file_clicked), fl);
    g_signal_connect (fl, "file-released", G_CALLBACK (on_file_released), fl);
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
        if (priv->sort_func (f2, f, this) == 1)
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

    if (!files)
        return;

    files = g_list_sort_with_data (files, (GCompareDataFunc) priv->sort_func, this);

    for (auto i = files; i; i = i->next)
        append_file(GNOME_CMD_FILE (i->data));

    if (files)
        g_list_free (files);
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
    gnome_cmd_dir_relist_files (get_toplevel_window (*this), cwd, gnome_cmd_con_needs_list_visprog (con));
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
            files = g_list_append (nullptr, f);
    }
    return files;
}


GnomeCmd::Collection<GnomeCmdFile *> GnomeCmdFileList::get_marked_files()
{
    GnomeCmd::Collection<GnomeCmdFile *> files;
    traverse_files([this, &files](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        if (is_selected_iter(iter))
            files.insert(file);
        return TraverseControl::TRAVERSE_CONTINUE;
    });
    return files;
}


std::vector<GnomeCmdFile *> GnomeCmdFileList::get_all_files()
{
    std::vector<GnomeCmdFile *> files;
    traverse_files([&files](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        files.push_back(file);
        return TraverseControl::TRAVERSE_CONTINUE;
    });
    return files;
}


GList *GnomeCmdFileList::get_visible_files()
{
    GList *files = nullptr;
    traverse_files([&files](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
        files = g_list_append (files, file);
        return TraverseControl::TRAVERSE_CONTINUE;
    });
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
    gtk_tree_view_get_cursor (*this, &path, NULL);
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


void GnomeCmdFileList::select_iter(GtkTreeIter *iter)
{
    gtk_list_store_set (priv->store, iter, DATA_COLUMN_SELECTED, TRUE, -1);
}


void GnomeCmdFileList::unselect_iter(GtkTreeIter *iter)
{
    gtk_list_store_set (priv->store, iter, DATA_COLUMN_SELECTED, FALSE, -1);
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
        if (GNOME_CMD_IS_DIR(file))
           unselect_iter(iter);
        else
           select_iter(iter);
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
            select_file(file);
            return TRAVERSE_CONTINUE;
        });
    }
    else
    {
        traverse_files ([this](GnomeCmdFile *file, GtkTreeIter *iter, GtkListStore *store) {
            if (!GNOME_CMD_IS_DIR (file))
                select_file(file);
            return TRAVERSE_CONTINUE;
        });
    }
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
                gtk_tree_view_scroll_to_cell (*this, path, nullptr, false, 0.0, 0.0);
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


void GnomeCmdFileList::invert_selection()
{
    traverse_files ([this](GnomeCmdFile *f, GtkTreeIter *iter, GtkListStore *store) {
        if (f && f->get_file_info() && (gnome_cmd_data.options.select_dirs || !GNOME_CMD_IS_DIR (f)))
        {
            if (!is_selected_iter(iter))
                select_iter(iter);
            else
                unselect_iter(iter);
        }
        return TRAVERSE_CONTINUE;
    });
}


void GnomeCmdFileList::select_all_with_same_extension()
{
    toggle_files_with_same_extension (this, TRUE);
}


void GnomeCmdFileList::unselect_all_with_same_extension()
{
    toggle_files_with_same_extension (this, FALSE);
}


void GnomeCmdFileList::restore_selection()
{
}


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

        return priv->sort_func(filea, fileb, this) < 0;
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

        gtk_widget_show_all (popover);
        gtk_popover_popup (GTK_POPOVER (popover));
    }
}


void gnome_cmd_file_list_show_delete_dialog (GnomeCmdFileList *fl, gboolean forceDelete)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();

    if (files)
    {
        gnome_cmd_delete_dialog_show (get_toplevel_window (*fl), files, forceDelete);
    }
}


void gnome_cmd_file_list_show_properties_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (f)
        gnome_cmd_file_show_properties (f);
}


void gnome_cmd_file_list_show_selpat_dialog (GnomeCmdFileList *fl, gboolean mode)
{
    if (fl->priv->selpat_dialog)  return;

    GtkWidget *dialog = gnome_cmd_patternsel_dialog_new (fl, mode);

    g_object_ref (dialog);
    g_signal_connect (dialog, "hide", G_CALLBACK (on_selpat_hide), fl);
    gtk_widget_show (dialog);

    fl->priv->selpat_dialog = dialog;
}


void gnome_cmd_file_list_cap_cut (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();

    if (files)
    {
        cap_cut_files (fl, files);
        g_list_free (files);
    }
}


void gnome_cmd_file_list_cap_copy (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();

    if (files)
    {
        cap_copy_files (fl, files);
        g_list_free (files);
    }
}


void gnome_cmd_file_list_view (GnomeCmdFileList *fl, bool useInternalViewer)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (!f)
    {
        return;
    }

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
    {
        gnome_cmd_show_message (get_toplevel_window (*fl), _("Not an ordinary file."), g_file_info_get_display_name(f->get_file_info()));
        return;
    }

    switch (useInternalViewer)
    {
        case TRUE:
        {
            gnome_cmd_file_view_internal(f);
            break;
        }
        case FALSE:
        {
            gnome_cmd_file_view_external(f);
            break;
        }
    }
}


void gnome_cmd_file_list_edit (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (!f)  return;

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        gnome_cmd_show_message (get_toplevel_window (*fl), _("Not an ordinary file."), g_file_info_get_display_name(f->get_file_info()));
    else
        gnome_cmd_file_edit (f);
}


gboolean gnome_cmd_file_list_quicksearch_shown (GnomeCmdFileList *fl)
{
    g_return_val_if_fail (fl!=nullptr, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);
    g_return_val_if_fail (fl->priv!=nullptr, FALSE);

    auto quicksearch_popup = g_weak_ref_get (&fl->priv->quicksearch_popup);
    if (quicksearch_popup == nullptr)
        return FALSE;

    gboolean shown = gtk_widget_get_visible (GTK_WIDGET (quicksearch_popup));
    g_object_unref (quicksearch_popup);
    return shown;
}


void gnome_cmd_file_list_show_quicksearch (GnomeCmdFileList *fl, gchar c)
{
    if (gnome_cmd_file_list_quicksearch_shown (fl))
        return;

    auto popup = gnome_cmd_quicksearch_popup_new (fl);
    g_weak_ref_set (&fl->priv->quicksearch_popup, g_object_ref (popup));
    gtk_popover_popup (GTK_POPOVER (popup));

    gnome_cmd_quicksearch_popup_set_char (GNOME_CMD_QUICKSEARCH_POPUP (popup), c);
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
            return (state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);

        case GNOME_CMD_QUICK_SEARCH_ALT:
            return !(state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);

        case GNOME_CMD_QUICK_SEARCH_JUST_A_CHARACTER:
            return !(state & GDK_CONTROL_MASK) && !(state & GDK_MOD1_MASK);

        default:
            return false;
    }
}


gboolean GnomeCmdFileList::key_pressed(GnomeCmdKeyPress *event)
{
    g_return_val_if_fail (event != nullptr, FALSE);

    if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                gnome_cmd_file_list_show_properties_dialog (this);
                return TRUE;

            case GDK_KEY_KP_Add:
                toggle_files_with_same_extension (this, TRUE);
                break;

            case GDK_KEY_KP_Subtract:
                toggle_files_with_same_extension (this, FALSE);
                break;

            default:
                break;
        }
    }
    else if (state_is_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_F6:
                gnome_cmd_file_list_show_rename_dialog (this);
                return TRUE;

            case GDK_KEY_F10:
                show_file_popup (this, nullptr);
                return TRUE;

            case GDK_KEY_Left:
            case GDK_KEY_KP_Left:
            case GDK_KEY_Right:
            case GDK_KEY_KP_Right:
                return FALSE;

            case GDK_KEY_Page_Up:
            case GDK_KEY_KP_Page_Up:
            case GDK_KEY_KP_9:
                priv->shift_down_key = 3;
                priv->shift_down_row.reset (gtk_tree_iter_copy (get_focused_file_iter().get()));
                return FALSE;

            case GDK_KEY_Page_Down:
            case GDK_KEY_KP_Page_Down:
            case GDK_KEY_KP_3:
                priv->shift_down_key = 4;
                priv->shift_down_row.reset (gtk_tree_iter_copy (get_focused_file_iter().get()));
                return FALSE;

            case GDK_KEY_Up:
            case GDK_KEY_KP_Up:
            case GDK_KEY_KP_8:
            case GDK_KEY_Down:
            case GDK_KEY_KP_Down:
            case GDK_KEY_KP_2:
                priv->shift_down_key = 1; // 2
                if (auto focused = get_focused_file_iter())
                    toggle_file(focused.get());
                return FALSE;

            case GDK_KEY_Home:
            case GDK_KEY_KP_Home:
            case GDK_KEY_KP_7:
                priv->shift_down_key = 5;
                priv->shift_down_row.reset (gtk_tree_iter_copy (get_focused_file_iter().get()));
                return FALSE;

            case GDK_KEY_End:
            case GDK_KEY_KP_End:
            case GDK_KEY_KP_1:
                priv->shift_down_key = 6;
                priv->shift_down_row.reset (gtk_tree_iter_copy (get_focused_file_iter().get()));
                return FALSE;

            case GDK_KEY_Delete:
            case GDK_KEY_KP_Delete:
                gnome_cmd_file_list_show_delete_dialog (this, TRUE);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_alt_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                show_visible_tree_sizes();
                return TRUE;
            default:
                break;
        }
    }
    else if (state_is_ctrl (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_X:
            case GDK_KEY_x:
                gnome_cmd_file_list_cap_cut (this);
                return TRUE;

            case GDK_KEY_C:
            case GDK_KEY_c:
                gnome_cmd_file_list_cap_copy (this);
                return TRUE;

            case GDK_KEY_F3:
                on_column_clicked (priv->columns[COLUMN_NAME], this);
                return TRUE;

            case GDK_KEY_F4:
                on_column_clicked (priv->columns[COLUMN_EXT], this);
                return TRUE;

            case GDK_KEY_F5:
                on_column_clicked (priv->columns[COLUMN_DATE], this);
                return TRUE;

            case GDK_KEY_F6:
                on_column_clicked (priv->columns[COLUMN_SIZE], this);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_blank (event->state))
    {
        switch (event->keyval)
        {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                return mime_exec_file (get_toplevel_window (*this), get_focused_file());

            case GDK_KEY_space:
                set_cursor_busy ();
                toggle();
                if (GnomeCmdFile *selfile = get_selected_file())
                    show_dir_tree_size(selfile);
                g_signal_emit (this, signals[FILES_CHANGED], 0);
                set_cursor_default ();
                return TRUE;

            case GDK_KEY_KP_Add:
            case GDK_KEY_plus:
            case GDK_KEY_equal:
                gnome_cmd_file_list_show_selpat_dialog (this, TRUE);
                return TRUE;

            case GDK_KEY_KP_Subtract:
            case GDK_KEY_minus:
                gnome_cmd_file_list_show_selpat_dialog (this, FALSE);
                return TRUE;

            case GDK_KEY_KP_Multiply:
                invert_selection();
                return TRUE;

            case GDK_KEY_KP_Divide:
                restore_selection();
                return TRUE;

            case GDK_KEY_Insert:
            case GDK_KEY_KP_Insert:
                toggle();
                {
                    auto iter = get_focused_file_iter();
                    if (iter && gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), iter.get()))
                        focus_file_at_row (iter.get());
                }
                return TRUE;

            case GDK_KEY_KP_Page_Up:
                event->keyval = GDK_KEY_Page_Up;
                return FALSE;

            case GDK_KEY_KP_Page_Down:
                event->keyval = GDK_KEY_Page_Down;
                return FALSE;

            case GDK_KEY_KP_Up:
                event->keyval = GDK_KEY_Up;
                return FALSE;

            case GDK_KEY_KP_Down:
                event->keyval = GDK_KEY_Down;
                return FALSE;

            case GDK_KEY_Home:
            case GDK_KEY_KP_Home:
                {
                    GtkTreeIter iter;
                    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter))
                        focus_file_at_row (&iter);
                }
                return TRUE;

            case GDK_KEY_End:
            case GDK_KEY_KP_End:
                {
                    gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL (priv->store), nullptr);
                    if (n > 0) {
                        GtkTreePath *path = gtk_tree_path_new_from_indices (n - 1, -1);
                        gtk_tree_view_set_cursor (*this, path, nullptr, FALSE);
                        gtk_tree_path_free (path);
                    }
                }
                return TRUE;

            case GDK_KEY_Delete:
            case GDK_KEY_KP_Delete:
                gnome_cmd_file_list_show_delete_dialog (this);
                return TRUE;

            case GDK_KEY_Shift_L:
            case GDK_KEY_Shift_R:
                if (!priv->shift_down)
                    priv->shift_down_row = get_focused_file_iter();
                return TRUE;

            case GDK_KEY_Menu:
                show_file_popup (this, nullptr);
                return TRUE;

            default:
                break;
        }
    }

    if (is_quicksearch_starting_modifier (event->state) && is_quicksearch_starting_character (event->keyval))
    {
        gnome_cmd_file_list_show_quicksearch (this, (gchar) event->keyval);
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
        g_signal_connect (new_con, "open-done", G_CALLBACK (on_con_open_done), this);
        g_signal_connect (new_con, "open-failed", G_CALLBACK (on_con_open_failed), this);
        priv->con_opening = new_con;

        create_con_open_progress_dialog (this);
        g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_con_open_progress, this);

        gnome_cmd_con_open (new_con);

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
        if (gnome_cmd_dir_is_local (lwd) && !gnome_cmd_dir_is_monitored (lwd) && gnome_cmd_dir_needs_mtime_update (lwd))
            gnome_cmd_dir_update_mtime (lwd);
    }

    cwd = dir;

    switch (dir->state)
    {
        case GnomeCmdDir::STATE_EMPTY:
            g_signal_connect (dir, "list-ok", G_CALLBACK (on_dir_list_ok), this);
            g_signal_connect (dir, "list-failed", G_CALLBACK (on_dir_list_failed), this);
            gnome_cmd_dir_list_files (get_toplevel_window (*this), dir, gnome_cmd_con_needs_list_visprog (con));
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
            if (gnome_cmd_dir_is_local (dir) && !gnome_cmd_dir_is_monitored (dir) && gnome_cmd_dir_update_mtime (dir))
                gnome_cmd_dir_relist_files (get_toplevel_window (*this), dir, gnome_cmd_con_needs_list_visprog (con));
            else
                on_dir_list_ok (dir, nullptr, this);
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
    // TODO: update CSS according to a selected theme

    PangoFontDescription *font_desc = pango_font_description_from_string (gnome_cmd_data.options.list_font);
    gtk_widget_override_font (*this, font_desc);
    pango_font_description_free (font_desc);
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
#ifdef HAVE_SAMBA
            if (g_str_has_prefix (dir, "\\\\"))
            {
                GnomeCmdPath *path = gnome_cmd_con_create_path (get_smb_con (), dir);
                if (path)
                    new_dir = gnome_cmd_dir_new (get_smb_con (), path);
            }
            else
#endif
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

    GdkModifierType mask;

    gdk_display_get_pointer (gdk_display_get_default (), nullptr, nullptr, nullptr, &mask);

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


void GnomeCmdFileList::init_dnd()
{
    // set up drag source
    gtk_tree_view_enable_model_drag_source (*this,
                                            GDK_BUTTON1_MASK,
                                            drag_types, G_N_ELEMENTS (drag_types),
                                            (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK));

    g_signal_connect (this, "drag-data-get", G_CALLBACK (drag_data_get), this);
    g_signal_connect (this, "drag-data-delete", G_CALLBACK (drag_data_delete), this);

    // set up drag destination
    gtk_tree_view_enable_model_drag_dest (*this,
                                          drop_types, G_N_ELEMENTS (drop_types),
                                          (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK));

    g_signal_connect (this, "drag-motion", G_CALLBACK (drag_motion), this);
    g_signal_connect (this, "drag-data-received", G_CALLBACK (drag_data_received), this);
}

void GnomeCmdFileList::drop_files(DndMode dndMode, GFileCopyFlags gFileCopyFlags, GList *gFileGlist, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    switch (dndMode)
    {
        case COPY:
            gnome_cmd_copy_gfiles_start (gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         nullptr,
                                         g_list_length (gFileGlist) == 1 ? g_file_get_basename ((GFile *) gFileGlist->data) : nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         nullptr,
                                         nullptr);
            break;
        case MOVE:
            gnome_cmd_move_gfiles_start (gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         nullptr,
                                         g_list_length (gFileGlist) == 1 ? g_file_get_basename ((GFile *) gFileGlist->data) : nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         nullptr,
                                         nullptr);
            break;
        case LINK:
            gnome_cmd_link_gfiles_start (gFileGlist,
                                         gnome_cmd_dir_ref (dir),
                                         nullptr,
                                         nullptr,
                                         g_list_length (gFileGlist) == 1 ? g_file_get_basename ((GFile *) gFileGlist->data) : nullptr,
                                         gFileCopyFlags,
                                         GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                         nullptr,
                                         nullptr);
            break;
        default:
            return;
    }
}

GtkTreeIterPtr GnomeCmdFileList::get_dest_row_at_pos (gint drag_x, gint drag_y)
{
    gint wx, wy;
    gtk_tree_view_convert_bin_window_to_widget_coords (*this, drag_x, drag_y, &wx, &wy);
    return get_dest_row_at_coords (wx, wy);
}

GtkTreeIterPtr GnomeCmdFileList::get_dest_row_at_coords (gdouble x, gdouble y)
{
    GtkTreePath *path;
    if (!gtk_tree_view_get_dest_row_at_pos (*this, x, y, &path, nullptr))
        return GtkTreeIterPtr(nullptr, &gtk_tree_iter_free);

    GtkTreeIter iter;
    bool found = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
    gtk_tree_path_free (path);
    if (found)
        return GtkTreeIterPtr(gtk_tree_iter_copy(&iter), &gtk_tree_iter_free);
    else
        return GtkTreeIterPtr(nullptr, &gtk_tree_iter_free);
}

