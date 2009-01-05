/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
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

#ifndef __LIBGVIEWER_DATA_PRESENTATION_H__
#define __LIBGVIEWER_DATA_PRESENTATION_H__

struct GVDataPresentation;

enum PRESENTATION
{
    PRSNT_NO_WRAP,
    PRSNT_WRAP,

    // Here, BIN_FIXED is "fixed number of binary characters per line" (e.g. CHAR=BYTE, no UTF-8 or other translations)
    PRSNT_BIN_FIXED
};

GVDataPresentation *gv_data_presentation_new();

void gv_init_data_presentation(GVDataPresentation *dp, GVInputModesData *imd, offset_type max_offset);
void gv_free_data_presentation(GVDataPresentation *dp);

void gv_set_data_presentation_mode(GVDataPresentation *dp, PRESENTATION present);
PRESENTATION gv_get_data_presentation_mode(GVDataPresentation *dp);
void gv_set_wrap_limit(GVDataPresentation *dp, guint chars_per_line);
void gv_set_fixed_count(GVDataPresentation *dp, guint chars_per_line);
void gv_set_tab_size(GVDataPresentation *dp, guint tab_size);

offset_type gv_align_offset_to_line_start(GVDataPresentation *dp, offset_type offset);
offset_type gv_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta);
offset_type gv_get_end_of_line_offset(GVDataPresentation *dp, offset_type start_of_line);

#endif
