/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#include "gnome-cmd-advrename-dialog.h"
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
    LIST_CLICKED,        // The file-list widget was clicked
    EMPTY_SPACE_CLICKED, // The file-list was clicked but not on a file
    SELECTION_CHANGED,   // At least on file was selected/unselected
    LAST_SIGNAL
};


GtkTargetEntry drag_types [] = {
    { TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
    { TARGET_TEXT_PLAIN_TYPE, 0, TARGET_TEXT_PLAIN },
    { TARGET_URL_TYPE, 0, TARGET_URL }
};

static GnomeCmdCListClass *parent_class = NULL;

static guint file_list_signals[LAST_SIGNAL] = { 0 };

GList *gnome_vfs_list_sort (GList *list,
                            GnomeVFSListCompareFunc compare_func,
                            gpointer data);

static gint sort_by_name (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_ext (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_dir (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_size (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_date (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_perm (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_owner (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);
static gint sort_by_group (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl);


GnomeCmdFileListColumn file_list_column[FILE_LIST_NUM_COLUMNS] =
{{FILE_LIST_COLUMN_ICON,"",16,GTK_JUSTIFY_CENTER,FILE_LIST_SORT_ASCENDING,NULL},
 {FILE_LIST_COLUMN_NAME,N_("name"),140,GTK_JUSTIFY_LEFT,FILE_LIST_SORT_ASCENDING,(GnomeVFSListCompareFunc)sort_by_name},
 {FILE_LIST_COLUMN_EXT,N_("ext"),40,GTK_JUSTIFY_LEFT,FILE_LIST_SORT_ASCENDING,(GnomeVFSListCompareFunc)sort_by_ext},
 {FILE_LIST_COLUMN_DIR,N_("dir"),240,GTK_JUSTIFY_LEFT,FILE_LIST_SORT_ASCENDING,(GnomeVFSListCompareFunc)sort_by_dir},
 {FILE_LIST_COLUMN_SIZE,N_("size"),70,GTK_JUSTIFY_RIGHT,FILE_LIST_SORT_DESCENDING,(GnomeVFSListCompareFunc)sort_by_size},
 {FILE_LIST_COLUMN_DATE,N_("date"),150,GTK_JUSTIFY_CENTER,FILE_LIST_SORT_DESCENDING,(GnomeVFSListCompareFunc)sort_by_date},
 {FILE_LIST_COLUMN_PERM,N_("perm"),70,GTK_JUSTIFY_LEFT,FILE_LIST_SORT_ASCENDING,(GnomeVFSListCompareFunc)sort_by_perm},
 {FILE_LIST_COLUMN_OWNER,N_("uid"),50,GTK_JUSTIFY_LEFT,FILE_LIST_SORT_ASCENDING,(GnomeVFSListCompareFunc)sort_by_owner},
 {FILE_LIST_COLUMN_GROUP,N_("gid"),50,GTK_JUSTIFY_LEFT,FILE_LIST_SORT_ASCENDING,(GnomeVFSListCompareFunc)sort_by_group}};


struct _GnomeCmdFileListPrivate
{
    GtkWidget *column_pixmaps[FILE_LIST_NUM_COLUMNS];
    GtkWidget *column_labels[FILE_LIST_NUM_COLUMNS];
    GtkWidget *popup_menu;

    GnomeVFSListCompareFunc sort_func;
    gint current_col;
    gboolean sort_raising[FILE_LIST_NUM_COLUMNS];
    GnomeCmdFileCollection *shown_files;
    GList *selected_files;                         // contains GnomeCmdFile pointers
    gint cur_file;
    gboolean shift_down;
    gint shift_down_row;
    GnomeCmdFile *right_mb_down_file;
    gboolean right_mouse_sel_state;
    guint right_mb_timeout_id;
    GtkWidget *selpat_dialog;
    GtkWidget *quicksearch_popup;
    gchar *focus_later;
};


typedef struct
{
    gchar *text[FILE_LIST_NUM_COLUMNS];

    gchar *dpath;
    gchar *fname;
    gchar *fext;
} FileFormatData;


inline int get_num_files (GnomeCmdFileList *fl)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), -1);

    return g_list_length (gnome_cmd_file_list_get_all_files (fl));
}


inline GnomeCmdFile *get_file_at_row (GnomeCmdFileList *fl, gint row)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), NULL);

    return (GnomeCmdFile *) gtk_clist_get_row_data (GTK_CLIST (fl), row);
}


inline gint get_row_from_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), -1);
    g_return_val_if_fail (finfo != NULL, -1);

    return gtk_clist_find_row_from_data (GTK_CLIST (fl), finfo);
}


static void
on_selpat_hide (GtkWidget *dialog, GnomeCmdFileList *fl)
{
    fl->priv->selpat_dialog = NULL;
}


static void
show_selpat_dialog (GnomeCmdFileList *fl, gboolean mode)
{
    GtkWidget *dialog;

    if (fl->priv->selpat_dialog) return;

    dialog = gnome_cmd_patternsel_dialog_new (fl, mode);

    gtk_widget_ref (dialog);
    gtk_signal_connect (GTK_OBJECT (dialog), "hide", GTK_SIGNAL_FUNC (on_selpat_hide), fl);
    gtk_widget_show (dialog);

    fl->priv->selpat_dialog = dialog;
}


/* Given a GnomeFileList, returns the upper-left corner of the selected file
 *
 */
static void get_focus_row_coordinates(GnomeCmdFileList *fl, gint *x, gint *y, gint *width, gint *height)
{
    #define CELL_SPACING 1
    #define COLUMN_INSET 3

    gint ox, oy, row, rowh, colx;
    GnomeCmdExtDispMode ext_disp_mode;

    ext_disp_mode = gnome_cmd_data_get_ext_disp_mode();

    gdk_window_get_origin (GTK_CLIST(fl)->clist_window, &ox, &oy);
    row = GTK_CLIST (fl)->focus_row;
    rowh = GTK_CLIST (fl)->row_height + CELL_SPACING;
    colx = GTK_CLIST (fl)->column[1].area.x - COLUMN_INSET - CELL_SPACING;
    *width = GTK_CLIST (fl)->column[1].area.width + COLUMN_INSET * 2;
    if (ext_disp_mode != GNOME_CMD_EXT_DISP_WITH_FNAME)
        *width += GTK_CLIST (fl)->column[2].area.width + COLUMN_INSET + CELL_SPACING;
    *height = rowh + CELL_SPACING * 2;

    *x = ox + colx;
    *y = oy + row*rowh + GTK_CLIST (fl)->voffset;
}


inline void focus_file_at_row (GnomeCmdFileList *fl, gint row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GTK_CLIST (fl)->focus_row = row;
    fl->priv->cur_file = row;
    gtk_clist_select_row (GTK_CLIST (fl), row, 0);

}


static void
on_quicksearch_popup_hide (GtkWidget *quicksearch_popup, GnomeCmdFileList *fl)
{
    fl->priv->quicksearch_popup = NULL;
}


static void
select_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (finfo != NULL);
    g_return_if_fail (finfo->info != NULL);

    if (strcmp (finfo->info->name, "..") == 0)
        return;

    gint row = get_row_from_file (fl, finfo);
    if (row == -1)
        return;

    if (!gnome_cmd_data_get_use_ls_colors ())
        gtk_clist_set_row_style (GTK_CLIST (fl), row, sel_list_style);
    else
    {
        GnomeCmdColorTheme *colors = gnome_cmd_data_get_current_color_theme ();
        if (!colors->respect_theme)
        {
            gtk_clist_set_foreground (GTK_CLIST (fl), row, colors->sel_fg);
            gtk_clist_set_background (GTK_CLIST (fl), row, colors->sel_bg);
        }
    }

    if (g_list_index (fl->priv->selected_files, finfo) != -1)
        return;

    gnome_cmd_file_ref (finfo);
    fl->priv->selected_files = g_list_append (fl->priv->selected_files, finfo);

    gtk_signal_emit (GTK_OBJECT (fl), file_list_signals[SELECTION_CHANGED]);
}


static void
unselect_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (finfo != NULL);

    gint row = get_row_from_file (fl, finfo);
    if (row == -1)
        return;

    if (g_list_index (fl->priv->selected_files, finfo) == -1)
        return;

    gnome_cmd_file_unref (finfo);
    fl->priv->selected_files = g_list_remove (fl->priv->selected_files, finfo);

    if (!gnome_cmd_data_get_use_ls_colors ())
        gtk_clist_set_row_style (GTK_CLIST (fl), row, list_style);
    else
    {
        LsColor *col = ls_colors_get (finfo);
        GnomeCmdColorTheme *colors = gnome_cmd_data_get_current_color_theme ();
        GdkColor *fg, *bg;
        if (col)
        {
            fg = col->fg;
            bg = col->bg;
            if (!fg)  fg = colors->norm_fg;
            if (!bg)  bg = colors->norm_bg;
            if (bg)  gtk_clist_set_background (GTK_CLIST (fl), row, bg);
            if (fg)  gtk_clist_set_foreground (GTK_CLIST (fl), row, fg);
        }
    }
    gtk_signal_emit (GTK_OBJECT (fl), file_list_signals[SELECTION_CHANGED]);
}


inline void toggle_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (finfo != NULL);

    gint row = get_row_from_file (fl, finfo);

    if (row == -1)
        return;

    if (row < gnome_cmd_file_collection_get_size (fl->priv->shown_files))
        if (g_list_index (fl->priv->selected_files, finfo) == -1)
            select_file (fl, finfo);
        else
            unselect_file (fl, finfo);
}


inline void select_file_at_row (GnomeCmdFileList *fl, gint row)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    fl->priv->cur_file = row;

    GnomeCmdFile *finfo = get_file_at_row (fl, row);

    if (finfo)
            select_file (fl, finfo);
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

    GnomeCmdFile *finfo = get_file_at_row (fl, row);

    if (finfo)
        toggle_file (fl, finfo);
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


static void toggle_files_with_same_extension (GnomeCmdFileList *fl,
                                              gboolean select)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *sel;
    const gchar *ext1, *ext2;

    GnomeCmdFile *f = gnome_cmd_file_list_get_selected_file (fl);
    if (!f) return;
    ext1 = gnome_cmd_file_get_extension (f);
    if (!ext1) return;

    sel = g_list_copy (gnome_cmd_file_list_get_selected_files (fl));

    for (GList *tmp=gnome_cmd_file_list_get_all_files (fl); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) tmp->data;

        if (finfo && finfo->info)
        {
            ext2 = gnome_cmd_file_get_extension (finfo);

            if (ext2 && strcmp (ext1, ext2) == 0)
            {
                if (select)
                    select_file (fl, finfo);
                else
                    unselect_file (fl, finfo);
            }
        }
    }

    g_list_free (sel);
}


static void
toggle_with_pattern (GnomeCmdFileList *fl, const gchar *pattern, gboolean case_sens, gboolean mode)
{

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    Filter *filter = filter_new (pattern, case_sens);
    g_return_if_fail (filter != NULL);

    for (GList *tmp=gnome_cmd_file_list_get_all_files (fl); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) tmp->data;

        if (finfo && finfo->info)
        {
            if (filter_match (filter, finfo->info->name))
            {
                if (mode)
                    select_file (fl, finfo);
                else
                    unselect_file (fl, finfo);
            }
        }
    }

    filter_free (filter);
}


static void
create_column_titles (GnomeCmdFileList *fl)
{
    gint i;

    gtk_clist_set_column_width (GTK_CLIST (fl), 0, gnome_cmd_data_get_fs_col_width (FILE_LIST_COLUMN_ICON));
    gtk_clist_set_column_justification (GTK_CLIST (fl), 0, file_list_column[FILE_LIST_COLUMN_ICON].justification);
    gtk_clist_column_title_passive (GTK_CLIST (fl), FILE_LIST_COLUMN_ICON);

    for (i=FILE_LIST_COLUMN_NAME; i<FILE_LIST_NUM_COLUMNS; i++)
    {
        GtkWidget *hbox,*pixmap;

        GdkPixmap *pm = IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_BLANK);
        GdkBitmap *bm = IMAGE_get_mask (PIXMAP_FLIST_ARROW_BLANK);

        hbox = gtk_hbox_new (FALSE, 1);
        gtk_widget_ref (hbox);
        gtk_object_set_data_full (GTK_OBJECT (fl), "column-hbox", hbox, (GtkDestroyNotify) gtk_widget_unref);
        gtk_widget_show (hbox);

        fl->priv->column_labels[i] = gtk_label_new (_(file_list_column[i].title));
        gtk_widget_ref (fl->priv->column_labels[i]);
        gtk_object_set_data_full (GTK_OBJECT (fl), "column-label", fl->priv->column_labels[i],
                                  (GtkDestroyNotify) gtk_widget_unref);
        gtk_widget_show (fl->priv->column_labels[i]);
        gtk_box_pack_start (GTK_BOX (hbox), fl->priv->column_labels[i], TRUE, TRUE, 0);

        pixmap = gtk_pixmap_new (pm, bm);
        gtk_widget_ref (pixmap);
        gtk_object_set_data_full (GTK_OBJECT (fl), "column-pixmap", pixmap, (GtkDestroyNotify) gtk_widget_unref);
        gtk_widget_show (pixmap);
        gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);

        fl->priv->column_pixmaps[i] = pixmap;
        gtk_clist_set_column_widget (GTK_CLIST (fl), i, hbox);
    }

    for (i=FILE_LIST_COLUMN_NAME; i<FILE_LIST_NUM_COLUMNS; i++)
    {
        gtk_clist_set_column_width (GTK_CLIST (fl), i, gnome_cmd_data_get_fs_col_width (i));
        gtk_clist_set_column_justification (GTK_CLIST (fl), i, file_list_column[i].justification);
    }

    gtk_clist_column_titles_show (GTK_CLIST (fl));
}


static void
update_column_sort_arrows (GnomeCmdFileList *fl)
{
    gint i;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    for (i=FILE_LIST_COLUMN_NAME; i<FILE_LIST_NUM_COLUMNS; i++)
    {
        if (i != fl->priv->current_col)
        {
            gtk_pixmap_set (GTK_PIXMAP (fl->priv->column_pixmaps[i]),
                            IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_BLANK),
                            IMAGE_get_mask (PIXMAP_FLIST_ARROW_BLANK));
        }
        else
        {
            if (fl->priv->sort_raising[i])
            {
                gtk_pixmap_set (GTK_PIXMAP (fl->priv->column_pixmaps[i]),
                                IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_UP),
                                IMAGE_get_mask (PIXMAP_FLIST_ARROW_UP));
            }
            else
            {
                gtk_pixmap_set (GTK_PIXMAP (fl->priv->column_pixmaps[i]),
                                IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_DOWN),
                                IMAGE_get_mask (PIXMAP_FLIST_ARROW_DOWN));
            }
        }
    }
}


/******************************************************
 * DnD functions
 **/

static char *
build_selected_file_list (GnomeCmdFileList *fl, int *file_list_len)
{
    GList *sel_files = gnome_cmd_file_list_get_selected_files (fl);
    int listlen = g_list_length (sel_files);

    if (listlen > 1)
    {
        char *data, *copy;
        int total_len = 0;
        GList *tmp = sel_files;
        GList *uri_str_list = NULL;

        // create a list with the uri's of the selected files and calculate the total_length needed
        while (tmp)
        {
            GnomeCmdFile *finfo = (GnomeCmdFile *) tmp->data;
            const gchar *fn = NULL;
            gchar *uri_str;

            if (gnome_vfs_uri_is_local (gnome_cmd_file_get_uri (finfo)))
            {
#ifdef UNESCAPE_LOCAL_FILES
                fn = gnome_vfs_unescape_string (gnome_cmd_file_get_uri_str (finfo), 0);
#endif
            }

            if (!fn)
                fn = gnome_cmd_file_get_uri_str (finfo);

            uri_str = g_strdup_printf ("%s\r\n", fn);
            uri_str_list = g_list_append (uri_str_list, uri_str);
            total_len += strlen (uri_str);

            tmp = tmp->next;
        }

        // allocate memory
        total_len++;
        data = copy = (gchar *) g_malloc (total_len+1);

        // put the uri_str_list in the allocated memory
        tmp = uri_str_list;
        while (tmp)
        {
            gchar *uri_str = (gchar *) tmp->data;

            strcpy (copy, uri_str);
            copy += strlen (uri_str);

            tmp = tmp->next;
        }

        g_list_foreach (uri_str_list, (GFunc)g_free, NULL);
        g_list_free (uri_str_list);

        data [total_len] = '\0';
        *file_list_len = total_len;
        return data;
    }
    else if (listlen == 1)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) sel_files->data;
        char *uri_str;

        uri_str = g_strdup (gnome_cmd_file_get_uri_str (finfo));

        *file_list_len = strlen (uri_str) + 1;
        return uri_str;
    }

    *file_list_len = 0;
    g_list_free (sel_files);
    return NULL;
}


static void
drag_data_get (GtkWidget        *widget,
               GdkDragContext   *context,
               GtkSelectionData *selection_data,
               guint            info,
               guint32          time,
               GnomeCmdFileList *fl)
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
            g_list_foreach (files, (GFunc)g_free, NULL);
            break;

        default:
            g_assert_not_reached ();
    }

    g_free (data);
}


static void
init_dnd (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gtk_drag_source_set (GTK_WIDGET (fl), GDK_BUTTON1_MASK,
                         drag_types, ELEMENTS (drag_types),
                         (GdkDragAction) (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK | GDK_ACTION_DEFAULT));

    gtk_signal_connect (GTK_OBJECT (fl), "drag_data_get", GTK_SIGNAL_FUNC (drag_data_get), fl);
}



static void popup_position_function (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
    GnomeCmdFileList *fl;
    gint w,h;

    fl = GNOME_CMD_FILE_LIST(user_data);
    get_focus_row_coordinates(fl, x, y, &w, &h);
}

static void
show_file_popup (GnomeCmdFileList *fl, GdkEventButton *event)
{
    // create the popup menu
    GtkWidget *menu = gnome_cmd_file_popmenu_new (fl);
    if (!menu) return;
    gtk_widget_ref (menu);
    gtk_object_set_data_full (GTK_OBJECT (fl), "file_popup_menu", menu, (GtkDestroyNotify)gtk_widget_unref);

    gnome_popup_menu_do_popup (menu, (GtkMenuPositionFunc)popup_position_function, fl, event, fl, NULL);
}


static void
show_file_popup_with_warp (GnomeCmdFileList *fl)
{
    gint x, y, w, h;

    get_focus_row_coordinates (fl, &x, &y, &w, &h);

    //FIXME: Warp the pointer to x, y here

    show_file_popup (fl, NULL);
}


static gboolean
on_right_mb_timeout (GnomeCmdFileList *fl)
{
    GnomeCmdFile *focus_file = gnome_cmd_file_list_get_focused_file (fl);

    if (fl->priv->right_mb_down_file == focus_file)
    {
        show_file_popup (fl, NULL);
        toggle_file (fl, focus_file);
        return FALSE;
    }

    fl->priv->right_mb_down_file = focus_file;
    return TRUE;
}


/******************************************************
 * Filesorting functions
 **/

static gint
my_strcmp (const gchar *s1, const gchar *s2, gboolean raising)
{
    int ret;

    if (gnome_cmd_data_get_case_sens_sort ())
        ret = strcmp (s1,s2);
    else
        ret = g_strcasecmp (s1,s2);

    if (ret > 0)
        return raising ? -1 : 1;
    else if (ret < 0)
        return raising ? 1 : -1;

    return 0;
}


static gint
my_intcmp (gint i1, gint i2, gboolean raising)
{
    if (i1 > i2)
        return raising ? -1 : 1;
    else if (i2 > i1)
        return raising ? 1 : -1;

    return 0;
}


static gint
sort_by_name (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];

    if (strcmp (f1->info->name, "..") == 0)
        return -1;
    else if (strcmp (f2->info->name, "..") == 0)
        return 1;
    else if (f1->info->type > f2->info->type)
        return -1;
    else if (f1->info->type < f2->info->type)
        return 1;

    return my_strcmp (f1->info->name, f2->info->name, raising);
}


static gint
sort_by_ext (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    gint ret;
    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];

    if (strcmp (f1->info->name, "..") == 0)
        return -1;
    else if (strcmp (f2->info->name, "..") == 0)
        return 1;
    else if (f1->info->type > f2->info->type)
        return -1;
    else if (f1->info->type < f2->info->type)
        return 1;

    if (!gnome_cmd_file_get_extension(f1) && !gnome_cmd_file_get_extension (f2))
        return my_strcmp (f1->info->name, f2->info->name, fl->priv->sort_raising[1]);

    if (!gnome_cmd_file_get_extension (f1))
        return raising?1:-1;
    if (!gnome_cmd_file_get_extension (f2))
        return raising?-1:1;

    ret = my_strcmp (gnome_cmd_file_get_extension (f1), gnome_cmd_file_get_extension (f2), raising);

    if (ret == 0)
        return my_strcmp (f1->info->name, f2->info->name, fl->priv->sort_raising[1]);

    return ret;
}


static gint
sort_by_dir (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    gint ret = 0;
    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];

    gchar *t1 = gnome_cmd_file_get_path (f1);
    gchar *t2 = gnome_cmd_file_get_path (f2);
    gchar *d1 = g_path_get_dirname (t1);
    gchar *d2 = g_path_get_dirname (t2);

    if (strcmp (f1->info->name, "..") == 0)
        ret = -1;
    else if (strcmp (f2->info->name, "..") == 0)
        ret = 1;
    else if (f1->info->type > f2->info->type)
        ret = -1;
    else if (f1->info->type < f2->info->type)
        ret = 1;

    g_free (t1);
    g_free (t2);
    g_free (d1);
    g_free (d2);

    return ret ? ret : my_strcmp (d1, d2, raising);
}


static gint
sort_by_size (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    int ret;
    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    if (strcmp (f1->info->name, "..") == 0)
        return -1;
    else if (strcmp (f2->info->name, "..") == 0)
        return 1;

    ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->size, f2->info->size, raising);
        if (!ret)
        {
            ret = my_strcmp (f1->info->name, f2->info->name, file_raising);
        }
    }
    return ret;
}


static gint
sort_by_perm (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    int ret;
    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    if (strcmp (f1->info->name, "..") == 0)
        return -1;
    else if (strcmp (f2->info->name, "..") == 0)
        return 1;

    ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->permissions, f2->info->permissions, raising);
        if (!ret)
        {
            ret = my_strcmp (f1->info->name, f2->info->name, file_raising);
        }
    }
    return ret;
}


static gint
sort_by_date (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    int ret;
    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    if (strcmp (f1->info->name, "..") == 0)
        return -1;
    else if (strcmp (f2->info->name, "..") == 0)
        return 1;

    ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->mtime, f2->info->mtime, raising);
        if (!ret)
        {
            ret = my_strcmp (f1->info->name, f2->info->name, file_raising);
        }
    }
    return ret;
}


static gint
sort_by_owner (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    int ret;
    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    if (strcmp (f1->info->name, "..") == 0)
        return -1;
    else if (strcmp (f2->info->name, "..") == 0)
        return 1;

    ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->uid, f2->info->uid, raising);
        if (!ret)
        {
            ret = my_strcmp (f1->info->name, f2->info->name, file_raising);
        }
    }
    return ret;
}


static gint
sort_by_group (GnomeCmdFile *f1, GnomeCmdFile *f2, GnomeCmdFileList *fl)
{
    int ret;
    gboolean raising = fl->priv->sort_raising[fl->priv->current_col];
    gboolean file_raising = fl->priv->sort_raising[1];

    if (strcmp (f1->info->name, "..") == 0)
        return -1;
    else if (strcmp (f2->info->name, "..") == 0)
        return 1;

    ret = my_intcmp (f1->info->type, f2->info->type, TRUE);
    if (!ret)
    {
        ret = my_intcmp (f1->info->gid, f2->info->gid, raising);
        if (!ret)
        {
            ret = my_strcmp (f1->info->name, f2->info->name, file_raising);
        }
    }
    return ret;
}


/*******************************
 * Callbacks
 *******************************/

static void
on_column_clicked                        (GtkCList *list,
                                          gint col,
                                          GnomeCmdFileList *fl)
{
    g_return_if_fail (GTK_IS_CLIST (list));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    fl->priv->sort_raising[col] = fl->priv->current_col == col ? !fl->priv->sort_raising[col] :
                                                                 file_list_column[col].default_sort_direction;

    fl->priv->sort_func = file_list_column[col].sort_func;
    fl->priv->current_col = col;
    update_column_sort_arrows (fl);

    gnome_cmd_file_list_sort (fl);

    gnome_cmd_data_set_sort_params (fl, col, fl->priv->sort_raising[col]);
}


static void
on_scroll_vertical                  (GtkCList        *clist,
                                     GtkScrollType    scroll_type,
                                     gfloat           position,
                                     GnomeCmdFileList *fl)
{
    g_return_if_fail (GTK_IS_CLIST (clist));
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gint num_files = g_list_length (gnome_cmd_file_list_get_all_files (fl));

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


static gboolean
on_button_press (GtkCList *clist, GdkEventButton *event, GnomeCmdFileList *fl)
{
    g_return_val_if_fail (clist != NULL, FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);

    if (GTK_CLIST (fl)->clist_window != event->window)
        return FALSE;

    gtk_signal_emit (GTK_OBJECT (fl), file_list_signals[LIST_CLICKED], event);

    gint row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fl), event->x, event->y);
    if (row < 0)
    {
        gtk_signal_emit (GTK_OBJECT (fl), file_list_signals[EMPTY_SPACE_CLICKED], event);
        return FALSE;
    }

    GnomeCmdFile *finfo = get_file_at_row (fl, row);

    gtk_signal_emit (GTK_OBJECT (fl), file_list_signals[FILE_CLICKED], finfo, event);

    gtk_signal_emit_stop_by_name (GTK_OBJECT (clist), "button-press-event");

    return TRUE;
}


static gboolean
mime_exec_file (GnomeCmdFile *finfo)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE (finfo), FALSE);

    if (finfo->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
    {
        mime_exec_single (finfo);
        return TRUE;
    }

    return FALSE;
}


static void
on_file_clicked (GnomeCmdFileList *fl,
                 GnomeCmdFile *finfo, GdkEventButton *event,
                 gpointer data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (finfo != NULL);
    g_return_if_fail (event != NULL);

    gint row = get_row_from_file (fl, finfo);

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
        mime_exec_file (finfo);
    }
    else if (event->type == GDK_BUTTON_PRESS && (event->button == 1 || event->button == 3))
    {
        gnome_cmd_file_list_select_row (fl, row);
        gtk_widget_grab_focus (GTK_WIDGET (fl));

        if (event->button == 1)
        {
            if (event->state & GDK_SHIFT_MASK)
                select_file_range (fl, fl->priv->shift_down_row, row);
            else if (event->state & GDK_CONTROL_MASK)
                toggle_file_at_row (fl, row);
        }
        else if (event->button == 3)
        {
            if (strcmp (finfo->info->name, "..") != 0)
            {
                if (gnome_cmd_data_get_right_mouse_button_mode() == RIGHT_BUTTON_SELECTS)
                {
                    if (g_list_index (fl->priv->selected_files, finfo) == -1)
                    {
                        select_file (fl, finfo);
                        fl->priv->right_mouse_sel_state = 1;
                    }
                    else
                    {
                        unselect_file (fl, finfo);
                        fl->priv->right_mouse_sel_state = 0;
                    }

                    fl->priv->right_mb_down_file = finfo;
                    fl->priv->right_mb_timeout_id =
                        gtk_timeout_add (POPUP_TIMEOUT, (GtkFunction)on_right_mb_timeout, fl);
                }
                else
                    show_file_popup (fl, event);
            }
        }
    }
}


static void
on_motion_notify                     (GtkCList *clist,
                                      GdkEventMotion *event,
                                      GnomeCmdFileList *fl)
{
    g_return_if_fail (event != NULL);

    if (event->state & GDK_BUTTON3_MASK)
    {
        gint row;
        gint y;

        y = event->y;
        y -= (clist->column_title_area.height - GTK_CONTAINER (clist)->border_width);

        row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fl), event->x, y);

        if (row != -1)
        {
            GnomeCmdFile *finfo = gnome_cmd_file_list_get_file_at_row (fl, row+1);
            if (finfo)
            {
                gnome_cmd_file_list_select_row (fl, row+1);
                if (fl->priv->right_mouse_sel_state)
                    select_file (fl, finfo);
                else
                    unselect_file (fl, finfo);
            }
        }
    }
}


static gint
on_button_release (GtkWidget *widget, GdkEventButton *event, GnomeCmdFileList *fl)
{
    gint row;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);

    if (GTK_CLIST (fl)->clist_window != event->window)
        return FALSE;

    row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fl), event->x, event->y);
    if (row < 0)
        return FALSE;

    if (event->type == GDK_BUTTON_RELEASE)
    {
        if (event->button == 1 && state_is_blank (event->state))
        {
            GnomeCmdFile *finfo = get_file_at_row (fl, row);
            if (finfo && g_list_index (fl->priv->selected_files, finfo) == -1)
                gnome_cmd_file_list_unselect_all (fl);
            return TRUE;
        }
        else if (event->button == 3)
        {
            gtk_timeout_remove (fl->priv->right_mb_timeout_id);
        }
    }

    return FALSE;
}


static void
on_realize                               (GnomeCmdFileList *fl,
                                          gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    update_column_sort_arrows (fl);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (object);

    gtk_object_destroy (GTK_OBJECT (fl->priv->shown_files));
    gnome_cmd_file_list_free (fl->priv->selected_files);

    if (fl->priv)
        g_free (fl->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdFileListClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdCListClass *) gtk_type_class (gnome_cmd_clist_get_type ());

    file_list_signals[FILE_CLICKED] =
        gtk_signal_new ("file_clicked",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, file_clicked),
                        gtk_marshal_NONE__POINTER_POINTER,
                        GTK_TYPE_NONE,
                        2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);

    file_list_signals[LIST_CLICKED] =
        gtk_signal_new ("list_clicked",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, list_clicked),
                        gtk_marshal_NONE__POINTER,
                        GTK_TYPE_NONE,
                        1,
                        GTK_TYPE_POINTER);

    file_list_signals[EMPTY_SPACE_CLICKED] =
        gtk_signal_new ("empty_space_clicked",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, empty_space_clicked),
                        gtk_marshal_NONE__POINTER,
                        GTK_TYPE_NONE,
                        1, GTK_TYPE_POINTER);

    file_list_signals[SELECTION_CHANGED] =
        gtk_signal_new ("selection_changed",
                        GTK_RUN_LAST,
                        G_OBJECT_CLASS_TYPE (object_class),
                        GTK_SIGNAL_OFFSET (GnomeCmdFileListClass, selection_changed),
                        gtk_marshal_NONE__NONE,
                        GTK_TYPE_NONE,
                        0);

    object_class->destroy = destroy;
    widget_class->map = ::map;
    klass->file_clicked = NULL;
    klass->list_clicked = NULL;
    klass->selection_changed = NULL;
}

static void
init (GnomeCmdFileList *fl)
{
    gint i;
    gboolean b;

    fl->priv = g_new (GnomeCmdFileListPrivate, 1);
    fl->priv->shown_files = gnome_cmd_file_collection_new ();
    fl->priv->selected_files = NULL;
    fl->priv->shift_down = FALSE;
    fl->priv->selpat_dialog = NULL;
    fl->priv->right_mb_down_file = NULL;
    fl->priv->right_mb_timeout_id = 0;
    fl->priv->quicksearch_popup = NULL;
    fl->priv->focus_later = NULL;

    for (i=0; i<FILE_LIST_NUM_COLUMNS; i++)
        fl->priv->sort_raising[i] = FALSE;

    gnome_cmd_data_get_sort_params (fl, &i, &b);
    fl->priv->current_col = i;
    fl->priv->sort_raising[i] = b;
    fl->priv->sort_func = file_list_column[i].sort_func;

    init_dnd (fl);

    for (i=0; i<FILE_LIST_NUM_COLUMNS; i++)
        gtk_clist_set_column_resizeable (GTK_CLIST (fl), i, TRUE);

    gtk_signal_connect_after (GTK_OBJECT (fl), "scroll_vertical", GTK_SIGNAL_FUNC (on_scroll_vertical), fl);
    gtk_signal_connect (GTK_OBJECT (fl), "click-column", GTK_SIGNAL_FUNC (on_column_clicked), fl);

    gtk_signal_connect (GTK_OBJECT (fl), "button-press-event", GTK_SIGNAL_FUNC (on_button_press), fl);
    gtk_signal_connect (GTK_OBJECT (fl), "button-release-event", GTK_SIGNAL_FUNC (on_button_release), fl);
    gtk_signal_connect (GTK_OBJECT (fl), "motion-notify-event", GTK_SIGNAL_FUNC (on_motion_notify), fl);

    gtk_signal_connect_after (GTK_OBJECT (fl), "realize", GTK_SIGNAL_FUNC (on_realize), fl);
    gtk_signal_connect (GTK_OBJECT (fl), "file-clicked", GTK_SIGNAL_FUNC (on_file_clicked), fl);
}


/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_file_list_get_type         (void)
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


GtkWidget*
gnome_cmd_file_list_new ()
{
    GnomeCmdFileList *fl = (GnomeCmdFileList *) g_object_new (gnome_cmd_file_list_get_type(), "n_columns", FILE_LIST_NUM_COLUMNS, NULL);

    create_column_titles (fl);

    return GTK_WIDGET (fl);
}


void
gnome_cmd_file_list_show_column         (GnomeCmdFileList *fl,
                                         GnomeCmdFileListColumnID col,
                                         gboolean value)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gtk_clist_set_column_visibility (GTK_CLIST (fl), col, value);
}


void
gnome_cmd_file_list_update_style (GnomeCmdFileList *fl)
{
    gtk_clist_set_row_height (GTK_CLIST (fl), gnome_cmd_data_get_list_row_height ());

    gnome_cmd_clist_update_style (GNOME_CMD_CLIST (fl));
}


static gchar *
strip_extension (const gchar *fname)
{
    gchar *s = g_strdup (fname);
    gchar *p = g_strrstr (s, ".");
    if (p && p != s)
        *p = '\0';
    return s;
}


static void
format_file_for_display (GnomeCmdFile *finfo, FileFormatData *data, gboolean tree_size)
{
    // If the user wants a character insted of icon for filetype set it now
    if (gnome_cmd_data_get_layout () == GNOME_CMD_LAYOUT_TEXT)
        data->text[FILE_LIST_COLUMN_ICON] = (gchar *) gnome_cmd_file_get_type_string (finfo);
    else
        data->text[FILE_LIST_COLUMN_ICON] = NULL;

    // Prepare the strings to show
    gchar *t1 = gnome_cmd_file_get_path (finfo);
    gchar *t2 = g_path_get_dirname (t1);
    data->dpath = get_utf8 (t2);
    g_free (t1);
    g_free (t2);

    if (gnome_cmd_data_get_ext_disp_mode() == GNOME_CMD_EXT_DISP_STRIPPED
        && finfo->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
    {
        gchar *t = strip_extension (gnome_cmd_file_get_name (finfo));
        data->fname = get_utf8 (t);
        g_free (t);
    }
    else
        data->fname = get_utf8 (gnome_cmd_file_get_name (finfo));

    if (gnome_cmd_data_get_ext_disp_mode() != GNOME_CMD_EXT_DISP_WITH_FNAME)
        data->fext = get_utf8 (gnome_cmd_file_get_extension (finfo));
    else
        data->fext = NULL;

    //Set other file information
    data->text[FILE_LIST_COLUMN_NAME]  = data->fname;
    data->text[FILE_LIST_COLUMN_EXT]   = data->fext;
    data->text[FILE_LIST_COLUMN_DIR]   = data->dpath;
    data->text[FILE_LIST_COLUMN_SIZE]  = tree_size ? (gchar *) gnome_cmd_file_get_tree_size (finfo) :
                                                     (gchar *) gnome_cmd_file_get_size (finfo);
    data->text[FILE_LIST_COLUMN_DATE]  = (gchar *) gnome_cmd_file_get_mdate (finfo, FALSE);
    data->text[FILE_LIST_COLUMN_PERM]  = (gchar *) gnome_cmd_file_get_perm (finfo);
    data->text[FILE_LIST_COLUMN_OWNER] = (gchar *) gnome_cmd_file_get_owner (finfo);
    data->text[FILE_LIST_COLUMN_GROUP] = (gchar *) gnome_cmd_file_get_group (finfo);
    data->text[FILE_LIST_NUM_COLUMNS]  = NULL;
}


static void
cleanup_file_format (FileFormatData *data)
{
    g_free (data->dpath);
    g_free (data->fname);
    if (data->fext)
        g_free (data->fext);
}


static void
add_file_to_clist (GnomeCmdFileList *fl, GnomeCmdFile *finfo, gint in_row)
{
    gint row;
    GtkCList *clist;
    FileFormatData data;

    clist = GTK_CLIST (fl);

    format_file_for_display (finfo, &data, FALSE);

    row = in_row == -1 ? gtk_clist_append (clist, data.text) : gtk_clist_insert (clist, in_row, data.text);

    cleanup_file_format (&data);


    /* Setup row data and color
     *
     */
    if (!gnome_cmd_data_get_use_ls_colors ())
        gtk_clist_set_row_style (clist, row, list_style);
    else
    {
        LsColor *col = ls_colors_get (finfo);
        if (col)
        {
            if (col->bg)
                gtk_clist_set_background (clist, row, col->bg);
            if (col->fg)
                gtk_clist_set_foreground (clist, row, col->fg);
        }
    }

    gtk_clist_set_row_data (clist, row, finfo);

    /* If the use wants icons to show file types set it now
     *
     */
    if (gnome_cmd_data_get_layout () != GNOME_CMD_LAYOUT_TEXT)
    {
        gtk_clist_set_pixmap (clist, row, 0,
                              gnome_cmd_file_get_type_pixmap (finfo),
                              gnome_cmd_file_get_type_mask (finfo));
    }

    // If we have been waiting for this file to show up, focus it
    if (fl->priv->focus_later &&
        strcmp (gnome_cmd_file_get_name (finfo), fl->priv->focus_later) == 0)
        focus_file_at_row (fl, row);
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_add_file
*
*   Purpose:  Add a file to the list
*
*   Params:   @fl: The FileList to add the file to
*             @finfo: The file to add
*             @in_row: The row to add the file at. Set to -1 to append the file at the end.
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/
void
gnome_cmd_file_list_add_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo, gint row)
{
    /* Add the file to the list
     *
     */
    gnome_cmd_file_collection_add (fl->priv->shown_files, finfo);

    add_file_to_clist (fl, finfo, row);
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_show_files
*
*   Purpose:  Show a list of files
*
*   Params:   @fl: The FileList to show the files in
*             @list: A list of files to show
*             @sort: Wether to sort the files or not
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/
void
gnome_cmd_file_list_show_files (GnomeCmdFileList *fl, GList *files, gboolean sort)
{
    GList *list, *tmp;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gnome_cmd_file_list_remove_all_files (fl);

    if (!files) return;

    list = g_list_copy (files);

    if (sort)
        tmp = list = gnome_vfs_list_sort (list, (GnomeVFSListCompareFunc)fl->priv->sort_func, fl);
    else
        tmp = list;

    gtk_clist_freeze (GTK_CLIST (fl));
    while (tmp)
    {
        gnome_cmd_file_list_add_file (fl, GNOME_CMD_FILE (tmp->data), -1);
        tmp = tmp->next;
    }
    gtk_clist_thaw (GTK_CLIST (fl));

    if (list)
        g_list_free (list);
}


void
gnome_cmd_file_list_insert_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    gint i;
    gint num_files = get_num_files (fl);

    for (i=0; i<num_files; i++)
    {
        GnomeCmdFile *finfo2 = get_file_at_row (fl, i);
        if (fl->priv->sort_func (finfo2, finfo, fl) == 1)
        {
            gnome_cmd_file_list_add_file (fl, finfo, i);
            return;
        }
    }

    // Insert the file at the end of the list
    gnome_cmd_file_list_add_file (fl, finfo, -1);
}


void
gnome_cmd_file_list_update_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    FileFormatData data;
    gint i,row;

    if (!gnome_cmd_file_needs_update (finfo))
        return;

    row = get_row_from_file (fl, finfo);
    if (row == -1)
        return;

    format_file_for_display (finfo, &data, FALSE);

    for (i=1; i<FILE_LIST_NUM_COLUMNS; i++)
        gtk_clist_set_text (GTK_CLIST (fl), row, i, data.text[i]);

    cleanup_file_format (&data);
}


void
gnome_cmd_file_list_show_dir_size (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    gint i,row;
    FileFormatData data;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (GNOME_CMD_IS_FILE (finfo));

    row = get_row_from_file (fl, finfo);
    if (row == -1)
        return;

    format_file_for_display (finfo, &data, TRUE);

    for (i=1; i<FILE_LIST_NUM_COLUMNS; i++)
        gtk_clist_set_text (GTK_CLIST (fl), row, i, data.text[i]);

    cleanup_file_format (&data);
}


void
gnome_cmd_file_list_remove_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    gint row;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (finfo != NULL);

    row = get_row_from_file (fl, finfo);
    if (row >= 0)
    {
        gtk_clist_remove (GTK_CLIST (fl), row);
        fl->priv->selected_files = g_list_remove (fl->priv->selected_files, finfo);
        gnome_cmd_file_collection_remove (fl->priv->shown_files, finfo);

        focus_file_at_row (fl, MIN(row, GTK_CLIST(fl)->focus_row));
    }
}


void
gnome_cmd_file_list_remove_file_by_uri (GnomeCmdFileList *fl, const gchar *uri_str)
{
    GnomeCmdFile *file;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));
    g_return_if_fail (uri_str != NULL);

    file = gnome_cmd_file_collection_lookup (fl->priv->shown_files, uri_str);
    g_return_if_fail (GNOME_CMD_IS_FILE (file));

    gnome_cmd_file_list_remove_file (fl, file);
}


void
gnome_cmd_file_list_remove_files (GnomeCmdFileList *fl, GList *files)
{
    for (; files; files = files->next)
        gnome_cmd_file_list_remove_file (fl, (GnomeCmdFile *) files->data);
}


void
gnome_cmd_file_list_remove_all_files (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gtk_clist_clear (GTK_CLIST (fl));
    gnome_cmd_file_collection_clear (fl->priv->shown_files);
    gnome_cmd_file_list_free (fl->priv->selected_files);
    fl->priv->selected_files = NULL;
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_get_selected_files
*
*   Purpose: Returns a list with all selected files. The list returned is
*            a copy and should be freed when no longer needed. The files
*            in the list is however not refed before returning.
*
*   Params:
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/

GList*
gnome_cmd_file_list_get_selected_files (GnomeCmdFileList *fl)
{
    GnomeCmdFile *file;

    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), NULL);

    if (fl->priv->selected_files)
        return g_list_copy (fl->priv->selected_files);

    file = gnome_cmd_file_list_get_selected_file (fl);

    return file ? g_list_append (NULL, file) : NULL;
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_get_marked_files
*
*   Purpose: Returns a list with all marked files. The list returned is
*            a copy and should be freed when no longer needed. The files
*            in the list is however not refed before returning.
*            A marked file is a file that has been selected with ins etc.
*            The file that is currently focused is not marked.
*
*   Params:
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/

GList*
gnome_cmd_file_list_get_marked_files (GnomeCmdFileList *fl)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), NULL);

    if (fl->priv->selected_files)
        return g_list_copy (fl->priv->selected_files);

    return NULL;
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_get_all_files
*
*   Purpose: Returns a list with all files shown in the file-list. The list
*            is the same as that in the file-list it self so make a copy and ref the files
*            if needed.
*
*   Params:
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/

GList*
gnome_cmd_file_list_get_all_files (GnomeCmdFileList *fl)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), NULL);

    return gnome_cmd_file_collection_get_list (fl->priv->shown_files);
}


/******************************************************************************
*
*   Function: gnome_cmd_file_list_get_selected_file
*
*   Purpose: Returns the currently focused file if any. The returned file is not
*            refed. The ".." file is NOT returned if focused
*
*   Params:
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/

GnomeCmdFile*
gnome_cmd_file_list_get_selected_file (GnomeCmdFileList *fl)
{
    GnomeCmdFile *finfo;

    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), NULL);

    finfo = gnome_cmd_file_list_get_focused_file (fl);

    return !finfo || strcmp (finfo->info->name, "..") == 0 ? NULL : finfo;
}



/******************************************************************************
*
*   Function: gnome_cmd_file_list_get_focused_file
*
*   Purpose: Returns the currently focused file if any. The returned file is not
*            refed. The ".." file is returned if focused
*
*   Params:
*
*   Returns:
*
*   Statuses:
*
******************************************************************************/

GnomeCmdFile*
gnome_cmd_file_list_get_focused_file (GnomeCmdFileList *fl)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), NULL);

    return fl->priv->cur_file < 0 ? NULL : get_file_at_row (fl, fl->priv->cur_file);
}


void
gnome_cmd_file_list_select_all (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    gnome_cmd_file_list_free (fl->priv->selected_files);
    fl->priv->selected_files = NULL;

    for (GList *tmp = gnome_cmd_file_list_get_all_files (fl); tmp; tmp = tmp->next)
        select_file (fl, (GnomeCmdFile *) tmp->data);
}


void
gnome_cmd_file_list_unselect_all (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *tmp;

    for (GList *tmp = g_list_copy (fl->priv->selected_files); tmp; tmp = tmp->next)
        unselect_file (fl, (GnomeCmdFile *) tmp->data);

    gnome_cmd_file_list_free (fl->priv->selected_files);
    fl->priv->selected_files = NULL;
    g_list_free (tmp);
}


void
gnome_cmd_file_list_toggle (GnomeCmdFileList *fl)
{
    GnomeCmdFile *finfo;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    finfo = get_file_at_row (fl, fl->priv->cur_file);
    if (finfo)
        toggle_file (fl, finfo);
}


void
gnome_cmd_file_list_toggle_and_step (GnomeCmdFileList *fl)
{
    GnomeCmdFile *finfo;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    finfo = get_file_at_row (fl, fl->priv->cur_file);
    if (finfo)
        toggle_file (fl, finfo);
    if (fl->priv->cur_file < get_num_files (fl)-1)
        focus_file_at_row (fl, fl->priv->cur_file+1);
}


void
gnome_cmd_file_list_focus_file (GnomeCmdFileList *fl,
                                const gchar *focus_file,
                                gboolean scroll_to_file)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    for (GList *tmp = gnome_cmd_file_list_get_all_files (fl); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) tmp->data;

        g_return_if_fail (finfo != NULL);
        g_return_if_fail (finfo->info != NULL);

        gint row = get_row_from_file (fl, finfo);
        if (row == -1)
            return;

        if (strcmp (finfo->info->name, focus_file) == 0)
        {
            fl->priv->cur_file = row;
            focus_file_at_row (fl, row);
            if (scroll_to_file)
                gtk_clist_moveto (GTK_CLIST (fl), row, 0, 0, 0);
            return;
        }
    }

    /* The file was not found, remember the filename in case the file gets
       added to the list in the future (after a FAM event etc). */
    if (fl->priv->focus_later)
        g_free (fl->priv->focus_later);
    fl->priv->focus_later = g_strdup (focus_file);
}


void
gnome_cmd_file_list_select_row (GnomeCmdFileList *fl, gint row)
{
    focus_file_at_row (fl, row == -1 ? fl->priv->cur_file : row);
}


void
gnome_cmd_file_list_select_pattern (GnomeCmdFileList *fl, const gchar *pattern, gboolean case_sens)
{
    toggle_with_pattern (fl, pattern, case_sens, TRUE);
}


void
gnome_cmd_file_list_unselect_pattern (GnomeCmdFileList *fl, const gchar *pattern, gboolean case_sens)
{
    toggle_with_pattern (fl, pattern, case_sens, FALSE);
}


void
gnome_cmd_file_list_invert_selection (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *sel = g_list_copy (fl->priv->selected_files);

    for (GList *tmp=gnome_cmd_file_list_get_all_files (fl); tmp; tmp = tmp->next)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) tmp->data;

        if (finfo && finfo->info)
        {
            if (g_list_index (sel, finfo) == -1)
                select_file (fl, finfo);
            else
                unselect_file (fl, finfo);
        }
    }

    g_list_free (sel);
}


void
gnome_cmd_file_list_select_all_with_same_extension (GnomeCmdFileList *fl)
{
    toggle_files_with_same_extension (fl, TRUE);
}


void
gnome_cmd_file_list_unselect_all_with_same_extension (GnomeCmdFileList *fl)
{
    toggle_files_with_same_extension (fl, FALSE);
}


void
gnome_cmd_file_list_restore_selection (GnomeCmdFileList *fl)
{
}


static gint
compare_filename(GnomeCmdFile *f1, GnomeCmdFile *f2)
{
    return strcmp(f1->info->name, f2->info->name);
}


void
gnome_cmd_file_list_compare_directories (void)
{
    GnomeCmdFileSelector *fs1 = gnome_cmd_main_win_get_active_fs (main_win);
    GnomeCmdFileList     *fl1 = fs1 ? fs1->list : NULL;
    GnomeCmdFileSelector *fs2 = gnome_cmd_main_win_get_inactive_fs (main_win);
    GnomeCmdFileList     *fl2 = fs2 ? fs2->list : NULL;

    GList *i;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl1) || GNOME_CMD_IS_FILE_LIST (fl2));

    gnome_cmd_file_list_unselect_all (fl1);
    gnome_cmd_file_list_select_all (fl2);

    for (i=gnome_cmd_file_list_get_all_files(fl1); i; i=i->next)
    {
        GnomeCmdFile *f1 = (GnomeCmdFile *) i->data;
        GnomeCmdFile *f2;
        GList *gl2 = g_list_find_custom (fl2->priv->selected_files, f1, (GCompareFunc)compare_filename);

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


void
gnome_cmd_file_list_sort (GnomeCmdFileList *fl)
{
    GList *list;
    GnomeCmdFile *selfile;
    gint selrow;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    selfile = gnome_cmd_file_list_get_selected_file (fl);

    gtk_clist_freeze (GTK_CLIST (fl));
    gtk_clist_clear (GTK_CLIST (fl));

    // Resort the files and readd them to the list
    list = gnome_cmd_file_list_get_all_files (fl);
    list = gnome_vfs_list_sort (list, fl->priv->sort_func, fl);
    while (list != NULL)
    {
        GnomeCmdFile *finfo;

        finfo = GNOME_CMD_FILE (list->data);
        add_file_to_clist (fl, finfo, -1);
        list = list->next;
    }
    g_list_free (list);

    /* refocus the previously selected file if this
       file-list has the focus */
    if (selfile && GTK_WIDGET_HAS_FOCUS (fl))
    {
        selrow = get_row_from_file (fl, selfile);
        gnome_cmd_file_list_select_row (fl, selrow);
        gtk_clist_moveto (GTK_CLIST (fl), selrow, -1, 1, 0);
    }

    // reselect the previously selected files
    for (list = fl->priv->selected_files; list != NULL; list = list->next)
        select_file (fl, GNOME_CMD_FILE (list->data));

    gtk_clist_thaw (GTK_CLIST (fl));
}


GnomeCmdFile *
gnome_cmd_file_list_get_file_at_row (GnomeCmdFileList *fl, gint row)
{
    return get_file_at_row (fl, row);
}


gint
gnome_cmd_file_list_get_row_from_file (GnomeCmdFileList *fl, GnomeCmdFile *finfo)
{
    return get_row_from_file (fl, finfo);
}


void
gnome_cmd_file_list_show_rename_dialog (GnomeCmdFileList *fl)
{

    GnomeCmdFile *finfo;
    gint x, y, w, h;

    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    finfo = gnome_cmd_file_list_get_selected_file (fl);

    if (GNOME_CMD_IS_FILE (finfo))
    {
        get_focus_row_coordinates(fl, &x, &y, &w, &h);
        GtkWidget *dialog = gnome_cmd_rename_dialog_new (finfo, x, y, w, h);

        gtk_widget_ref (dialog);
        gtk_widget_show (dialog);
    }
}


void
gnome_cmd_file_list_show_delete_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = gnome_cmd_file_list_get_selected_files (fl);

    if (files)
    {
        gnome_cmd_delete_dialog_show (files);
        g_list_free (files);
    }
}


void
gnome_cmd_file_list_show_chown_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = gnome_cmd_file_list_get_selected_files (fl);

    if (files)
    {
        GtkWidget *dialog = gnome_cmd_chown_dialog_new (files);

        gtk_widget_ref (dialog);
        gtk_widget_show (dialog);
        g_list_free (files);
    }
}


void
gnome_cmd_file_list_show_chmod_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = gnome_cmd_file_list_get_selected_files (fl);

    if (files)
    {
        GtkWidget *dialog = gnome_cmd_chmod_dialog_new (files);

        gtk_widget_ref (dialog);
        gtk_widget_show (dialog);
        g_list_free (files);
    }
}


void
gnome_cmd_file_list_show_advrename_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = gnome_cmd_file_list_get_selected_files (fl);

    if (files)
    {
        GtkWidget *dialog = gnome_cmd_advrename_dialog_new (files);

        gtk_widget_ref (dialog);
        gtk_widget_show (dialog);
    }
}


void
gnome_cmd_file_list_show_properties_dialog (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *finfo = gnome_cmd_file_list_get_selected_file (fl);

    if (finfo)
        gnome_cmd_file_show_properties (finfo);
}


void
gnome_cmd_file_list_show_selpat_dialog (GnomeCmdFileList *fl, gboolean mode)
{
    show_selpat_dialog (fl, mode);
}


void
gnome_cmd_file_list_cap_cut (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = gnome_cmd_file_list_get_selected_files (fl);

    if (files)
    {
        cap_cut_files (fl, files);
        g_list_free (files);
    }
}


void
gnome_cmd_file_list_cap_copy (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GList *files = gnome_cmd_file_list_get_selected_files (fl);

    if (files)
    {
        cap_copy_files (fl, files);
        g_list_free (files);
    }
}


void
gnome_cmd_file_list_view (GnomeCmdFileList *fl, gint internal_viewer)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *finfo = gnome_cmd_file_list_get_selected_file (fl);

    if (!finfo)  return;

    if (finfo->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        create_error_dialog(_("Not an ordinary file: %s"), finfo->info->name);
    else
        gnome_cmd_file_view (finfo, internal_viewer!=-1 ? internal_viewer :
                                                          gnome_cmd_data_get_use_internal_viewer ());
}


void
gnome_cmd_file_list_edit (GnomeCmdFileList *fl)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_LIST (fl));

    GnomeCmdFile *finfo = gnome_cmd_file_list_get_selected_file (fl);

    if (!finfo)  return;

    if (finfo->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        create_error_dialog(_("Not an ordinary file: %s"), finfo->info->name);
    else
        gnome_cmd_file_edit (finfo);
}

gboolean
gnome_cmd_file_list_quicksearch_shown (GnomeCmdFileList *fl)
{
    g_return_val_if_fail(fl!=NULL,FALSE);
    g_return_val_if_fail(GNOME_CMD_IS_FILE_LIST(fl),FALSE);
    g_return_val_if_fail(fl->priv!=NULL,FALSE);

    return fl->priv->quicksearch_popup!=NULL;
}

void
gnome_cmd_file_list_show_quicksearch (GnomeCmdFileList *fl, gchar c)
{
    gchar text[2];
    if (fl->priv->quicksearch_popup)
        return;

    gtk_clist_unselect_all (GTK_CLIST (fl));

    fl->priv->quicksearch_popup = gnome_cmd_quicksearch_popup_new (fl);
    text[0] = c;
    text[1] = '\0';
    gtk_widget_ref (fl->priv->quicksearch_popup);
    gtk_widget_show (fl->priv->quicksearch_popup);
    if (c != 0)
    {
        GnomeCmdQuicksearchPopup *popup = GNOME_CMD_QUICKSEARCH_POPUP (fl->priv->quicksearch_popup);
        gtk_entry_set_text (GTK_ENTRY (popup->entry), text);
        gtk_entry_set_position (GTK_ENTRY (popup->entry), 1);
    }

    gtk_signal_connect (GTK_OBJECT (fl->priv->quicksearch_popup), "hide",
                        GTK_SIGNAL_FUNC (on_quicksearch_popup_hide), fl);
}


gboolean
gnome_cmd_file_list_keypressed (GnomeCmdFileList *fl,
                                GdkEventKey *event)
{
    gint num_files;

    g_return_val_if_fail (event != NULL, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_FILE_LIST (fl), FALSE);

    num_files = get_num_files (fl);

    if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
                gnome_cmd_file_list_show_properties_dialog (fl);
                return TRUE;

            case GDK_KP_Add:
                toggle_files_with_same_extension (fl, TRUE);
                break;

            case GDK_KP_Subtract:
                toggle_files_with_same_extension (fl, FALSE);
                break;
        }
    }
    else if (state_is_ctrl_alt (event->state) || state_is_ctrl_alt_shift (event->state))
    {
        if ((event->keyval >= GDK_a && event->keyval <= GDK_z)
            || (event->keyval >= GDK_A && event->keyval <= GDK_Z)
            || event->keyval == GDK_period)
            gnome_cmd_file_list_show_quicksearch (fl, (gchar)event->keyval);
    }
    else if (state_is_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_F2:
                gnome_cmd_file_list_compare_directories ();
                return TRUE;

            case GDK_F6:
                gnome_cmd_file_list_show_rename_dialog (fl);
                return TRUE;

            case GDK_F10:
                show_file_popup_with_warp (fl);
                return TRUE;

            case GDK_Left:
            case GDK_Right:
                event->state -= GDK_SHIFT_MASK;
                return FALSE;

            case GDK_Page_Up:
            case GDK_KP_Page_Up:
                fl->priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_PAGE_BACKWARD, 0.0, NULL);
                return FALSE;

            case GDK_Page_Down:
            case GDK_KP_Page_Down:
                fl->priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_PAGE_FORWARD, 0.0, NULL);
                return FALSE;

            case GDK_Up:
            case GDK_KP_Up:
                fl->priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_STEP_BACKWARD, 0.0, NULL);
                return FALSE;

            case GDK_Down:
            case GDK_KP_Down:
                fl->priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_STEP_FORWARD, 0.0, NULL);
                return FALSE;

            case GDK_Home:
            case GDK_KP_Home:
                fl->priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_JUMP, 0.0);
                return TRUE;

            case GDK_End:
            case GDK_KP_End:
                fl->priv->shift_down = TRUE;
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_JUMP, 1.0);
                return TRUE;
        }
    }
    else if (state_is_ctrl_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_a:
            case GDK_A:
                gnome_cmd_file_list_unselect_all (fl);
                return TRUE;

        }
    }
    else if (state_is_ctrl (event->state))
    {
        switch (event->keyval)
        {
            case GDK_X:
            case GDK_x:
                gnome_cmd_file_list_cap_cut (fl);
                return TRUE;

            case GDK_C:
            case GDK_c:
                gnome_cmd_file_list_cap_copy (fl);
                return TRUE;

            case GDK_m:
            case GDK_M:
            case GDK_t:
            case GDK_T:
                gnome_cmd_file_list_show_advrename_dialog (fl);
                return TRUE;

            case GDK_a:
            case GDK_A:
            case GDK_KP_Add:
            case GDK_equal:
                gnome_cmd_file_list_select_all (fl);
                return TRUE;

            case GDK_minus:
            case GDK_KP_Subtract:
                gnome_cmd_file_list_unselect_all (fl);
                return TRUE;

            case GDK_F3:
                on_column_clicked (GTK_CLIST (fl), FILE_LIST_COLUMN_NAME, fl);
                return TRUE;
            case GDK_F4:
                on_column_clicked (GTK_CLIST (fl), FILE_LIST_COLUMN_EXT, fl);
                return TRUE;
            case GDK_F5:
                on_column_clicked (GTK_CLIST (fl), FILE_LIST_COLUMN_DATE, fl);
                return TRUE;
            case GDK_F6:
                on_column_clicked (GTK_CLIST (fl), FILE_LIST_COLUMN_SIZE, fl);
                return TRUE;
        }
    }
    else if (state_is_blank (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
            case GDK_KP_Enter:
                return mime_exec_file (gnome_cmd_file_list_get_focused_file (fl));

            case GDK_KP_Add:
            case GDK_plus:
            case GDK_equal:
                show_selpat_dialog (fl, TRUE);
                return TRUE;

            case GDK_KP_Subtract:
            case GDK_minus:
                show_selpat_dialog (fl, FALSE);
                return TRUE;

            case GDK_KP_Multiply:
                gnome_cmd_file_list_invert_selection (fl);
                return TRUE;

            case GDK_KP_Divide:
                gnome_cmd_file_list_restore_selection (fl);
                return TRUE;

            case GDK_Insert:
            case GDK_KP_Insert:
                gnome_cmd_file_list_toggle (fl);
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_STEP_FORWARD, 0.0, NULL);
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
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_JUMP, 0.0);
                return TRUE;

            case GDK_End:
            case GDK_KP_End:
                gtk_signal_emit_by_name (GTK_OBJECT (fl), "scroll_vertical", GTK_SCROLL_JUMP, 1.0);
                return TRUE;

            case GDK_Delete:
            case GDK_KP_Delete:
                gnome_cmd_file_list_show_delete_dialog (fl);
                return TRUE;

            case GDK_Shift_L:
            case GDK_Shift_R:
                if (!fl->priv->shift_down)
                    fl->priv->shift_down_row = fl->priv->cur_file;
                return TRUE;

            case GDK_Menu:
                show_file_popup_with_warp (fl);
                return TRUE;
        }
    }

    return FALSE;
}


static GList *
gnome_vfs_list_sort_merge (GList *l1,
                          GList *l2,
                          GnomeVFSListCompareFunc compare_func,
                          gpointer data)
{
    GList list;
    GList *l = &list;
    GList  *lprev = NULL;

    while (l1 && l2)
    {
        if (compare_func (l1->data, l2->data, data) < 0)
        {
            l->next = l1;
            l = l->next;
            l->prev = lprev;
            lprev = l;
            l1 = l1->next;
        }
        else
        {
            l->next = l2;
            l = l->next;
            l->prev = lprev;
            lprev = l;
            l2 = l2->next;
        }
    }

    l->next = l1 ? l1 : l2;
    l->next->prev = l;

    return list.next;
}


GList *
gnome_vfs_list_sort (GList *list,
                     GnomeVFSListCompareFunc compare_func,
                     gpointer data)
{
    if (!list || !list->next)
        return list;

    GList *l1 = list;
    GList *l2 = list->next;

    while ((l2 = l2->next) != NULL)
    {
        if ((l2 = l2->next) == NULL)
            break;
        l1 = l1->next;
    }

    l2 = l1->next;
    l1->next = NULL;

    return gnome_vfs_list_sort_merge(gnome_vfs_list_sort (list, compare_func, data),
                                     gnome_vfs_list_sort (l2, compare_func, data),
                                     compare_func,
                                     data);
}


GList *
gnome_cmd_file_list_sort_selection (GList *list, GnomeCmdFileList *fl)
{
    return gnome_vfs_list_sort(list, (GnomeVFSListCompareFunc)fl->priv->sort_func, fl);
}
