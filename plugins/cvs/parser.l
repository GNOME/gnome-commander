/*

RCS file: /cvsroot/gcmd/gnome-commander/src/main.c,v
Working file: main.c
head: 1.14
branch:
locks: strict
access list:
symbolic names:
        DEV11: 1.14.0.2
        start: 1.1.1.1
        gnome-commander: 1.1.1
keyword substitution: kv
total revisions: 17;    selected revisions: 17
description:
----------------------------
revision 1.14
date: 2003/01/30 02:42:59;  author: opum;  state: Exp;  lines: +2 -0
branches:  1.14.2;
Added automatic download of remove files when opening them or viewing them

*/

%option noyywrap
%option nounput
%option prefix="parser"
%option outfile="parser.c"

%{
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>                        // for NAME_MAX
#include "parser.h"

static Revision *rev = NULL;
static Revision *rev_new ();
static LogHistory *log = NULL;
%}



%%

----\nrevision.*\n	{
	gchar tmp[256];	

    fprintf (stderr, "LEX: Creating revision\n");
	sscanf (yytext+14,"%s", tmp);

	rev = rev_new ();
	rev->number = g_strdup (tmp);
	log->revisions = g_list_append (log->revisions, rev);
	log->rev_names = g_list_append (log->rev_names, rev->number);
	g_hash_table_insert (log->rev_map, rev->number, rev);
}

date:.*$  {
	gchar d[16], t[16], a[64], s[32], l1[8], l2[8];

    fprintf (stderr, "LEX: Creating date\n");
    sscanf (yytext, "date: %s %s  author: %s  state: %s  lines: %s %s",
		d, t, a, s, l1, l2);
		
	g_strdelimit (t, ";", '\0');
	g_strdelimit (a, ";", '\0');
	g_strdelimit (s, ";", '\0');
		
	rev->date = g_strdup_printf ("%s %s", d, t);
	rev->author = g_strdup (a);
	rev->state = g_strdup (s);
	rev->lines = g_strdup_printf ("%s %s", l1, l2);
}

branches:[:alnum:]*;  {
	gchar tmp[256];	
	sscanf (yytext+9,"%s", tmp);
	printf ("BranchesTEST: %s\n", tmp);
}

^.*\n---   {
    fprintf (stderr, "LEX: Found message\n");
	if (rev)
		rev->message = g_strdup (yytext);
}


%%

static Revision *rev_new ()
{
	return g_new0 (Revision, 1);
}


static void rev_free (Revision *rev) 
{
	if (rev) {
		g_free (rev->number);
		g_free (rev->date);
		g_free (rev->author);
		g_free (rev->state);
		g_free (rev->lines);
		g_free (rev->message);
		g_free (rev);
	}
}


static LogHistory *log_new ()
{
	LogHistory *log = g_new (LogHistory, 1);
	log->revisions = NULL;
	log->rev_names = NULL;
	log->rev_map = g_hash_table_new (g_str_hash, g_str_equal);
	return log;
}


void log_free (LogHistory *log)
{
	GList *revs = log->revisions;
	while (revs) {
		Revision *rev = (Revision*)revs->data;
		rev_free (rev);
	}
}


LogHistory *log_create (const gchar *fpath)
{
	gchar *cmd;
	gchar *dir;
    //gchar tmp[16];

	dir = g_dirname (fpath);
	if (dir) {
		if (chdir (dir) != 0)
			return NULL;
		g_free (dir);
	}
	
	log = log_new ();	
	log->fname = g_strdup (g_basename (fpath));
	cmd = g_strdup_printf ("cvs log %s", log->fname);

	yyin = popen (cmd, "r");
	if (!yyin) return NULL;

    fprintf (stderr, "LEX: 1\n");
	yylex ();
    fprintf (stderr, "LEX: 2\n");
	
	pclose (yyin);
	
	return log;
}
