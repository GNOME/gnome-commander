#include <gnome.h>


void
on_print                               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_print_setup                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_rev_list_select_row                 (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_quit                                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_main_win_show                       (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_diff_dialog_diff                    (GtkButton       *button,
                                        CvsPlugin       *plugin);

void
on_diff_dialog_close                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_compare_revisions                   (GtkMenuItem     *menuitem,
                                        CvsPlugin       *plugin);

void
on_about                               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_wordwrap_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_linewrap_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_font_toggled                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
