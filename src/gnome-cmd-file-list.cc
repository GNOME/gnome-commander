/**
 * @file gnome-cmd-file-list.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-plain-path.h"
#include "utils.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-xfer.h"
#include "imageloader.h"
#include "cap.h"
#include "gnome-cmd-style.h"
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
    GtkJustification justification;
    GtkSortType default_sort_direction;
    GCompareDataFunc sort_func;
};


static gint sort_by_name (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_ext (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_dir (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_size (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_date (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_perm (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_owner (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_group (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);


static GnomeCmdFileListColumn file_list_column[GnomeCmdFileList::NUM_COLUMNS] =
{{GnomeCmdFileList::COLUMN_ICON,"",GTK_JUSTIFY_CENTER,GTK_SORT_ASCENDING, nullptr},
 {GnomeCmdFileList::COLUMN_NAME, N_("name"), GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_name},
 {GnomeCmdFileList::COLUMN_EXT, N_("ext"), GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_ext},
 {GnomeCmdFileList::COLUMN_DIR, N_("dir"), GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_dir},
 {GnomeCmdFileList::COLUMN_SIZE, N_("size"), GTK_JUSTIFY_RIGHT, GTK_SORT_DESCENDING, (GCompareDataFunc) sort_by_size},
 {GnomeCmdFileList::COLUMN_DATE, N_("date"), GTK_JUSTIFY_LEFT, GTK_SORT_DESCENDING, (GCompareDataFunc) sort_by_date},
 {GnomeCmdFileList::COLUMN_PERM, N_("perm"), GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_perm},
 {GnomeCmdFileList::COLUMN_OWNER, N_("uid"), GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_owner},
 {GnomeCmdFileList::COLUMN_GROUP, N_("gid"), GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_group}};


struct GnomeCmdFileListClass
{
    GnomeCmdCListClass parent_class;

    void (* file_clicked)        (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *button);
    void (* file_released)       (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *button);
    void (* list_clicked)        (GnomeCmdFileList *fl, GdkEventButton *button);
    void (* empty_space_clicked) (GnomeCmdFileList *fl, GdkEventButton *button);
    void (* files_changed)       (GnomeCmdFileList *fl);
    void (* dir_changed)         (GnomeCmdFileList *fl, GnomeCmdDir *dir);
    void (* con_changed)         (GnomeCmdFileList *fl, GnomeCmdCon *con);
};


struct GnomeCmdFileList::Private
{
    GtkWidget *column_pixmaps[NUM_COLUMNS];
    GtkWidget *column_labels[NUM_COLUMNS];

    gint cur_file = -1;
    GnomeCmdFileCollection visible_files;
    GnomeCmd::Collection<GnomeCmdFile *> selected_files;      // contains GnomeCmdFile pointers, no refing

    gchar *base_dir;

    GCompareDataFunc sort_func;
    gint current_col;
    gboolean sort_raising[NUM_COLUMNS];

    gboolean shift_down;
    gint shift_down_row;
    GnomeCmdFile *right_mb_down_file;
    gboolean right_mb_sel_state;
    guint right_mb_timeout_id;
    GtkWidget *selpat_dialog;
    GtkWidget *quicksearch_popup;
    gchar *focus_later;

    gboolean autoscroll_dir;
    guint autoscroll_timeout;
    gint autoscroll_y;

    GnomeCmdCon *con_opening;
    GtkWidget *con_open_dialog;
    GtkWidget *con_open_dialog_label;
    GtkWidget *con_open_dialog_pbar;

    GtkItemFactory *ifac;

    explicit Private(GnomeCmdFileList *fl);
    ~Private();

    static gchar *translate_menu(const gchar *path, gpointer);

    static void on_dnd_popup_menu_copy(GnomeCmdFileList *fl, GFileCopyFlags gFileCopyFlags, GtkWidget *widget);
    static void on_dnd_popup_menu_move(GnomeCmdFileList *fl, GFileCopyFlags gFileCopyFlags, GtkWidget *widget);
    static void on_dnd_popup_menu_link(GnomeCmdFileList *fl, GFileCopyFlags gFileCopyFlags, GtkWidget *widget);
};


GnomeCmdFileList::Private::Private(GnomeCmdFileList *fl)
{
    memset(column_pixmaps, 0, sizeof(column_pixmaps));
    memset(column_labels, 0, sizeof(column_labels));

    base_dir = nullptr;

    quicksearch_popup = nullptr;
    selpat_dialog = nullptr;

    focus_later = nullptr;
    shift_down = FALSE;
    shift_down_row = 0;
    right_mb_sel_state = FALSE;
    right_mb_down_file = nullptr;
    right_mb_timeout_id = 0;

    autoscroll_dir = FALSE;
    autoscroll_timeout = 0;
    autoscroll_y = 0;

    con_opening = nullptr;
    con_open_dialog = nullptr;
    con_open_dialog_label = nullptr;
    con_open_dialog_pbar = nullptr;

    memset(sort_raising, GTK_SORT_ASCENDING, sizeof(sort_raising));

    for (gint i=0; i<NUM_COLUMNS; i++)
        gtk_clist_set_column_resizeable (*fl, i, TRUE);

    static GtkItemFactoryEntry items[] = {
                                            {(gchar*) N_("/_Copy here"), (gchar*) "<control>", (GtkItemFactoryCallback) on_dnd_popup_menu_copy, G_FILE_COPY_NONE, (gchar*) "<StockItem>", GTK_STOCK_COPY},
                                            {(gchar*) N_("/_Move here"), (gchar*) "<shift>", (GtkItemFactoryCallback) on_dnd_popup_menu_move, G_FILE_COPY_NONE, (gchar*) "<StockItem>", GTK_STOCK_COPY},
                                            {(gchar*) N_("/_Link here"), (gchar*) "<control><shift>", (GtkItemFactoryCallback) on_dnd_popup_menu_link,G_FILE_COPY_NONE, (gchar*) "<StockItem>", GTK_STOCK_CONVERT},
                                            {(gchar*) "/", nullptr, nullptr, 0, (gchar*) "<Separator>"},
                                            {(gchar*) N_("/C_ancel"), (gchar*) "Esc", nullptr, 1, (gchar*) "<StockItem>", GTK_STOCK_CANCEL}
                                         };

    ifac = gtk_item_factory_new (GTK_TYPE_MENU, (const gchar*) "<main>", nullptr);
    gtk_item_factory_set_translate_func (ifac, translate_menu, nullptr, nullptr);
    gtk_item_factory_create_items (ifac, G_N_ELEMENTS (items), items, fl);
}


GnomeCmdFileList::Private::~Private()
{
    g_object_unref (ifac);
}


gchar *GnomeCmdFileList::Private::translate_menu(const gchar *path, gpointer unused)
{
    return _(path);
}


void GnomeCmdFileList::Private::on_dnd_popup_menu_copy(GnomeCmdFileList *fl, GFileCopyFlags gFileCopyFlags, GtkWidget *widget)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    auto data = (gpointer *) gtk_item_factory_popup_data_from_widget (widget);
    auto gFileGlist = (GList *) data[0];
    auto to = static_cast<GnomeCmdDir*> (data[1]);

    data[0] = nullptr;
    fl->drop_files(GnomeCmdFileList::DndMode::COPY, gFileCopyFlags, gFileGlist, to);
}

void GnomeCmdFileList::Private::on_dnd_popup_menu_move(GnomeCmdFileList *fl, GFileCopyFlags gFileCopyFlags, GtkWidget *widget)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gpointer *data = (gpointer *) gtk_item_factory_popup_data_from_widget (widget);
    GList *gFileGlist = (GList *) data[0];
    auto to = static_cast<GnomeCmdDir*> (data[1]);

    data[0] = nullptr;
    fl->drop_files(GnomeCmdFileList::DndMode::MOVE, gFileCopyFlags, gFileGlist, to);
}

void GnomeCmdFileList::Private::on_dnd_popup_menu_link(GnomeCmdFileList *fl, GFileCopyFlags gFileCopyFlags, GtkWidget *widget)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gpointer *data = (gpointer *) gtk_item_factory_popup_data_from_widget (widget);
    GList *gFileGlist = (GList *) data[0];
    auto to = static_cast<GnomeCmdDir*> (data[1]);

    data[0] = nullptr;
    fl->drop_files(GnomeCmdFileList::DndMode::LINK, gFileCopyFlags, gFileGlist, to);
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
}


inline gchar *strip_extension (const gchar *fname)
{
    gchar *s = g_strdup (fname);
    gchar *p = strrchr(s,'.');
    if (p && p != s)
        *p = '\0';
    return s;
}


struct FileFormatData
{
    gchar *text[GnomeCmdFileList::NUM_COLUMNS];

    gchar *dpath;
    gchar *fname;
    gchar *fext;

    static gchar empty_string[];

    FileFormatData(GnomeCmdFileList *fl, GnomeCmdFile *f, gboolean tree_size);
    ~FileFormatData();
};


gchar FileFormatData::empty_string[] = "";


FileFormatData::FileFormatData(GnomeCmdFileList *fl, GnomeCmdFile *f, gboolean tree_size)
{
    // If the user wants a character instead of icon for filetype set it now
    if (gnome_cmd_data.options.layout == GNOME_CMD_LAYOUT_TEXT)
        text[GnomeCmdFileList::COLUMN_ICON] = (gchar *) f->get_type_string();
    else
        text[GnomeCmdFileList::COLUMN_ICON] = nullptr;

    // Prepare the strings to show
    gchar *t1 = f->get_path();
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
        fname = g_strdup(f->get_name());

    if (fl->priv->base_dir != nullptr)
        text[GnomeCmdFileList::COLUMN_DIR] = g_strconcat(get_utf8("."), dpath + (strlen(fl->priv->base_dir)-1), nullptr);
    else
        text[GnomeCmdFileList::COLUMN_DIR] = dpath;

    DEBUG ('l', "FileFormatData text[GnomeCmdFileList::COLUMN_DIR]=[%s]\n", text[GnomeCmdFileList::COLUMN_DIR]);

    if (gnome_cmd_data.options.ext_disp_mode != GNOME_CMD_EXT_DISP_WITH_FNAME)
        fext = get_utf8 (f->get_extension());
    else
        fext = nullptr;

    //Set other file information
    text[GnomeCmdFileList::COLUMN_NAME]  = fname;
    text[GnomeCmdFileList::COLUMN_EXT]   = fext;

    text[GnomeCmdFileList::COLUMN_SIZE]  = tree_size ? (gchar *) f->get_tree_size_as_str() : (gchar *) f->get_size();

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY || !f->is_dotdot)
    {
        text[GnomeCmdFileList::COLUMN_DATE]  = (gchar *) f->get_mdate(FALSE);
        text[GnomeCmdFileList::COLUMN_PERM]  = (gchar *) f->get_perm();
        text[GnomeCmdFileList::COLUMN_OWNER] = (gchar *) f->get_owner();
        text[GnomeCmdFileList::COLUMN_GROUP] = (gchar *) f->get_group();
    }
    else
    {
        text[GnomeCmdFileList::COLUMN_DATE]  = empty_string;
        text[GnomeCmdFileList::COLUMN_PERM]  = empty_string;
        text[GnomeCmdFileList::COLUMN_OWNER] = empty_string;
        text[GnomeCmdFileList::COLUMN_GROUP] = empty_string;
    }
}


FileFormatData::~FileFormatData()
{
    g_free (dpath);
    g_free (fname);
    g_free (fext);
}


G_DEFINE_TYPE (GnomeCmdFileList, gnome_cmd_file_list, GNOME_CMD_TYPE_CLIST)


static void
g_cclosure_marshal_VOID__POINTER_POINTER (GClosure     *closure,
                                          GValue       *return_value G_GNUC_UNUSED,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint G_GNUC_UNUSED,
                                          gpointer      marshal_data)
{
    GCClosure *cc = (GCClosure *) closure;
    gpointer data1, data2;

    g_return_if_fail (n_param_values == 3);

    if (G_CCLOSURE_SWAP_DATA (closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }

    typedef void (*GMarshalFunc_VOID__POINTER_POINTER) (gpointer data1,
                                                        gpointer arg_1,
                                                        gpointer arg_2,
                                                        gpointer data2);

    GMarshalFunc_VOID__POINTER_POINTER callback = (GMarshalFunc_VOID__POINTER_POINTER) (marshal_data ? marshal_data : cc->callback);

    callback (data1,
              g_value_get_pointer (param_values + 1),
              g_value_get_pointer (param_values + 2),
              data2);
}


static void on_selpat_hide (GtkWidget *dialog, GnomeCmdFileList *fl)
{
    fl->priv->selpat_dialog = nullptr;
}


// given a GnomeFileList, returns the upper-left corner of the selected file
static void get_focus_row_coordinates (GnomeCmdFileList *fl, gint &x, gint &y, gint &width, gint &height)
{
    #define CELL_SPACING 1
    #define COLUMN_INSET 3

    gint x0, y0;

    gdk_window_get_origin (GTK_CLIST (fl)->clist_window, &x0, &y0);

    gint row = GTK_CLIST (fl)->focus_row;
    gint rowh = GTK_CLIST (fl)->row_height + CELL_SPACING;
    gint colx = GTK_CLIST (fl)->column[GnomeCmdFileList::COLUMN_NAME].area.x - COLUMN_INSET - CELL_SPACING;

    x = x0 + colx;
    y = y0 + row*rowh + GTK_CLIST (fl)->voffset;

    width = GTK_CLIST (fl)->column[GnomeCmdFileList::COLUMN_NAME].area.width + 2*COLUMN_INSET;
    if (gnome_cmd_data.options.ext_disp_mode != GNOME_CMD_EXT_DISP_BOTH)
        width += GTK_CLIST (fl)->column[GnomeCmdFileList::COLUMN_EXT].area.width + 2*COLUMN_INSET + CELL_SPACING;

    height = rowh + 2*CELL_SPACING;
}


inline void focus_file_at_row (GnomeCmdFileList *fl, gint row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GTK_CLIST (fl)->focus_row = row;
    gtk_clist_select_row (*fl, row, 0);
    fl->priv->cur_file = GTK_CLIST (fl)->focus_row;
}


static void on_quicksearch_popup_hide (GtkWidget *quicksearch_popup, GnomeCmdFileList *fl)
{
    fl->priv->quicksearch_popup = nullptr;
}


void GnomeCmdFileList::select_file(GnomeCmdFile *f, gint row)
{
    g_return_if_fail (f != nullptr);

    if (f->is_dotdot)
        return;

    if (row == -1)
        row = get_row_from_file(f);
    if (row == -1)
        return;


    if (!gnome_cmd_data.options.use_ls_colors)
        gtk_clist_set_row_style (*this, row, (row % 2) ? alt_sel_list_style : sel_list_style);
    else
    {
        GnomeCmdColorTheme *colors = gnome_cmd_data.options.get_current_color_theme();
        if (!colors->respect_theme)
        {
            gtk_clist_set_foreground (*this, row, colors->sel_fg);
            gtk_clist_set_background (*this, row, colors->sel_bg);
        }
    }

    if (priv->selected_files.contain(f))
        return;

    priv->selected_files.add(f);

    g_signal_emit (this, signals[FILES_CHANGED], 0);
}


void GnomeCmdFileList::unselect_file(GnomeCmdFile *f, gint row)
{
    g_return_if_fail (f != nullptr);

    if (!priv->selected_files.contain(f))
        return;

    if (row == -1)
        row = get_row_from_file(f);
    if (row == -1)
        return;

    priv->selected_files.remove(f);

    if (!gnome_cmd_data.options.use_ls_colors)
        gtk_clist_set_row_style (*this, row, (row % 2) ? alt_list_style : list_style);
    else
    {
        GnomeCmdColorTheme *colors = gnome_cmd_data.options.get_current_color_theme();
        if (LsColor *col = ls_colors_get (f))
        {
            GdkColor *fg = col->fg ? col->fg : colors->norm_fg;
            GdkColor *bg = col->bg ? col->bg : colors->norm_bg;

            if (bg)  gtk_clist_set_background (*this, row, bg);
            if (fg)  gtk_clist_set_foreground (*this, row, fg);
        }
        else
        {
            if (!colors->respect_theme)
            {
                gtk_clist_set_foreground (*this, row, colors->norm_fg);
                gtk_clist_set_background (*this, row, colors->norm_bg);
            }
        }
    }
    g_signal_emit (this, signals[FILES_CHANGED], 0);
}


void GnomeCmdFileList::toggle_file(GnomeCmdFile *f)
{
    gint row = get_row_from_file(f);

    if (row == -1)
        return;

    guint real_row = row;
    if (real_row < priv->visible_files.size())
    {
        if (!priv->selected_files.contain(f))
            select_file(f, real_row);
        else
            unselect_file(f, real_row);
    }
}


inline void select_file_range (GnomeCmdFileList *fl, gint start_row, gint end_row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (start_row > end_row)
    {
        gint i = start_row;
        start_row = end_row;
        end_row = i;
    }

    for (gint i=start_row; i<=end_row; i++)
        fl->select_file(fl->get_file_at_row(i), i);

    fl->priv->cur_file = end_row;
}


inline void toggle_file_at_row (GnomeCmdFileList *fl, gint row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    fl->priv->cur_file = row;

    GnomeCmdFile *f = fl->get_file_at_row(row);

    if (f)
        fl->toggle_file(f);
}


inline void toggle_file_range (GnomeCmdFileList *fl, gint start_row, gint end_row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    if (start_row > end_row)
    {
        gint i = start_row;
        start_row = end_row;
        end_row = i;
    }

    for (gint i=start_row; i<=end_row; i++)
        toggle_file_at_row (fl, i);
}


static void toggle_files_with_same_extension (GnomeCmdFileList *fl, gboolean select)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();
    if (!f) return;

    const gchar *ext1 = f->get_extension();
    if (!ext1) return;

    for (auto i = fl->get_visible_files(); i; i = i->next)
    {
        auto ff = static_cast<GnomeCmdFile*> (i->data);

        if (ff && ff->gFileInfo)
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
    }
}


void GnomeCmdFileList::toggle_with_pattern(Filter &pattern, gboolean mode)
{
    if (gnome_cmd_data.options.select_dirs)
        for (auto i = get_visible_files(); i; i = i->next)
        {
            auto f = static_cast<GnomeCmdFile*> (i->data);

            if (f && f->gFileInfo && pattern.match(g_file_info_get_display_name(f->gFileInfo)))
            {
                if (mode)
                    select_file(f);
                else
                    unselect_file(f);
            }
        }
    else
        for (auto i = get_visible_files(); i; i = i->next)
        {
            auto f = static_cast<GnomeCmdFile*> (i->data);

            if (f && !GNOME_CMD_IS_DIR (f) && f->gFileInfo && pattern.match(g_file_info_get_display_name(f->gFileInfo)))
            {
                if (mode)
                    select_file(f);
                else
                    unselect_file(f);
            }
        }
}


void GnomeCmdFileList::create_column_titles()
{
    gtk_clist_column_title_passive (*this, COLUMN_ICON);

    for (gint i=COLUMN_NAME; i<NUM_COLUMNS; i++)
    {
        GtkWidget *hbox, *pixmap;

        GdkPixmap *pm = IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_BLANK);
        GdkBitmap *bm = IMAGE_get_mask (PIXMAP_FLIST_ARROW_BLANK);

        hbox = gtk_hbox_new (FALSE, 1);
        g_object_ref (hbox);
        g_object_set_data_full (*this, "column-hbox", hbox, g_object_unref);
        gtk_widget_show (hbox);

        priv->column_labels[i] = gtk_label_new (_(file_list_column[i].title));
        g_object_ref (priv->column_labels[i]);
        g_object_set_data_full (*this, "column-label", priv->column_labels[i], g_object_unref);
        gtk_widget_show (priv->column_labels[i]);
        gtk_box_pack_start (GTK_BOX (hbox), priv->column_labels[i], TRUE, TRUE, 0);

        // ToDo: Replace GtkPixmap with GtkImage
        pixmap = gtk_pixmap_new (pm, bm);
        g_object_ref (pixmap);
        g_object_set_data_full (*this, "column-pixmap", pixmap, g_object_unref);
        gtk_widget_show (pixmap);
        gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);

        priv->column_pixmaps[i] = pixmap;
        gtk_clist_set_column_widget (*this, i, hbox);
    }

    for (gint i=COLUMN_ICON; i<NUM_COLUMNS; i++)
    {
        gtk_clist_set_column_width (*this, i, gnome_cmd_data.fs_col_width[i]);
        gtk_clist_set_column_justification (*this, i, file_list_column[i].justification);
    }

    gtk_clist_column_titles_show (*this);
}


static void update_column_sort_arrows (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    for (gint i=GnomeCmdFileList::COLUMN_NAME; i<GnomeCmdFileList::NUM_COLUMNS; i++)
    {
        if (i != fl->priv->current_col)
            gtk_pixmap_set (GTK_PIXMAP (fl->priv->column_pixmaps[i]),
                            IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_BLANK),
                            IMAGE_get_mask (PIXMAP_FLIST_ARROW_BLANK));
        else
            if (fl->priv->sort_raising[i])
                gtk_pixmap_set (GTK_PIXMAP (fl->priv->column_pixmaps[i]),
                                IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_UP),
                                IMAGE_get_mask (PIXMAP_FLIST_ARROW_UP));
            else
                gtk_pixmap_set (GTK_PIXMAP (fl->priv->column_pixmaps[i]),
                                IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_DOWN),
                                IMAGE_get_mask (PIXMAP_FLIST_ARROW_DOWN));
    }
}


/******************************************************
 * DnD functions
 **/

static char *build_selected_file_list (GnomeCmdFileList *fl, int *file_list_len)
{
    GList *sel_files = fl->get_selected_files();
    int listlen = g_list_length (sel_files);

    if (listlen > 1)
    {
        int total_len = 0;
        GList *uri_str_list = nullptr;

        // create a list with the uri's of the selected files and calculate the total_length needed
        for (auto i = sel_files; i; i = i->next)
        {
            auto f = static_cast<GnomeCmdFile*> (i->data);
            auto uriString = f->get_uri_str();

            gchar *uri_str = g_strconcat (uriString, "\r\n", nullptr);
            uri_str_list = g_list_append (uri_str_list, uri_str);
            total_len += strlen (uri_str);
            g_free(uriString);
        }

        // allocate memory
        total_len++;

        char *data, *copy;

        data = copy = (gchar *) g_malloc (total_len+1);

        // put the uri_str_list in the allocated memory
        for (auto i = uri_str_list; i; i = i->next)
        {
            gchar *uri_str = (gchar *) i->data;

            strcpy (copy, uri_str);
            copy += strlen (uri_str);
        }

        g_list_foreach (uri_str_list, (GFunc) g_free, nullptr);
        g_list_free (uri_str_list);

        data [total_len] = '\0';
        *file_list_len = total_len;
        return data;
    }
    else
    {
        if (listlen == 1)
        {
            auto f = static_cast<GnomeCmdFile*> (sel_files->data);
            char *uri_str = f->get_uri_str();

            *file_list_len = strlen (uri_str) + 1;
            return uri_str;
        }
    }

    *file_list_len = 0;
    g_list_free (sel_files);

    return nullptr;
}


static void get_popup_pos (GtkMenu *menu, gint *x, gint *y, gboolean push_in, GnomeCmdFileList *gnomeCmdFileList)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (gnomeCmdFileList));

    gint w, h;

    get_focus_row_coordinates (gnomeCmdFileList, *x, *y, w, h);
}


static void show_file_popup (GnomeCmdFileList *fl, GdkEventButton *event)
{
    // create the popup menu
    GtkWidget *menu = gnome_cmd_file_popmenu_new (fl);
    if (!menu) return;

    g_object_ref (menu);
    g_object_set_data_full (*fl, "file_popup_menu", menu, g_object_unref);

    if (event)
    {
        gtk_menu_popup (GTK_MENU (menu),
                        nullptr,
                        nullptr,
                        nullptr,
                        fl,
                        event->button,
                        event->time);
    }
    else
    {
        gtk_menu_popup (GTK_MENU (menu),
                        nullptr,
                        nullptr,
                        (GtkMenuPositionFunc) get_popup_pos,
                        fl,
                        0,
                        gtk_get_current_event_time ());
    }
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


inline gint my_filesizecmp (GnomeVFSFileSize i1, GnomeVFSFileSize i2, gboolean raising)
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

    if(g_file_info_get_file_type(f1->gFileInfo) > g_file_info_get_file_type(f2->gFileInfo))
        return -1;

    if (g_file_info_get_file_type(f1->gFileInfo) < g_file_info_get_file_type(f2->gFileInfo))
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

    if(g_file_info_get_file_type(f1->gFileInfo) > g_file_info_get_file_type(f2->gFileInfo))
        return -1;

    if (g_file_info_get_file_type(f1->gFileInfo) < g_file_info_get_file_type(f2->gFileInfo))
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

    gint ret = my_intcmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE),
                          f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE), TRUE);

    if (!ret)
    {
        ret = my_filesizecmp (f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_SIZE),
                              f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_SIZE), raising);
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
        ret = my_intcmp (get_gfile_attribute_uint32(f1->gFile, G_FILE_ATTRIBUTE_UNIX_MODE),
                         get_gfile_attribute_uint32(f2->gFile, G_FILE_ATTRIBUTE_UNIX_MODE), raising);
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

 static void on_column_clicked (GtkCList *list, gint col, GnomeCmdFileList *fl)
{
    g_return_if_fail (GTK_IS_CLIST (list));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    fl->priv->sort_raising[col] = fl->priv->current_col == col ? !fl->priv->sort_raising[col] :
                                                                 (static_cast<bool>(file_list_column[col].default_sort_direction));

    fl->priv->sort_func = file_list_column[col].sort_func;
    fl->priv->current_col = col;
    update_column_sort_arrows (fl);

    fl->sort();
}


static void on_scroll_vertical (GtkCList *clist, GtkScrollType scroll_type, gfloat position, GnomeCmdFileList *fl)
{
    g_return_if_fail (GTK_IS_CLIST (clist));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gint num_files = fl->size();

    if (fl->priv->shift_down)
    {
        int start_row = fl->priv->cur_file;
        int end_row = clist->focus_row;

        fl->priv->shift_down = FALSE;

        if (start_row < 0 || end_row < 0)
            return;

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
        switch (scroll_type)
        {
            case GTK_SCROLL_STEP_BACKWARD:
            case GTK_SCROLL_STEP_FORWARD:
                toggle_file_at_row (fl, start_row);
                break;

            case GTK_SCROLL_PAGE_BACKWARD:
                toggle_file_range (fl, start_row, end_row);
                if (clist->focus_row > 0)
                    focus_file_at_row (fl, --clist->focus_row);
                break;

            case GTK_SCROLL_PAGE_FORWARD:
                toggle_file_range (fl, start_row, end_row);
                if (clist->focus_row < num_files - 1)
                    focus_file_at_row (fl, ++clist->focus_row);
                break;

            default:
                toggle_file_range (fl, start_row, end_row);
                break;
        }
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    fl->priv->cur_file = clist->focus_row;
}


static gboolean on_button_press (GtkCList *clist, GdkEventButton *event, GnomeCmdFileList *fl)
{
    g_return_val_if_fail (clist != nullptr, FALSE);
    g_return_val_if_fail (event != nullptr, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);

    if (GTK_CLIST (fl)->clist_window != event->window)
        return FALSE;

    gint row = gnome_cmd_clist_get_row (*fl, event->x, event->y);

    if (row < 0)
    {
        g_signal_emit (fl, signals[LIST_CLICKED], 0, nullptr, event);
        g_signal_emit (fl, signals[EMPTY_SPACE_CLICKED], 0, event);
        return FALSE;
    }

    GnomeCmdFile *f = fl->get_file_at_row(row);

    g_signal_emit (fl, signals[LIST_CLICKED], 0, f, event);
    g_signal_emit (fl, signals[FILE_CLICKED], 0, f, event);

    g_signal_stop_emission_by_name (clist, "button-press-event");

    return TRUE;
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
    gFileList = g_list_append(gFileList, gnomeCmdFile->gFile);
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

        gnome_cmd_tmp_download (g_list_append (nullptr, sourceGFile),
                                g_list_append (nullptr, destGFile),
                                G_FILE_COPY_OVERWRITE,
                                GTK_SIGNAL_FUNC (do_mime_exec_single),
                                dldata->args);
    }
    else
    {
        gnome_cmd_app_free (static_cast<GnomeCmdApp*> (dldata->args[0]));
        g_free (dldata->args);
    }

    gtk_widget_destroy (dldata->dialog);
    g_free (dldata);
}


static void mime_exec_single (GnomeCmdFile *f)
{
    g_return_if_fail (f != nullptr);
    g_return_if_fail (f->gFileInfo != nullptr);

    gpointer *args;
    GnomeCmdApp *app;

    // Check if the file is a binary executable that lacks the executable bit

    if (!f->is_executable())
    {
        if (f->has_content_type("application/x-executable") || f->has_content_type("application/x-executable-binary"))
        {
            gchar *msg = g_strdup_printf (_("“%s” seems to be a binary executable file but it lacks the executable bit. Do you want to set it and then run the file?"), f->get_name());
            gint ret = run_simple_dialog (*main_win, FALSE, GTK_MESSAGE_QUESTION, msg,
                                          _("Make Executable?"),
                                          -1, _("Cancel"), _("OK"), nullptr);
            g_free (msg);

            if (ret != 1)
            {
                return;
            }

           if(!f->chmod(get_gfile_attribute_uint32(f->gFile, G_FILE_ATTRIBUTE_UNIX_MODE) | GNOME_CMD_PERM_USER_EXEC, nullptr))
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
                gint ret = run_simple_dialog (*main_win, FALSE, GTK_MESSAGE_QUESTION, msg, _("Run or Display"),
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
        gnome_cmd_show_message (nullptr, msg, _("Open the \"Applications\" page in the Control Center to add one."));
        g_free (contentType);
        g_free (msg);
        return;
    }

    app = gnome_cmd_app_new_from_app_info (gAppInfo);

    args = g_new0 (gpointer, 3);

    if (f->is_local())
    {
        GList *gFileList = nullptr;
        gFileList = g_list_append(gFileList, f->gFile);
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
            GtkWidget *dialog = gtk_message_dialog_new (*main_win, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", msg);
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


inline gboolean mime_exec_file (GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, FALSE);

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_REGULAR)
    {
        mime_exec_single (f);
	    return TRUE;
    }
    return FALSE;

 }


static void on_file_clicked (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *event, gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (GNOME_CMD_IS_FILE (f));
    g_return_if_fail (event != nullptr);

    fl->modifier_click = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK);

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1 && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
    {
        mime_exec_file (f);
    }
    else
        if (event->type == GDK_BUTTON_PRESS && (event->button == 1 || event->button == 3))
        {
            gint prev_row = fl->priv->cur_file;
            gint row = fl->get_row_from_file(f);

            fl->select_row(row);
            gtk_widget_grab_focus (*fl);

            if (event->button == 1)
            {
                if (event->state & GDK_SHIFT_MASK)
                {
                    select_file_range (fl, fl->priv->shift_down_row, row);
                }
                else
                    if (event->state & GDK_CONTROL_MASK)
                    {
                        if (!fl->priv->selected_files.empty() || gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
                            toggle_file_at_row (fl, row);
                        else
                        {
                            if (prev_row!=row)
                                fl->select_file(fl->get_file_at_row(prev_row), prev_row);
                            fl->select_file(fl->get_file_at_row(row), row);
                            fl->priv->cur_file = row;
                        }
                    }
            }
            else
                if (event->button == 3)
                    if (!f->is_dotdot)
                    {
                        if (gnome_cmd_data.options.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
                        {
                            if (!fl->priv->selected_files.contain(f))
                            {
                                fl->select_file(f);
                                fl->priv->right_mb_sel_state = 1;
                            }
                            else
                            {
                                fl->unselect_file(f);
                                fl->priv->right_mb_sel_state = 0;
                            }

                            fl->priv->right_mb_down_file = f;
                            fl->priv->right_mb_timeout_id =
                                g_timeout_add (POPUP_TIMEOUT, (GSourceFunc) on_right_mb_timeout, fl);
                        }
                        else
                            show_file_popup (fl, event);
                    }
        }
}


static void on_file_released (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *event, gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (GNOME_CMD_IS_FILE (f));
    g_return_if_fail (event != nullptr);

    if (event->type == GDK_BUTTON_RELEASE && event->button == 1 && !fl->modifier_click && gnome_cmd_data.options.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        mime_exec_file (f);
}


static void on_motion_notify (GtkCList *clist, GdkEventMotion *event, GnomeCmdFileList *fl)
{
    g_return_if_fail (event != nullptr);

    if (event->state & GDK_BUTTON3_MASK)
    {
        gint row = gnome_cmd_clist_get_row (*fl, event->x, event->y);

        if (row != -1)
        {
            GnomeCmdFile *f = fl->get_file_at_row(row);
            if (f)
            {
                fl->select_row(row);
                if (fl->priv->right_mb_sel_state)
                    fl->select_file(f, row);
                else
                    fl->unselect_file(f, row);
            }
        }
    }
}


static gint on_button_release (GtkWidget *widget, GdkEventButton *event, GnomeCmdFileList *fl)
{
    g_return_val_if_fail (widget != nullptr, FALSE);
    g_return_val_if_fail (event != nullptr, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);

    if (GTK_CLIST (fl)->clist_window != event->window)
        return FALSE;

    gint row = gnome_cmd_clist_get_row (*fl, event->x, event->y);
    if (row < 0)
        return FALSE;

    GnomeCmdFile *f = fl->get_file_at_row(row);

    g_signal_emit (fl, signals[FILE_RELEASED], 0, f, event);

    if (event->type == GDK_BUTTON_RELEASE)
    {
        if (event->button == 1 && state_is_blank (event->state))
        {
            if (f && !fl->priv->selected_files.contain(f) && gnome_cmd_data.options.left_mouse_button_unselects)
                fl->unselect_all();
            return TRUE;
        }
        else
            if (event->button == 3 && fl->priv->right_mb_timeout_id > 0)
                g_source_remove (fl->priv->right_mb_timeout_id);
    }

    return FALSE;
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
static void on_dir_list_failed (GnomeCmdDir *dir, gpointer *unused, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_failed\n");

    if (dir->error)
    {
        gnome_cmd_show_message (nullptr, _("Directory listing failed."), dir->error->message);
        g_clear_error(&(dir->error));
    }

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

    gtk_widget_destroy (fl->priv->con_open_dialog);
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

    gnome_cmd_show_message (nullptr,  _("Failed to open connection."), con->open_failed_msg);

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

    gtk_widget_destroy (fl->priv->con_open_dialog);
    fl->priv->con_open_dialog = nullptr;
    fl->priv->con_opening = nullptr;
}


static gboolean update_con_open_progress (GnomeCmdFileList *fl)
{
    if (!fl->priv->con_open_dialog)
        return FALSE;

    const gchar *msg = gnome_cmd_con_get_open_msg (fl->priv->con_opening);
    gtk_label_set_text (GTK_LABEL (fl->priv->con_open_dialog_label), msg);
    progress_bar_update (fl->priv->con_open_dialog_pbar, FL_PBAR_MAX);

    return TRUE;
}


static void create_con_open_progress_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    fl->priv->con_open_dialog = gnome_cmd_dialog_new (nullptr);
    g_object_ref (fl->priv->con_open_dialog);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (fl->priv->con_open_dialog),
                                 GTK_STOCK_CANCEL,
                                 GTK_SIGNAL_FUNC (on_con_open_cancel), fl);

    GtkWidget *vbox = create_vbox (fl->priv->con_open_dialog, FALSE, 0);

    fl->priv->con_open_dialog_label = create_label (fl->priv->con_open_dialog, "");

    fl->priv->con_open_dialog_pbar = create_progress_bar (fl->priv->con_open_dialog);
    gtk_progress_set_show_text (GTK_PROGRESS (fl->priv->con_open_dialog_pbar), FALSE);
    gtk_progress_set_activity_mode (GTK_PROGRESS (fl->priv->con_open_dialog_pbar), TRUE);
    gtk_progress_configure (GTK_PROGRESS (fl->priv->con_open_dialog_pbar), 0, 0, FL_PBAR_MAX);

    gtk_box_pack_start (GTK_BOX (vbox), fl->priv->con_open_dialog_label, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), fl->priv->con_open_dialog_pbar, FALSE, TRUE, 0);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (fl->priv->con_open_dialog), vbox);

    gtk_window_set_transient_for (GTK_WINDOW (fl->priv->con_open_dialog), *main_win);
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
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER_POINTER,
            G_TYPE_NONE,
            2, G_TYPE_POINTER, G_TYPE_POINTER);

    signals[FILE_RELEASED] =
        g_signal_new ("file-released",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, file_released),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER_POINTER,
            G_TYPE_NONE,
            2, G_TYPE_POINTER, G_TYPE_POINTER);

    signals[LIST_CLICKED] =
        g_signal_new ("list-clicked",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, list_clicked),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER_POINTER,
            G_TYPE_NONE,
            2, G_TYPE_POINTER, G_TYPE_POINTER);

    signals[EMPTY_SPACE_CLICKED] =
        g_signal_new ("empty-space-clicked",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, empty_space_clicked),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILES_CHANGED] =
        g_signal_new ("files-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, files_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[DIR_CHANGED] =
        g_signal_new ("dir-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, dir_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[CON_CHANGED] =
        g_signal_new ("con-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, con_changed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);
}


static void gnome_cmd_file_list_init (GnomeCmdFileList *fl)
{
    fl->priv = new GnomeCmdFileList::Private(fl);

    fl->init_dnd();

    g_signal_connect_after (fl, "scroll-vertical", G_CALLBACK (on_scroll_vertical), fl);
    g_signal_connect (fl, "click-column", G_CALLBACK (on_column_clicked), fl);

    g_signal_connect (fl, "button-press-event", G_CALLBACK (on_button_press), fl);
    g_signal_connect (fl, "button-release-event", G_CALLBACK (on_button_release), fl);
    g_signal_connect (fl, "motion-notify-event", G_CALLBACK (on_motion_notify), fl);

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


inline void add_file_to_clist (GnomeCmdFileList *fl, GnomeCmdFile *f, gint in_row)
{
    GtkCList *clist = *fl;

    FileFormatData data(fl, f,FALSE);

    gint row = in_row == -1 ? gtk_clist_append (clist, data.text) : gtk_clist_insert (clist, in_row, data.text);

    // Setup row data and color
    if (!gnome_cmd_data.options.use_ls_colors)
        gtk_clist_set_row_style (clist, row, (row % 2) ? alt_list_style : list_style);
    else
    {
        LsColor *col = ls_colors_get (f);
        if (col)
        {
            if (col->bg)
                gtk_clist_set_background (clist, row, col->bg);
            if (col->fg)
                gtk_clist_set_foreground (clist, row, col->fg);
        }
    }

    gtk_clist_set_row_data (clist, row, f);

    // If the use wants icons to show file types set it now
    if (gnome_cmd_data.options.layout != GNOME_CMD_LAYOUT_TEXT)
    {
        GdkPixmap *pixmap;
        GdkBitmap *mask;

        if (f->get_type_pixmap_and_mask(&pixmap, &mask))
            gtk_clist_set_pixmap (clist, row, 0, pixmap, mask);
    }

    // If we have been waiting for this file to show up, focus it
    if (fl->priv->focus_later && strcmp (f->get_name(), fl->priv->focus_later)==0)
        focus_file_at_row (fl, row);
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
    priv->visible_files.add(f);
    add_file_to_clist (this, f, -1);
}


gboolean GnomeCmdFileList::insert_file(GnomeCmdFile *f)
{
    if (!file_is_wanted(f))
        return FALSE;

    gint num_files = size();

    for (gint i=0; i<num_files; i++)
    {
        GnomeCmdFile *f2 = get_file_at_row(i);
        if (priv->sort_func (f2, f, this) == 1)
        {
            priv->visible_files.add(f);
            add_file_to_clist (this, f, i);

            if (i<=priv->cur_file)
                priv->cur_file++;

            return TRUE;
        }
    }

    // Insert the file at the end of the list
    append_file(f);

    return TRUE;
}


void GnomeCmdFileList::show_files(GnomeCmdDir *dir)
{
    remove_all_files();

    GList *files = nullptr;

    // select the files to show
    for (auto i = gnome_cmd_dir_get_files (dir); i; i = i->next)
    {
        GnomeCmdFile *f = GNOME_CMD_FILE (i->data);

        if (file_is_wanted (f))
            files = g_list_append (files, f);
    }

    // Create a parent dir file (..) if appropriate
    gchar *path = GNOME_CMD_FILE (dir)->get_path();
    if (path && strcmp (path, G_DIR_SEPARATOR_S) != 0)
        files = g_list_append (files, gnome_cmd_dir_new_parent_dir_file (dir));
    g_free (path);

    if (!files)
        return;

    files = g_list_sort_with_data (files, (GCompareDataFunc) priv->sort_func, this);

    gtk_clist_freeze (*this);
    for (auto i = files; i; i = i->next)
        append_file(GNOME_CMD_FILE (i->data));
    gtk_clist_thaw (*this);

    if (files)
        g_list_free (files);
}


void GnomeCmdFileList::update_file(GnomeCmdFile *f)
{
    if (!f->needs_update())
        return;

    gint row = get_row_from_file(f);
    if (row == -1)
        return;

    FileFormatData data(this, f, FALSE);

    for (gint i=1; i<NUM_COLUMNS; i++)
        gtk_clist_set_text (*this, row, i, data.text[i]);

    if (gnome_cmd_data.options.layout != GNOME_CMD_LAYOUT_TEXT)
    {
        GdkPixmap *pixmap;
        GdkBitmap *mask;

        if (f->get_type_pixmap_and_mask(&pixmap, &mask))
            gtk_clist_set_pixmap (*this, row, 0, pixmap, mask);
    }
}


void GnomeCmdFileList::show_dir_tree_size(GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));

    gint row = get_row_from_file(f);
    if (row == -1)
        return;

    FileFormatData data(this, f,TRUE);

    for (gint i=1; i<NUM_COLUMNS; i++)
        gtk_clist_set_text (*this, row, i, data.text[i]);
}


void GnomeCmdFileList::show_visible_tree_sizes()
{
    invalidate_tree_size();

    for (auto files = get_visible_files(); files; files = files->next)
        show_dir_tree_size(static_cast<GnomeCmdFile*> (files->data));

    g_signal_emit (this, signals[FILES_CHANGED], 0);
}


gboolean GnomeCmdFileList::remove_file(GnomeCmdFile *f)
{
    g_return_val_if_fail (f != nullptr, FALSE);

    gint row = get_row_from_file(f);

    if (row<0)                              // f not found in the shown file list...
        return FALSE;

    gtk_clist_remove (*this, row);

    priv->selected_files.remove(f);
    priv->visible_files.remove(f);

    if (gtk_widget_is_focus(*this))
        focus_file_at_row (this, MIN (row, GTK_CLIST (this)->focus_row));

    return TRUE;
}


gboolean GnomeCmdFileList::remove_file(const gchar *uri_str)
{
    g_return_val_if_fail (uri_str != nullptr, FALSE);

    return remove_file (priv->visible_files.find(uri_str));
}


void GnomeCmdFileList::clear()
{
    gtk_clist_clear (*this);
    priv->visible_files.clear();
    priv->selected_files.clear();
}


void GnomeCmdFileList::reload()
{
    g_return_if_fail (GNOME_CMD_IS_DIR (cwd));

    unselect_all();
    gnome_cmd_dir_relist_files (cwd, gnome_cmd_con_needs_list_visprog (con));
}


GList *GnomeCmdFileList::get_selected_files()
{
    if (!priv->selected_files.empty())
        return priv->selected_files.get_list();

    GnomeCmdFile *f = get_selected_file();

    return f ? g_list_append (nullptr, f) : nullptr;
}


GnomeCmd::Collection<GnomeCmdFile *> &GnomeCmdFileList::get_marked_files()
{
    return priv->selected_files;
}


GList *GnomeCmdFileList::get_visible_files()
{
    return priv->visible_files.get_list();
}


GnomeCmdFile *GnomeCmdFileList::get_first_selected_file()
{
    if (priv->selected_files.empty())
        return get_selected_file();

    auto sel = sort_selection(priv->selected_files.get_list());
    auto f = static_cast <GnomeCmdFile*> (sel->data);
    g_list_free (sel);

    return f;
}


GnomeCmdFile *GnomeCmdFileList::get_focused_file()
{
    return priv->cur_file < 0 ? nullptr : get_file_at_row(priv->cur_file);
}


void GnomeCmdFileList::select_all()
{
    priv->selected_files.clear();

    if (gnome_cmd_data.options.select_dirs)
        for (auto i = get_visible_files(); i; i = i->next)
            select_file(static_cast<GnomeCmdFile*> (i->data));
    else
        for (auto i = get_visible_files(); i; i = i->next)
        {
            auto f = static_cast<GnomeCmdFile*> (i->data);

            if (!GNOME_CMD_IS_DIR (f))
                select_file(f);
        }
}


void GnomeCmdFileList::unselect_all()
{
    GnomeCmd::Collection<GnomeCmdFile *> sel = priv->selected_files;

    for (GnomeCmd::Collection<GnomeCmdFile *>::iterator i=sel.begin(); i!=sel.end(); ++i)
        unselect_file(*i);

    priv->selected_files.clear();
}


void GnomeCmdFileList::toggle()
{
    GnomeCmdFile *f = get_file_at_row(priv->cur_file);

    if (f)
        toggle_file(f);
}


void GnomeCmdFileList::toggle_and_step()
{

    GnomeCmdFile *f = get_file_at_row(priv->cur_file);

    if (f)
        toggle_file(f);
    if (priv->cur_file < (gint)size()-1)
        focus_file_at_row (this, priv->cur_file+1);
}


void GnomeCmdFileList::focus_file(const gchar *file_to_focus, gboolean scroll_to_file)
{
    g_return_if_fail(file_to_focus != nullptr);

    for (auto i = get_visible_files(); i; i = i->next)
    {
        auto f = static_cast<GnomeCmdFile*> (i->data);

        g_return_if_fail (f != nullptr);

        gint row = get_row_from_file (f);
        if (row == -1)
            return;

        auto basename = g_file_get_basename(f->gFile);
        if (basename && strcmp (basename, file_to_focus) == 0)
        {
            g_free(basename);
            priv->cur_file = row;
            focus_file_at_row (this, row);
            if (scroll_to_file)
                gtk_clist_moveto (*this, row, 0, 0, 0);
            return;
        }
        g_free(basename);
    }

    /* The file was not found, remember the filename in case the file gets
       added to the list in the future (after a FAM event etc). */
    g_free (priv->focus_later);
    priv->focus_later = g_strdup (file_to_focus);
}


void GnomeCmdFileList::select_row(gint row)
{
    focus_file_at_row (this, row==-1 ? 0 : row);
}


void GnomeCmdFileList::invert_selection()
{
    GnomeCmd::Collection<GnomeCmdFile *> sel = priv->selected_files;

    if (gnome_cmd_data.options.select_dirs)
        for (auto i = get_visible_files(); i; i = i->next)
        {
            auto f = static_cast<GnomeCmdFile*> (i->data);

            if (f && f->gFileInfo)
            {
                if (!sel.contain(f))
                    select_file(f);
                else
                    unselect_file(f);
            }
        }
    else
        for (auto i = get_visible_files(); i; i = i->next)
        {
            auto f = static_cast<GnomeCmdFile*> (i->data);

            if (f && !GNOME_CMD_IS_DIR (f) && f->gFileInfo)
            {
                if (!sel.contain(f))
                    select_file(f);
                else
                    unselect_file(f);
            }
        }
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

    gtk_clist_freeze (*this);
    gtk_clist_clear (*this);

    // resort the files and readd them to the list
    for (GList *list = priv->visible_files.sort(priv->sort_func, this); list; list = list->next)
        add_file_to_clist (this, GNOME_CMD_FILE (list->data), -1);

    // refocus the previously selected file if this file list has the focus
    if (selfile && GTK_WIDGET_HAS_FOCUS (this))
    {
        gint selrow = get_row_from_file(selfile);
        select_row(selrow);
        gtk_clist_moveto (*this, selrow, -1, 1, 0);
    }

    // reselect the previously selected files
    for (GnomeCmd::Collection<GnomeCmdFile *>::iterator i=priv->selected_files.begin(); i!=priv->selected_files.end(); ++i)
        select_file(*i);

    gtk_clist_thaw (*this);
}


void gnome_cmd_file_list_show_rename_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (GNOME_CMD_IS_FILE (f))
    {
        gint x, y, w, h;

        get_focus_row_coordinates (fl, x, y, w, h);

        GtkWidget *dialog = gnome_cmd_rename_dialog_new (f, x, y, w, h);

        g_object_ref (dialog);
        gtk_widget_show (dialog);
    }
}


void gnome_cmd_file_list_show_delete_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();

    if (files)
    {
        gnome_cmd_delete_dialog_show (files);
        g_list_free (files);
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


void gnome_cmd_file_list_view (GnomeCmdFileList *fl, gint internal_viewer)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (!f)
        return;

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        gnome_cmd_show_message (*main_win, _("Not an ordinary file."), g_file_info_get_display_name(f->gFileInfo));
    else
        gnome_cmd_file_view (f, internal_viewer);
}


void gnome_cmd_file_list_edit (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (!f)  return;

    if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        gnome_cmd_show_message (*main_win, _("Not an ordinary file."), g_file_info_get_display_name(f->gFileInfo));
    else
        gnome_cmd_file_edit (f);
}


gboolean gnome_cmd_file_list_quicksearch_shown (GnomeCmdFileList *fl)
{
    g_return_val_if_fail (fl!=nullptr, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);
    g_return_val_if_fail (fl->priv!=nullptr, FALSE);

    return fl->priv->quicksearch_popup!=nullptr;
}


void gnome_cmd_file_list_show_quicksearch (GnomeCmdFileList *fl, gchar c)
{
    gchar text[2];
    if (fl->priv->quicksearch_popup)
        return;

    gtk_clist_unselect_all (*fl);

    fl->priv->quicksearch_popup = gnome_cmd_quicksearch_popup_new (fl);
    text[0] = c;
    text[1] = '\0';
    g_object_ref (fl->priv->quicksearch_popup);
    gtk_widget_show (fl->priv->quicksearch_popup);
    if (c != 0)
    {
        GnomeCmdQuicksearchPopup *popup = GNOME_CMD_QUICKSEARCH_POPUP (fl->priv->quicksearch_popup);
        gtk_entry_set_text (GTK_ENTRY (popup->entry), text);
        gtk_editable_set_position (GTK_EDITABLE (popup->entry), 1);
    }

    g_signal_connect (fl->priv->quicksearch_popup, "hide", G_CALLBACK (on_quicksearch_popup_hide), fl);
}


gboolean GnomeCmdFileList::key_pressed(GdkEventKey *event)
{
    g_return_val_if_fail (event != nullptr, FALSE);

    if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
            case GDK_KP_Enter:
                gnome_cmd_file_list_show_properties_dialog (this);
                return TRUE;

            case GDK_KP_Add:
                toggle_files_with_same_extension (this, TRUE);
                break;

            case GDK_KP_Subtract:
                toggle_files_with_same_extension (this, FALSE);
                break;

            default:
                break;
        }
    }
    else if ((gnome_cmd_data.options.quick_search == GNOME_CMD_QUICK_SEARCH_CTRL_ALT)
             && (state_is_ctrl_alt (event->state) || state_is_ctrl_alt_shift (event->state)))
    {
        if ((event->keyval >= GDK_a && event->keyval <= GDK_z)
            || (event->keyval >= GDK_A && event->keyval <= GDK_Z)
            || event->keyval == GDK_period)
            gnome_cmd_file_list_show_quicksearch (this, (gchar) event->keyval);
    }
    else if (state_is_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_F6:
                gnome_cmd_file_list_show_rename_dialog (this);
                return TRUE;

            case GDK_F10:
                show_file_popup (this, nullptr);
                return TRUE;

            case GDK_Left:
            case GDK_KP_Left:
            case GDK_Right:
            case GDK_KP_Right:
                event->state -= GDK_SHIFT_MASK;
                return FALSE;

            case GDK_Page_Up:
            case GDK_KP_Page_Up:
            case GDK_KP_9:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_PAGE_BACKWARD, 0.0, nullptr);
                return FALSE;

            case GDK_Page_Down:
            case GDK_KP_Page_Down:
            case GDK_KP_3:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_PAGE_FORWARD, 0.0, nullptr);
                return FALSE;

            case GDK_Up:
            case GDK_KP_Up:
            case GDK_KP_8:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_STEP_BACKWARD, 0.0, nullptr);
                return FALSE;

            case GDK_Down:
            case GDK_KP_Down:
            case GDK_KP_2:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_STEP_FORWARD, 0.0, nullptr);
                return FALSE;

            case GDK_Home:
            case GDK_KP_Home:
            case GDK_KP_7:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_JUMP, 0.0);
                return TRUE;

            case GDK_End:
            case GDK_KP_End:
            case GDK_KP_1:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_JUMP, 1.0);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_alt_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
            case GDK_KP_Enter:
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
            case GDK_X:
            case GDK_x:
                gnome_cmd_file_list_cap_cut (this);
                return TRUE;

            case GDK_C:
            case GDK_c:
                gnome_cmd_file_list_cap_copy (this);
                return TRUE;

            case GDK_F3:
                on_column_clicked (*this, COLUMN_NAME, this);
                return TRUE;

            case GDK_F4:
                on_column_clicked (*this, COLUMN_EXT, this);
                return TRUE;

            case GDK_F5:
                on_column_clicked (*this, COLUMN_DATE, this);
                return TRUE;

            case GDK_F6:
                on_column_clicked (*this, COLUMN_SIZE, this);
                return TRUE;

            default:
                break;
        }
    }
    else if (state_is_blank (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
            case GDK_KP_Enter:
                return mime_exec_file (get_focused_file());

            case GDK_space:
                set_cursor_busy ();
                toggle();
                show_dir_tree_size(get_selected_file());
                g_signal_stop_emission_by_name (this, "key-press-event");
                g_signal_emit (this, signals[FILES_CHANGED], 0);
                set_cursor_default ();
                return TRUE;

            case GDK_KP_Add:
            case GDK_plus:
            case GDK_equal:
                gnome_cmd_file_list_show_selpat_dialog (this, TRUE);
                return TRUE;

            case GDK_KP_Subtract:
            case GDK_minus:
                gnome_cmd_file_list_show_selpat_dialog (this, FALSE);
                return TRUE;

            case GDK_KP_Multiply:
                invert_selection();
                return TRUE;

            case GDK_KP_Divide:
                restore_selection();
                return TRUE;

            case GDK_Insert:
            case GDK_KP_Insert:
                toggle();
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_STEP_FORWARD, 0.0, nullptr);
                return TRUE;

            case GDK_KP_Page_Up:
                event->keyval = GDK_Page_Up;
                return FALSE;

            case GDK_KP_Page_Down:
                event->keyval = GDK_Page_Down;
                return FALSE;

            case GDK_KP_Up:
                event->keyval = GDK_Up;
                return FALSE;

            case GDK_KP_Down:
                event->keyval = GDK_Down;
                return FALSE;

            case GDK_Home:
            case GDK_KP_Home:
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_JUMP, 0.0);
                return TRUE;

            case GDK_End:
            case GDK_KP_End:
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_JUMP, 1.0);
                return TRUE;

            case GDK_Delete:
            case GDK_KP_Delete:
                gnome_cmd_file_list_show_delete_dialog (this);
                return TRUE;

            case GDK_Shift_L:
            case GDK_Shift_R:
                if (!priv->shift_down)
                    priv->shift_down_row = priv->cur_file;
                return TRUE;

            case GDK_Menu:
                show_file_popup (this, nullptr);
                return TRUE;

            default:
                break;
        }
    }

    return FALSE;
}


GList *GnomeCmdFileList::sort_selection(GList *list)
{
    return g_list_sort_with_data (list, (GCompareDataFunc) priv->sort_func, this);
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
        cwd->voffset = gnome_cmd_clist_get_voffset (*this);
    }

    cwd = dir;

    switch (dir->state)
    {
        case GnomeCmdDir::STATE_EMPTY:
            g_signal_connect (dir, "list-ok", G_CALLBACK (on_dir_list_ok), this);
            g_signal_connect (dir, "list-failed", G_CALLBACK (on_dir_list_failed), this);
            gnome_cmd_dir_list_files (dir, gnome_cmd_con_needs_list_visprog (con));
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
                gnome_cmd_dir_relist_files (dir, gnome_cmd_con_needs_list_visprog (con));
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
    gtk_clist_set_row_height (*this, gnome_cmd_data.options.list_row_height);
    gnome_cmd_clist_update_style (*this);
}


gboolean GnomeCmdFileList::file_is_wanted(GnomeCmdFile *gnomeCmdFile)
{
    g_return_val_if_fail (gnomeCmdFile != nullptr, FALSE);
    g_return_val_if_fail (gnomeCmdFile->gFile != nullptr, FALSE);

    GError *error;
    error = nullptr;

    auto gFileInfo = g_file_query_info(gnomeCmdFile->gFile,
                                   "standard::*",
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   nullptr,
                                   &error);
    if (error)
    {
        g_message ("file_is_wanted: retrieving file info for %s failed: %s", g_file_info_get_name(gnomeCmdFile->gFileInfo), error->message);
        g_error_free (error);
        return TRUE;
    }

    auto gFileType       = g_file_info_get_attribute_uint32(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_TYPE);
    auto gFileIsHidden   = g_file_info_get_attribute_boolean(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN);
    auto gFileIsSymLink  = g_file_info_get_attribute_boolean(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);
    auto gFileIsVirtual  = g_file_info_get_attribute_boolean(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL);
    auto gFileIsVolatile = g_file_info_get_attribute_boolean(gFileInfo, G_FILE_ATTRIBUTE_STANDARD_IS_VOLATILE);

    auto returnValue = TRUE;

    auto fileNameString = g_file_get_basename(gnomeCmdFile->gFile);

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
    const gchar *focus_dir = nullptr;
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
        focus_dir = GNOME_CMD_FILE (cwd)->get_name();
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
        focus_file(focus_dir, FALSE);

    g_free (dir);
}


void GnomeCmdFileList::invalidate_tree_size()
{
    for (auto i = get_visible_files(); i; i = i->next)
    {
        auto f = static_cast<GnomeCmdFile*> (i->data);
        if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
            f->invalidate_tree_size();
    }
}


/******************************************************
 * DnD functions
 **/

static gboolean do_scroll (GnomeCmdFileList *fl)
{
    gint w, h;
    gint focus_row, top_row;
    gint row_count;
    GtkCList *clist = *fl;

    gdk_drawable_get_size (GTK_WIDGET (clist)->window, &w, &h);

    row_count = clist->rows;
    focus_row = gnome_cmd_clist_get_row (*fl, 1, fl->priv->autoscroll_y);
    top_row = gnome_cmd_clist_get_row (*fl, 1, 0);

    if (!fl->priv->autoscroll_dir)
    {
        if (focus_row > 0)
            gtk_clist_moveto (clist, top_row-1, 0, 0, 0);
        else
            return FALSE;
    }
    else
    {
        if (focus_row < row_count)
            gtk_clist_moveto (clist, top_row+1, 0, 0, 0);
        else
            return FALSE;
    }

    return TRUE;
}


static void autoscroll_if_appropriate (GnomeCmdFileList *fl, gint x, gint y)
{
    if (y < 0) return;

    GtkCList *clist = *fl;
    gint w, h;

    gdk_drawable_get_size (GTK_WIDGET (clist)->window, &w, &h);

    gint smin = h/8;
    gint smax = h-smin;

    if (y < smin)
    {
        if (fl->priv->autoscroll_timeout) return;
        fl->priv->autoscroll_dir = FALSE;
        fl->priv->autoscroll_y = y;
        fl->priv->autoscroll_timeout = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) do_scroll, fl);
    }
    else if (y > smax)
    {
        if (fl->priv->autoscroll_timeout) return;
        fl->priv->autoscroll_dir = TRUE;
        fl->priv->autoscroll_y = y;
        fl->priv->autoscroll_timeout = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) do_scroll, fl);
    }
    else
    {
        if (fl->priv->autoscroll_timeout)
        {
            g_source_remove (fl->priv->autoscroll_timeout);
            fl->priv->autoscroll_timeout = 0;
        }
    }
}


static void drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data, guint info, guint32 time, GnomeCmdFileList *fl)
{
    int len;
    GList *files;

    gchar *data = (gchar *) build_selected_file_list (fl, &len);

    if (!data) return;

    switch (info)
    {
        case TARGET_URI_LIST:
        case TARGET_TEXT_PLAIN:
            gtk_selection_data_set (selection_data, selection_data->target, 8, (const guchar *) data, len);
            break;

        case TARGET_URL:
            files = gnome_vfs_uri_list_parse (data);
            if (files)
                gtk_selection_data_set (selection_data, selection_data->target, 8, (const guchar *) files->data, strlen ((const char *) files->data));
            g_list_foreach (files, (GFunc) g_free, nullptr);
            break;

        default:
            g_assert_not_reached ();
    }

    g_free (data);
}


inline void restore_drag_indicator (GnomeCmdFileList *fl)
{
    gnome_cmd_clist_set_drag_row (*fl, -1);
}


static void unref_gfile_list (GList *list)
{
    g_list_foreach (list, (GFunc) g_object_unref, nullptr);
}


static void free_dnd_popup_data (gpointer *data)
{
    if (data)
    {
        GList *gFileGlist = (GList *) data[0];
        unref_gfile_list (gFileGlist);
    }

    g_free (data);
}


static void drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint32 time, GnomeCmdFileList *fl)
{
    GtkCList *clist = *fl;

    // find the row that the file was dropped on
    y -= clist->column_title_area.height - GTK_CONTAINER (clist)->border_width;
    if (y < 0) return;

    GnomeCmdFile *f = fl->get_file_at_row (gnome_cmd_clist_get_row (*fl, x, y));
    GnomeCmdDir *to = fl->cwd;

    // the drop was over a directory in the list, which means that the
    // xfer should be done to that directory instead of the current one in the list
    if (f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        to = f->is_dotdot
            ? gnome_cmd_dir_get_parent (to)
            : gnome_cmd_dir_get_child (to, g_file_info_get_display_name(f->gFileInfo));

    // transform the drag data to a list with GFiles
    GList *gFileGlist = uri_strings_to_gfiles ((gchar *) selection_data->data);

    GdkModifierType mask;

    gdk_display_get_pointer (gdk_display_get_default (), nullptr, &x, &y, &mask);

    if (gnome_cmd_data.options.confirm_mouse_dnd && !(mask&(GDK_SHIFT_MASK|GDK_CONTROL_MASK)))
    {
        gpointer *arr = g_new (gpointer, 2);

        arr[0] = gFileGlist;
        arr[1] = to;

        gtk_item_factory_popup_with_data (fl->priv->ifac, arr, (GDestroyNotify) free_dnd_popup_data, x, y, 0, time);

        return;
    }

    // find out what operation to perform
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (context->action)
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


static void drag_end (GtkWidget *widget, GdkDragContext *context, GnomeCmdFileList *fl)
{
    restore_drag_indicator (fl);
}


static void drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, GnomeCmdFileList *fl)
{
    if (fl->priv->autoscroll_timeout)
    {
        g_source_remove (fl->priv->autoscroll_timeout);
        fl->priv->autoscroll_timeout = 0;
    }

    restore_drag_indicator (fl);
}


static gboolean drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, GnomeCmdFileList *fl)
{
    gdk_drag_status (context, context->suggested_action, time);

    GtkCList *clist = *fl;

    y -= (clist->column_title_area.height - GTK_CONTAINER (clist)->border_width);

    gint row = gnome_cmd_clist_get_row (*fl, x, y);

    if (row > -1)
    {
        GnomeCmdFile *f = fl->get_file_at_row(row);

        if (f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY)
            row = -1;

        gnome_cmd_clist_set_drag_row (*fl, row);
    }

    autoscroll_if_appropriate (fl, x, y);

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
    gtk_drag_source_set (*this,
                         GDK_BUTTON1_MASK,
                         drag_types, G_N_ELEMENTS (drag_types),
                         (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK));

    g_signal_connect (this, "drag-end", G_CALLBACK (drag_end), this);
    g_signal_connect (this, "drag-data-get", G_CALLBACK (drag_data_get), this);
    g_signal_connect (this, "drag-data-delete", G_CALLBACK (drag_data_delete), this);

    // set up drag destination
    gtk_drag_dest_set (*this,
                       GTK_DEST_DEFAULT_DROP,
                       drop_types, G_N_ELEMENTS (drop_types),
                       (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK));

    g_signal_connect (this, "drag-motion", G_CALLBACK (drag_motion), this);
    g_signal_connect (this, "drag-leave", G_CALLBACK (drag_leave), this);
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
