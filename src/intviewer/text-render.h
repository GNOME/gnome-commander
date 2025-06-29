/**
 * @file text-render.h
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
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
 *
 */

#pragma once

#define TYPE_TEXT_RENDER               (text_render_get_type ())
#define TEXT_RENDER(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_TEXT_RENDER, TextRender))
#define TEXT_RENDER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_TEXT_RENDER, TextRenderClass))
#define IS_TEXT_RENDER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_TEXT_RENDER))
#define IS_TEXT_RENDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_TEXT_RENDER))
#define TEXT_RENDER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_TEXT_RENDER, TextRenderClass))


extern "C" GType text_render_get_type ();


struct TextRender;


extern "C" ViewerFileOps *text_render_get_file_ops(TextRender *w);
extern "C" GVInputModesData *text_render_get_input_mode_data(TextRender *w);
extern "C" GVDataPresentation *text_render_get_data_presentation(TextRender *w);

extern "C" offset_type text_render_get_current_offset(TextRender *w);
extern "C" int text_render_get_column(TextRender *w);
