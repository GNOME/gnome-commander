/**
 * @file iv_datapresentation_test.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
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

#include <intviewer/libgviewer.h>
#include "gtest/gtest.h"

const gchar *filename = "../INSTALL";

////////////////////////////////////////////////////////////////////////

class DataPresentationEncodingTest : public ::testing::TestWithParam<PRESENTATION> {};

INSTANTIATE_TEST_CASE_P(InstantiationName,
                        DataPresentationEncodingTest,
                        ::testing::Values(PRSNT_WRAP, PRSNT_NO_WRAP, PRSNT_BIN_FIXED));

TEST_P(DataPresentationEncodingTest, gv_set_data_presentation_mode_test) {
    GVDataPresentation *dp = NULL;
    dp = gv_data_presentation_new();
    gv_set_data_presentation_mode(dp, GetParam());
    ASSERT_EQ(GetParam(), gv_get_data_presentation_mode(dp));
    if (dp)
        gv_free_data_presentation(dp);
    g_free(dp);
}

////////////////////////////////////////////////////////////////////////



