/**
 * @file iv_imagerenderer_test.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 * 
 * @details In this single test the creation of a gtk widget is tested
 * in which two elements are visualized. It is not to be considered to
 * be a unit test, as individual functions are not fully tested!
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2015 Uwe Scholz\n
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

#include "gtest/gtest.h"
#include <iv_imagerenderer_test.h>

const gchar *filename = "../pixmaps/gnome-commander.png";
static gboolean best_fit = TRUE;
static double scale_factor = 1.0; //0.1 < scale_factor < 3.0

TEST_F(ImageRendererTest, image_render_test) {
    GtkWidget *window;
    GtkWidget *scrollbox;
    GtkWidget *imgr;

    gtk_init(NULL, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(window,600,400);

    scrollbox = scroll_box_new();

    imgr = image_render_new();

    image_render_set_v_adjustment(IMAGE_RENDER(imgr),
        scroll_box_get_v_adjustment(SCROLL_BOX(scrollbox)));

    image_render_set_h_adjustment(IMAGE_RENDER(imgr),
        scroll_box_get_h_adjustment(SCROLL_BOX(scrollbox)));

    image_render_load_file(IMAGE_RENDER(imgr), filename);

    image_render_set_best_fit(IMAGE_RENDER(imgr),best_fit);

    if (!best_fit)
        image_render_set_scale_factor(IMAGE_RENDER(imgr), scale_factor);

    scroll_box_set_client(SCROLL_BOX(scrollbox),imgr);

    gtk_container_add(GTK_CONTAINER(window), scrollbox);

    gtk_widget_show(imgr);
    gtk_widget_show(scrollbox);
    gtk_widget_show(window);

    while (g_main_context_pending(NULL))
    {
        g_main_context_iteration(NULL, FALSE);
    }
    gtk_widget_destroy (window);
}
