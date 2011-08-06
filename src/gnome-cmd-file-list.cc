/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include <stdio.h>
#include <glib-object.h>
#include <libgnomeui/gnome-popup-menu.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-patternsel-dialog.h"
#include "imageloader.h"
#include "cap.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-file-popmenu.h"
#include "gnome-cmd-rename-dialog.h"
#include "gnome-cmd-delete-dialog.h"
#include "gnome-cmd-quicksearch-popup.h"
#include "gnome-cmd-file-collection.h"
#include "ls_colors.h"

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


struct GnomeCmdFileListColumn
{
    guint id;
    const gchar *title;
    guint default_width;
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
{{GnomeCmdFileList::COLUMN_ICON,"",16,GTK_JUSTIFY_CENTER,GTK_SORT_ASCENDING, NULL},
 {GnomeCmdFileList::COLUMN_NAME, N_("name"), 140, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_name},
 {GnomeCmdFileList::COLUMN_EXT, N_("ext"), 40, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_ext},
 {GnomeCmdFileList::COLUMN_DIR, N_("dir"), 240, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_dir},
 {GnomeCmdFileList::COLUMN_SIZE, N_("size"), 70, GTK_JUSTIFY_RIGHT, GTK_SORT_DESCENDING, (GCompareDataFunc) sort_by_size},
 {GnomeCmdFileList::COLUMN_DATE, N_("date"), 150, GTK_JUSTIFY_LEFT, GTK_SORT_DESCENDING, (GCompareDataFunc) sort_by_date},
 {GnomeCmdFileList::COLUMN_PERM, N_("perm"), 70, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_perm},
 {GnomeCmdFileList::COLUMN_OWNER, N_("uid"), 50, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_owner},
 {GnomeCmdFileList::COLUMN_GROUP, N_("gid"), 50, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_group}};


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


class GnomeCmdFileList::Private
{
  public:

    GtkWidget *column_pixmaps[GnomeCmdFileList::NUM_COLUMNS];
    GtkWidget *column_labels[GnomeCmdFileList::NUM_COLUMNS];
    GtkWidget *popup_menu;

    gint cur_file;
    GnomeCmdFileCollection visible_files;
    GnomeCmd::Collection<GnomeCmdFile *> selected_files;      // contains GnomeCmdFile pointers, no refing

    GCompareDataFunc sort_func;
    gint current_col;
    gboolean sort_raising[GnomeCmdFileList::NUM_COLUMNS];

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

    Private(GnomeCmdFileList *fl);
    ~Private();
};


GnomeCmdFileList::Private::Private(GnomeCmdFileList *fl)
{
    memset(column_pixmaps, NULL, sizeof(column_pixmaps));
    memset(column_labels, NULL, sizeof(column_labels));

    popup_menu = NULL;
    quicksearch_popup = NULL;
    selpat_dialog = NULL;

    focus_later = NULL;
    shift_down = FALSE;
    shift_down_row = 0;
    right_mb_sel_state = FALSE;
    right_mb_down_file = NULL;
    right_mb_timeout_id = 0;

    autoscroll_dir = FALSE;
    autoscroll_timeout = 0;
    autoscroll_y = 0;

    con_opening = NULL;
    con_open_dialog = NULL;
    con_open_dialog_label = NULL;
    con_open_dialog_pbar = NULL;

    memset(sort_raising, GTK_SORT_ASCENDING, sizeof(sort_raising));

    for (gint i=0; i<NUM_COLUMNS; i++)
        gtk_clist_set_column_resizeable (*fl, i, TRUE);
}


GnomeCmdFileList::Private::~Private()
{
}


GnomeCmdFileList::GnomeCmdFileList(ColumnID sort_col, GtkSortType sort_order)
{
    tab_label_pin = NULL;
    tab_label_text = NULL;
    realized = FALSE;
    modifier_click = FALSE;
    locked = FALSE;
    con = NULL;
    cwd = NULL;
    lwd = NULL;
    connected_dir = NULL;

    priv->current_col = sort_col;
    priv->sort_raising[sort_col] = sort_order;
    priv->sort_func = file_list_column[sort_col].sort_func;

    create_column_titles();

    init_dnd();
}


inline gchar *strip_extension (const gchar *fname)
{
    gchar *s = g_strdup (fname);
    gchar *p = g_strrstr (s, ".");
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

    FileFormatData(GnomeCmdFile *f, gboolean tree_size);
    ~FileFormatData();
};


gchar FileFormatData::empty_string[] = "";


inline FileFormatData::FileFormatData(GnomeCmdFile *f, gboolean tree_size)
{
    // If the user wants a character instead of icon for filetype set it now
    if (gnome_cmd_data.layout == GNOME_CMD_LAYOUT_TEXT)
        text[GnomeCmdFileList::COLUMN_ICON] = (gchar *) f->get_type_string();
    else
        text[GnomeCmdFileList::COLUMN_ICON] = NULL;

    // Prepare the strings to show
    gchar *t1 = f->get_path();
    gchar *t2 = g_path_get_dirname (t1);
    dpath = get_utf8 (t2);
    g_free (t1);
    g_free (t2);

    if (gnome_cmd_data.ext_disp_mode == GNOME_CMD_EXT_DISP_STRIPPED
        && f->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
    {
        gchar *t = strip_extension (f->get_name());
        fname = get_utf8 (t);
        g_free (t);
    }
    else
        fname = get_utf8 (f->get_name());

    if (gnome_cmd_data.ext_disp_mode != GNOME_CMD_EXT_DISP_WITH_FNAME)
        fext = get_utf8 (f->get_extension());
    else
        fext = NULL;

    //Set other file information
    text[GnomeCmdFileList::COLUMN_NAME]  = fname;
    text[GnomeCmdFileList::COLUMN_EXT]   = fext;
    text[GnomeCmdFileList::COLUMN_DIR]   = dpath;
    text[GnomeCmdFileList::COLUMN_SIZE]  = tree_size ? (gchar *) f->get_tree_size_as_str() : (gchar *) f->get_size();

    if (f->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY || !f->is_dotdot)
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


inline FileFormatData::~FileFormatData()
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
    register GCClosure *cc = (GCClosure *) closure;
    register gpointer data1, data2;

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

    register GMarshalFunc_VOID__POINTER_POINTER callback = (GMarshalFunc_VOID__POINTER_POINTER) (marshal_data ? marshal_data : cc->callback);

    callback (data1,
              g_value_get_pointer (param_values + 1),
              g_value_get_pointer (param_values + 2),
              data2);
}


static void on_selpat_hide (GtkWidget *dialog, GnomeCmdFileList *fl)
{
    fl->priv->selpat_dialog = NULL;
}


inline void show_selpat_dialog (GnomeCmdFileList *fl, gboolean mode)
{
    if (fl->priv->selpat_dialog)  return;

    GtkWidget *dialog = gnome_cmd_patternsel_dialog_new (fl, mode);

    g_object_ref (dialog);
    g_signal_connect (dialog, "hide", G_CALLBACK (on_selpat_hide), fl);
    gtk_widget_show (dialog);

    fl->priv->selpat_dialog = dialog;
}


// Given a GnomeFileList, returns the upper-left corner of the selected file
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
    if (gnome_cmd_data.ext_disp_mode != GNOME_CMD_EXT_DISP_BOTH)
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
    fl->priv->quicksearch_popup = NULL;
}


void GnomeCmdFileList::select_file(GnomeCmdFile *f, gint row)
{
    g_return_if_fail (f != NULL);
    g_return_if_fail (f->info != NULL);

    if (f->is_dotdot)
        return;

    if (row == -1)
        row = get_row_from_file(f);
    if (row == -1)
        return;


    if (!gnome_cmd_data.use_ls_colors)
        gtk_clist_set_row_style (*this, row, row%2 ? alt_sel_list_style : sel_list_style);
    else
    {
        GnomeCmdColorTheme *colors = gnome_cmd_data_get_current_color_theme ();
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
    g_return_if_fail (f != NULL);

    if (!priv->selected_files.contain(f))
        return;

    if (row == -1)
        row = get_row_from_file(f);
    if (row == -1)
        return;

    priv->selected_files.remove(f);

    if (!gnome_cmd_data.use_ls_colors)
        gtk_clist_set_row_style (*this, row, row%2 ? alt_list_style : list_style);
    else
        if (LsColor *col = ls_colors_get (f))
        {
            GnomeCmdColorTheme *colors = gnome_cmd_data_get_current_color_theme ();
            GdkColor *fg = col->fg ? col->fg : colors->norm_fg;
            GdkColor *bg = col->bg ? col->bg : colors->norm_bg;

            if (bg)  gtk_clist_set_background (*this, row, bg);
            if (fg)  gtk_clist_set_foreground (*this, row, fg);
        }

    g_signal_emit (this, signals[FILES_CHANGED], 0);
}


void GnomeCmdFileList::toggle_file(GnomeCmdFile *f)
{
    gint row = get_row_from_file(f);

    if (row == -1)
        return;

    if (row < priv->visible_files.size())
        if (!priv->selected_files.contain(f))
            select_file(f, row);
        else
            unselect_file(f, row);
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

    for (GList *tmp=fl->get_visible_files(); tmp; tmp=tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;

        if (f && f->info)
        {
            const gchar *ext2 = f->get_extension();

            if (ext2 && strcmp (ext1, ext2) == 0)
            {
                if (select)
                    fl->select_file(f);
                else
                    fl->unselect_file(f);
            }
        }
    }
}


void GnomeCmdFileList::toggle_with_pattern(Filter &pattern, gboolean mode)
{
    for (GList *i=get_visible_files(); i; i=i->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        if (f && f->info && pattern.match(f->info->name))
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
        GList *uri_str_list = NULL;

        // create a list with the uri's of the selected files and calculate the total_length needed
        for (GList *tmp=sel_files; tmp; tmp=tmp->next)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;
            const gchar *fn = NULL;
            gchar *uri_str;

            if (gnome_vfs_uri_is_local (f->get_uri()))
            {
#ifdef UNESCAPE_LOCAL_FILES
                fn = gnome_vfs_unescape_string (f->get_uri_str(), 0);
#endif
            }

            if (!fn)
                fn = f->get_uri_str();

            uri_str = g_strconcat (fn, "\r\n", NULL);
            uri_str_list = g_list_append (uri_str_list, uri_str);
            total_len += strlen (uri_str);
        }

        // allocate memory
        total_len++;

        char *data, *copy;

        data = copy = (gchar *) g_malloc (total_len+1);

        // put the uri_str_list in the allocated memory
        for (GList *tmp=uri_str_list; tmp; tmp=tmp->next)
        {
            gchar *uri_str = (gchar *) tmp->data;

            strcpy (copy, uri_str);
            copy += strlen (uri_str);
        }

        g_list_foreach (uri_str_list, (GFunc) g_free, NULL);
        g_list_free (uri_str_list);

        data [total_len] = '\0';
        *file_list_len = total_len;
        return data;
    }
    else
        if (listlen == 1)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) sel_files->data;
            char *uri_str = f->get_uri_str();

            *file_list_len = strlen (uri_str) + 1;
            return uri_str;
        }

    *file_list_len = 0;
    g_list_free (sel_files);
    return NULL;
}


static void popup_position_function (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
    GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (user_data);

    gint unused_x, unused_w, unused_h;

    get_focus_row_coordinates (fl, unused_x, *y, unused_w, unused_h);
}


static void show_file_popup (GnomeCmdFileList *fl, GdkEventButton *event)
{
    // create the popup menu
    GtkWidget *menu = gnome_cmd_file_popmenu_new (fl);
    if (!menu) return;

    g_object_ref (menu);
    g_object_set_data_full (*fl, "file_popup_menu", menu, g_object_unref);

    gnome_popup_menu_do_popup (menu, (GtkMenuPositionFunc) popup_position_function, fl, event, fl, NULL);
}


inline void show_file_popup_with_warp (GnomeCmdFileList *fl)
{
    gint x, y, w, h;

    get_focus_row_coordinates (fl, x, y, w, h);

    //FIXME: Warp the pointer to x, y here

    show_file_popup (fl, NULL);
}


static gboolean on_right_mb_timeout (GnomeCmdFileList *fl)
{
    GnomeCmdFile *focus_file = fl->get_focused_file();

    if (fl->priv->right_mb_down_file == focus_file)
    {
        fl->select_file(focus_file);
        show_file_popup (fl, NULL);
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

    if (f1->info->type > f2->info->type)
        return -1;

    if (f1->info->type < f2->info->type)
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

    if (f1->info->type > f2->info->type)
        return -1;

    if (f1->info->type < f2->info->type)
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

    if (f1->info->type > f2->info->type)
        return -1;

    if (f1->info->type < f2->info->type)
        return 1;

    // gchar *t1 = f1->get_path();
    // gchar *t2 = f2->get_path();
    // gchar *d1 = g_path_get_dirname (t1);
    // gchar *d2 = g_path_get_dirname (t2);

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    // gint ret = my_strcmp (d1, d2, raising);

    // g_free (t1);
    // g_free (t2);
    // g_free (d1);
    // g_free (d2);

    // return ret;

    return my_strcmp (f1->get_collation_fname(), f2->get_collation_fname(), raising);
}


static gint sort_by_size (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (f1->is_dotdot)
        return -1;

    if (f2->is_dotdot)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);

    if (!ret)
    {
        ret = my_filesizecmp (f1->info->size, f2->info->size, raising);
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

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->permissions, f2->info->permissions, raising);
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

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->mtime, f2->info->mtime, raising);
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

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->uid, f2->info->uid, raising);
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

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->gid, f2->info->gid, raising);
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
                                                                 file_list_column[col].default_sort_direction;

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

    fl->priv->cur_file = clist->focus_row;
}


static gboolean on_button_press (GtkCList *clist, GdkEventButton *event, GnomeCmdFileList *fl)
{
    g_return_val_if_fail (clist != NULL, FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);

    if (GTK_CLIST (fl)->clist_window != event->window)
        return FALSE;

    gint row = gnome_cmd_clist_get_row (*fl, event->x, event->y);

    if (row < 0)
    {
        g_signal_emit (fl, signals[LIST_CLICKED], 0, NULL, event);
        g_signal_emit (fl, signals[EMPTY_SPACE_CLICKED], 0, event);
        return FALSE;
    }

    GnomeCmdFile *f = fl->get_file_at_row(row);

    g_signal_emit (fl, signals[LIST_CLICKED], 0, f, event);
    g_signal_emit (fl, signals[FILE_CLICKED], 0, f, event);

    g_signal_stop_emission_by_name (clist, "button-press-event");

    return TRUE;
}


inline gboolean mime_exec_file (GnomeCmdFile *f)
{
    if (f->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
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
    g_return_if_fail (event != NULL);

    fl->modifier_click = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK);

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1 && gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
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
                        if (!fl->priv->selected_files.empty() || gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
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
                        if (gnome_cmd_data.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
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
    g_return_if_fail (event != NULL);

    if (event->type == GDK_BUTTON_RELEASE && event->button == 1 && !fl->modifier_click && gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        mime_exec_file (f);
}


static void on_motion_notify (GtkCList *clist, GdkEventMotion *event, GnomeCmdFileList *fl)
{
    g_return_if_fail (event != NULL);

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
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
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
            if (f && !fl->priv->selected_files.contain(f) && gnome_cmd_data.left_mouse_button_unselects)
                fl->unselect_all();
            return TRUE;
        }
        else
            if (event->button == 3)
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


static void on_dir_list_failed (GnomeCmdDir *dir, GnomeVFSResult result, GnomeCmdFileList *fl)
{
    DEBUG('l', "on_dir_list_failed\n");

    if (result != GNOME_VFS_OK)
        gnome_cmd_show_message (NULL, _("Directory listing failed."), gnome_vfs_result_to_string (result));

    g_signal_handlers_disconnect_matched (fl->cwd, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, fl);
    fl->connected_dir = NULL;
    gnome_cmd_dir_unref (fl->cwd);
    set_cursor_default_for_widget (*fl);
    gtk_widget_set_sensitive (*fl, TRUE);

    if (fl->lwd && fl->con == gnome_cmd_dir_get_connection (fl->lwd))
    {
        fl->cwd = fl->lwd;
        g_signal_connect (fl->cwd, "list-ok", G_CALLBACK (on_dir_list_ok), fl);
        g_signal_connect (fl->cwd, "list-failed", G_CALLBACK (on_dir_list_failed), fl);
        fl->lwd = NULL;
    }
    else
        g_timeout_add (1, (GSourceFunc) set_home_connection, fl);
}


static void on_con_open_done (GnomeCmdCon *con, GnomeCmdFileList *fl)
{
    DEBUG('m', "on_con_open_done\n");

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (fl->priv->con_opening != NULL);
    g_return_if_fail (fl->priv->con_opening == con);
    g_return_if_fail (fl->priv->con_open_dialog != NULL);

    g_signal_handlers_disconnect_matched (con, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, fl);

    fl->set_connection (con);

    gtk_widget_destroy (fl->priv->con_open_dialog);
    fl->priv->con_open_dialog = NULL;
    fl->priv->con_opening = NULL;
}


static void on_con_open_failed (GnomeCmdCon *con, const gchar *msg, GnomeVFSResult result, GnomeCmdFileList *fl)
{
    DEBUG('m', "on_con_open_failed\n");

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (fl->priv->con_opening != NULL);
    g_return_if_fail (fl->priv->con_opening == con);
    g_return_if_fail (fl->priv->con_open_dialog != NULL);

    g_signal_handlers_disconnect_matched (con, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, fl);

    if (msg)
        gnome_cmd_show_message (NULL, msg);
    else
        gnome_cmd_show_message (NULL, _("Failed to open connection."), gnome_vfs_result_to_string (result));

    fl->priv->con_open_dialog = NULL;
    fl->priv->con_opening = NULL;
}


static void on_con_open_cancel (GtkButton *button, GnomeCmdFileList *fl)
{
    DEBUG('m', "on_con_open_cancel\n");

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (fl->priv->con_opening != NULL);
    g_return_if_fail (fl->priv->con_opening->state == GnomeCmdCon::STATE_OPENING);

    gnome_cmd_con_cancel_open (fl->priv->con_opening);

    gtk_widget_destroy (fl->priv->con_open_dialog);
    fl->priv->con_open_dialog = NULL;
    fl->priv->con_opening = NULL;
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

    fl->priv->con_open_dialog = gnome_cmd_dialog_new (NULL);
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
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER_POINTER,
            G_TYPE_NONE,
            2, G_TYPE_POINTER, G_TYPE_POINTER);

    signals[FILE_RELEASED] =
        g_signal_new ("file-released",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, file_released),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER_POINTER,
            G_TYPE_NONE,
            2, G_TYPE_POINTER, G_TYPE_POINTER);

    signals[LIST_CLICKED] =
        g_signal_new ("list-clicked",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, list_clicked),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER_POINTER,
            G_TYPE_NONE,
            2, G_TYPE_POINTER, G_TYPE_POINTER);

    signals[EMPTY_SPACE_CLICKED] =
        g_signal_new ("empty-space-clicked",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, empty_space_clicked),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[FILES_CHANGED] =
        g_signal_new ("files-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, files_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[DIR_CHANGED] =
        g_signal_new ("dir-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, dir_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    signals[CON_CHANGED] =
        g_signal_new ("con-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdFileListClass, con_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);
}

static void gnome_cmd_file_list_init (GnomeCmdFileList *fl)
{
    fl->priv = new GnomeCmdFileList::Private(fl);

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

guint GnomeCmdFileList::get_column_default_width (GnomeCmdFileList::ColumnID col)
{
    return file_list_column[col].default_width;
}


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

    FileFormatData data(f,FALSE);

    gint row = in_row == -1 ? gtk_clist_append (clist, data.text) : gtk_clist_insert (clist, in_row, data.text);

    // Setup row data and color
    if (!gnome_cmd_data.use_ls_colors)
        gtk_clist_set_row_style (clist, row, row%2 ? alt_list_style : list_style);
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
    if (gnome_cmd_data.layout != GNOME_CMD_LAYOUT_TEXT)
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

    GList *files = NULL;

    // select the files to show
    for (GList *i = gnome_cmd_dir_get_files (dir); i; i = i->next)
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
    for (GList *i = files; i; i = i->next)
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

    FileFormatData data(f, FALSE);

    for (gint i=1; i<GnomeCmdFileList::NUM_COLUMNS; i++)
        gtk_clist_set_text (*this, row, i, data.text[i]);

    if (gnome_cmd_data.layout != GNOME_CMD_LAYOUT_TEXT)
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

    FileFormatData data(f,TRUE);

    for (gint i=1; i<NUM_COLUMNS; i++)
        gtk_clist_set_text (*this, row, i, data.text[i]);
}


void GnomeCmdFileList::show_visible_tree_sizes()
{
    invalidate_tree_size();

    for (GList *files = get_visible_files(); files; files = files->next)
        show_dir_tree_size((GnomeCmdFile *) files->data);

    g_signal_emit (this, signals[FILES_CHANGED], 0);
}


gboolean GnomeCmdFileList::remove_file(GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, FALSE);

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
    g_return_val_if_fail (uri_str != NULL, FALSE);

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

    return f ? g_list_append (NULL, f) : NULL;
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

    GList *sel = sort_selection(priv->selected_files.get_list());
    GnomeCmdFile *f = (GnomeCmdFile *) sel->data;
    g_list_free (sel);

    return f;
}


GnomeCmdFile *GnomeCmdFileList::get_focused_file()
{
    return priv->cur_file < 0 ? NULL : get_file_at_row(priv->cur_file);
}


void GnomeCmdFileList::select_all()
{
    priv->selected_files.clear();

    for (GList *tmp = get_visible_files(); tmp; tmp = tmp->next)
        select_file((GnomeCmdFile *) tmp->data);
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
    if (priv->cur_file < size()-1)
        focus_file_at_row (this, priv->cur_file+1);
}


void GnomeCmdFileList::focus_file(const gchar *focus_file, gboolean scroll_to_file)
{
    for (GList *tmp = get_visible_files(); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;

        g_return_if_fail (f != NULL);
        g_return_if_fail (f->info != NULL);

        gint row = get_row_from_file (f);
        if (row == -1)
            return;

        if (strcmp (f->info->name, focus_file) == 0)
        {
            priv->cur_file = row;
            focus_file_at_row (this, row);
            if (scroll_to_file)
                gtk_clist_moveto (*this, row, 0, 0, 0);
            return;
        }
    }

    /* The file was not found, remember the filename in case the file gets
       added to the list in the future (after a FAM event etc). */
    g_free (priv->focus_later);
    priv->focus_later = g_strdup (focus_file);
}


void GnomeCmdFileList::select_row(gint row)
{
    focus_file_at_row (this, row==-1 ? 0 : row);
}


void GnomeCmdFileList::invert_selection()
{
    GnomeCmd::Collection<GnomeCmdFile *> sel = priv->selected_files;

    for (GList *tmp=get_visible_files(); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;

        if (f && f->info)
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
    show_selpat_dialog (fl, mode);
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

    if (!f)  return;

    if (f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        gnome_cmd_show_message (*main_win, _("Not an ordinary file"), f->info->name);
    else
        gnome_cmd_file_view (f, internal_viewer);
}


void gnome_cmd_file_list_edit (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (!f)  return;

    if (f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        gnome_cmd_show_message (*main_win, _("Not an ordinary file"), f->info->name);
    else
        gnome_cmd_file_edit (f);
}


gboolean gnome_cmd_file_list_quicksearch_shown (GnomeCmdFileList *fl)
{
    g_return_val_if_fail (fl!=NULL, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);
    g_return_val_if_fail (fl->priv!=NULL, FALSE);

    return fl->priv->quicksearch_popup!=NULL;
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
    g_return_val_if_fail (event != NULL, FALSE);

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
        }
    }
    else if (state_is_ctrl_alt (event->state) || state_is_ctrl_alt_shift (event->state))
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
                show_file_popup_with_warp (this);
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
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_PAGE_BACKWARD, 0.0, NULL);
                return FALSE;

            case GDK_Page_Down:
            case GDK_KP_Page_Down:
            case GDK_KP_3:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_PAGE_FORWARD, 0.0, NULL);
                return FALSE;

            case GDK_Up:
            case GDK_KP_Up:
            case GDK_KP_8:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_STEP_BACKWARD, 0.0, NULL);
                return FALSE;

            case GDK_Down:
            case GDK_KP_Down:
            case GDK_KP_2:
                priv->shift_down = TRUE;
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_STEP_FORWARD, 0.0, NULL);
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
                on_column_clicked (*this, GnomeCmdFileList::COLUMN_NAME, this);
                return TRUE;

            case GDK_F4:
                on_column_clicked (*this, GnomeCmdFileList::COLUMN_EXT, this);
                return TRUE;

            case GDK_F5:
                on_column_clicked (*this, GnomeCmdFileList::COLUMN_DATE, this);
                return TRUE;

            case GDK_F6:
                on_column_clicked (*this, GnomeCmdFileList::COLUMN_SIZE, this);
                return TRUE;
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
                show_selpat_dialog (this, TRUE);
                return TRUE;

            case GDK_KP_Subtract:
            case GDK_minus:
                show_selpat_dialog (this, FALSE);
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
                g_signal_emit_by_name (this, "scroll-vertical", GTK_SCROLL_STEP_FORWARD, 0.0, NULL);
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
                show_file_popup_with_warp (this);
                return TRUE;
        }
    }

    return FALSE;
}


GList *GnomeCmdFileList::sort_selection(GList *list)
{
    return g_list_sort_with_data (list, (GCompareDataFunc) priv->sort_func, this);
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
        lwd = NULL;
    }
    if (cwd)
    {
        gnome_cmd_dir_cancel_monitoring (cwd);
        gnome_cmd_dir_unref (cwd);
        cwd = NULL;
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
        g_signal_handlers_disconnect_matched (lwd, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, this);
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
                on_dir_list_ok (dir, NULL, this);
            break;
    }

    gnome_cmd_dir_start_monitoring (dir);
}


void GnomeCmdFileList::update_style()
{
    gtk_clist_set_row_height (*this, gnome_cmd_data.list_row_height);
    gnome_cmd_clist_update_style (*this);
}


gboolean GnomeCmdFileList::file_is_wanted(GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, FALSE);

    GnomeVFSFileInfo *info = f->info;

    if (strcmp (info->name, ".") == 0)
        return FALSE;
    if (f->is_dotdot)
        return FALSE;
    if (gnome_cmd_data.hide_type(info->type))
        return FALSE;
    if (info->symlink_name && gnome_cmd_data.hide_type(GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK))
        return FALSE;
    if (info->name[0] == '.' && gnome_cmd_data.filter_settings.hidden)
        return FALSE;
    if (gnome_cmd_data.filter_settings.backup && patlist_matches (gnome_cmd_data_get_backup_pattern_list (), info->name))
        return FALSE;

    return TRUE;
}


void GnomeCmdFileList::goto_directory(const gchar *in_dir)
{
    g_return_if_fail (in_dir != NULL);

    GnomeCmdDir *new_dir = NULL;
    const gchar *focus_dir = NULL;
    gchar *dir;

    if (g_str_has_prefix (in_dir, "~"))
        dir = gnome_vfs_expand_initial_tilde (in_dir);
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
        focus_file(focus_dir, FALSE);

    g_free (dir);
}


void GnomeCmdFileList::invalidate_tree_size()
{
    for (GList *tmp = get_visible_files(); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;
        if (f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
            f->invalidate_tree_size();
    }
}


/******************************************************
 * DnD functions
 **/

static gboolean do_scroll (GnomeCmdFileList *fl)
{
    gint w, h;
    gint focus_row, top_row, bottom_row;
    gint row_count;
    guint offset;
    gint row_height;
    GtkCList *clist = *fl;

    gdk_drawable_get_size (GTK_WIDGET (clist)->window, &w, &h);

    offset = (0-clist->voffset);
    row_height = gnome_cmd_data.list_row_height;
    row_count = clist->rows;
    focus_row = gnome_cmd_clist_get_row (*fl, 1, fl->priv->autoscroll_y);
    top_row = gnome_cmd_clist_get_row (*fl, 1, 0);
    bottom_row = gnome_cmd_clist_get_row (*fl, 1, h);

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
            g_list_foreach (files, (GFunc) g_free, NULL);
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


static void unref_uri_list (GList *list)
{
    g_list_foreach (list, (GFunc) gnome_vfs_uri_unref, NULL);
}


static void drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint32 time, GnomeCmdFileList *fl)
{
    GtkCList *clist = *fl;
    GnomeCmdFile *f;
    GnomeCmdDir *to, *cwd;
    GList *uri_list = NULL;
    gchar *to_fn = NULL;
    GnomeVFSXferOptions xferOptions;

    // Find out what operation to perform
    switch (context->action)
    {
        case GDK_ACTION_MOVE:
            xferOptions = GNOME_VFS_XFER_REMOVESOURCE;
            break;

        case GDK_ACTION_COPY:
            xferOptions = GNOME_VFS_XFER_RECURSIVE;
            break;

        case GDK_ACTION_LINK:
            xferOptions = GNOME_VFS_XFER_LINK_ITEMS;
            break;

        default:
            g_warning ("Unknown context->action in drag_data_received");
            return;
    }


    // Find the row that the file was dropped on
    y -= (clist->column_title_area.height - GTK_CONTAINER (clist)->border_width);
    if (y < 0) return;

    int row = gnome_cmd_clist_get_row (*fl, x, y);

    // Transform the drag data to a list with uris
    uri_list = strings_to_uris ((gchar *) selection_data->data);

    if (g_list_length (uri_list) == 1)
    {
        GnomeVFSURI *uri = (GnomeVFSURI *) uri_list->data;
        to_fn = gnome_vfs_unescape_string (gnome_vfs_uri_extract_short_name (uri), 0);
    }

    f = fl->get_file_at_row(row);
    cwd = fl->cwd;

    if (f && f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
        // The drop was over a directory in the list, which means that the
        // xfer should be done to that directory instead of the current one in the list
        to = f->is_dotdot ? gnome_cmd_dir_get_parent (cwd) : gnome_cmd_dir_get_child (cwd, f->info->name);
    }
    else
        to = cwd;

    g_return_if_fail (GNOME_CMD_IS_DIR (to));

    // Start the xfer
    gnome_cmd_xfer_uris_start (uri_list,
                               gnome_cmd_dir_ref (to),
                               NULL,
                               NULL,
                               to_fn,
                               xferOptions,
                               GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
                               GTK_SIGNAL_FUNC (unref_uri_list),
                               uri_list);
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

        if (f->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
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

    g_signal_connect (this, "drag-data-get", G_CALLBACK (drag_data_get), this);
    g_signal_connect (this, "drag-end", G_CALLBACK (drag_end), this);
    g_signal_connect (this, "drag-leave", G_CALLBACK (drag_leave), this);
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


XML::xstream &operator << (XML::xstream &xml, GnomeCmdFileList &fl)
{
    return xml << XML::tag("Tab") << XML::attr("dir") << GNOME_CMD_FILE (fl.cwd)->get_real_path() << XML::attr("sort") << fl.get_sort_column() << XML::attr("asc") << fl.get_sort_order() << XML::attr("lock") << fl.locked << XML::endtag();
}

