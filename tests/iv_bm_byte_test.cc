/**
 * @file iv_bm_byte_test.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @details In this file all tests are placed which belong to the
 * Boyer–Moore string search algorithm, used in the internal viewer of
 * GCMD. Currently, only a single test is implemented in which a short
 * pattern of integers is searched inside a bigger array of integers
 * (see definitions in @link BmByteTest @endlink).
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
#include <iv_bm_byte_test.h>

TEST_F(BmByteTest, match_test) {

    GViewerBMByteData *data;
    int expected_good_data[7] {5, 5, 5, 5, 5, 7, 1};
    int expected_bad_data[4] {1, 5, 4, 3,};

    data = create_bm_byte_data(pattern,sizeof(pattern));

    printf("Good suffices table:\n");
    for (int i = 0; i < data->pattern_len; i++)
    {
        ASSERT_EQ(data->good[i], expected_good_data[i]) << "Vectors data->good and expected_good_data differ at index " << i;

        printf("%d ", data->good[i]);
    }
    printf("\n\n");

    printf("Bad characters table:\nChar(ASCII)\tValue\n");
    for (int i = 0, j = 0; i < 256; i++)
    {
        if (data->bad[i] != data->pattern_len)
        {
            ASSERT_EQ(data->bad[i], expected_bad_data[j]) << "Vectors data->bad and expected_bad_data differ at index " << j;
            j++;
            printf("%c(%d)\t\t%d\n",i , i, data->bad[i]);
        }
    }
    printf("(All other characters have value of %d)\n\n", data->pattern_len);

    // Do the actual search: The test search should find a match at 
    // position 142 and 248, nowhere else in the sample text.
    int m = data->pattern_len;
    int n = sizeof(text);
    int i; 
    int j = 0;
    while (j <= n - m) {
        for (i = m - 1; i >= 0 && pattern[i] == text[i + j]; --i);
        if (i < 0) {
            printf(" Found match at offset = %d\n", j);
            ASSERT_TRUE(j == 142 || j == 248);
            j += data->good[0];
        }
        else
            j += MAX(data->good[i], data->bad[(int)text[i + j]] - m + 1 + i);
    }

    free_bm_byte_data(data);
}
