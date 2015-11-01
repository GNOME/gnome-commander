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
#include <iv_bm_search_test.h>

/**
 * In this test a short pattern of integers is searched inside a 
 * bigger array of integers.
 * (see definitions in @link BmByteTest @endlink)
 */
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


/**
 * In this test a pattern of UTF-8 encoded letters is searched in a text
 * with UTF-8 encoded letters.
 */
TEST_F(BmByteTest, chartype_test) {
    /* 
     * This is a valid UTF8 string, with four hebrew letters in it:
     *  0xD7 0x90 = Aleph (Unicode U+5D0)
     *  0xD7 0x95 = Vav   (Unicode U+5D5)
     *  0xD7 0x94 = He    (Unicode U+5D4)
     *  0xD7 0x91 = Bet   (Unicode U+5D1)
     *  (Aleph-Vav-He-Bet, pronounced "ohev", means "love" in hebrew, FYI :-)
     */
    const gchar *pattern = "I \xd7\x90\xd7\x95\xd7\x94\xd7\x91 you";


    // This is a valid UTF8 text, with pangrams in several languages (I hope I got it right...)
    const gchar *text = \
        "English:" \
        "The quick brown fox jumps over the lazy dog" \
        "Irish:" \
        "An \xe1\xb8\x83 fuil do \xc4\x8bro\xc3\xad ag buala\xe1\xb8\x8b \xc3\xb3 \xe1\xb8\x9f ait\xc3\xados an \xc4\xa1r\xc3\xa1 a \xe1\xb9\x81 eall lena \xe1\xb9\x97\xc3\xb3g \xc3\xa9 ada \xc3\xb3 \xe1\xb9\xa1l\xc3\xad do leasa \xe1\xb9\xab\xc3\xba\x3f" \
        "Swedish:" \
        "Flygande b\xc3\xa4 ckasiner s\xc3\xb6ka strax hwila p\xc3\xa5 mjuka tuvor" \
        "(our match: I \xd7\x90\xd7\x95\xd7\x94\xd7\x91 You)" \
        "Hebrew:" \
        "\xd7\x96\xd7\x94 \xd7\x9b\xd7\x99\xd7\xa3 \xd7\xa1\xd7\xaa\xd7\x9d \xd7\x9c\xd7\xa9\xd7\x9e\xd7\x95\xd7\xa2 \xd7\x90\xd7\x99\xd7\x9a \xd7\xaa\xd7\xa0\xd7\xa6\xd7\x97 \xd7\xa7\xd7\xa8\xd7\xa4\xd7\x93 \xd7\xa2\xd7\xa5 \xd7\x98\xd7\x95\xd7\x91 \xd7\x91\xd7\x92\xd7\x9f" \
        "French:" \
        "Les na\xc3\xaf fs \xc3\xa6githales h\xc3\xa2tifs pondant \xc3\xa0 No\xc3\xabl o\xc3\xb9 il g\xc3\xa8le sont s\xc3\xbbrs d\x27\xc3\xaatre d\xc3\xa9\xc3\xa7us et de voir leurs dr\xc3\xb4les d\x27\xc5\x93ufs ab\xc3\xaem\xc3\xa9s\x2e";

    int i;
    int j;
    int m;
    int n;
    char_type *ct_text;
    int  ct_text_len;

    GViewerBMChartypeData *data;

    data = create_bm_chartype_data(pattern,FALSE);

    // Convert the UTF8 text string to a chartype array
    ct_text = convert_utf8_to_chartype_array(text, ct_text_len);
    ASSERT_TRUE(ct_text) << "Failed to convert text to 'char_type' array (maybe 'text' is not a valid UTF8 string?)\n";

    // Do the actual search
    m = data->pattern_len;
    n = ct_text_len;
    j = 0;
    int found_at = 0;
    while (j <= n - m)
    {
        for (i = m - 1; i >= 0 && bm_chartype_equal(data,i,ct_text[i + j]); --i);

        if (i < 0)
        {
            printf(" Found match at offset = %d\n", j);
            found_at = j;
            j += bm_chartype_get_good_match_advancement(data);

        }
        else
            j += bm_chartype_get_advancement(data, i, ct_text[i+j]);
    }
    ASSERT_EQ(found_at, 217) << "String with UTF-8 letters not found in text where it should be found.";

    g_free(ct_text);
    free_bm_chartype_data(data);
}
