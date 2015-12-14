/**
 * @file iv_fileops_test.cc
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

#include "gtest/gtest.h"
#include <libgviewer.h>
#include <gvtypes.h>
#include <fileops.h>

// The fixture for testing class FileOpsTest.
class FileOpsTest : public ::testing::Test {};

TEST_F(FileOpsTest, gv_file_get_byte_does_read) {
    const char *file_path = "../README";
    ViewerFileOps *fops;
    offset_type end;
    offset_type current;
    int value;

    fops = gv_fileops_new();

    ASSERT_NE (-1, gv_file_open(fops, file_path));

    end = gv_file_get_max_offset(fops);

    for (current = 0; current < end; current++)
    {
        value = gv_file_get_byte(fops, current);
        ASSERT_TRUE (0 <= value && value <= 255 );
    }

    ASSERT_NE(-1, gv_file_open(fops, file_path));

    gv_file_free(fops);
    g_free(fops);
}
