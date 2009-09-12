/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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
#include <regex.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-patternsel-dialog.h"
#include "imageloader.h"
#include "cap.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-file-popmenu.h"
#include "gnome-cmd-rename-dialog.h"
#include "gnome-cmd-chown-dialog.h"
#include "gnome-cmd-chmod-dialog.h"
#include "gnome-cmd-delete-dialog.h"
#include "gnome-cmd-quicksearch-popup.h"
#include "gnome-cmd-file-collection.h"
#include "gnome-cmd-user-actions.h"
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


enum
{
    FILE_CLICKED,        // A file in the list was clicked
    FILE_RELEASED,       // A file in the list has been clicked and mouse button has been released
    LIST_CLICKED,        // The file list widget was clicked
    EMPTY_SPACE_CLICKED, // The file list was clicked but not on a file
    FILES_CHANGED,       // The visible content of the file list has changed (files have been: selected, created, deleted or modified)
    DIR_CHANGED,         // The current directory has been changed
    LAST_SIGNAL
};


GtkTargetEntry drag_types [] = {
    { TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
    { TARGET_TEXT_PLAIN_TYPE, 0, TARGET_TEXT_PLAIN },
    { TARGET_URL_TYPE, 0, TARGET_URL }
};

static GnomeCmdCListClass *parent_class = NULL;

static guint file_list_signals[LAST_SIGNAL] = { 0 };


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


GnomeCmdFileListColumn file_list_column[GnomeCmdFileList::NUM_COLUMNS] =
{{GnomeCmdFileList::COLUMN_ICON,"",16,GTK_JUSTIFY_CENTER,GTK_SORT_ASCENDING, NULL},
 {GnomeCmdFileList::COLUMN_NAME, N_("name"), 140, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_name},
 {GnomeCmdFileList::COLUMN_EXT, N_("ext"), 40, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_ext},
 {GnomeCmdFileList::COLUMN_DIR, N_("dir"), 240, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_dir},
 {GnomeCmdFileList::COLUMN_SIZE, N_("size"), 70, GTK_JUSTIFY_RIGHT, GTK_SORT_DESCENDING, (GCompareDataFunc) sort_by_size},
 {GnomeCmdFileList::COLUMN_DATE, N_("date"), 150, GTK_JUSTIFY_LEFT, GTK_SORT_DESCENDING, (GCompareDataFunc) sort_by_date},
 {GnomeCmdFileList::COLUMN_PERM, N_("perm"), 70, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_perm},
 {GnomeCmdFileList::COLUMN_OWNER, N_("uid"), 50, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_owner},
 {GnomeCmdFileList::COLUMN_GROUP, N_("gid"), 50, GTK_JUSTIFY_LEFT, GTK_SORT_ASCENDING, (GCompareDataFunc) sort_by_group}};


class GnomeCmdFileList::Private
{
  public:

    GtkWidget *column_pixmaps[GnomeCmdFileList::NUM_COLUMNS];
    GtkWidget *column_labels[GnomeCmdFileList::NUM_COLUMNS];
    GtkWidget *popup_menu;

    gint cur_file;
    GnomeCmdFileCollection visible_files;
    GList *selected_files;                         // contains GnomeCmdFile pointers

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

    Private(GnomeCmdFileList *fl);
    ~Private();
};


GnomeCmdFileList::Private::Private(GnomeCmdFileList *fl)
{
    selected_files = NULL;

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

    memset(sort_raising, FALSE, sizeof(sort_raising));

    gint col = COLUMN_NAME;             // defaults,
    gboolean b = GTK_SORT_ASCENDING;    // used when not set by gnome_cmd_data_get_sort_params()

    gnome_cmd_data_get_sort_params (fl, col, b);
    current_col = col;
    sort_raising[col] = b;
    sort_func = file_list_column[col].sort_func;

    for (gint i=0; i<NUM_COLUMNS; i++)
        gtk_clist_set_column_resizeable (*fl, i, TRUE);
}


GnomeCmdFileList::Private::~Private()
{
    gnome_cmd_file_list_free (selected_files);
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

    FileFormatData(GnomeCmdFile *f, gboolean tree_size);
    ~FileFormatData();
};


inline FileFormatData::FileFormatData(GnomeCmdFile *f, gboolean tree_size)
{
    // If the user wants a character instead of icon for filetype set it now
    if (gnome_cmd_data.layout == GNOME_CMD_LAYOUT_TEXT)
        text[GnomeCmdFileList::COLUMN_ICON] = (gchar *) gnome_cmd_file_get_type_string (f);
    else
        text[GnomeCmdFileList::COLUMN_ICON] = NULL;

    // Prepare the strings to show
    gchar *t1 = gnome_cmd_file_get_path (f);
    gchar *t2 = g_path_get_dirname (t1);
    dpath = get_utf8 (t2);
    g_free (t1);
    g_free (t2);

    if (gnome_cmd_data.ext_disp_mode == GNOME_CMD_EXT_DISP_STRIPPED
        && f->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
    {
        gchar *t = strip_extension (gnome_cmd_file_get_name (f));
        fname = get_utf8 (t);
        g_free (t);
    }
    else
        fname = get_utf8 (gnome_cmd_file_get_name (f));

    if (gnome_cmd_data.ext_disp_mode != GNOME_CMD_EXT_DISP_WITH_FNAME)
        fext = get_utf8 (gnome_cmd_file_get_extension (f));
    else
        fext = NULL;

    //Set other file information
    text[GnomeCmdFileList::COLUMN_NAME]  = fname;
    text[GnomeCmdFileList::COLUMN_EXT]   = fext;
    text[GnomeCmdFileList::COLUMN_DIR]   = dpath;
    text[GnomeCmdFileList::COLUMN_SIZE]  = tree_size ? (gchar *) gnome_cmd_file_get_tree_size_as_str (f) :
                                                       (gchar *) gnome_cmd_file_get_size (f);
    text[GnomeCmdFileList::COLUMN_DATE]  = (gchar *) gnome_cmd_file_get_mdate (f, FALSE);
    text[GnomeCmdFileList::COLUMN_PERM]  = (gchar *) gnome_cmd_file_get_perm (f);
    text[GnomeCmdFileList::COLUMN_OWNER] = (gchar *) gnome_cmd_file_get_owner (f);
    text[GnomeCmdFileList::COLUMN_GROUP] = (gchar *) gnome_cmd_file_get_group (f);
}


inline FileFormatData::~FileFormatData()
{
    g_free (dpath);
    g_free (fname);
    g_free (fext);
}


static void on_selpat_hide (GtkWidget *dialog, GnomeCmdFileList *fl)
{
    fl->priv->selpat_dialog = NULL;
}


inline void show_selpat_dialog (GnomeCmdFileList *fl, gboolean mode)
{
    if (fl->priv->selpat_dialog)  return;

    GtkWidget *dialog = gnome_cmd_patternsel_dialog_new (fl, mode);

    gtk_widget_ref (dialog);
    gtk_signal_connect (GTK_OBJECT (dialog), "hide", GTK_SIGNAL_FUNC (on_selpat_hide), fl);
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


static void select_file (GnomeCmdFileList *fl, GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (f != NULL);
    g_return_if_fail (f->info != NULL);

    if (strcmp (f->info->name, "..") == 0)
        return;

    gint row = fl->get_row_from_file(f);
    if (row == -1)
        return;


    if (!gnome_cmd_data.use_ls_colors)
        gtk_clist_set_row_style (*fl, row, row%2 ? alt_sel_list_style : sel_list_style);
    else
    {
        GnomeCmdColorTheme *colors = gnome_cmd_data_get_current_color_theme ();
        if (!colors->respect_theme)
        {
            gtk_clist_set_foreground (*fl, row, colors->sel_fg);
            gtk_clist_set_background (*fl, row, colors->sel_bg);
        }
    }

    if (g_list_index (fl->priv->selected_files, f) != -1)
        return;

    gnome_cmd_file_ref (f);
    fl->priv->selected_files = g_list_append (fl->priv->selected_files, f);

    gtk_signal_emit (*fl, file_list_signals[FILES_CHANGED]);
}


static void unselect_file (GnomeCmdFileList *fl, GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (f != NULL);

    gint row = fl->get_row_from_file(f);
    if (row == -1)
        return;

    if (g_list_index (fl->priv->selected_files, f) == -1)
        return;

    gnome_cmd_file_unref (f);
    fl->priv->selected_files = g_list_remove (fl->priv->selected_files, f);

    if (!gnome_cmd_data.use_ls_colors)
        gtk_clist_set_row_style (*fl, row, row%2 ? alt_list_style : list_style);
    else
        if (LsColor *col = ls_colors_get (f))
        {
            GnomeCmdColorTheme *colors = gnome_cmd_data_get_current_color_theme ();
            GdkColor *fg = col->fg ? col->fg : colors->norm_fg;
            GdkColor *bg = col->bg ? col->bg : colors->norm_bg;

            if (bg)  gtk_clist_set_background (*fl, row, bg);
            if (fg)  gtk_clist_set_foreground (*fl, row, fg);
        }

    gtk_signal_emit (*fl, file_list_signals[FILES_CHANGED]);
}


inline void toggle_file (GnomeCmdFileList *fl, GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (f != NULL);

    gint row = fl->get_row_from_file(f);

    if (row == -1)
        return;

    if (row < fl->priv->visible_files.size())
        if (g_list_index (fl->priv->selected_files, f) == -1)
            select_file (fl, f);
        else
            unselect_file (fl, f);
}


inline void select_file_at_row (GnomeCmdFileList *fl, gint row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    fl->priv->cur_file = row;

    GnomeCmdFile *f = fl->get_file_at_row(row);

    if (f)
        select_file (fl, f);
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
        select_file_at_row (fl, i);
}


inline void toggle_file_at_row (GnomeCmdFileList *fl, gint row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    fl->priv->cur_file = row;

    GnomeCmdFile *f = fl->get_file_at_row(row);

    if (f)
        toggle_file (fl, f);
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

    const gchar *ext1 = gnome_cmd_file_get_extension (f);
    if (!ext1) return;

    for (GList *tmp=fl->get_visible_files(); tmp; tmp=tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;

        if (f && f->info)
        {
            const gchar *ext2 = gnome_cmd_file_get_extension (f);

            if (ext2 && strcmp (ext1, ext2) == 0)
            {
                if (select)
                    select_file (fl, f);
                else
                    unselect_file (fl, f);
            }
        }
    }
}


inline void toggle_with_pattern (GnomeCmdFileList *fl, const gchar *pattern, gboolean case_sens, gboolean mode)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    Filter filter(pattern, case_sens, gnome_cmd_data.filter_type);

    for (GList *tmp=fl->get_visible_files(); tmp; tmp=tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;

        if (f && f->info)
            if (filter.match(f->info->name))
            {
                if (mode)
                    select_file (fl, f);
                else
                    unselect_file (fl, f);
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
        gtk_widget_ref (hbox);
        gtk_object_set_data_full (*this, "column-hbox", hbox, (GtkDestroyNotify) gtk_widget_unref);
        gtk_widget_show (hbox);

        priv->column_labels[i] = gtk_label_new (_(file_list_column[i].title));
        gtk_widget_ref (priv->column_labels[i]);
        gtk_object_set_data_full (*this, "column-label", priv->column_labels[i], (GtkDestroyNotify) gtk_widget_unref);
        gtk_widget_show (priv->column_labels[i]);
        gtk_box_pack_start (GTK_BOX (hbox), priv->column_labels[i], TRUE, TRUE, 0);

        pixmap = gtk_pixmap_new (pm, bm);
        gtk_widget_ref (pixmap);
        gtk_object_set_data_full (*this, "column-pixmap", pixmap, (GtkDestroyNotify) gtk_widget_unref);
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

            if (gnome_vfs_uri_is_local (gnome_cmd_file_get_uri (f)))
            {
#ifdef UNESCAPE_LOCAL_FILES
                fn = gnome_vfs_unescape_string (gnome_cmd_file_get_uri_str (f), 0);
#endif
            }

            if (!fn)
                fn = gnome_cmd_file_get_uri_str (f);

            uri_str = g_strdup_printf ("%s\r\n", fn);
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
            char *uri_str = g_strdup (gnome_cmd_file_get_uri_str (f));

            *file_list_len = strlen (uri_str) + 1;
            return uri_str;
        }

    *file_list_len = 0;
    g_list_free (sel_files);
    return NULL;
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


inline void init_dnd (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gtk_drag_source_set (*fl, GDK_BUTTON1_MASK, drag_types, G_N_ELEMENTS (drag_types),
                         (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK | GDK_ACTION_DEFAULT));

    gtk_signal_connect (*fl, "drag-data-get", GTK_SIGNAL_FUNC (drag_data_get), fl);
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

    gtk_widget_ref (menu);
    gtk_object_set_data_full (*fl, "file_popup_menu", menu, (GtkDestroyNotify) gtk_widget_unref);

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
        select_file (fl, focus_file);
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


static gint sort_by_name (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (strcmp (f1->info->name, "..") == 0)
        return -1;

    if (strcmp (f2->info->name, "..") == 0)
        return 1;

    if (f1->info->type > f2->info->type)
        return -1;

    if (f1->info->type < f2->info->type)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];

    return my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), raising);
}


static gint sort_by_ext (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (strcmp (f1->info->name, "..") == 0)
        return -1;

    if (strcmp (f2->info->name, "..") == 0)
        return 1;

    if (f1->info->type > f2->info->type)
        return -1;

    if (f1->info->type < f2->info->type)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];

    if (!gnome_cmd_file_get_extension (f1) && !gnome_cmd_file_get_extension (f2))
        return my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), fl->priv->sort_raising[1]);

    if (!gnome_cmd_file_get_extension (f1))
        return raising?1:-1;
    if (!gnome_cmd_file_get_extension (f2))
        return raising?-1:1;

    gint ret = my_strcmp (gnome_cmd_file_get_extension (f1), gnome_cmd_file_get_extension (f2), raising);

    return ret ? ret : my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), fl->priv->sort_raising[1]);
}


static gint sort_by_dir (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (strcmp (f1->info->name, "..") == 0)
        return -1;

    if (strcmp (f2->info->name, "..") == 0)
        return 1;

    if (f1->info->type > f2->info->type)
        return -1;

    if (f1->info->type < f2->info->type)
        return 1;

    // gchar *t1 = gnome_cmd_file_get_path (f1);
    // gchar *t2 = gnome_cmd_file_get_path (f2);
    // gchar *d1 = g_path_get_dirname (t1);
    // gchar *d2 = g_path_get_dirname (t2);

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    // gint ret = my_strcmp (d1, d2, raising);

    // g_free (t1);
    // g_free (t2);
    // g_free (d1);
    // g_free (d2);

    // return ret;

    return my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), raising);
}


static gint sort_by_size (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (strcmp (f1->info->name, "..") == 0)
        return -1;

    if (strcmp (f2->info->name, "..") == 0)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);

    if (!ret)
    {
        ret = my_intcmp (f1->info->size, f2->info->size, raising);
        if (!ret)
            ret = my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), file_raising);
    }
    return ret;
}


static gint sort_by_perm (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (strcmp (f1->info->name, "..") == 0)
        return -1;

    if (strcmp (f2->info->name, "..") == 0)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->permissions, f2->info->permissions, raising);
        if (!ret)
            ret = my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), file_raising);
    }
    return ret;
}


static gint sort_by_date (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (strcmp (f1->info->name, "..") == 0)
        return -1;

    if (strcmp (f2->info->name, "..") == 0)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->mtime, f2->info->mtime, raising);
        if (!ret)
            ret = my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), file_raising);
    }
    return ret;
}


static gint sort_by_owner (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (strcmp (f1->info->name, "..") == 0)
        return -1;

    if (strcmp (f2->info->name, "..") == 0)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->uid, f2->info->uid, raising);
        if (!ret)
            ret = my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), file_raising);
    }
    return ret;
}


static gint sort_by_group (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    if (strcmp (f1->info->name, "..") == 0)
        return -1;

    if (strcmp (f2->info->name, "..") == 0)
        return 1;

    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    gint ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->gid, f2->info->gid, raising);
        if (!ret)
            ret = my_strcmp (gnome_cmd_file_get_collation_fname (f1), gnome_cmd_file_get_collation_fname (f2), file_raising);
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

    gnome_cmd_data_set_sort_params (fl, col, fl->priv->sort_raising[col]);
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

    gtk_signal_emit (*fl, file_list_signals[LIST_CLICKED], event);

    gint row = gnome_cmd_clist_get_row (*fl, event->x, event->y);
    if (row < 0)
    {
        gtk_signal_emit (*fl, file_list_signals[EMPTY_SPACE_CLICKED], event);
        return FALSE;
    }

    GnomeCmdFile *f = fl->get_file_at_row(row);

    gtk_signal_emit (*fl, file_list_signals[FILE_CLICKED], f, event);

    gtk_signal_emit_stop_by_name (GTK_OBJECT (clist), "button-press-event");

    return TRUE;
}


inline gboolean mime_exec_file (GnomeCmdFile *f)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (f), FALSE);

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
    g_return_if_fail (f != NULL);
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
                        if (fl->priv->selected_files || gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
                            toggle_file_at_row (fl, row);
                        else
                        {
                            if (prev_row!=row)
                                select_file (fl, fl->get_file_at_row(prev_row));
                            select_file_at_row (fl, row);
                        }
                    }
            }
            else
                if (event->button == 3)
                    if (strcmp (f->info->name, "..") != 0)
                    {
                        if (gnome_cmd_data.right_mouse_button_mode == GnomeCmdData::RIGHT_BUTTON_SELECTS)
                        {
                            if (g_list_index (fl->priv->selected_files, f) == -1)
                            {
                                select_file (fl, f);
                                fl->priv->right_mb_sel_state = 1;
                            }
                            else
                            {
                                unselect_file (fl, f);
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
    g_return_if_fail (f != NULL);
    g_return_if_fail (event != NULL);

    if (event->type == GDK_BUTTON_RELEASE && event->button == 1 && !fl->modifier_click && gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        mime_exec_file (f);
}


static void on_motion_notify (GtkCList *clist, GdkEventMotion *event, GnomeCmdFileList *fl)
{
    g_return_if_fail (event != NULL);

    if (event->state & GDK_BUTTON3_MASK)
    {
        gint row;
        gint y;

        y = event->y;
        y -= (clist->column_title_area.height - GTK_CONTAINER (clist)->border_width);

        row = gnome_cmd_clist_get_row (*fl, event->x, y);

        if (row != -1)
        {
            GnomeCmdFile *f = fl->get_file_at_row(row+1);
            if (f)
            {
                fl->select_row(row+1);
                if (fl->priv->right_mb_sel_state)
                    select_file (fl, f);
                else
                    unselect_file (fl, f);
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

    gtk_signal_emit (*fl, file_list_signals[FILE_RELEASED], f, event);

    if (event->type == GDK_BUTTON_RELEASE)
    {
        if (event->button == 1 && state_is_blank (event->state))
        {
            if (f && g_list_index (fl->priv->selected_files, f)==-1 && gnome_cmd_data.left_mouse_button_unselects)
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


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (object);

    delete fl->priv;

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdFileListClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdCListClass *) gtk_type_class (gnome_cmd_clist_get_type ());

    file_list_signals[FILE_CLICKED] =
        gtk_signal_new ("file-clicked",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, file_clicked),
                        gtk_marshal_NONE__POINTER_POINTER,
                        GTK_TYPE_NONE,
                        2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);

    file_list_signals[FILE_RELEASED] =
        gtk_signal_new ("file-released",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, file_released),
                        gtk_marshal_NONE__POINTER_POINTER,
                        GTK_TYPE_NONE,
                        2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);

    file_list_signals[LIST_CLICKED] =
        gtk_signal_new ("list-clicked",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, list_clicked),
                        gtk_marshal_NONE__POINTER,
                        GTK_TYPE_NONE,
                        1,
                        GTK_TYPE_POINTER);

    file_list_signals[EMPTY_SPACE_CLICKED] =
        gtk_signal_new ("empty-space-clicked",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, empty_space_clicked),
                        gtk_marshal_NONE__POINTER,
                        GTK_TYPE_NONE,
                        1, GTK_TYPE_POINTER);

    file_list_signals[FILES_CHANGED] =
        gtk_signal_new ("files-changed",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, files_changed),
                        gtk_marshal_NONE__NONE,
                        GTK_TYPE_NONE,
                        0);

    file_list_signals[DIR_CHANGED] =
        gtk_signal_new ("dir-changed",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, dir_changed),
                        gtk_marshal_NONE__POINTER,
                        GTK_TYPE_NONE,
                        1, GTK_TYPE_POINTER);

    object_class->destroy = destroy;
    widget_class->map = ::map;
    klass->file_clicked = NULL;
    klass->file_released = NULL;
    klass->list_clicked = NULL;
    klass->files_changed = NULL;
    klass->dir_changed = NULL;
}

static void init (GnomeCmdFileList *fl)
{
    fl->priv = new GnomeCmdFileList::Private(fl);

    init_dnd (fl);

    gtk_signal_connect_after (*fl, "scroll-vertical", GTK_SIGNAL_FUNC (on_scroll_vertical), fl);
    gtk_signal_connect (*fl, "click-column", GTK_SIGNAL_FUNC (on_column_clicked), fl);

    gtk_signal_connect (*fl, "button-press-event", GTK_SIGNAL_FUNC (on_button_press), fl);
    gtk_signal_connect (*fl, "button-release-event", GTK_SIGNAL_FUNC (on_button_release), fl);
    gtk_signal_connect (*fl, "motion-notify-event", GTK_SIGNAL_FUNC (on_motion_notify), fl);

    gtk_signal_connect_after (*fl, "realize", GTK_SIGNAL_FUNC (on_realize), fl);
    gtk_signal_connect (*fl, "file-clicked", GTK_SIGNAL_FUNC (on_file_clicked), fl);
    gtk_signal_connect (*fl, "file-released", GTK_SIGNAL_FUNC (on_file_released), fl);
}


/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_file_list_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdFileList",
            sizeof (GnomeCmdFileList),
            sizeof (GnomeCmdFileListClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gnome_cmd_clist_get_type (), &info);
    }
    return type;
}


guint GnomeCmdFileList::get_column_default_width (GnomeCmdFileList::ColumnID col)
{
    return file_list_column[col].default_width;
}


GnomeCmdFileList::ColumnID GnomeCmdFileList::get_sort_column()
{
    return (ColumnID) priv->current_col;
}


inline void add_file_to_clist (GnomeCmdFileList *fl, GnomeCmdFile *f, gint in_row)
{
    GtkCList *clist = GTK_CLIST (fl);

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
        gtk_clist_set_pixmap (clist, row, 0,
                              gnome_cmd_file_get_type_pixmap (f),
                              gnome_cmd_file_get_type_mask (f));
    }

    // If we have been waiting for this file to show up, focus it
    if (fl->priv->focus_later &&
        strcmp (gnome_cmd_file_get_name (f), fl->priv->focus_later) == 0)
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

    GList *list;
    GList *files = NULL;

    // select the files to show
    for (gnome_cmd_dir_get_files (dir, &list); list; list = list->next)
    {
        GnomeCmdFile *f = GNOME_CMD_FILE (list->data);

        if (file_is_wanted (f))
            files = g_list_append (files, f);
    }

    // Create a parent dir file (..) if appropriate
    gchar *path = gnome_cmd_file_get_path (GNOME_CMD_FILE (dir));
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
    if (!gnome_cmd_file_needs_update (f))
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

        if (gnome_cmd_file_get_type_pixmap_and_mask (f, &pixmap, &mask))
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


gboolean GnomeCmdFileList::remove_file(GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, FALSE);

    gint row = get_row_from_file(f);

    if (row<0)                              // f not found in the shown file list...
        return FALSE;

    gtk_clist_remove (GTK_CLIST (this), row);

    priv->selected_files = g_list_remove (priv->selected_files, f);
    priv->visible_files.remove(f);

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
    gtk_clist_clear (GTK_CLIST (this));
    priv->visible_files.clear();
    gnome_cmd_file_list_free (priv->selected_files);
    priv->selected_files = NULL;
}


GList *GnomeCmdFileList::get_selected_files()
{
    if (priv->selected_files)
        return g_list_copy (priv->selected_files);

    GnomeCmdFile *file = get_selected_file();

    return file ? g_list_append (NULL, file) : NULL;
}


GList *GnomeCmdFileList::get_marked_files()
{
    return priv->selected_files;
}


GList *GnomeCmdFileList::get_visible_files()
{
    return priv->visible_files.get_list();
}


GnomeCmdFile *GnomeCmdFileList::get_first_selected_file()
{
    return priv->selected_files ? (GnomeCmdFile *) priv->selected_files->data : get_selected_file();
}


GnomeCmdFile *GnomeCmdFileList::get_focused_file()
{
    return priv->cur_file < 0 ? NULL : get_file_at_row(priv->cur_file);
}


void GnomeCmdFileList::select_all()
{
    gnome_cmd_file_list_free (priv->selected_files);
    priv->selected_files = NULL;

    for (GList *tmp = get_visible_files(); tmp; tmp = tmp->next)
        select_file (this, (GnomeCmdFile *) tmp->data);
}


void GnomeCmdFileList::unselect_all()
{
    GList *selfiles = g_list_copy (priv->selected_files);

    for (GList *tmp = selfiles; tmp; tmp = tmp->next)
        unselect_file (this, (GnomeCmdFile *) tmp->data);

    g_list_free (selfiles);

    gnome_cmd_file_list_free (priv->selected_files);
    priv->selected_files = NULL;
}


void GnomeCmdFileList::toggle()
{
    GnomeCmdFile *f = get_file_at_row(priv->cur_file);

    if (f)
        toggle_file (this, f);
}


void GnomeCmdFileList::toggle_and_step()
{

    GnomeCmdFile *f = get_file_at_row(priv->cur_file);

    if (f)
        toggle_file (this, f);
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
    focus_file_at_row (this, row==-1 ? priv->cur_file : row);
}


void GnomeCmdFileList::select_pattern(const gchar *pattern, gboolean case_sens)
{
    toggle_with_pattern (this, pattern, case_sens, TRUE);
}


void GnomeCmdFileList::unselect_pattern(const gchar *pattern, gboolean case_sens)
{
    toggle_with_pattern (this, pattern, case_sens, FALSE);
}


void GnomeCmdFileList::invert_selection()
{
    GList *sel = g_list_copy (priv->selected_files);

    for (GList *tmp=get_visible_files(); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;

        if (f && f->info)
        {
            if (g_list_index (sel, f) == -1)
                select_file (this, f);
            else
                unselect_file (this, f);
        }
    }

    g_list_free (sel);
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


static gint compare_filename (GnomeCmdFile *f1, GnomeCmdFile *f2)
{
    return strcmp (f1->info->name, f2->info->name);
}


void gnome_cmd_file_list_compare_directories (GnomeCmdFileList *fl1, GnomeCmdFileList *fl2)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl1) || GNOME_CMD_IS_FILE_LIST (fl2));

    fl1->unselect_all();
    fl2->select_all();

    for (GList *i=fl1->get_visible_files(); i; i=i->next)
    {
        GnomeCmdFile *f1 = (GnomeCmdFile *) i->data;
        GnomeCmdFile *f2;
        GList *gl2 = g_list_find_custom (fl2->priv->selected_files, f1, (GCompareFunc) compare_filename);

        if (!gl2)
        {
            select_file (fl1, f1);
            continue;
        }

        f2 = (GnomeCmdFile *) gl2->data;

        if (f1->info->type==GNOME_VFS_FILE_TYPE_DIRECTORY || f2->info->type==GNOME_VFS_FILE_TYPE_DIRECTORY)
        {
            unselect_file (fl2, f2);
            continue;
        }

        if (f1->info->mtime > f2->info->mtime)
        {
            select_file (fl1, f1);
            unselect_file (fl2, f2);
            continue;
        }

        if (f1->info->mtime == f2->info->mtime)
        {
            if (f1->info->size == f2->info->size)
                unselect_file (fl2, f2);
            else
                select_file (fl1, f1);
        }
    }
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
        gtk_clist_moveto (GTK_CLIST (this), selrow, -1, 1, 0);
    }

    // reselect the previously selected files
    for (GList *list = priv->selected_files; list; list = list->next)
        select_file (this, GNOME_CMD_FILE (list->data));

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

        gtk_widget_ref (dialog);
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


void gnome_cmd_file_list_show_chown_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();

    if (files)
    {
        GtkWidget *dialog = gnome_cmd_chown_dialog_new (files);

        gtk_widget_ref (dialog);
        gtk_widget_show (dialog);
        g_list_free (files);
    }
}


void gnome_cmd_file_list_show_chmod_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = fl->get_selected_files();

    if (files)
    {
        GtkWidget *dialog = gnome_cmd_chmod_dialog_new (files);

        gtk_widget_ref (dialog);
        gtk_widget_show (dialog);
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
        create_error_dialog (_("Not an ordinary file: %s"), f->info->name);
    else
        gnome_cmd_file_view (f, internal_viewer);
}


void gnome_cmd_file_list_edit (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *f = fl->get_selected_file();

    if (!f)  return;

    if (f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        create_error_dialog (_("Not an ordinary file: %s"), f->info->name);
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
    gtk_widget_ref (fl->priv->quicksearch_popup);
    gtk_widget_show (fl->priv->quicksearch_popup);
    if (c != 0)
    {
        GnomeCmdQuicksearchPopup *popup = GNOME_CMD_QUICKSEARCH_POPUP (fl->priv->quicksearch_popup);
        gtk_entry_set_text (GTK_ENTRY (popup->entry), text);
        gtk_editable_set_position (GTK_EDITABLE (popup->entry), 1);
    }

    gtk_signal_connect (GTK_OBJECT (fl->priv->quicksearch_popup), "hide",
                        GTK_SIGNAL_FUNC (on_quicksearch_popup_hide), fl);
}


gboolean GnomeCmdFileList::key_pressed(GdkEventKey *event)
{
    g_return_val_if_fail (event != NULL, FALSE);

    if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
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
            case GDK_Right:
                event->state -= GDK_SHIFT_MASK;
                return FALSE;

            case GDK_Page_Up:
            case GDK_KP_Page_Up:
                priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_PAGE_BACKWARD, 0.0, NULL);
                return FALSE;

            case GDK_Page_Down:
            case GDK_KP_Page_Down:
                priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_PAGE_FORWARD, 0.0, NULL);
                return FALSE;

            case GDK_Up:
            case GDK_KP_Up:
                priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_STEP_BACKWARD, 0.0, NULL);
                return FALSE;

            case GDK_Down:
            case GDK_KP_Down:
                priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_STEP_FORWARD, 0.0, NULL);
                return FALSE;

            case GDK_Home:
            case GDK_KP_Home:
                priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_JUMP, 0.0);
                return TRUE;

            case GDK_End:
            case GDK_KP_End:
                priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_JUMP, 1.0);
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
                on_column_clicked (GTK_CLIST (this), GnomeCmdFileList::COLUMN_NAME, this);
                return TRUE;

            case GDK_F4:
                on_column_clicked (GTK_CLIST (this), GnomeCmdFileList::COLUMN_EXT, this);
                return TRUE;

            case GDK_F5:
                on_column_clicked (GTK_CLIST (this), GnomeCmdFileList::COLUMN_DATE, this);
                return TRUE;

            case GDK_F6:
                on_column_clicked (GTK_CLIST (this), GnomeCmdFileList::COLUMN_SIZE, this);
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
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_STEP_FORWARD, 0.0, NULL);
                return TRUE;

            case GDK_KP_Page_Up:
                event->keyval = GDK_Page_Up;
                return TRUE;

            case GDK_KP_Page_Down:
                event->keyval = GDK_Page_Down;
                return TRUE;

            case GDK_KP_Up:
                event->keyval = GDK_Up;
                return TRUE;

            case GDK_KP_Down:
                event->keyval = GDK_Down;
                return TRUE;

            case GDK_Home:
            case GDK_KP_Home:
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_JUMP, 0.0);
                return TRUE;

            case GDK_End:
            case GDK_KP_End:
                gtk_signal_emit_by_name (GTK_OBJECT (this), "scroll-vertical", GTK_SCROLL_JUMP, 1.0);
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
    if (strcmp (info->name, "..") == 0)
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


void GnomeCmdFileList::invalidate_tree_size()
{
    for (GList *tmp = get_visible_files(); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;
        if (f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
            gnome_cmd_file_invalidate_tree_size (f);
    }
}
