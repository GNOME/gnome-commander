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
*/


%option noyywrap
%option nounput


%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-advrename-lexer.h"
#include "utils.h"


#define   ECHO  {                                                                     \
                  CHUNK *p = g_new0(CHUNK,1);                                         \
                                                                                      \
                  p->type = TEXT;                                                     \
                  p->s = g_string_new(yytext);                                        \
                  fname_template = g_list_append(fname_template, (gpointer) p);       \
                }


#define   MAX_PRECISION   16

enum {TEXT=1,NAME,EXTENSION,FULL_NAME,COUNTER,PARENT_DIR,GRANDPARENT_DIR,TAG_FILE,TAG_ID3,TAG_EXIF,TAG_IPTC};

typedef struct
{
  int type;
  union
  {
    GString *s;
    struct
    {
      int beg;          // default: 0
      int end;          // default: 0
      GString *name;    // default: NULL
      GList   *opt;     // default: NULL
    } tag;
    struct
    {
      unsigned long n;  // default: start
      int start;        // default: default_counter_start (1)
      int step;         // default: default_counter_step  (1)
      int prec;         // default: default_counter_prec (-1)
    } counter;
  };
} CHUNK;


static GList *fname_template = NULL;


static unsigned long default_counter_start = 1;
static unsigned      default_counter_step = 1;
static unsigned      default_counter_prec = -1;
static char          counter_fmt[8] = "%lu";

%}

int        -?[0-9]+
uint       0*[1-9][0-9]*

range      {int}|{int}?:{int}?|{int},{uint}

file       [fF][iI][lL][eE]
id3        [iI][dD]3
exif       [eE][xX][iI][fF]
iptc       [iI][pP][tT][cC]

tag_name   {file}|{id3}|{exif}|{iptc}

%%


%{
  static int from, to;
%}


\$T\({tag_name},[a-zA-Z][a-zA-Z0-9]*(,[^,)]+)*,0+(,{int})?\)   ECHO;        // don't substitute $T(file|id3|exif|iptc,tag,...,0,...)

\$[egnNp]\({range}\)            {
                                  gchar **a = g_strsplit_set(yytext+3,":,()",0);

                                  CHUNK *p = g_new0(CHUNK,1);

                                  switch (yytext[1])
                                  {
                                    case 'e' : p->type = EXTENSION;       break;
                                    case 'g' : p->type = GRANDPARENT_DIR; break;
                                    case 'n' : p->type = NAME;            break;
                                    case 'N' : p->type = FULL_NAME;       break;
                                    case 'p' : p->type = PARENT_DIR;      break;
                                  }

                                  from = to = 0;

                                  switch (g_strv_length(a))                 // glib >= 2.6
                                  {
                                      case 2:
                                          sscanf(a[0],"%d",&from);
                                          break;
                                      case 3:
                                          sscanf(a[0],"%d",&from);
                                          sscanf(a[1],"%d",&to);
                                          if (strchr(yytext+3,','))
                                              to = from<0 && to+from>0 ? 0 : from+to;
                                          break;
                                  }

                                  g_strfreev(a);

                                  p->tag.beg = from;
                                  p->tag.end = to;
                                  p->tag.name = NULL;
                                  p->tag.opt = NULL;

                                  fname_template = g_list_append(fname_template, (gpointer) p);
                                }

\$[c]\({uint}\)                 {
                                  CHUNK *p = g_new0(CHUNK,1);

                                  int precision = default_counter_prec;

                                  sscanf(yytext+3,"%d",&precision);

                                  p->type = COUNTER;
                                  p->counter.n = p->counter.start = default_counter_start;
                                  p->counter.step = default_counter_step;
                                  p->counter.prec = precision<MAX_PRECISION ? precision : MAX_PRECISION;

                                  fname_template = g_list_append(fname_template, (gpointer) p);
                                }

\$T\({tag_name},[a-zA-Z][a-zA-Z0-9]*(,[^,)]+)*(,{range})?\)   {

                                  gchar **a = g_strsplit_set(yytext+3,":,()",0);
                                  guint n = g_strv_length(a);                     // glib >= 2.6

                                  CHUNK *p = g_new0(CHUNK,1);

                                  int optind = 0;
                                  int i;

                                  from = to = 0;

                                  if (n>3)    // if there is any tag option ...
                                  {
                                    gchar *s = g_strjoinv(",",a+n-3);

                                    optind = sscanf(s,"%d,%d,",&from,&to);

                                    if (optind!=2)
                                    {
                                      from = 1;
                                      optind = sscanf(s,"%*[^,],%d,",&from);
                                    }

                                    g_free(s);
                                  }

                                  switch (yytext[4])
                                  {
                                    case 'i':
                                    case 'I': p->type = TAG_FILE; break;

                                    case 'd':
                                    case 'D': p->type = TAG_ID3;  break;

                                    case 'x':
                                    case 'X': p->type = TAG_EXIF; break;

                                    case 'p':
                                    case 'P': p->type = TAG_IPTC; break;

                                    default : break;
                                  }

                                  p->tag.beg = from;
                                  p->tag.end = to;
                                  p->tag.name = g_string_new(a[1]);
                                  p->tag.opt = NULL;

                                  for (i=n-optind; i>2; --i)
                                    p->tag.opt = g_list_prepend(p->tag.opt, (gpointer) g_string_new(a[i]));

                                  g_strfreev(a);

                                  fname_template = g_list_append(fname_template, (gpointer) p);
                                }

\$[cegnNp]\([^\)]*\)?           ECHO;                                      // don't substitute broken $x tokens like $x(-1), $x(abc) or $x(abc

\$[egnNp]                       {
                                  CHUNK *p = g_new0(CHUNK,1);

                                  switch (yytext[1])
                                  {
                                    case 'e' : p->type = EXTENSION;       break;
                                    case 'g' : p->type = GRANDPARENT_DIR; break;
                                    case 'n' : p->type = NAME;            break;
                                    case 'N' : p->type = FULL_NAME;       break;
                                    case 'p' : p->type = PARENT_DIR;      break;
                                  }

                                  p->tag.beg = 0;
                                  p->tag.end = 0;
                                  p->tag.name = NULL;
                                  p->tag.opt = NULL;

                                  fname_template = g_list_append(fname_template, (gpointer) p);
                                }

\$[c]                           {
                                  CHUNK *p = g_new0(CHUNK,1);

                                  p->type = COUNTER;
                                  p->counter.n = p->counter.start = default_counter_start;
                                  p->counter.step = default_counter_step;
                                  p->counter.prec = default_counter_prec;

                                  fname_template = g_list_append(fname_template, (gpointer) p);
                                }

\$\$                            {
                                  CHUNK *p = g_new0(CHUNK,1);

                                  p->type = TEXT;
                                  p->s = g_string_new("$");

                                  fname_template = g_list_append(fname_template, (gpointer) p);
                                }

%[Dnt]                          {
                                  CHUNK *p = g_new0(CHUNK,1);

                                  p->type = TEXT;
                                  p->s = g_string_new("%%");

                                  fname_template = g_list_append(fname_template, (gpointer) p);
                                }
%%


//  TODO:  since counters are to be indivual, it's necessary to provide mechanism for resetting/changing implicit parameters - $c

void gnome_cmd_advrename_reset_counter(unsigned start, unsigned precision, unsigned step)
{
  GList *gl = fname_template;

  for (; gl; gl=gl->next)
  {
    CHUNK *p = gl->data;

    if (p->type==COUNTER)
      p->counter.n = p->counter.start;
  }

  default_counter_start = start;
  default_counter_step = step;
  default_counter_prec = precision;
  sprintf(counter_fmt,"%%0%ulu",(precision<MAX_PRECISION ? precision : MAX_PRECISION));
}


void gnome_cmd_advrename_parse_fname(const char *fname)
{
  if (fname_template)               // delete fname_template if any
  {
    GList *gl = fname_template;

    for (; gl; gl=gl->next)
    {
      CHUNK *p = gl->data;

      switch (p->type)
      {
        case TEXT :  g_string_free(p->s,TRUE);
                     g_free(p);
                     break;
      }
    }

    g_list_free(fname_template);
  }

  fname_template = NULL;

  yy_scan_string(fname);
  yylex();
  yy_delete_buffer(YY_CURRENT_BUFFER);
}


static void mksubstr(int src_len, const CHUNK *p, int *pos, int *len)
{
  *pos = p->tag.beg<0 ? p->tag.beg+src_len : p->tag.beg;
  *pos = MAX(*pos, 0);

  if (*pos>=src_len)
  {
    *pos = *len = 0;
    return;
  }

  *len = p->tag.end>0 ? p->tag.end-*pos : src_len+p->tag.end-*pos;
  *len = CLAMP(*len, 0, src_len-*pos);
}


static void find_parent_dir(const char *path, int *offset, int *len)
{
  char *slash = g_utf8_strrchr(path, -1, G_DIR_SEPARATOR);
  char *s = slash;

  *offset = *len = 0;

  if (!slash)  return;

  while (s!=path)
    if (*--s==G_DIR_SEPARATOR)
    {
      *offset = ++s - path;
      *len = slash - s;

      return;
    }

  *len = slash-path;
}


static void find_grandparent_dir(const char *path, int *offset, int *len)
{
  char *slash = g_utf8_strrchr(path, -1, G_DIR_SEPARATOR);
  char *s;

  *offset = *len = 0;

  if (slash==path || !slash)  return;

  s = slash = g_utf8_strrchr(path, slash-path-1, G_DIR_SEPARATOR);

  if (!slash)  return;

  while (s!=path)
    if (*--s==G_DIR_SEPARATOR)
    {
      *offset = ++s - path;
      *len = slash - s;

      return;
    }

  *len = slash-path;
}


char *gnome_cmd_advrename_gen_fname(char *new_fname, size_t new_fname_size, GnomeCmdFile *finfo)
{
  char *fname = get_utf8(finfo->info->name);
  char *s = g_utf8_strrchr (fname, -1, '.');

  GString *fmt = g_string_sized_new(256);
  GList   *gl  = fname_template;

  int full_name_len = g_utf8_strlen(fname, -1);

  int name_len = full_name_len;
  int ext_len = 0;
  int ext_offset = 0;

  int parent_dir_len, parent_dir_offset;
  int grandparent_dir_len, grandparent_dir_offset;

  char custom_counter_fmt[8];

  int from = 0;
  int length = 0;

  *new_fname = '\0';

  if (s)
  {
    name_len = s-fname;
    ext_offset = name_len+1;
    ext_len = g_utf8_strlen(s+1, -1);
  }

  find_parent_dir(gnome_cmd_file_get_path(finfo),&parent_dir_offset,&parent_dir_len);
  find_grandparent_dir(gnome_cmd_file_get_path(finfo),&grandparent_dir_offset,&grandparent_dir_len);

  for (; gl; gl=gl->next)
  {
    CHUNK *p = gl->data;

    switch (p->type)
    {
      case TEXT  :
                    fmt = g_string_append(fmt,p->s->str);
                    break;

      case NAME  :
                    mksubstr(name_len,p,&from,&length);
                    fmt = g_string_append_len(fmt,fname+from,length);
                    break;

      case EXTENSION:
                    mksubstr(ext_len,p,&from,&length);
                    fmt = g_string_append_len(fmt,fname+ext_offset+from,length);
                    break;

      case FULL_NAME:
                    mksubstr(full_name_len,p,&from,&length);
                    fmt = g_string_append_len(fmt,fname+from,length);
                    break;

      case PARENT_DIR:
                    mksubstr(parent_dir_len,p,&from,&length);
                    fmt = g_string_append_len(fmt,gnome_cmd_file_get_path(finfo)+parent_dir_offset+from,length);
                    break;

      case GRANDPARENT_DIR:
                    mksubstr(grandparent_dir_len,p,&from,&length);
                    fmt = g_string_append_len(fmt,gnome_cmd_file_get_path(finfo)+grandparent_dir_offset+from,length);
                    break;

      case COUNTER:
                    if (p->counter.prec!=-1)
                      sprintf(custom_counter_fmt,"%%0%ilu",p->counter.prec);
                    g_string_append_printf(fmt,(p->counter.prec==-1 ? counter_fmt : custom_counter_fmt),p->counter.n);
                    p->counter.n += p->counter.step;
                    break;

      case TAG_FILE:
      case TAG_ID3:
      case TAG_EXIF:
      case TAG_IPTC:
                    mksubstr(p->tag.name->len,p,&from,&length);
                    fmt = g_string_append_len(fmt,p->tag.name->str+from,length);
                    break;

      default :     break;
    }
  }

  strftime(new_fname,new_fname_size,fmt->str,localtime(&finfo->info->mtime));

  g_free(fname);
  g_string_free(fmt,TRUE);

  return new_fname;
}
