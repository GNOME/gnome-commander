/**
 * @file iv_dualrenderer_test.cc
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
#include <intviewer/libgviewer.h>
#include <X11/Xlib.h>  // to check for an available display

class DualRendererTest : public ::testing::Test {};
TEST_F(DualRendererTest, dualrenderer_test) {
    GtkWidget *window;
    GtkWidget *tscrollbox, *iscrollbox;
    GtkWidget *imgr;
    GtkWidget *textr;
    GtkWidget *box;

    gtk_init(NULL, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);

    box = gtk_hbox_new(TRUE,5);

    // image render
    iscrollbox = scroll_box_new();
    ASSERT_FALSE(iscrollbox == NULL);
    imgr = image_render_new();
    ASSERT_FALSE(imgr == NULL);

    image_render_set_v_adjustment(IMAGE_RENDER(imgr),
        scroll_box_get_v_adjustment(SCROLL_BOX(iscrollbox)));
    image_render_set_h_adjustment(IMAGE_RENDER(imgr),
        scroll_box_get_h_adjustment(SCROLL_BOX(iscrollbox)));
        
    image_render_load_file(IMAGE_RENDER(imgr), "../pixmaps/gnome-commander.png");
    image_render_set_best_fit(IMAGE_RENDER(imgr), TRUE);
    image_render_set_scale_factor(IMAGE_RENDER(imgr), 1);
    scroll_box_set_client(SCROLL_BOX(iscrollbox),imgr);
    gtk_widget_show(imgr);
    gtk_widget_show(iscrollbox);

    gtk_box_pack_start(GTK_BOX(box),iscrollbox,TRUE,TRUE,0);

    // text render
    tscrollbox = scroll_box_new();
    ASSERT_FALSE(tscrollbox == NULL);
    textr = text_render_new();
    ASSERT_FALSE(textr == NULL);

    text_render_set_v_adjustment(TEXT_RENDER(textr),
        scroll_box_get_v_adjustment(SCROLL_BOX(tscrollbox)));
    text_render_set_h_adjustment(TEXT_RENDER(textr),
        scroll_box_get_h_adjustment(SCROLL_BOX(tscrollbox)));

    text_render_load_file(TEXT_RENDER(textr), "../INSTALL");
    scroll_box_set_client(SCROLL_BOX(tscrollbox),textr);
    gtk_widget_show(textr);
    gtk_widget_show(tscrollbox);

    gtk_box_pack_start(GTK_BOX(box),tscrollbox,TRUE,TRUE,0);

    gtk_container_add(GTK_CONTAINER(window), box);

    gtk_widget_show(box);
    gtk_widget_show(window);

    while (g_main_context_pending(NULL))
    {
        g_main_context_iteration(NULL, FALSE);
    }
    gtk_widget_destroy (window);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (XOpenDisplay(NULL))
      return RUN_ALL_TESTS();
  else
      return(77);
}
