/*
  GNOME Commander - A GNOME based file manager
  Copyright (C) 2001-2006 Marcus Bjurman
  Copyright (C) 2007-2011 Piotr Eljasiak

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
static gboolean best_fit = TRUE;
static double scale_factor = 1.0;

void usage()
{
    fprintf(stderr,"This program tests the image-render widget in 'libgviewer'.\n\n");

    fprintf(stderr,"Usage: test-imagerenderer [-s scale] filename\n\n");

    fprintf(stderr,"\t-s scale: use fixed scaling factor (0.1 to 3.0)\n\t\t(Default is using best-fit-to-window)\n");
    fprintf(stderr,"\tfilename: The file to display.\n");
    exit(0);
}

void parse_command_line(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind, opterr, optopt;
    int c;

    best_fit = TRUE;
    scale_factor = 1.0;

    while ((c=getopt(argc,argv,"s:")) != -1) {
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
    GtkWidget *scrollbox;
    GtkWidget *imgr;

    gtk_init(&argc,&argv);

    parse_command_line(argc,argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(window,600,400);

    scrollbox = scroll_box_new();

    imgr = image_render_new();

    image_render_set_v_adjustment(IMAGE_RENDER(imgr),
        scroll_box_get_v_adjustment(SCROLL_BOX(scrollbox)));

    image_render_set_h_adjustment(IMAGE_RENDER(imgr),
        scroll_box_get_h_adjustment(SCROLL_BOX(scrollbox)));

    image_render_load_file(IMAGE_RENDER(imgr), filename);

    image_render_set_best_fit(IMAGE_RENDER(imgr),best_fit);

    if (!best_fit)
        image_render_set_scale_factor(IMAGE_RENDER(imgr), scale_factor);

    scroll_box_set_client(SCROLL_BOX(scrollbox),imgr);

    gtk_container_add(GTK_CONTAINER(window), scrollbox);

    gtk_widget_show(imgr);
    gtk_widget_show(scrollbox);
    gtk_widget_show(window);

    gtk_main();

    return 0;
}
