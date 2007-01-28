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
#include <libgnomeui/libgnomeui.h>

#include <libgviewer/libgviewer.h>


int main(int argc, char* argv[])
{
	SEARCHMODE sm;
	gchar *text;
	guint8 *buffer;
	guint i;
	guint buflen;
	GViewerSearchDlg* srch_dlg;
	GtkWidget *w;

	gnome_init("gnome-commander","0.1",argc,argv);

	w = gviewer_search_dlg_new(NULL);
	g_warning("_new finished");
	if (gtk_dialog_run(GTK_DIALOG(w))==GTK_RESPONSE_OK) {
		srch_dlg = GVIEWER_SEARCH_DLG(w);

		sm = gviewer_search_dlg_get_search_mode(srch_dlg);
		if (sm == SEARCH_MODE_TEXT) {
			printf("Search mode: text\n");
			printf("Case Mode: %ssensitive\n",
				gviewer_search_dlg_get_case_sensitive(srch_dlg) ?
				"" : "in" );

			text = gviewer_search_dlg_get_search_text_string(srch_dlg);
			printf("Text: \"%s\"\n", text);
			g_free(text);

		} else {
			printf("Search mode: hex\n");
			buffer = gviewer_search_dlg_get_search_hex_buffer(srch_dlg, &buflen);

			printf("Buffer Length: %d bytes\n", buflen);
			if (buflen>0 && buffer!=NULL) {
				printf("Buffer:\n");
				for (i=0;i<buflen;i++) {
					printf("%02x ", buffer[i]);
					if (i>0 && (i%16==0))
						printf("\n");
				}
				printf("\n");
			}
		}

	} else {
		printf ( "Search Canceled\n" );
	}
	gtk_widget_destroy(w);

	return 0;
}
