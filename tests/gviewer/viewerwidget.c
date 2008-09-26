/*
  GNOME Commander - A GNOME based file manager
  Copyright (C) 2001-2006 Marcus Bjurman
  Copyright (C) 2007-2008 Piotr Eljasiak

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

#include <glib.h>
#include <gtk/gtk.h>

#include <libgviewer/libgviewer.h>

static gchar *filename = NULL;
static gchar *encoding = NULL;
static VIEWERDISPLAYMODE dispmode = DISP_MODE_TEXT_FIXED;
static guint tab_size;
static guint fixed_limit;
static gboolean wrap_mode;
static gboolean best_fit = TRUE;
static double scale_factor = 1.0;
static gboolean auto_detect_display_mode = TRUE;

void usage()
{
    fprintf(stderr,"This program tests the gviewer widget in 'libgviewer'.\n\n");

    fprintf(stderr,"Usage: test-textrenderer [-e encoding] [-d dispmode] [-w] [-f fixed_limit] [-t tab_size] [-s scale] filename\n\n");

    fprintf(stderr,"\t-e enconding: ASCII, UTF8, CP437, CP1251, etc\n");
    fprintf(stderr,"\t-d Display Mode:\n\t     auto(default)\n\t     Text\n\t     Binary\n\t     Hex\n\t      Image\n");
    fprintf(stderr,"\t-w In fixed/variable text displays, turns on wrapping.\n");
    fprintf(stderr,"\t-f fixed_limit: In Binary display mode, sets number of Bytes per line.\n");
    fprintf(stderr,"\t-t tab size: In fixed/variable text displays, set number of space per TAB character.\n");
    fprintf(stderr,"\t-s scale: In Image mode, use fixed scaling factor (0.1 to 3.0)\n\t\t(Default is using best-fit-to-window)\n");
    fprintf(stderr,"\tfilename: The file to display.\n");
    exit(0);
}

void parse_command_line(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind, opterr, optopt;
    int c;

    tab_size = 8;
    fixed_limit = 40;
    dispmode = DISP_MODE_TEXT_FIXED;
    encoding = g_strdup("ASCII");
    wrap_mode = FALSE;
    best_fit = TRUE;
    scale_factor = 1.0;
    auto_detect_display_mode = TRUE;

    while ((c=getopt(argc,argv,"d:e:f:t:s:w")) != -1) {
        switch(c)
        {
        case 's':
            best_fit = FALSE;
            scale_factor = atof(optarg);
            if (scale_factor<0.1 || scale_factor>3.0) {
                g_warning("Invalid scale factor \"%f\".\n", scale_factor);
                usage();
            }
            break;

        case 'w':
            wrap_mode = TRUE;
            break;

        case 'e':
            g_free(encoding);
            encoding = g_strdup(optarg);
            break;

        case 'd':
            auto_detect_display_mode = FALSE;
            if (g_ascii_strcasecmp(optarg,"fixed")==0)
                dispmode = DISP_MODE_TEXT_FIXED;
            else if (g_ascii_strcasecmp(optarg,"binary")==0)
                dispmode = DISP_MODE_BINARY;
            else if (g_ascii_strcasecmp(optarg,"hex")==0)
                dispmode = DISP_MODE_HEXDUMP;
            else if (g_ascii_strcasecmp(optarg,"image")==0)
                dispmode = DISP_MODE_IMAGE;
            else if (g_ascii_strcasecmp(optarg,"auto")==0)
                auto_detect_display_mode = TRUE;
            else {
                g_warning("Invalid display mode \"%s\".\n", optarg);
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

        default:
            usage();
            break;
        }
    }

    if (optind == argc) {
        g_warning("Need file name to work with...\n");
        usage();
    }
    filename = g_strdup(argv[optind++]);
}


int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *viewer;

    gtk_init(&argc,&argv);

    parse_command_line(argc,argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);

    viewer = gviewer_new();
    gviewer_load_file(GVIEWER(viewer), filename);

    if (!auto_detect_display_mode)
        gviewer_set_display_mode(GVIEWER(viewer),dispmode);

    gviewer_set_encoding(GVIEWER(viewer),encoding);
    gviewer_set_tab_size(GVIEWER(viewer),tab_size);
    gviewer_set_fixed_limit(GVIEWER(viewer),40);
    gviewer_set_wrap_mode(GVIEWER(viewer),wrap_mode);
    gviewer_set_best_fit(GVIEWER(viewer),best_fit);
    gviewer_set_scale_factor(GVIEWER(viewer),scale_factor);

    gtk_widget_show(viewer);

    gtk_container_add(GTK_CONTAINER(window), viewer);
    gtk_widget_show(window);

    gtk_main();

    return 0;
}
