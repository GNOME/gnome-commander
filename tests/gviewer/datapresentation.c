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

   Author: Assaf Gordon  <agordon88@gmail.com>
*/

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libgviewer/libgviewer.h>
#include <libgviewer/gvtypes.h>
#include <libgviewer/fileops.h>
#include <libgviewer/inputmodes.h>
#include <libgviewer/datapresentation.h>

static offset_type start_line;
static offset_type end_line;
static gchar *filename = NULL;
static gchar *encoding = NULL;
static PRESENTATION presentation;
static ViewerFileOps *fops = NULL;
static GVInputModesData *imd = NULL;
static GVDataPresentation *dp = NULL;
static guint tab_size;
static guint wrap_limit;
static guint fixed_limit;
static gboolean hexdump;

void usage()
{
    fprintf(stderr,"This program tests the data-presentation module in 'libgviewer'.\n\n");

    fprintf(stderr,"Usage: test-datapresentation [-e encoding] [-p presentation] [-w wrap limit]\n");
    fprintf(stderr,"\t\t[-f fixed limit] [-t tab size] [-x] filename [start_line] [end_line]\n\n");

    fprintf(stderr,"\t-e enconding: ASCII, UTF8, CP437, CP1251, etc\n");
    fprintf(stderr,"\t-p presentation: NOWRAP, WRAP, FIXED.\n");
    fprintf(stderr,"\t-w wrap limit: max characters per line (in WRAP mode).\n");
    fprintf(stderr,"\t-f fixed limit: fixed number of characters per line (in FIXED mode).\n");
    fprintf(stderr,"\t-t tab size: number of space per TAB character.\n");
    fprintf(stderr,"\t-x hex dump: display a hex dump\n.");
    fprintf(stderr,"\t\t(hex dump forces FIXED presentation mode.)\n\n");
    fprintf(stderr,"\tfilename: The file to read. UTF8 output will be sent to STDOUT.\n");
    fprintf(stderr,"\tstart_line: first line to display (default=0).\n");
    fprintf(stderr,"\tend_line: last line to display (default=end-of-file).\n");
    exit(0);
}

void parse_command_line(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind, opterr, optopt;
    int c;

    start_line = 0;
    end_line = -1;
    tab_size = 8;
    wrap_limit = 80;
    fixed_limit = 40;
    hexdump = FALSE;
    presentation = PRSNT_NO_WRAP;
    encoding = g_strdup("ASCII");

    while ((c=getopt(argc,argv,"xe:p:f:t:w:")) != -1) {
        switch(c)
        {
        case 'x':
            hexdump = TRUE;
            break;
        case 'e':
            g_free(encoding);
            encoding = g_strdup(optarg);
            break;

        case 'p':
            if (g_ascii_strcasecmp(optarg,"WRAP")==0)
                presentation = PRSNT_WRAP;
            else if (g_ascii_strcasecmp(optarg,"NOWRAP")==0)
                presentation = PRSNT_NO_WRAP;
            else if (g_ascii_strcasecmp(optarg,"FIXED")==0)
                presentation = PRSNT_BIN_FIXED;
            else {
                g_warning("Invalid presentation mode \"%s\".\n", optarg);
                usage();
            }
            break;

        case 'w':
            wrap_limit = atoi(optarg);
            if (wrap_limit<=0) {
                g_warning("Invalid wrap limit \"%s\".\n", optarg);
                usage();
            }
            break;

        case 't':
            tab_size = atoi(optarg);
            if (tab_size <=0) {
                g_warning("Invalid tab size \"%s\".\n", optarg);
                usage();
            }
            break;

        case 'f':
            fixed_limit = atoi(optarg);
            if (fixed_limit<=0) {
                g_warning("Invalid fixed limit \"%s\".\n", optarg);
                usage();
            }
            break;
        }
    }

    if (hexdump && (g_ascii_strcasecmp(encoding, "UTF8")==0)) {
        g_warning("Can't use HexDump mode with UTF8 encoding. (Hexdump requires each character to be one byte exactly)\n");
        exit(0);
    }

    if (hexdump) {
        presentation = PRSNT_BIN_FIXED;
        fixed_limit = 16;
    }

    if (optind == argc) {
        g_warning("Need file name to work with...\n");
        usage();
    }
    filename = g_strdup(argv[optind++]);

    if (optind < argc) {
        start_line = atol(argv[optind++]);
    }
    if (optind < argc) {
        end_line = atol(argv[optind++]);
    }

    fprintf(stderr,"filename(%s) encoding(%s) presentation(%d) wrap(%d) fixed(%d) tabsize(%d) start(%d) end(%d)\n",
        filename, encoding, (int)presentation, wrap_limit, fixed_limit, tab_size, (int)start_line, (int)end_line);
}

int init()
{
    // Setup the fileops
    fops = gv_fileops_new();
    if (gv_file_open(fops, filename)==-1) {
        fprintf(stderr,"Failed to open \"%s\"\n", filename);
        return -1;
    }

    // Setup the input mode translations
    imd = gv_input_modes_new();
    gv_init_input_modes(imd, (get_byte_proc)gv_file_get_byte, fops);
    gv_set_input_mode(imd,encoding);

    // Setup the data presentation mode
    dp = gv_data_presentation_new();
    gv_init_data_presentation(dp, imd, gv_file_get_max_offset(fops));
    gv_set_data_presentation_mode(dp, presentation);
    gv_set_wrap_limit(dp, wrap_limit);
    gv_set_fixed_count(dp, fixed_limit);
    gv_set_tab_size(dp, tab_size);

    return 0;
}

void cleanup()
{
    g_free(filename);
    g_free(encoding);

    if (dp)
        gv_free_data_presentation(dp);
    g_free(dp);

    if (imd)
        gv_free_input_modes(imd);
    g_free(imd);

    if (fops)
        gv_file_free(fops);
    g_free(fops);
}

void display_utf8_char(char_type value)
{
    // value is UTF-8 character, packed into 32bits
    printf("%c", GV_FIRST_BYTE(value));
    if (GV_SECOND_BYTE(value)) {
        printf("%c", GV_SECOND_BYTE(value));

        if (GV_THIRD_BYTE(value)) {
            printf("%c", GV_THIRD_BYTE(value));

            if (GV_FOURTH_BYTE(value)) {
                printf("%c", GV_FOURTH_BYTE(value));
            }
        }
    }
}

int display_line(offset_type start_of_line, offset_type end_of_line, gboolean display_control_chars)
{
    offset_type current;
    char_type value;

#if 0
    printf("line: %d -> %d\n", (int)start_of_line, (int)end_of_line);
#else
    current = start_of_line;
    while (current < end_of_line) {
        /* Read a UTF8 character from the input file.
           The "inputmode" module is responsible for converting the file into UTF8 */
        value = gv_input_mode_get_utf8_char(imd, current);
        if (value==INVALID_CHAR) {
            printf("\n");
            return -1;
        }

        // move to the next character's offset
        current = gv_input_get_next_char_offset(imd,current);

        if (value=='\r' || value=='\n') {
            if (display_control_chars)
                value = gv_input_mode_byte_to_utf8(imd,(unsigned char)value);
            else
                continue;
        }

        if (value=='\t') {
            if (display_control_chars)
                value = gv_input_mode_byte_to_utf8(imd,(unsigned char)value);
            else {
                int i;
                for(i=0;i<tab_size;i++)
                    display_utf8_char(' ');
                continue;
            }
        }

        display_utf8_char(value);
    }
    printf("\n");
#endif
    return 0;
}

int display_hexdump_line(offset_type start_of_line, offset_type end_of_line)
{
    int byte_value;
    char_type value;
    offset_type current;

    printf("%08lx ", (unsigned long) start_of_line);

    current = start_of_line;
    while (current < end_of_line) {
        byte_value = gv_input_mode_get_raw_byte(imd, current);
        if (byte_value==-1)
            break;
        current++;
        printf("%02x ", (unsigned char)byte_value);
    }

    current = start_of_line;
    while (current < end_of_line) {
        byte_value = gv_input_mode_get_raw_byte(imd, current);
        if (byte_value==-1)
            break;
        value = gv_input_mode_byte_to_utf8(imd, (unsigned char)byte_value);
        display_utf8_char(value);
        current++;
    }

    printf("\n");

    return 0;
}

int main(int argc, char *argv[])
{
    offset_type current;
    int lines;
    int rc;

    parse_command_line(argc,argv);

    if (init()==-1)
        goto error;


    lines = 0;
    current = gv_scroll_lines(dp, 0, start_line);
    while (1) {
        offset_type eol_offset;

        eol_offset = gv_get_end_of_line_offset(dp, current);
        if (eol_offset == current)
            break;

        if (hexdump)
            rc =display_hexdump_line(current, eol_offset);
        else
            rc = display_line(current, eol_offset, (presentation==PRSNT_BIN_FIXED));

        if (rc==-1)
            break;

        current = eol_offset;

        lines++;
        if (start_line + lines > end_line)
            break;
    }

error:
    cleanup();
    return 0;
}
