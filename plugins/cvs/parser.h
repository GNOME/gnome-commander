#ifndef __PARSER_H__
#define __PARSER_H__

#include <gnome.h>

typedef struct {
    gchar *number;
    gchar *date;
    gchar *author;
    gchar *state;
    gchar *lines;
    gchar *message;
} Revision;

#include "cvs-plugin.h"

typedef struct {
    gchar *fname;
    GList *revisions;
    GList *rev_names;
    GHashTable *rev_map;
    CvsPlugin *plugin;

    GtkWidget *rev_label;
    GtkWidget *date_label;
    GtkWidget *author_label;
    GtkWidget *state_label;
    GtkWidget *lines_label;
    GtkWidget *msg_text_view;
} LogHistory;


LogHistory *log_create (CvsPlugin *plugin, const gchar *fpath);
void log_free (LogHistory *log);

#endif // __PARSER_H__
