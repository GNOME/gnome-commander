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
 * @copyright (C) 2013-2016 Uwe Scholz\n
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

#include <libgviewer.h>
#include "gtest/gtest.h"

class ImageRendererBestFitTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        ImageRendererBestFitTest,
                        ::testing::Bool());

TEST_P(ImageRendererBestFitTest, image_renderer_set_best_fit_test) {
    GtkWidget *imgr;
    imgr = image_render_new();
    image_render_set_best_fit(IMAGE_RENDER(imgr),GetParam());
    ASSERT_EQ(GetParam(), image_render_get_best_fit(IMAGE_RENDER(imgr)));
}

////////////////////////////////////////////////////////////////////////

class ImageRendererScaleFactorTest : public ::testing::Test {};

TEST_F(ImageRendererScaleFactorTest, image_renderer_set_scale_factor_test) {
    GtkWidget *imgr;
    imgr = image_render_new();
    image_render_set_scale_factor(IMAGE_RENDER(imgr), 1);
    ASSERT_EQ(1, image_render_get_scale_factor(IMAGE_RENDER(imgr)));
}
