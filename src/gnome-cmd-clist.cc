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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-clist.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-style.h"

using namespace std;


static GtkCListClass *parent_class = NULL;


struct GnomeCmdCListPrivate
{
};


/*******************************************
 * TEST TEST TEST
 ****/

// this defines the base grid spacing
#define CELL_SPACING 1

// added the horizontal space at the beginning and end of a row
#define COLUMN_INSET 3


/* gives the top pixel of the given row in context of
 * the clist's voffset */
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
                    (((row) + 1) * CELL_SPACING) + \
                    (clist)->voffset)

/* returns the row index from a y pixel location in the
 * context of the clist's voffset */
#define ROW_FROM_YPIXEL(clist, y)  (((y) - (clist)->voffset) / \
                    ((clist)->row_height + CELL_SPACING))

/* gives the left pixel of the given column in context of
 * the clist's hoffset */
#define COLUMN_LEFT_XPIXEL(clist, colnum)  ((clist)->column[(colnum)].area.x + \
                        (clist)->hoffset)

/* returns the column index from a x pixel location in the
 * context of the clist's hoffset */
inline gint COLUMN_FROM_XPIXEL (GtkCList * clist, gint x)
{
    for (gint i = 0; i < clist->columns; i++)
        if (clist->column[i].visible)
        {
            gint cx = clist->column[i].area.x + clist->hoffset;

            if (x >= (cx - (COLUMN_INSET + CELL_SPACING)) &&  x <= (cx + clist->column[i].area.width + COLUMN_INSET))
                return i;
        }

    // no match
    return -1;
}


/* returns the top pixel of the given row in the context of
 * the list height */
#define ROW_TOP(clist, row)        (((clist)->row_height + CELL_SPACING) * (row))

/* returns the left pixel of the given column in the context of
 * the list width */
#define COLUMN_LEFT(clist, colnum) ((clist)->column[(colnum)].area.x)

// returns the total height of the list
#define LIST_HEIGHT(clist)         (((clist)->row_height * ((clist)->rows)) + \
                                    (CELL_SPACING * ((clist)->rows + 1)))


// returns the total width of the list
inline gint LIST_WIDTH (GtkCList * clist)
{
  gint last_column;

  for (last_column = clist->columns - 1;
       last_column >= 0 && !clist->column[last_column].visible; last_column--);

  if (last_column >= 0)
    return clist->column[last_column].area.x + clist->column[last_column].area.width + COLUMN_INSET + CELL_SPACING;

  return 0;
}

// returns the GList item for the nth row
#define    ROW_ELEMENT(clist, row)    (((row) == (clist)->rows - 1) ? (clist)->row_list_end : \
                                                                      g_list_nth ((clist)->row_list, (row)))

static void
get_cell_style (GtkCList     *clist,
                GtkCListRow  *clist_row,
                gint          state,
                gint          column,
                GtkStyle    **style,
                GdkGC       **fg_gc,
                GdkGC       **bg_gc)
{
    gint fg_state;

    if ((state == GTK_STATE_NORMAL) && (GTK_WIDGET (clist)->state == GTK_STATE_INSENSITIVE))
        fg_state = GTK_STATE_INSENSITIVE;
    else
        fg_state = state;

    if (clist_row->cell[column].style)
    {
        if (style)
            *style = clist_row->cell[column].style;
        if (fg_gc)
            *fg_gc = clist_row->cell[column].style->fg_gc[fg_state];
        if (bg_gc) {
            if (state == GTK_STATE_SELECTED)
                *bg_gc = clist_row->cell[column].style->bg_gc[state];
            else
                *bg_gc = clist_row->cell[column].style->base_gc[state];
        }
    }
    else
        if (clist_row->style)
        {
            if (style)
                *style = clist_row->style;
            if (fg_gc)
                *fg_gc = clist_row->style->fg_gc[fg_state];
            if (bg_gc)
            {
                if (state == GTK_STATE_SELECTED)
                    *bg_gc = clist_row->style->bg_gc[state];
                else
                    *bg_gc = clist_row->style->base_gc[state];
            }
        }
        else
        {
            if (style)
                *style = GTK_WIDGET (clist)->style;
            if (fg_gc)
                *fg_gc = GTK_WIDGET (clist)->style->fg_gc[fg_state];
            if (bg_gc)
            {
                if (state == GTK_STATE_SELECTED)
                    *bg_gc = GTK_WIDGET (clist)->style->bg_gc[state];
                else
                    *bg_gc = GTK_WIDGET (clist)->style->base_gc[state];
            }

            if (state != GTK_STATE_SELECTED)
            {
                if (fg_gc && clist_row->fg_set)
                    *fg_gc = clist->fg_gc;
                if (bg_gc && clist_row->bg_set)
                    *bg_gc = clist->bg_gc;
            }
        }
}


static gint
draw_cell_pixmap (GdkWindow    *window,
                  GdkRectangle *clip_rectangle,
                  GdkGC        *fg_gc,
                  GdkPixmap    *pixmap,
                  GdkBitmap    *mask,
                  gint          x,
                  gint          y,
                  gint          width,
                  gint          height)
{
    gint xsrc = 0;
    gint ysrc = 0;

    if (mask)
    {
        gdk_gc_set_clip_mask (fg_gc, mask);
        gdk_gc_set_clip_origin (fg_gc, x, y);
    }

    if (x < clip_rectangle->x)
    {
        xsrc = clip_rectangle->x - x;
        width -= xsrc;
        x = clip_rectangle->x;
    }
    if (x + width > clip_rectangle->x + clip_rectangle->width)
        width = clip_rectangle->x + clip_rectangle->width - x;

    if (y < clip_rectangle->y)
    {
        ysrc = clip_rectangle->y - y;
        height -= ysrc;
        y = clip_rectangle->y;
    }
    if (y + height > clip_rectangle->y + clip_rectangle->height)
        height = clip_rectangle->y + clip_rectangle->height - y;

    gdk_draw_pixmap (window, fg_gc, pixmap, xsrc, ysrc, x, y, width, height);
    gdk_gc_set_clip_origin (fg_gc, 0, 0);
    if (mask)
        gdk_gc_set_clip_mask (fg_gc, NULL);

    return x + MAX (width, 0);
}


inline PangoLayout *my_gtk_clist_create_cell_layout (GtkCList *clist, GtkCListRow *clist_row, gint column)
{
    PangoLayout *layout;
    GtkStyle *style;
    gchar *text;

    get_cell_style (clist, clist_row, GTK_STATE_NORMAL, column, &style, NULL, NULL);

    GtkCell *cell = &clist_row->cell[column];

    switch (cell->type)
    {
        case GTK_CELL_TEXT:
        case GTK_CELL_PIXTEXT:
            text = ((cell->type == GTK_CELL_PIXTEXT) ?
                    GTK_CELL_PIXTEXT (*cell)->text :
                    GTK_CELL_TEXT (*cell)->text);

            if (!text)
                return NULL;

            layout = gtk_widget_create_pango_layout (GTK_WIDGET (clist),
                                                     ((cell->type == GTK_CELL_PIXTEXT) ?
                                                      GTK_CELL_PIXTEXT (*cell)->text :
                                                      GTK_CELL_TEXT (*cell)->text));
            pango_layout_set_font_description (layout, style->font_desc);

            return layout;

        default:
            return NULL;
    }
}


static void draw_row (GtkCList *clist, GdkRectangle *area, gint row, GtkCListRow *clist_row)
{
    g_return_if_fail (clist != NULL);

    GdkRectangle *rect;
    GdkRectangle row_rectangle;
    GdkRectangle cell_rectangle;
    GdkRectangle clip_rectangle;
    GdkRectangle intersect_rectangle;
    gint last_column;
    gint state;

    // bail out now if we aren't drawable yet
    if (!GTK_WIDGET_DRAWABLE (clist) || row < 0 || row >= clist->rows)
        return;

    GtkWidget *widget = GTK_WIDGET (clist);

    // if the function is passed the pointer to the row instead of null, it avoids this expensive lookup
    if (!clist_row)
        clist_row = (GtkCListRow *) ROW_ELEMENT (clist, row)->data;

    // rectangle of the entire row
    row_rectangle.x = 0;
    row_rectangle.y = ROW_TOP_YPIXEL (clist, row);
    row_rectangle.width = clist->clist_window_width;
    row_rectangle.height = clist->row_height;

    // rectangle of the cell spacing above the row
    cell_rectangle.x = 0;
    cell_rectangle.y = row_rectangle.y - CELL_SPACING;
    cell_rectangle.width = row_rectangle.width;
    cell_rectangle.height = CELL_SPACING;

    /* rectangle used to clip drawing operations, its y and height
     * positions only need to be set once, so we set them once here.
     * the x and width are set withing the drawing loop below once per
     * column */
    clip_rectangle.y = row_rectangle.y;
    clip_rectangle.height = row_rectangle.height;

    if (clist_row->state == GTK_STATE_NORMAL)
    {
        if (clist_row->fg_set)
            gdk_gc_set_foreground (clist->fg_gc, &clist_row->foreground);
        if (clist_row->bg_set)
            gdk_gc_set_foreground (clist->bg_gc, &clist_row->background);
    }

    state = clist_row->state;

    // draw the cell borders and background
    if (area)
    {
        rect = &intersect_rectangle;
        if (gdk_rectangle_intersect (area, &cell_rectangle,
                                     &intersect_rectangle))
            gdk_draw_rectangle (clist->clist_window,
                                widget->style->base_gc[GTK_STATE_NORMAL],
                                TRUE,
                                intersect_rectangle.x,
                                intersect_rectangle.y,
                                intersect_rectangle.width,
                                intersect_rectangle.height);

        // the last row has to clear its bottom cell spacing too
        if (clist_row == clist->row_list_end->data)
        {
            cell_rectangle.y += clist->row_height + CELL_SPACING;

            if (gdk_rectangle_intersect (area, &cell_rectangle, &intersect_rectangle))
                gdk_draw_rectangle (clist->clist_window,
                                    widget->style->base_gc[GTK_STATE_NORMAL],
                                    TRUE,
                                    intersect_rectangle.x,
                                    intersect_rectangle.y,
                                    intersect_rectangle.width,
                                    intersect_rectangle.height);
        }

        if (!gdk_rectangle_intersect (area, &row_rectangle, &intersect_rectangle))
            return;

    }
    else
    {
        rect = &clip_rectangle;
        gdk_draw_rectangle (clist->clist_window,
                            widget->style->base_gc[GTK_STATE_NORMAL],
                            TRUE,
                            cell_rectangle.x,
                            cell_rectangle.y,
                            cell_rectangle.width,
                            cell_rectangle.height);

        // the last row has to clear its bottom cell spacing too
        if (clist_row == clist->row_list_end->data)
        {
            cell_rectangle.y += clist->row_height + CELL_SPACING;

            gdk_draw_rectangle (clist->clist_window,
                                widget->style->base_gc[GTK_STATE_NORMAL],
                                TRUE,
                                cell_rectangle.x,
                                cell_rectangle.y,
                                cell_rectangle.width,
                                cell_rectangle.height);
        }
    }

    for (last_column = clist->columns - 1; last_column >= 0 && !clist->column[last_column].visible; last_column--);

    // iterate and draw all the columns (row cells) and draw their contents
    for (gint i = 0; i < clist->columns; i++)
    {
        GtkStyle *style;
        GdkGC *fg_gc;
        GdkGC *bg_gc;
        PangoLayout *layout;
        PangoRectangle logical_rect;

        gint width;
        gint height;
        gint pixmap_width;
        gint offset = 0;

        if (!clist->column[i].visible)
            continue;

        get_cell_style (clist, clist_row, state, i, &style, &fg_gc, &bg_gc);

        clip_rectangle.x = clist->column[i].area.x + clist->hoffset;
        clip_rectangle.width = clist->column[i].area.width;

        // calculate clipping region clipping region
        clip_rectangle.x -= COLUMN_INSET + CELL_SPACING;
        clip_rectangle.width += (2 * COLUMN_INSET + CELL_SPACING +
                                 (i == last_column) * CELL_SPACING);

        if (area && !gdk_rectangle_intersect (area, &clip_rectangle, &intersect_rectangle))
            continue;

        gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
                            rect->x, rect->y, rect->width, rect->height);

        clip_rectangle.x += COLUMN_INSET + CELL_SPACING;
        clip_rectangle.width -= (2 * COLUMN_INSET + CELL_SPACING + (i == last_column) * CELL_SPACING);


        // calculate real width for column justification

        layout = my_gtk_clist_create_cell_layout (clist, clist_row, i);
        if (layout)
        {
            pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
            width = logical_rect.width;
        }
        else
            width = 0;

        pixmap_width = 0;
        offset = 0;
        switch (clist_row->cell[i].type)
        {
            case GTK_CELL_PIXMAP:
                gdk_window_get_size (GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap, &pixmap_width, &height);
                width += pixmap_width;
                break;

            case GTK_CELL_PIXTEXT:
                gdk_window_get_size (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap, &pixmap_width, &height);
                width += pixmap_width + GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
                break;

            default:
                break;
        }

        switch (clist->column[i].justification)
        {
            case GTK_JUSTIFY_LEFT:
                offset = clip_rectangle.x + clist_row->cell[i].horizontal;
                break;

            case GTK_JUSTIFY_RIGHT:
                offset = (clip_rectangle.x + clist_row->cell[i].horizontal + clip_rectangle.width - width);
                break;

            case GTK_JUSTIFY_CENTER:
            case GTK_JUSTIFY_FILL:
                offset = (clip_rectangle.x + clist_row->cell[i].horizontal + (clip_rectangle.width/2) - (width/2));
                break;
        };

        // Draw Text and/or Pixmap
        switch (clist_row->cell[i].type)
        {
            case GTK_CELL_PIXMAP:
                draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
                                  GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
                                  GTK_CELL_PIXMAP (clist_row->cell[i])->mask,
                                  offset,
                                  clip_rectangle.y + clist_row->cell[i].vertical +
                                  (clip_rectangle.height - height) / 2,
                                  pixmap_width, height);
                break;
            case GTK_CELL_PIXTEXT:
                offset =
                    draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
                                      GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
                                      GTK_CELL_PIXTEXT (clist_row->cell[i])->mask,
                                      offset,
                                      clip_rectangle.y + clist_row->cell[i].vertical+
                                      (clip_rectangle.height - height) / 2,
                                      pixmap_width, height);
                offset += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;

                // Fall through
            case GTK_CELL_TEXT:
                if (layout)
                {
                    gint row_center_offset = (clist->row_height - logical_rect.height - 1) / 2;

                    gdk_gc_set_clip_rectangle (fg_gc, &clip_rectangle);
                    gdk_draw_layout (clist->clist_window, fg_gc,
                                     offset,
                                     row_rectangle.y + row_center_offset + clist_row->cell[i].vertical,
                                     layout);
                    g_object_unref (G_OBJECT (layout));
                    gdk_gc_set_clip_rectangle (fg_gc, NULL);
                }
                break;

            default:
                break;
        }
    }

    // draw focus rectangle
    if (GNOME_CMD_CLIST (clist)->drag_motion_row == row)
    {
        if (!area)
            gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
                                row_rectangle.x, row_rectangle.y,
                                row_rectangle.width - 1, row_rectangle.height - 1);
        else
            if (gdk_rectangle_intersect (area, &row_rectangle, &intersect_rectangle))
            {
                gdk_gc_set_clip_rectangle (clist->xor_gc, &intersect_rectangle);
                gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
                                    row_rectangle.x, row_rectangle.y,
                                    row_rectangle.width - 1,
                                    row_rectangle.height - 1);
                gdk_gc_set_clip_rectangle (clist->xor_gc, NULL);
            }
    }
}


/*******************************************
 * END OF TEST
 ****/
static void on_hadj_value_changed (GtkAdjustment *adjustment, GnomeCmdCList *clist)
{
    gtk_widget_draw(GTK_WIDGET(clist), NULL);
}


static void on_scroll_vertical (GtkCList *clist, GtkScrollType scroll_type, gfloat position, gpointer data)
{
    gtk_clist_select_row (clist, clist->focus_row, 0);
}


static void on_realize (GtkCList *clist, gpointer data)
{
    for (gint i=0; i<clist->columns; i++)
        if (clist->column[i].button)
            GTK_WIDGET_UNSET_FLAGS (clist->column[i].button, GTK_CAN_FOCUS);

    if (GTK_CLIST(clist)->hadjustment)
        gtk_signal_connect_after(GTK_OBJECT(GTK_CLIST(clist)->hadjustment), "value-changed",
            GTK_SIGNAL_FUNC(on_hadj_value_changed), clist);
}


/*******************************
 * Gtk class implementation
 *******************************/


static void destroy (GtkObject *object)
{
    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdCListClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkCListClass *clist_class = GTK_CLIST_CLASS (klass);

    parent_class = (GtkCListClass *) gtk_type_class (gtk_clist_get_type ());

    object_class->destroy = destroy;

    widget_class->map = ::map;

    clist_class->draw_row = draw_row;
}


static void init (GnomeCmdCList *clist)
{
    clist->drag_motion_row = -1;

    gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_SINGLE);
    
    GTK_CLIST (clist)->focus_row = 0;

    gtk_signal_connect_after (GTK_OBJECT (clist), "scroll-vertical", GTK_SIGNAL_FUNC (on_scroll_vertical), NULL);
    gtk_signal_connect (GTK_OBJECT (clist), "realize", GTK_SIGNAL_FUNC (on_realize), NULL);
}


/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_clist_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdCList",
            sizeof (GnomeCmdCList),
            sizeof (GnomeCmdCListClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_clist_get_type (), &info);
    }
    return type;
}


GtkWidget *gnome_cmd_clist_new_with_titles (gint columns, gchar **titles)
{
    GnomeCmdCList *clist = (GnomeCmdCList *) g_object_new (gnome_cmd_clist_get_type(), "n-columns", columns, NULL);

    for (gint i=0; i<columns; i++)
        gtk_clist_set_column_auto_resize (GTK_CLIST (clist), i, TRUE);

    if (titles != NULL)
        for (gint i=0; i<columns; i++)
            gtk_clist_set_column_title (GTK_CLIST (clist), i, titles[i]);

    return GTK_WIDGET (clist);
}


gint gnome_cmd_clist_get_voffset (GnomeCmdCList *clist)
{
    g_return_val_if_fail (GNOME_CMD_IS_CLIST (clist), 0);

    return GTK_ADJUSTMENT(GTK_CLIST(clist)->vadjustment)->value;
}


void gnome_cmd_clist_update_style (GnomeCmdCList *clist)
{
    gtk_widget_set_style (GTK_WIDGET (clist), list_style);
}


void gnome_cmd_clist_set_voffset (GnomeCmdCList *clist, gint voffset)
{
    g_return_if_fail (GNOME_CMD_IS_CLIST (clist));

    gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_CLIST (clist)->vadjustment), voffset);
}


gint gnome_cmd_clist_get_row (GnomeCmdCList *clist, gint x, gint y)
{
    gint row;

    g_return_val_if_fail (GNOME_CMD_IS_CLIST (clist), -1);

    if (gtk_clist_get_selection_info (GTK_CLIST (clist), x, y, &row, NULL) == 0)
        row = -1;

    return row;
}


void gnome_cmd_clist_set_drag_row (GnomeCmdCList *clist, gint row)
{
    g_return_if_fail (GNOME_CMD_IS_CLIST (clist));

    gint last_row = GNOME_CMD_CLIST (clist)->drag_motion_row;
    GNOME_CMD_CLIST (clist)->drag_motion_row = row;

    if (row == last_row) return;

    if (last_row >= 0)
        draw_row (GTK_CLIST (clist), NULL, last_row, NULL);
    if (row >= 0)
        draw_row (GTK_CLIST (clist), NULL, row, NULL);
}
