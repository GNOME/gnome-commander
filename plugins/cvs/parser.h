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

typedef struct {
	gchar *fname;
	GList *revisions;
	GList *rev_names;
	GHashTable *rev_map;
} LogHistory;


LogHistory *log_create (const gchar *fpath);
void log_free (LogHistory *log);

#endif // __PARSER_H__
