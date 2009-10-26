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


%option noyywrap
%option nounput


%{
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include <vector>


#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-advrename-lexer.h"
#include "tags/gnome-cmd-tags.h"
#include "utils.h"

using namespace std;


#define   ECHO  echo(yytext, yyleng)


enum {TEXT=1,NAME,EXTENSION,FULL_NAME,COUNTER,XRANDOM,XXRANDOM,PARENT_DIR,GRANDPARENT_DIR,METATAG};

const int GLOBAL_COUNTER_STEP = -1;
const int GLOBAL_COUNTER_PREC = -1;

const int MAX_PRECISION = 16;
const int MAX_XRANDOM_PRECISION = 8;


struct CHUNK
{
  int type;
  union
  {
    GString *s;

    struct
    {
      int beg;          // default: 0
      int end;          // default: 0
      char *name;       // default: NULL
      GnomeCmdTag tag;  // default: TAG_NONE
      GList *opt;       // default: NULL
    } tag;

    struct
    {
      long n;
      long start;
      int step;
      int prec;
      int init_step;
      int init_prec;
      char fmt[9];
    } counter;

    struct
    {
      guint x_prec;     // default: MAX_XRANDOM_PRECISION  (8)
    } random;
  };
};


static vector<CHUNK *> fname_template;

static gboolean      fname_template_has_counters = FALSE;
static gboolean      fname_template_has_percent = FALSE;

#define    SUB_S "\x1A"
const char SUB = '\x1A';
const char ESC = '\x1B';


inline void echo(const gchar *s, gssize length)
{
  if (fname_template.empty() || fname_template.back()->type!=TEXT)
  {
    CHUNK *p = g_new0 (CHUNK, 1);
    p->type = TEXT;
    p->s = g_string_sized_new (16);
    g_string_append_len (p->s, s, length);
    fname_template.push_back(p);
  }
  else
    g_string_append_len (fname_template.back()->s, s, length);
}

%}

int        -?[0-9]+
uint       0*[1-9][0-9]*

range      {int}|{int}?:{int}?|{int},{uint}

ape         [aA][pP][eE]
audio       [aA][uU][dD][iI][oO]
chm         [cC][hH][mM]
doc         [dD][oO][cC]
exif        [eE][xX][iI][fF]
file        [fF][iI][lL][eE]
flac        [fF][lL][aA][cC]
icc         [iI][cC][cC]
id3         [iI][dD]3
image       [iI][mM][aA][gG][eE]
iptc        [iI][pP][tT][cC]
pdf         [pP][dD][fF]
rpm         [rR][pP][mM]
vorbis      [vV][oO][rR][bB][iI][sS]

tag_name    {ape}|{audio}|{doc}|{exif}|{file}|{flac}|{id3}|{image}|{iptc}|{pdf}|{vorbis}

%%


%{
  static int from, to;
%}


\$[egnNp]\({range}\)            {
                                  gchar **a = g_strsplit_set(yytext+3,":,()",0);

                                  CHUNK *p = g_new0 (CHUNK,1);

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

                                  fname_template.push_back(p);
                                }

\$[c]\({uint}\)                 {
                                  CHUNK *p = g_new0 (CHUNK,1);

                                  int precision = 1;

                                  sscanf(yytext+3,"%d",&precision);

                                  p->type = COUNTER;
                                  p->counter.n = p->counter.start = 1;      //  default counter value
                                  p->counter.init_step = GLOBAL_COUNTER_STEP;
                                  p->counter.step = 1;                      //  default counter step
                                  p->counter.prec = p->counter.init_prec = min (precision, MAX_PRECISION);
                                  sprintf(p->counter.fmt,"%%0%ili",p->counter.prec);

                                  fname_template.push_back(p);

                                  fname_template_has_counters = TRUE;
                                }

\$[xX]\({uint}\)                {
                                  CHUNK *p = g_new0 (CHUNK,1);

                                  int precision = MAX_XRANDOM_PRECISION;

                                  sscanf(yytext+3,"%d",&precision);

                                  switch (yytext[1])
                                  {
                                    case 'x' : p->type = XRANDOM;       break;
                                    case 'X' : p->type = XXRANDOM;      break;
                                  }
                                  p->random.x_prec = min (precision, MAX_XRANDOM_PRECISION);

                                  fname_template.push_back(p);
                                }

\$T\({tag_name}(\.[a-zA-Z][a-zA-Z0-9]+)+(,[^,)]+)*\)   {

                                  gchar **a = g_strsplit_set(yytext+3,",()",0);
                                  guint n = g_strv_length(a);                     // glib >= 2.6

                                  CHUNK *p = g_new0 (CHUNK,1);

                                  int i;

                                  p->type = METATAG;
                                  p->tag.name = g_strdup (a[0]);
                                  p->tag.tag = gcmd_tags_get_tag_by_name(a[0]);
                                  p->tag.opt = NULL;

                                  for (i=n-2; i>0; --i)
                                    p->tag.opt = g_list_prepend(p->tag.opt, (gpointer) g_string_new(a[i]));

                                  g_strfreev(a);

                                  fname_template.push_back(p);
                                }

\$[cxXegnNp]\([^\)]*\)?         ECHO;                                      // don't substitute broken $x tokens like $x(-1), $x(abc) or $x(abc

\$[egnNp]                       {
                                  CHUNK *p = g_new0 (CHUNK,1);

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

                                  fname_template.push_back(p);
                                }

\$[c]                           {
                                  CHUNK *p = g_new0 (CHUNK,1);

                                  p->type = COUNTER;
                                  p->counter.n = p->counter.start = 1;      //  default counter value
                                  p->counter.init_step = GLOBAL_COUNTER_STEP;
                                  p->counter.step = 1;                      //  default counter step
                                  p->counter.init_prec = GLOBAL_COUNTER_PREC;
                                  p->counter.prec = 1;                      //  default counter prec]
                                  sprintf(p->counter.fmt,"%%0%ili",p->counter.prec);

                                  fname_template.push_back(p);

                                  fname_template_has_counters = TRUE;
                                }

\$[xX]                          {
                                  CHUNK *p = g_new0 (CHUNK,1);

                                  switch (yytext[1])
                                  {
                                    case 'x' : p->type = XRANDOM;       break;
                                    case 'X' : p->type = XXRANDOM;      break;
                                  }
                                  p->random.x_prec = MAX_XRANDOM_PRECISION;

                                  fname_template.push_back(p);
                                }

\$\$                            echo("$",1);

%[Dnt]                          {                                          // substitute %[Dnt] with %%[Dnt]
                                  echo(SUB_S SUB_S,2);
                                  fname_template_has_percent = TRUE;
                                }

%.                              {
                                  *yytext = SUB;
                                  ECHO;
                                  fname_template_has_percent = TRUE;
                                }

[^%$]+                          ECHO;                                      // concatenate consecutive non-[%$] chars into single TEXT chunk
%%


void gnome_cmd_advrename_reset_counter(long start, int precision, int step)
{
  precision = precision<MAX_PRECISION ? precision : MAX_PRECISION;

  for (vector<CHUNK *>::iterator i=fname_template.begin(); i!=fname_template.end(); ++i)
    if ((*i)->type==COUNTER)
    {
      (*i)->counter.n = (*i)->counter.start = start;
      (*i)->counter.step = (*i)->counter.init_step==GLOBAL_COUNTER_STEP ? step : (*i)->counter.init_step;
      (*i)->counter.prec = (*i)->counter.init_step==GLOBAL_COUNTER_PREC ? precision : (*i)->counter.init_prec;
      sprintf((*i)->counter.fmt,"%%0%ili",(*i)->counter.prec);
    }
}


void gnome_cmd_advrename_parse_template(const char *template_string, gboolean &has_counters)
{
  for (vector<CHUNK *>::iterator i=fname_template.begin(); i!=fname_template.end(); ++i)
    switch ((*i)->type)
    {
      case TEXT:
          g_string_free((*i)->s,TRUE);
          g_free(*i);
          break;

      case METATAG:
          // FIXME: free memory here for all (*i) members
          g_free((*i)->tag.name);
          g_free(*i);
          break;
    }

  fname_template.clear();
  fname_template_has_counters = FALSE;
  fname_template_has_percent = FALSE;

  yy_scan_string(template_string);
  yylex();
  yy_delete_buffer(YY_CURRENT_BUFFER);

  has_counters = fname_template_has_counters;
}


inline void mk_substr (int src_len, const CHUNK *p, int &pos, int &len)
{
  pos = p->tag.beg<0 ? p->tag.beg+src_len : p->tag.beg;
  pos = max(pos, 0);

  if (pos>=src_len)
  {
    pos = len = 0;
    return;
  }

  len = p->tag.end>0 ? p->tag.end-pos : src_len+p->tag.end-pos;
  len = CLAMP (len, 0, src_len-pos);
}


inline void append_utf8_chunk (string &s, const CHUNK *p, const char *path, int path_len)
{
  int from, length;

  mk_substr (path_len, p, from, length);

  if (!length)
    return;

  const char *beg = g_utf8_offset_to_pointer (path, from);
  const char *end = g_utf8_offset_to_pointer (beg, length);

  s.append(path, beg-path, end-beg);
}


inline void find_dirs (const gchar *path, const gchar *&parent_dir, const gchar *&grandparent_dir, int &parent_dir_len, int &grandparent_dir_len)
{
    const gchar *dir0 = "";

    grandparent_dir = parent_dir = dir0;

    int offset = 0;

    int offset0 = 0;
    int offset1 = 0;
    int offset2 = 0;

    for (const gchar *s = path; *s;)
    {
        gboolean sep = *s==G_DIR_SEPARATOR;

        s = g_utf8_next_char (s);
        ++offset;

        if (!sep)
            continue;

        grandparent_dir = parent_dir;
        parent_dir = dir0;
        dir0 = s;

        offset2 = offset1;
        offset1 = offset0;
        offset0 = offset;
    }

    parent_dir_len = max(offset0-offset1-1,0);
    grandparent_dir_len = max(offset1-offset2-1,0);
}


inline gboolean convert (char *s, char from, char to)
{
  gboolean retval = FALSE;

  for (; *s; ++s)
    if (*s==from)
    {
      *s = to;
      retval = TRUE;
    }

  return retval;
}


char *gnome_cmd_advrename_gen_fname (GnomeCmdFile *f, size_t new_fname_size)
{
  if (fname_template.empty())
    return g_strdup ("");

  string fmt;
  fmt.reserve(256);

  char *fname = get_utf8 (f->info->name);
  char *ext = g_utf8_strrchr (fname, -1, '.');

  int full_name_len = g_utf8_strlen (fname, -1);
  int name_len = full_name_len;
  int ext_len = 0;

  if (!ext)  ext = "";  else
  {
    ++ext;
    ext_len = g_utf8_strlen(ext, -1);
    name_len = full_name_len - ext_len - 1;
  }

  const char *parent_dir, *grandparent_dir;
  int parent_dir_len, grandparent_dir_len;

  find_dirs(gnome_cmd_file_get_path(f), parent_dir, grandparent_dir, parent_dir_len, grandparent_dir_len);

  for (vector<CHUNK *>::iterator i=fname_template.begin(); i!=fname_template.end(); ++i)
    switch ((*i)->type)
    {
      case TEXT  :
                    fmt += (*i)->s->str;
                    break;

      case NAME  :
                    append_utf8_chunk (fmt, *i, fname, name_len);
                    break;

      case EXTENSION:
                    append_utf8_chunk (fmt, *i, ext, ext_len);
                    break;

      case FULL_NAME:
                    append_utf8_chunk (fmt, *i, fname, full_name_len);
                    break;

      case PARENT_DIR:
                    append_utf8_chunk (fmt, *i, parent_dir, parent_dir_len);
                    break;

      case GRANDPARENT_DIR:
                    append_utf8_chunk (fmt, *i, grandparent_dir, grandparent_dir_len);
                    break;

      case COUNTER:
                    {
                      static char counter_value[MAX_PRECISION+1];

                      snprintf (counter_value, MAX_PRECISION+1, (*i)->counter.fmt, (*i)->counter.n);
                      fmt += counter_value;

                      (*i)->counter.n += (*i)->counter.step;
                    }
                    break;

      case XRANDOM:
      case XXRANDOM:
                    {
                      static char custom_counter_fmt[8];
                      static char random_value[MAX_XRANDOM_PRECISION+1];

                      sprintf (custom_counter_fmt, "%%0%u%c", (*i)->random.x_prec, (*i)->type==XRANDOM ? 'x' : 'X');
                      snprintf (random_value, MAX_XRANDOM_PRECISION+1, custom_counter_fmt, (*i)->random.x_prec<MAX_XRANDOM_PRECISION ? g_random_int_range (0,1 << 4*(*i)->random.x_prec)
                                                                                                                                     : g_random_int ());
                      fmt += random_value;
                    }
                    break;

      case METATAG: // currently ranges are NOT supported for $T() tokens !!!

                    // const gchar *tag_value = gcmd_tags_get_value (f,(*i)->tag.tag);

                    // if (tag_value)
                      // append_utf8_chunk (fmt, *i, tag_value, g_utf8_strlen (tag_value, -1));

                    fmt += gcmd_tags_get_value (f,(*i)->tag.tag);
                    break;

      default :     break;
    }

  g_free (fname);

  if (!fname_template_has_percent)
    return g_strdup (fmt.c_str());

  char *new_fname = g_new (char, new_fname_size+1);

  gboolean new_fname_has_percent = convert ((char *) fmt.c_str(), '%', ESC);
  convert ((char *) fmt.c_str(), SUB, '%');

  if (!strftime(new_fname, new_fname_size+1, fmt.c_str(), localtime(&f->info->mtime)))      // if new_fname is not big enough...
    new_fname[new_fname_size] = '\0';                                                       //      ... truncate

  if (new_fname_has_percent)
    convert (new_fname, ESC, '%');

  return new_fname;
}
