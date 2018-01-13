/**
 * @file utils_no_dependencies_test.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @details In this file all tests are placed which belong to the
 * Boyer–Moore string search algorithm, used in the internal viewer of
 * GCMD. Currently, only a single test is implemented in which a short
 * pattern of integers is searched inside a bigger array of integers.
 *
 * @copyright (C) 2013-2018 Uwe Scholz\n
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

#include <glib.h>
#include <gtest/gtest.h>
#include "../src/gnome-cmd-includes.h"
#include "../src/utils-no-dependencies.h"


TEST(StrUriBasename, ReturnNull)
{
    EXPECT_EQ (NULL, str_uri_basename(NULL));

    // If argument string length is shorter
    // than two bytes return null
    EXPECT_EQ (NULL, str_uri_basename((gchar*) "a"));
}


TEST(StrUriBasename, ReturnEscapedStringAfterLastSlash)
{
    EXPECT_STREQ ("xyz", str_uri_basename((gchar*) "http://xyz"));
    EXPECT_STREQ ("xyz?", str_uri_basename((gchar*) "http://xyz?"));
}

