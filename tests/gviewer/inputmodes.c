/*
  GNOME Commander - A GNOME based file manager
  Copyright (C) 2001-2006 Marcus Bjurman
  Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2014 Uwe Scholz

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
#include <libgviewer/libgviewer.h>
#include <libgviewer/gvtypes.h>
#include <libgviewer/fileops.h>
#include <libgviewer/inputmodes.h>

static offset_type start;
static offset_type end;
static gchar *filename = NULL;
static gchar *input_mode = NULL;
static ViewerFileOps *fops = NULL;
static GVInputModesData *imd = NULL;

void usage()
{
    fprintf(stderr,"This program tests the input mode module in 'libgviewer'.\n\n");

    fprintf(stderr,"Usage: test-inputmodes mode filename [start_offset] [end_offset]\n");
    fprintf(stderr,"\t'filename' will be printed to STDOUT, always in UTF-8 encoding.\n");
    fprintf(stderr,"\t'start_offset' and 'end_offset' are optinal.\n\n");
    fprintf(stderr,"\tmode = input mode. possible values:\n");
    fprintf(stderr,"\tASCII\n\tCP437\n\tUTF8\n\tAnd all other encodings readable by Iconv library.\n");
    exit(0);
}

void parse_command_line(int argc, char *argv[])
{
    if (argc<3)
        usage();

    input_mode = g_strdup(argv[1]);
    filename = g_strdup(argv[2]);

    if (argc>3)
        start = atol(argv[3]);
    else
        start = 0;

    if (argc>4)
        end = atol(argv[4]);
    else
        end = -1;
}

int init()
{
    fops = gv_fileops_new();

    if (gv_file_open(fops, filename)==-1) {
        fprintf(stderr,"Failed to open \"%s\"\n", filename);
        return -1;
    }

    if (end==-1)
        end = gv_file_get_max_offset(fops);

    imd = gv_input_modes_new();
    gv_init_input_modes(imd, (get_byte_proc)gv_file_get_byte, fops);

    gv_set_input_mode(imd,input_mode);

    return 0;
}

void cleanup()
{
    g_free(filename);
    g_free(input_mode);

    if (fops)
        gv_file_free(fops);
    g_free(fops);

    if (imd)
        gv_free_input_modes(imd);
    g_free(imd);
}

int main(int argc, char *argv[])
{
    offset_type current;
    char_type value;

    parse_command_line(argc,argv);

    if (init()==-1)
        goto error;


    current = start;
    while (current < end) {
        /* Read a UTF8 character from the input file.
           The "inputmode" module is responsible for converting the file into UTF8 */
        value = gv_input_mode_get_utf8_char(imd, current);
        if (value==INVALID_CHAR)
            break;

        // move to the next character's offset
        current = gv_input_get_next_char_offset(imd,current);

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


error:
    cleanup();
    return 0;
}
