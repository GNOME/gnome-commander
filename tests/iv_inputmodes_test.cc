/**
 * @file iv_inputmodes_test.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
 *
 * @details This module tests the input mode module in libgviewer.
 * Possible values: ASCII, CP437, UTF8 and all other encodings readable
 * by the iconv library. Currently you will find only a very simple,
 * nearly meaningless test. Many other tests of this module are missing.
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

class ViewerInputModeTest : public ::testing::TestWithParam<const char *> {};

INSTANTIATE_TEST_CASE_P(InstantiationScaleFactor,
                        ViewerInputModeTest,
                        ::testing::Values("ASCII", "UTF8", "CP437", "CP1251"));

TEST_P(ViewerInputModeTest, gv_set_input_mode_test)
{
    GVInputModesData *imd = NULL;
    imd = gv_input_modes_new();

    gv_set_input_mode(imd, GetParam());
    ASSERT_STREQ(GetParam(), gv_get_input_mode(imd));

    gv_free_input_modes(imd);
    g_free(imd);
}
