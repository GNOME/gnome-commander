/**
 * @file iv_textrenderer_test.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
 *
 * @details This module tests gviewer widget setter functions in libgviewer
 * for text:
 * @li Enconding: ASCII, UTF8, CP437, CP1251
 * @li Display Mode: Text, Binary, Hex, Image
 * @li Wrapping: TRUE, FALSE
 * @li Fixed_limit: Sets number of Bytes per line
 * @li Tab size: Set number of space per TAB character
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

const gchar *filename = "../README";

////////////////////////////////////////////////////////////////////////

class TextRendererDisplayModeTest : public ::testing::TestWithParam<TextRender::DISPLAYMODE> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        TextRendererDisplayModeTest,
                        ::testing::Values(TextRender::DISPLAYMODE_TEXT,
                                          TextRender::DISPLAYMODE_BINARY,
                                          TextRender::DISPLAYMODE_HEXDUMP));

TEST_P(TextRendererDisplayModeTest, text_render_set_display_mode_test)
{
    GtkWidget *textr;
    textr = text_render_new();
    text_render_load_file(TEXT_RENDER(textr), filename);
    text_render_set_display_mode(TEXT_RENDER(textr), GetParam());
    ASSERT_EQ(GetParam(), text_render_get_display_mode(TEXT_RENDER(textr)));
}

////////////////////////////////////////////////////////////////////////

class TextRendererTabSizeTest : public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        TextRendererTabSizeTest,
                        ::testing::Range(1,10));

TEST_P(TextRendererTabSizeTest, text_render_set_tab_size)
{
    GtkWidget *textr;
    textr = text_render_new();
    text_render_load_file(TEXT_RENDER(textr), filename);
    text_render_set_tab_size(TEXT_RENDER(textr), GetParam());
    ASSERT_EQ(GetParam(), text_render_get_tab_size(TEXT_RENDER(textr)));
}

////////////////////////////////////////////////////////////////////////

class TextRendererWrapModeTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        TextRendererWrapModeTest,
                        ::testing::Bool());

TEST_P(TextRendererWrapModeTest, gviewer_set_wrap_mode_test)
{
    GtkWidget *textr;
    textr = text_render_new();
    text_render_load_file(TEXT_RENDER(textr), filename);
    text_render_set_wrap_mode(TEXT_RENDER(textr), GetParam());
    ASSERT_EQ(GetParam(), text_render_get_wrap_mode(TEXT_RENDER(textr)));
}

////////////////////////////////////////////////////////////////////////

class TextRendererFixedLimitTest : public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        TextRendererFixedLimitTest,
                        ::testing::Values(1,10));

TEST_P(TextRendererFixedLimitTest, gviewer_set_set_fixed_limit)
{
    GtkWidget *textr;
    textr = text_render_new();
    text_render_load_file(TEXT_RENDER(textr), filename);
    text_render_set_fixed_limit(TEXT_RENDER(textr), GetParam());
    ASSERT_EQ(GetParam(), text_render_get_fixed_limit(TEXT_RENDER(textr)));
}

////////////////////////////////////////////////////////////////////////

class TextRendererSetEncodingTest : public ::testing::TestWithParam<const char *> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        TextRendererSetEncodingTest,
                        ::testing::Values("ASCII", "UTF8", "CP437", "CP1251"));

TEST_P(TextRendererSetEncodingTest, gviewer_set_encoding_test)
{
    GtkWidget *textr;
    textr = text_render_new();
    text_render_load_file(TEXT_RENDER(textr), filename);
    text_render_set_encoding(TEXT_RENDER(textr), GetParam());
    ASSERT_STREQ(GetParam(), text_render_get_encoding(TEXT_RENDER(textr)));
}
