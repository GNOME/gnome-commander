GtkWidget *create_log_win  (CvsPlugin *plugin);
GtkWidget *create_diff_win (CvsPlugin *plugin);

void add_log_tab (CvsPlugin *plugin, const gchar *fname);
void add_diff_tab (CvsPlugin *plugin, const gchar *command, const gchar *fname);
