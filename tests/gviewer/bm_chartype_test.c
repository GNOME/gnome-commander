/*
  GNOME Commander - A GNOME based file manager
  Copyright (C) 2001-2006 Marcus Bjurman

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

   Author: Assaf Gordon  <agordon88@gmail.com>
*/

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgviewer/gvtypes.h>
#include <libgviewer/viewer-utils.h>
#include <libgviewer/bm_chartype.h>

void print_badchar(gpointer key, gpointer value, gpointer user_data)
{
	printf("%02x %02x %02x %02x\t%d\n", 
		GV_FIRST_BYTE((guint32)key),
		GV_SECOND_BYTE((guint32)key),
		GV_THIRD_BYTE((guint32)key),
		GV_FOURTH_BYTE((guint32)key),
		(guint32)value);
}

int main()
{
	/* This is a valid UTF8 string, with four hebrew letters in it:
	    D7 90 =  Aleph (Unicode 0x5D0)
	    D7 95 =  Vav (unicode 0x5D5)
	    D7 94 =  He (unicode 0x5D4)
	    D7 91 = Bet (Unicode 0x5D1) 
	    (Aleph-Vav-He-Bet is pronounced "ohev", means "love" in hebrew, FYI :-)
	*/
	const gchar *pattern = "I \xd7\x90\xd7\x95\xd7\x94\xd7\x91 you";
	
	
	/* This is a valid UTF8 text, with pangrams in several languages (I hope I got it right...) */
	const gchar *text = 
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
"Les na\xc3\xaf fs \xc3\xa6githales h\xc3\xa2tifs pondant \xc3\xa0 No\xc3\xabl o\xc3\xb9 il g\xc3\xa8le sont s\xc3\xbbrs d\x27\xc3\xaatre d\xc3\xa9\xc3\xa7us et de voir leurs dr\xc3\xb4les d\x27\xc5\x93ufs ab\xc3\xaem\xc3\xa9s\x2e" ;

	
	int i;
        int j;
	int m;
	int n;
	char_type *ct_text;
	int  ct_text_len ;
	
	GViewerBMChartypeData *data ;
	
	data = create_bm_chartype_data(pattern,FALSE);
	
	printf("Good Suffixes table:\n");
	for (i=0;i<data->pattern_len;i++)
		printf("%d ", data->good[i]) ;
	printf("\n\n");
	
	printf("Bad characters table:\nUTF-8 char\tValue\n");
	g_hash_table_foreach(data->bad, print_badchar, NULL);		
	printf("(All other characters have value of %d)\n\n", data->pattern_len) ;

	/* Convert the UTF8 text string to a chartype array */
	ct_text = convert_utf8_to_chartype_array(text, &ct_text_len);
	if (!ct_text) {
		fprintf(stderr,"failed to convert text to 'char_type' array (maybe 'text' is not a valid UTF8 string?)\n");
		exit(-1);
	}
	
	/* Do the actual search */
	m = data->pattern_len;
	n = ct_text_len;
	j = 0;
	while (j <= n - m) {
		for (i = m - 1; i >= 0 && bm_chartype_equal(data,i,ct_text[i + j]); --i);
		if (i < 0) {
			printf(" Found match at offset = %d\n", j);
			j += bm_chartype_get_good_match_advancement(data);
		}
		else
			j += bm_chartype_get_advancement(data, i, ct_text[i+j]) ;
	}
	
	g_free(ct_text);
	free_bm_chartype_data(data);
	
	return 0;
}
