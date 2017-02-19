/**
 * @file iv_viewerwidget_test.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
 *
 * @details This module tests gviewer widget setter functions in libgviewer.
 * @li Enconding: ASCII, UTF8, CP437, CP1251
 * @li Display Mode: Text, Binary, Hex, Image
 * @li Wrapping: TRUE, FALSE
 * @li Fixed_limit: Sets number of Bytes per line
 * @li Tab size: Set number of space per TAB character
 * @li Scale: Sets scaling factor (0.1 to 3.0) in image mode
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
#include <libgviewer.h>

const gchar *filename = "../README";

////////////////////////////////////////////////////////////////////////

class ViewerWidgetDisplayModeTest : public ::testing::TestWithParam<VIEWERDISPLAYMODE> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        ViewerWidgetDisplayModeTest,
                        ::testing::Values(DISP_MODE_TEXT_FIXED,
                                          DISP_MODE_BINARY,
                                          DISP_MODE_HEXDUMP,
                                          DISP_MODE_IMAGE));

TEST_P(ViewerWidgetDisplayModeTest, gviewer_set_display_mode_test)
{
    GtkWidget *viewer;
    viewer = gviewer_new();
    gviewer_load_file(GVIEWER(viewer), filename);
    gviewer_set_display_mode(GVIEWER(viewer),GetParam());
    ASSERT_EQ(GetParam(), gviewer_get_display_mode(GVIEWER(viewer)));
}

////////////////////////////////////////////////////////////////////////

class ViewerWidgetTabSizeTest : public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        ViewerWidgetTabSizeTest,
                        ::testing::Range(1,10)); //TODO: Add test for x <= 0

TEST_P(ViewerWidgetTabSizeTest, gviewer_set_tab_size_test)
{
    GtkWidget *viewer;
    viewer = gviewer_new();
    gviewer_load_file(GVIEWER(viewer), filename);
    gviewer_set_tab_size(GVIEWER(viewer),GetParam());
    ASSERT_EQ(GetParam(), gviewer_get_tab_size(GVIEWER(viewer)));
}

//////////////////////////////////////////////////////////////////////////

class ViewerWidgetFixedLimitTest : public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        ViewerWidgetFixedLimitTest,
                        ::testing::Range(1,10)); //TODO: Add test for x <= 0

TEST_P(ViewerWidgetFixedLimitTest, gviewer_set_fixed_limit_test)
{
    GtkWidget *viewer;
    viewer = gviewer_new();
    gviewer_load_file(GVIEWER(viewer), filename);
    gviewer_set_fixed_limit(GVIEWER(viewer),GetParam());
    ASSERT_EQ(GetParam(), gviewer_get_fixed_limit(GVIEWER(viewer)));
}

//////////////////////////////////////////////////////////////////////////

class ViewerWidgetEncodingTest : public ::testing::TestWithParam<const char *> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        ViewerWidgetEncodingTest,
                        ::testing::Values("ASCII", "UTF8", "CP437", "CP1251"));

TEST_P(ViewerWidgetEncodingTest, gviewer_set_encoding_test)
{
    GtkWidget *viewer;
    viewer = gviewer_new();
    gviewer_load_file(GVIEWER(viewer), filename);
    gviewer_set_encoding(GVIEWER(viewer),GetParam());
    ASSERT_STREQ(GetParam(), gviewer_get_encoding(GVIEWER(viewer)));
}

//////////////////////////////////////////////////////////////////////////

class ViewerWidgetWrapModeTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        ViewerWidgetWrapModeTest,
                        ::testing::Bool());

TEST_P(ViewerWidgetWrapModeTest, gviewer_set_wrap_mode_test)
{
    GtkWidget *viewer;
    viewer = gviewer_new();
    gviewer_load_file(GVIEWER(viewer), filename);
    gviewer_set_wrap_mode(GVIEWER(viewer),GetParam());
    ASSERT_EQ(GetParam(), gviewer_get_wrap_mode(GVIEWER(viewer)));
}

//////////////////////////////////////////////////////////////////////////

class ViewerWidgetBestFitTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        ViewerWidgetBestFitTest,
                        ::testing::Bool());

TEST_P(ViewerWidgetBestFitTest, gviewer_set_best_fit_test)
{
    GtkWidget *viewer;
    viewer = gviewer_new();
    gviewer_load_file(GVIEWER(viewer), filename);
    gviewer_set_best_fit(GVIEWER(viewer),GetParam());
    ASSERT_EQ(GetParam(), gviewer_get_best_fit(GVIEWER(viewer)));
}

//////////////////////////////////////////////////////////////////////////

class ViewerWidgetScaleFactorTest : public ::testing::TestWithParam<double> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        ViewerWidgetScaleFactorTest,
                        ::testing::Values(0.1, 0.5, 1, 2, 3));

TEST_P(ViewerWidgetScaleFactorTest, gviewer_set_scale_factor_test)
{
    GtkWidget *viewer;
    viewer = gviewer_new();
    gviewer_load_file(GVIEWER(viewer), filename);
    gviewer_set_scale_factor(GVIEWER(viewer),GetParam());
    ASSERT_EQ(GetParam(), gviewer_get_scale_factor(GVIEWER(viewer)));
}

//////////////////////////////////////////////////////////////////////////
