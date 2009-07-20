/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/* scan.c - 2000/06/16 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include "gnome-cmd-includes.h"
#include "utils.h"

using namespace std;


/*
 * Quick roman numeral check (non-roman numerals may also return true)
 * Patch from Slash Bunny (2007.08.12)
 * (http://home.hiwaay.net/~lkseitz/math/roman/numerals.shtml)
 *    I = 1    (one)
 *    V = 5    (five)
 *    X = 10   (ten)
 *    L = 50   (fifty)
 *    C = 100  (one hundred)
 *    D = 500  (five hundred)
 *    M = 1000 (one thousand)
 */
inline gint word_is_roman_numeral (gchar *text)
{
    gint len = 0;

    for (gchar *tmp = text; *tmp; ++tmp, ++len)
        if (*tmp != (gunichar)'i' && *tmp != (gunichar)'v' && *tmp != (gunichar)'x' && *tmp != (gunichar)'l' &&
            *tmp != (gunichar)'c' && *tmp != (gunichar)'d' && *tmp != (gunichar)'m')
            return *tmp == ' ' || *tmp == '_' ? len : 0;

    return len;
}


gchar *gcmd_convert_unchanged (gchar *string)
{
    return string;
}


gchar *gcmd_convert_ltrim (gchar *string)
{
    if (!string || !*string)
        return string;

    return g_strchug (string);
}


gchar *gcmd_convert_rtrim (gchar *string)
{
    if (!string || !*string)
        return string;

    return g_strchomp (string);
}


gchar *gcmd_convert_strip (gchar *string)
{
    if (!string || !*string)
        return string;

    return g_strstrip (string);
}


gchar *gcmd_convert_lowercase (gchar *string)
{
    if (!string || !*string)
        return string;

    gchar *converted_string = g_utf8_strdown (string, -1);

    g_free (string);

    return converted_string;
}


gchar *gcmd_convert_uppercase (gchar *string)
{
    if (!string || !*string)
        return string;

    gchar *converted_string = g_utf8_strup (string, -1);

    g_free (string);

    return converted_string;
}


// Function to set the first letter of each word to uppercase, according the "Chicago Manual of Style" (http://www.chicagomanualofstyle.org/)
// No needed to reallocate
gchar *gcmd_convert_sentence_case (gchar *string)
{
    if (!string || !*string)
        return string;

    // Bariº Çiçek version
    gint len;
    gchar utf8_character[6];
    gunichar c;
    // There have to be space at the end of words to seperate them from prefix
    // Chicago Manual of Style "Heading caps" Capitalization Rules (CMS 1993, 282) (http://www.docstyles.com/cmscrib.htm#Note2)
    static gchar *exempt[] =
    {
        "a ",       "a_",
        "against ", "against_",
        "an ",      "an_",
        "and ",     "and_",
        "at ",      "at_",
        "between ", "between_",
        "but ",     "but_",
        "for ",     "for_",
        "in ",      "in_",
        "nor ",     "nor_",
        "of ",      "of_",
        "on ",      "on_",
        "or ",      "or_",
        "so ",      "so_",
        "the ",     "the_",
        "to ",      "to_",
        "with ",    "with_",
        "yet ",     "yet_",
        NULL
    };

    gcmd_convert_lowercase (string);

    // Removes trailing whitespace
    gchar *i = string = g_strchomp (string);

    // If the word is a roman numeral, capitalize all of it
    if ((len = word_is_roman_numeral (i)))
        strncpy (string, g_utf8_strup (i, len), len);
    else
    {
        // Set first character to uppercase
        c = g_utf8_get_char (i);
        strncpy (string, utf8_character, g_unichar_to_utf8 (g_unichar_toupper (c), utf8_character));
    }

    // Uppercase first character of each word, except for 'exempt[]' words lists
    while (i)
    {
        gchar *word = i; // Needed if there is only one word
        gchar *word1 = g_utf8_strchr (i,-1,' ');
        gchar *word2 = g_utf8_strchr (i,-1,'_');

        // Take the first string found (near beginning of string)
        if (word1 && word2)
            word = MIN (word1, word2);
        else if (word1)
            word = word1;
        else if (word2)
            word = word2;
        else
        {
            // Last word of the string: the first letter is always uppercase,
            // even if it's in the exempt list. This is a Chicago Manual of Style rule.
            // Last Word In String - Should Capitalize Regardless of Word (Chicago Manual of Style)
            c = g_utf8_get_char (word);
            strncpy (word, utf8_character, g_unichar_to_utf8 (g_unichar_toupper (c), utf8_character));
            break;
        }

        // Go to first character of the word (char. after ' ' or '_')
        ++word;

        // If the word is a roman numeral, capitalize all of it
        if ((len = word_is_roman_numeral (word)))
            strncpy (word, g_utf8_strup (word, len), len);
        else
        {
            // Set uppercase the first character of this word
            c = g_utf8_get_char (word);
            strncpy (word, utf8_character, g_unichar_to_utf8 (g_unichar_toupper (c), utf8_character));

            // Set lowercase the first character of this word if found in the exempt words list
            for (gint i=0; exempt[i]!=NULL; ++i)
                if (g_ascii_strncasecmp (exempt[i], word, strlen (exempt[i])) == 0)
                {
                    c = g_utf8_get_char (word);
                    strncpy (word, utf8_character, g_unichar_to_utf8 (g_unichar_tolower (c), utf8_character));
                    break;
            }
        }

        i = word;
    }

    // Uppercase letter placed after some characters like ' (', '[', '{'
    gboolean set_to_upper_case = FALSE;
    for (i = string; *i; i = g_utf8_next_char (i))
    {
        c = g_utf8_get_char (i);

        if (set_to_upper_case && g_unichar_islower (c))
            strncpy (i, utf8_character, g_unichar_to_utf8 (g_unichar_toupper (c), utf8_character));

        set_to_upper_case = c == (gunichar) '(' || c == (gunichar) '[' || c == (gunichar) '{' ||
                            c == (gunichar) '"' || c == (gunichar) ':' || c == (gunichar) '.' ||
                            c == (gunichar) '`' || c == (gunichar) '-';
    }

    return string;
}


gchar *gcmd_convert_initial_caps (gchar *string)
{
    if (!string || !*string)
        return string;

    gchar temp2[6];
    gboolean set_to_upper_case = TRUE;
    gchar utf8_character[6];

    for (gchar *i = string; *i; i = g_utf8_next_char (i))
    {
        gunichar c = g_utf8_get_char (i);
        if (set_to_upper_case && g_unichar_islower (c))
            strncpy (i, temp2, g_unichar_to_utf8 (g_unichar_toupper (c), temp2));
        else
            if (!set_to_upper_case && g_unichar_isupper (c))
                strncpy (i, temp2, g_unichar_to_utf8 (g_unichar_tolower (c), temp2));
        set_to_upper_case = FALSE; // After the first time, all will be lower case
    }

    // Uppercase again the word 'I' in english
    for (gchar *i=string; *i; )
    {
        gchar *word = i; // Needed if there is only one word
        gchar *word1 = g_utf8_strchr (i,-1,' ');
        gchar *word2 = g_utf8_strchr (i,-1,'_');

        // Take the first string found (near beginning of string)
        if (word1 && word2)
            word = MIN (word1,word2);
        else if (word1)
            word = word1;
        else if (word2)
            word = word2;
        else
            // Last word of the string
            break;

        // Go to first character of the word (char after ' ' or '_')
        ++word;

        // Set uppercase word 'I'
        if (g_ascii_strncasecmp ("I ", word, strlen ("I ")) == 0)
        {
            gunichar c = g_utf8_get_char (word);
            strncpy (word, utf8_character, g_unichar_to_utf8 (g_unichar_toupper (c), utf8_character));
        }

        i = word;
    }

    return string;
}


gchar *gcmd_convert_toggle_case (gchar *string)
{
    return string;
}
