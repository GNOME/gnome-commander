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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>

#include <libgcmd/libgcmd.h>
#include "cvs-plugin.h"
#include "interface.h"
#include "close.xpm"


static GtkWidget *create_compare_win (LogHistory *log_history);


static void on_rev_list_select_row  (GtkCList *clist,
                                     gint row,
                                     gint column,
                                     GdkEvent *event,
                                     LogHistory *log_history)
{
    Revision *rev = (Revision *)gtk_clist_get_row_data (clist, row);

    gtk_label_set_text (GTK_LABEL (log_history->rev_label), rev->number);
    gtk_label_set_text (GTK_LABEL (log_history->date_label), rev->date);
    gtk_label_set_text (GTK_LABEL (log_history->author_label), rev->author);
    gtk_label_set_text (GTK_LABEL (log_history->state_label), rev->state);
    gtk_label_set_text (GTK_LABEL (log_history->lines_label), rev->lines);

    log_history->plugin->selected_rev = rev;

    if (rev->message)
    {
        GtkTextBuffer *buf = gtk_text_buffer_new (NULL);
        gtk_text_buffer_set_text (buf, rev->message, strlen (rev->message));
        gtk_text_view_set_buffer (GTK_TEXT_VIEW (log_history->msg_text_view), buf);
    }
}


static void on_compare_clicked (GtkButton *button, LogHistory *log_history)
{
    GtkWidget *dlg = create_compare_win (log_history);
    GtkWidget *combo = lookup_widget (GTK_WIDGET (dlg), "rev_combo");

    gtk_combo_set_popdown_strings (GTK_COMBO (combo), log_history->rev_names);
    gtk_widget_show (dlg);
}


static void on_compare_ok (GtkButton *button, GtkWidget *dialog)
{
    gchar *cmd, *args, *prev_rev;
    const gchar *selected_rev, *selected_other_rev;
    GtkWidget *combo = lookup_widget (GTK_WIDGET (button), "rev_combo");
    GtkWidget *head_radio = lookup_widget (GTK_WIDGET (button), "head_radio");
    GtkWidget *prev_rev_radio = lookup_widget (GTK_WIDGET (button), "prev_rev_radio");
    LogHistory *log_history = (LogHistory *) lookup_widget (GTK_WIDGET (button), "log_history");

    selected_rev = gtk_object_get_data (GTK_OBJECT (dialog), "selected_rev");
    selected_other_rev = get_combo_text (combo);
    prev_rev = gtk_object_get_data (GTK_OBJECT (dialog), "prev_rev");

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (head_radio)))
        args = g_strdup_printf ("-r %s", selected_rev);
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prev_rev_radio)))
        args = g_strdup_printf ("-r %s -r %s", prev_rev, selected_rev);
    else
        args = g_strdup_printf ("-r %s -r %s", selected_other_rev, selected_rev);

    cmd = g_strdup_printf ("cvs -z%d diff %s %s %s",
                           log_history->plugin->compression_level,
                           log_history->plugin->unidiff?"-u":"",
                           args,
                           log_history->fname);

    if (!log_history->plugin->diff_win)
        log_history->plugin->diff_win = create_diff_win (log_history->plugin);

    add_diff_tab (log_history->plugin, cmd, log_history->fname);
    g_free (cmd);
    g_free (args);
    gtk_widget_destroy (dialog);
}


static void on_compare_cancel (GtkButton *button, GtkWidget *dialog)
{
    gtk_widget_destroy (dialog);
}


static void on_other_rev_toggled (GtkToggleButton *btn, GtkWidget *dialog)
{
    GtkWidget *combo = lookup_widget (dialog, "rev_combo");
    GtkWidget *entry = GTK_COMBO (combo)->entry;

    if (gtk_toggle_button_get_active (btn))
    {
        gtk_widget_set_sensitive (combo, TRUE);
        gtk_widget_grab_focus (entry);
    }
    else
        gtk_widget_set_sensitive (combo, FALSE);
}


static Revision *find_prev_rev (LogHistory *h, Revision *rev)
{
    GList *l = g_list_find (h->revisions, rev);

    if (!l)
        return NULL;

    l = l->next;

    return l ? (Revision *) l->data : NULL;
}


static GtkWidget *create_compare_win (LogHistory *log_history)
{
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkWidget *cat;
    GtkWidget *radio;
    GtkWidget *combo;
    Revision *prev_rev;

    dialog = gnome_cmd_dialog_new ("Compare");
    gtk_widget_ref (dialog);

    prev_rev = find_prev_rev (log_history, log_history->plugin->selected_rev);
    if (prev_rev)
        gtk_object_set_data (GTK_OBJECT (dialog), "prev_rev", prev_rev->number);

    gtk_object_set_data (GTK_OBJECT (dialog), "selected_rev", log_history->plugin->selected_rev->number);
    gtk_object_set_data (GTK_OBJECT (dialog), "log_history", log_history);


    /**
     * Compare with
     */
    vbox = create_vbox (dialog, FALSE, 6);
    cat = create_category (dialog, vbox, _("Compare with"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), cat);

    radio = create_radio (dialog, NULL, _("HEAD"), "head_radio");
    gtk_box_pack_start (GTK_BOX (vbox), radio, TRUE, FALSE, 0);

    radio = create_radio (dialog, get_radio_group(radio), _("The previous revision"), "prev_rev_radio");
    gtk_box_pack_start (GTK_BOX (vbox), radio, TRUE, FALSE, 0);
    if (!prev_rev)
        gtk_widget_set_sensitive (radio, FALSE);

    radio = create_radio (dialog, get_radio_group(radio), _("Other revision"), "other_rev_radio");
    gtk_box_pack_start (GTK_BOX (vbox), radio, TRUE, FALSE, 0);

    gtk_signal_connect (GTK_OBJECT (radio), "toggled", GTK_SIGNAL_FUNC (on_other_rev_toggled), dialog);

    combo = create_combo (dialog);
    gtk_object_set_data_full (GTK_OBJECT (dialog), "rev_combo", combo, (GtkDestroyNotify) gtk_widget_unref);
    gtk_box_pack_start (GTK_BOX (vbox), combo, TRUE, FALSE, 0);
    gtk_widget_set_sensitive (combo, FALSE);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL,
                                 GTK_SIGNAL_FUNC (on_compare_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK,
                                 GTK_SIGNAL_FUNC (on_compare_ok), dialog);

    return dialog;
}


static void on_close_tab (GtkButton *button, GtkWidget *tab)
{
    gtk_widget_destroy (tab);
}


static void on_diff_window_close (GtkButton *button, CvsPlugin *plugin)
{
    gtk_widget_destroy (plugin->diff_win);
    plugin->diff_win = NULL;
}


static void on_log_window_close (GtkButton *button, CvsPlugin *plugin)
{
    gtk_widget_destroy (plugin->log_win);
    plugin->log_win = NULL;
}


static gboolean on_log_win_delete (GtkWidget *widget, GdkEvent *event, CvsPlugin *plugin)
{
    gtk_widget_destroy (widget);
    plugin->log_win = NULL;
    return FALSE;
}


static gboolean on_diff_win_delete (GtkWidget *widget, GdkEvent *event, CvsPlugin *plugin)
{
    gtk_widget_destroy (widget);
    plugin->diff_win = NULL;
    return FALSE;
}


static gboolean on_log_win_destroy (GtkWidget *widget, GdkEvent *event, CvsPlugin *plugin)
{
    plugin->log_win = NULL;
    return FALSE;
}


static gboolean on_diff_win_destroy (GtkWidget *widget, GdkEvent *event, CvsPlugin *plugin)
{
    plugin->diff_win = NULL;
    return FALSE;
}


GtkWidget *create_diff_win (CvsPlugin *plugin)
{
    GtkWidget *dialog;
    GtkWidget *notebook;

    dialog = gnome_cmd_dialog_new (_("CVS Diff"));
    gtk_widget_ref (dialog);
    gnome_cmd_dialog_set_resizable (GNOME_CMD_DIALOG (dialog), TRUE);
    gtk_window_set_default_size (GTK_WINDOW (dialog), 510, 300);

    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), GTK_STOCK_CLOSE,
        GTK_SIGNAL_FUNC (on_diff_window_close), plugin);

    gtk_signal_connect (GTK_OBJECT (dialog), "delete-event", GTK_SIGNAL_FUNC (on_diff_win_delete), plugin);
    gtk_signal_connect (GTK_OBJECT (dialog), "destroy-event", GTK_SIGNAL_FUNC (on_diff_win_destroy), plugin);

    notebook = gtk_notebook_new ();
    gtk_widget_ref (notebook);
    gtk_object_set_data_full (GTK_OBJECT (dialog), "notebook", notebook, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (notebook);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), notebook);

    gtk_widget_show (dialog);

    return dialog;
}


GtkWidget *create_log_win (CvsPlugin *plugin)
{
    GtkWidget *dialog;
    GtkWidget *notebook;

    dialog = gnome_cmd_dialog_new ("CVS Log");
    gtk_widget_ref (dialog);
    gnome_cmd_dialog_set_resizable (GNOME_CMD_DIALOG (dialog), TRUE);
    gtk_window_set_default_size (GTK_WINDOW (dialog), 510, 300);
    gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);

    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog), GTK_STOCK_CLOSE,
        GTK_SIGNAL_FUNC (on_log_window_close), plugin);

    gtk_signal_connect (GTK_OBJECT (dialog), "delete-event",
                        GTK_SIGNAL_FUNC (on_log_win_delete), plugin);
    gtk_signal_connect (GTK_OBJECT (dialog), "destroy-event",
                        GTK_SIGNAL_FUNC (on_log_win_destroy), plugin);

    notebook = gtk_notebook_new ();
    gtk_widget_ref (notebook);
    gtk_object_set_data_full (GTK_OBJECT (dialog), "notebook", notebook, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (notebook);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), notebook);

    gtk_widget_show (dialog);

    return dialog;
}


static GtkWidget *create_tab_label (GtkWidget *parent,
                                    const gchar *label,
                                    GtkSignalFunc on_close,
                                    gpointer user_data)
{
    GtkWidget *hbox;
    GtkWidget *btn;
    GtkWidget *lbl;
    GtkWidget *img;
    GdkPixbuf *pb;

    hbox = create_hbox (parent, FALSE, 3);

    pb = gdk_pixbuf_new_from_xpm_data ((const gchar**)close_xpm);
    img = gtk_image_new_from_pixbuf (pb);
    gdk_pixbuf_unref (pb);
    gtk_widget_show (img);
    btn = gtk_button_new ();
    gtk_button_set_relief (GTK_BUTTON (btn), GTK_RELIEF_NONE);
    gtk_widget_show (btn);
    gtk_container_add (GTK_CONTAINER (btn), img);
    gtk_signal_connect (GTK_OBJECT (btn), "clicked", on_close, user_data);

    lbl = create_label (hbox, label);

    gtk_box_pack_start (GTK_BOX (hbox), lbl, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);

    return hbox;
}


void add_diff_tab (CvsPlugin *plugin, const gchar *cmd, const gchar *fname)
{
    gint ret;
    FILE *fd;
    gchar buf[256];
    GtkWidget *sw;
    GtkWidget *text_view;
    GtkTextBuffer *text_buffer = gtk_text_buffer_new (NULL);
    GtkWidget *tab_label;
    GtkWidget *notebook;

    sw = create_sw (plugin->diff_win);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

    text_view = gtk_text_view_new ();
    gtk_container_add (GTK_CONTAINER (sw), text_view);
    gtk_widget_ref (text_view);
    gtk_object_set_data_full (GTK_OBJECT (sw), "text_view", text_view, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (text_view);

    notebook = lookup_widget (plugin->diff_win, "notebook");

    tab_label = create_tab_label (notebook, fname, GTK_SIGNAL_FUNC (on_close_tab), sw);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, tab_label);


    fd = popen (cmd, "r");
    if (fd==0) return;

    do
    {
        ret = fread (buf, 1, 256, fd);
        gtk_text_buffer_insert_at_cursor (text_buffer, buf, ret);
    }
    while (ret == 256);

    gtk_text_view_set_buffer (GTK_TEXT_VIEW (text_view), text_buffer);
    pclose (fd);
}


void add_log_tab (CvsPlugin *plugin, const gchar *fname)
{
    GList *revs;
    GtkWidget *hpaned;
    GtkWidget *rev_list;
    GtkWidget *vbox2;
    GtkWidget *hbox;
    GtkWidget *table1;
    GtkWidget *label;
    GtkWidget *sw;
    GtkWidget *msg_text;
    GtkWidget *button;
    GtkWidget *notebook;
    GtkWidget *tab_label;
    LogHistory *log_history;

    log_history = log_create (plugin, fname);
    if (!log_history) return;

    hpaned = gtk_hpaned_new ();
    gtk_container_set_border_width (GTK_CONTAINER (hpaned), 6);
    gtk_widget_ref (hpaned);
    gtk_object_set_data_full (GTK_OBJECT (plugin->log_win), "hpaned", hpaned,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_object_set_data_full (GTK_OBJECT (hpaned), "log_history", log_history,
                              (GtkDestroyNotify) log_free);
    gtk_widget_show (hpaned);
    gtk_paned_set_position (GTK_PANED (hpaned), 100);

    rev_list = create_clist (hpaned, "revision_list", 1, 16, NULL, NULL);
    create_clist_column (rev_list, 0, 80, _("revision"));
    gtk_paned_pack1 (GTK_PANED (hpaned), rev_list, FALSE, TRUE);
    rev_list = lookup_widget (rev_list, "revision_list");
    gtk_clist_column_titles_hide (GTK_CLIST (rev_list));

    vbox2 = create_vbox (hpaned, FALSE, 0);
    gtk_paned_pack2 (GTK_PANED (hpaned), vbox2, TRUE, TRUE);
    gtk_container_set_border_width (GTK_CONTAINER (vbox2), 6);

    table1 = create_table (hpaned, 6, 3);
    gtk_box_pack_start (GTK_BOX (vbox2), table1, TRUE, TRUE, 0);
    gtk_table_set_row_spacings (GTK_TABLE (table1), 12);
    gtk_table_set_col_spacings (GTK_TABLE (table1), 6);

    label = create_bold_label (hpaned, _("Revision:"));
    gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 0, 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    hbox = create_hbox (hpaned, FALSE, 6);
    gtk_table_attach (GTK_TABLE (table1), hbox, 1, 2, 0, 1,
                      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
                      (GtkAttachOptions) (0), 0, 0);

    label = create_label (hpaned, "...");
    log_history->rev_label = label;
    gtk_object_set_data (GTK_OBJECT (hpaned), "rev_label", label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    button = create_button_with_data (
        plugin->log_win, _("Compare..."), GTK_SIGNAL_FUNC (on_compare_clicked), log_history);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);

    label = create_bold_label (hpaned, _("Author:"));
    gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 1, 2,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = create_label (hpaned, "...");
    log_history->author_label = label;
    gtk_object_set_data (GTK_OBJECT (hpaned), "author_label", label);
    gtk_table_attach (GTK_TABLE (table1), label, 1, 2, 1, 2,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = create_bold_label (hpaned, _("Date:"));
    gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 2, 3,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = create_label (hpaned, "...");
    log_history->date_label = label;
    gtk_object_set_data (GTK_OBJECT (hpaned), "date_label", label);
    gtk_table_attach (GTK_TABLE (table1), label, 1, 2, 2, 3,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = create_bold_label (hpaned, _("State:"));
    gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 3, 4,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = create_label (hpaned, "...");
    log_history->state_label = label;
    gtk_object_set_data (GTK_OBJECT (hpaned), "state_label", label);
    gtk_table_attach (GTK_TABLE (table1), label, 1, 2, 3, 4,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = create_bold_label (hpaned, _("Lines:"));
    gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 4, 5,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = create_label (hpaned, "...");
    log_history->lines_label = label;
    gtk_object_set_data (GTK_OBJECT (hpaned), "lines_label", label);
    gtk_table_attach (GTK_TABLE (table1), label, 1, 2, 4, 5,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = create_bold_label (hpaned, _("Message:"));
    gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 5, 6,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0);

    sw = create_sw (hpaned);
    gtk_scrolled_window_set_shadow_type (
        GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
    gtk_table_attach (GTK_TABLE (table1), sw, 1, 3, 5, 6,
                      (GtkAttachOptions) (GTK_EXPAND|GTK_FILL),
                      (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), 0, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    msg_text = gtk_text_view_new ();
    log_history->msg_text_view = msg_text;
    gtk_widget_ref (msg_text);
    gtk_object_set_data_full (GTK_OBJECT (hpaned), "msg_text", msg_text,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (msg_text);
    gtk_container_add (GTK_CONTAINER (sw), msg_text);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (msg_text), GTK_WRAP_WORD);

    notebook = lookup_widget (plugin->log_win, "notebook");

    tab_label = create_tab_label (notebook, fname, GTK_SIGNAL_FUNC (on_close_tab), hpaned);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), hpaned, tab_label);

    for (revs = log_history->revisions; revs; revs = revs->next)
    {
        Revision *rev = (Revision *)revs->data;
        gint row;
        gchar *text[2];
        text[0] = rev->number;
        text[1] = NULL;
        // Add this rev. to the list
        row = gtk_clist_append (GTK_CLIST (rev_list), text);
        gtk_clist_set_row_data (GTK_CLIST (rev_list), row, rev);
    }

    gtk_signal_connect (GTK_OBJECT (rev_list), "select-row", GTK_SIGNAL_FUNC (on_rev_list_select_row), log_history);

    gtk_clist_select_row (GTK_CLIST (rev_list), 0, 0);
}
