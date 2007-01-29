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
#include <unistd.h>
#include <err.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libgviewer/libgviewer.h>

static gchar *filename = NULL;
static gchar *encoding = NULL;
static TEXTDISPLAYMODE dispmode = TR_DISP_MODE_TEXT;
static guint tab_size;
static guint fixed_limit;
static gboolean wrap_mode;

void usage()
{
    fprintf(stderr,"This program tests the text-render widget in 'libgviewer'.\n\n");

    fprintf(stderr,"Usage: test-textrenderer [-e encoding] [-d dispmode] [-w] [-f fixed_limit] [-t tab_size] filename\n\n");
    
    fprintf(stderr,"\t-e enconding: ASCII, UTF8, CP437, CP1251, etc\n");
    fprintf(stderr,"\t-d Display Mode:\n\t     Fixed(text)\n\t     Binary\n\t     Hex\n");
    fprintf(stderr,"\t-w In fixed/variable text displays, turns on wrapping.\n");
    fprintf(stderr,"\t-f fixed_limit: In Binary display mode, sets number of Bytes per line.\n");
    fprintf(stderr,"\t-t tab size: In fixed/variable text displays, set number of space per TAB character.\n");
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
    dispmode = TR_DISP_MODE_TEXT;
    encoding = g_strdup("ASCII");
    wrap_mode = FALSE;
    
    while ((c=getopt(argc,argv,"d:e:f:t:w")) != -1) {
        switch(c)
        {
        case 'w':
            wrap_mode = TRUE;
            break;
        
        case 'e':
            g_free(encoding);
            encoding = g_strdup(optarg);
            break;
        
        case 'd':
            if (g_ascii_strcasecmp(optarg,"fixed")==0)
                dispmode = TR_DISP_MODE_TEXT;
            else if (g_ascii_strcasecmp(optarg,"binary")==0)
                dispmode = TR_DISP_MODE_BINARY;
            else if (g_ascii_strcasecmp(optarg,"hex")==0)
                dispmode = TR_DISP_MODE_HEXDUMP;
            else {
                warnx("Invalid display mode \"%s\".\n", optarg);
                usage();
            }                
            break;
        
        case 't':
            tab_size = atoi(optarg);
            if (tab_size <=0) {
                warnx("Invalid tab size \"%s\".\n", optarg);
                usage();
            }
            break;
        
        case 'f':
            fixed_limit = atoi(optarg);
            if (fixed_limit<=0) {
                warnx("Invalid fixed limit \"%s\".\n", optarg);
                usage();
            }
            break;

        default:
            usage();
            break;
        }
    }
    
    if (optind == argc) {
        warnx("Need file name to work with...\n");
        usage();
    }
    filename = g_strdup(argv[optind++]);
}

int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *scrollbox;
    GtkWidget *textr;

    gtk_init(&argc,&argv);
    
    parse_command_line(argc,argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(window,600,400);

    scrollbox = scroll_box_new();

    textr = text_render_new();

    text_render_set_v_adjustment(TEXT_RENDER(textr),
        scroll_box_get_v_adjustment(SCROLL_BOX(scrollbox)));
    
    text_render_set_h_adjustment(TEXT_RENDER(textr),
        scroll_box_get_h_adjustment(SCROLL_BOX(scrollbox)));

    text_render_load_file(TEXT_RENDER(textr), filename);
    text_render_set_display_mode(TEXT_RENDER(textr), dispmode);
    text_render_set_tab_size(TEXT_RENDER(textr), tab_size);
    text_render_set_wrap_mode(TEXT_RENDER(textr), wrap_mode);
    text_render_set_fixed_limit(TEXT_RENDER(textr), fixed_limit);
    text_render_set_encoding(TEXT_RENDER(textr), encoding);
    
    scroll_box_set_client(SCROLL_BOX(scrollbox),textr);
    
    gtk_container_add(GTK_CONTAINER(window), scrollbox);
    
    gtk_widget_show(textr);
    gtk_widget_show(scrollbox);
    gtk_widget_show(window);
    
    gtk_main();
    
    return 0;
}
