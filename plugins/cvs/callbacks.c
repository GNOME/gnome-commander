#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <libgcmd/libgcmd.h>
#include "cvs-plugin.h"
#include "callbacks.h"
#include "interface.h"
#include "parser.h"

void
on_print                               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_print_setup                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_rev_list_select_row                 (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
	GtkWidget *rev_label = lookup_widget (GTK_WIDGET (clist), "rev_label");
	GtkWidget *date_label = lookup_widget (GTK_WIDGET (clist), "date_label");
	GtkWidget *author_label = lookup_widget (GTK_WIDGET (clist), "author_label");
	GtkWidget *state_label = lookup_widget (GTK_WIDGET (clist), "state_label");
	GtkWidget *lines_label = lookup_widget (GTK_WIDGET (clist), "lines_label");
	GtkWidget *msg_text_view = lookup_widget (GTK_WIDGET (clist), "msg_text");
	Revision *rev = (Revision *)gtk_clist_get_row_data (clist, row);
	
	gtk_label_set_text (GTK_LABEL (rev_label), rev->number);	
	gtk_label_set_text (GTK_LABEL (date_label), rev->date);	
	gtk_label_set_text (GTK_LABEL (author_label), rev->author);	
	gtk_label_set_text (GTK_LABEL (state_label), rev->state);	
	gtk_label_set_text (GTK_LABEL (lines_label), rev->lines);	
	if (rev->message) {
		GtkTextBuffer *buf = gtk_text_buffer_new (NULL);
		gtk_text_buffer_set_text (buf, rev->message, strlen (rev->message));
		gtk_text_view_set_buffer (GTK_TEXT_VIEW (msg_text_view), buf);
	}
}


void
on_quit                                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkWidget *main_win = lookup_widget (GTK_WIDGET (menuitem), "main_win");
	gtk_widget_destroy (main_win);
}


void
on_main_win_show                       (GtkWidget       *widget,
                                        gpointer         user_data)
{
}


void
on_diff_dialog_diff                    (GtkButton       *button,
                                        CvsPlugin       *plugin)
{
	FILE *fd;
	gchar *cmd;
	gchar text[256];
	gint ret;
	GtkWidget *entry1 = lookup_widget (GTK_WIDGET (button), "rev1_entry");
	GtkWidget *entry2 = lookup_widget (GTK_WIDGET (button), "rev2_entry");
	GtkWidget *diff_text = lookup_widget (GTK_WIDGET (button), "diff_text");
	GtkTextBuffer *buf = gtk_text_buffer_new (NULL);

	cmd = g_strdup_printf ("cvs diff -r %s -r %s %s",
		gtk_entry_get_text (GTK_ENTRY (entry1)),
		gtk_entry_get_text (GTK_ENTRY (entry2)),
		plugin->log_history->fname);
	
	printf ("Running: %s\n", cmd);
	fd = popen (cmd, "r");
	if (fd < 0) return;
	do
	{
		ret = fread (text, 1, 256, fd);
		gtk_text_buffer_insert_at_cursor (buf, text, ret);
	} while (ret == 256);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (diff_text), buf);
	pclose (fd);
	g_free (cmd);
}


void
on_diff_dialog_close                   (GtkButton       *button,
                                        gpointer         user_data)
{
	GtkWidget *dlg = lookup_widget (GTK_WIDGET (button), "diff_dialog");
	gtk_widget_destroy (dlg);
}


void
on_compare_revisions                   (GtkMenuItem     *menuitem,
                                        CvsPlugin       *plugin)
{
	GtkWidget *dlg = create_diff_dialog (plugin);
	GtkWidget *combo1 = lookup_widget (GTK_WIDGET (dlg), "rev1_combo");
	GtkWidget *combo2 = lookup_widget (GTK_WIDGET (dlg), "rev2_combo");
	
	gtk_combo_set_popdown_strings (GTK_COMBO (combo1),
								   plugin->log_history->rev_names);
	gtk_combo_set_popdown_strings (GTK_COMBO (combo2),
								   plugin->log_history->rev_names);
	gtk_widget_show (dlg);	
}


void
on_about                               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkWidget *dlg = create_about_dialog ();
	gtk_widget_show (dlg);
}

void
on_font_toggled                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
	/*
	GtkWidget *diff_text = lookup_widget (GTK_WIDGET (togglebutton), "diff_text");
	gnome_less_set_fixed_font (GNOME_LESS (diff_text), 
		gtk_toggle_button_get_active (togglebutton));
	gnome_less_reshow (GNOME_LESS (diff_text));
	*/
}
