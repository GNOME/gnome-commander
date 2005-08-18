/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2005 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/  


%option noyywrap
%option nounput


%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>                        // for NAME_MAX

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-advrename-lexer.h"
#include "utils.h"


#define   ECHO        if (fname_template_size>yyleng) { memcpy(s,yytext,yyleng); s+=yyleng; fname_template_size-=yyleng; }


#define   MAX_PRECISION   16

enum {NAME=1,EXTENSION,FULL_NAME,COUNTER,PARENT_DIR,GRANDPARENT_DIR};

static char fname_template[2*NAME_MAX];       // more than enough ;o)
static int  fname_template_size;

static unsigned long counter = 1;
static char          counter_fmt[8] = "%lu";
static unsigned      counter_step = 1;

%}

int        [0-9]+


%%


%{
  char *s = fname_template;
  int from, length;
  int precision;
%}


\$[egnNp]\(0+(,{int})?\)        ECHO;                                       // don't substitute $x(0,...) 

\$[c]\(0+\)                     ECHO;                                       // don't substitute $c(0) 

\$[egnNp]\({int}(,{int})?\)     {
                                  if (fname_template_size > 1+2*sizeof(int)) 
                                  {
                                    from = 1;
                                    length = -1;
                                  
                                    sscanf(yytext+3,"%d,%d",&from,&length);

                                    if (length!=0)                          // eliminate $x(...,0) entries
                                    {
                                      switch (yytext[1])
                                      {
                                        case 'e' : *s++ = EXTENSION;       break;
                                        case 'g' : *s++ = GRANDPARENT_DIR; break;
                                        case 'n' : *s++ = NAME;            break;
                                        case 'N' : *s++ = FULL_NAME;       break;
                                        case 'p' : *s++ = PARENT_DIR;      break;
                                      }
                                    
                                      *((int *) s) = --from; s += sizeof(int);
                                      *((int *) s) = length; s += sizeof(int);
                                    }

                                    fname_template_size -= sizeof(char)+2*sizeof(int);
                                  }
                                }

\$[c]\({int}\)                  {
                                  if (fname_template_size > 1+sizeof(int)) 
                                  {
				    precision = -1;
                                    sscanf(yytext+3,"%d",&precision);
                                    *s++ = COUNTER;
                                    *((int *) s) = precision<MAX_PRECISION ? precision : MAX_PRECISION; s += sizeof(int);
                                    fname_template_size -= sizeof(char)+sizeof(int);
                                  }
                                }

\$[cegnNp]\([^\)]*\)?        ECHO;                                      // don't substitute broken $x tokens like $x(-1), $x(abc) or $x(abc

\$[egnNp]                       {
                                  if (fname_template_size > 1+2*sizeof(int)) 
                                  {
                                    switch (yytext[1])
                                    {
                                      case 'e' : *s++ = EXTENSION;       break;
                                      case 'g' : *s++ = GRANDPARENT_DIR; break;
                                      case 'n' : *s++ = NAME;            break;
                                      case 'N' : *s++ = FULL_NAME;       break;
                                      case 'p' : *s++ = PARENT_DIR;      break;
                                    }
                                    
                                    *((int *) s) = 0; s += sizeof(int);
                                    *((int *) s) = -1; s += sizeof(int);

                                    fname_template_size -= sizeof(char)+2*sizeof(int);
                                  }
                                }

\$[c]                           {
                                  if (fname_template_size>1+sizeof(int))
                                  {
                                    *s++ = COUNTER;
                                    *((int *) s) = -1; s += sizeof(int);
                                    fname_template_size -= sizeof(char)+sizeof(int);
                                  }
                                }

\$\$                            {
                                  if (fname_template_size>1)
                                  {
                                    *s++ = '$';
                                    --fname_template_size;
                                  }
                                }

%[Dnt]                          {
                                  if (fname_template_size>2)
                                  {
                                    strcpy(s,"%%");
                                    s += 2;
                                    fname_template_size -= 2;
                                  }
                                }
%%


void gnome_cmd_advrename_reset_counter(unsigned start, unsigned precision, unsigned step)
{
  counter = start;
  counter_step = step;
  sprintf(counter_fmt,"%%0%ulu",(precision<MAX_PRECISION ? precision : MAX_PRECISION));
}


void gnome_cmd_advrename_parse_fname(const char *fname)
{
  fname_template_size = sizeof(fname_template);
  memset(fname_template,0,sizeof(fname_template));
  yy_scan_string(fname);
  yylex();
  yy_delete_buffer(YY_CURRENT_BUFFER);
}


static void mksubstr(size_t src_len, int *pos, int *len)
{
  if (*pos<0)  *pos = 0;
  
  if (*pos>=src_len)
  {
    *pos = *len = 0;
    return;
  }
  
  if (*len==-1 || *len>src_len-*pos)
  {
    *len = src_len-*pos;
    return;
  }
}


static void find_parent_dir(const char *path, int *offset, int *len)
{
  char *slash = g_utf8_strrchr(path, -1, '/');
  char *s = slash;
  
  *offset = *len = 0;
  
  if (!slash)  return;
  
  while (s!=path)
    if (*--s=='/')
    {
      *offset = ++s - path;
      *len = slash - s;
      
      return;
    }
  
  *len = slash-path;
}


static void find_grandparent_dir(const char *path, int *offset, int *len)
{
  char *slash = g_utf8_strrchr(path, -1, '/');
  char *s;
  
  *offset = *len = 0;
  
  if (slash==path || !slash)  return;

  s = slash = g_utf8_strrchr(path, slash-path-1, '/');
  
  if (!slash)  return;

  while (s!=path)
    if (*--s=='/')
    {
      *offset = ++s - path;
      *len = slash - s;
      
      return;
    }
  
  *len = slash-path;
}


#define min(x,y)  ((x)<(y) ? (x) : (y))


static char *
skip_n_chars (gchar *in, gint num_chars)
{
  while (num_chars--)
    in = g_utf8_next_char (in);
  return in;
}

char *gnome_cmd_advrename_gen_fname(char *new_fname, size_t new_fname_size, GnomeCmdFile *finfo)
{
  char *fmt = malloc(NAME_MAX);
  
  char *src, *dest;
  char *s;
  char *fname;

  char custom_counter_fmt[8];

  int ext_len, ext_offset;
  int name_len, full_name_len;
  int parent_dir_len, parent_dir_offset;
  int grandparent_dir_len, grandparent_dir_offset;

  int from, length;
  int precision;

  *new_fname = '\0';

  if (!fmt)  return new_fname;

  fname = get_utf8 (finfo->info->name);

  memset(fmt,0,NAME_MAX);

  full_name_len = g_utf8_strlen(fname, -1);

  s = g_utf8_strrchr (fname, -1, '.');
  
  if (s)
  {
    name_len = s-fname;
    ext_offset = name_len+1;
    ext_len = g_utf8_strlen(s+1, -1);
  }
  else
  {
    name_len = full_name_len;
    ext_offset = 0;
    ext_len = 0;
  }
  
  find_parent_dir(gnome_cmd_file_get_path(finfo),&parent_dir_offset,&parent_dir_len);
  find_grandparent_dir(gnome_cmd_file_get_path(finfo),&grandparent_dir_offset,&grandparent_dir_len);
  
  src = fname_template;
  dest = fmt;

  while (*src)
    switch (*src)
    {
      case NAME:
               ++src;
               from = *((int *) src); src += sizeof(int);
	       length = *((int *) src); src += sizeof(int);
		       mksubstr(name_len,&from,&length);
		       if (length<=NAME_MAX-(dest-fmt))
		       {
                 g_utf8_strncpy(dest,fname+from,length);
                 //dest += length;
                 dest = skip_n_chars (dest, length);
		       }
		       break;

      case EXTENSION:
               ++src;
               from = *((int *) src); src += sizeof(int);
	       length = *((int *) src); src += sizeof(int);
		       mksubstr(ext_len,&from,&length);
		       if (length<=NAME_MAX-(dest-fmt))
		       {
		         g_utf8_strncpy(dest,fname+ext_offset+from,length);
                 dest += length;
		       }
		       break;

      case FULL_NAME:
               ++src;
               from = *((int *) src); src += sizeof(int);
	       length = *((int *) src); src += sizeof(int);
		       mksubstr(full_name_len,&from,&length);
		       if (length<=NAME_MAX-(dest-fmt))
		       {
		         g_utf8_strncpy(dest,fname+from,length);
                 //dest += length;
                 dest = skip_n_chars (dest, length);
		       }
		       break;

      case PARENT_DIR:
               ++src;
               from = *((int *) src); src += sizeof(int);
	       length = *((int *) src); src += sizeof(int);
		       mksubstr(parent_dir_len,&from,&length);
		       if (length<=NAME_MAX-(dest-fmt))
		       {
		         g_utf8_strncpy(dest,gnome_cmd_file_get_path(finfo)+parent_dir_offset+from,length);
                 dest += length;
		       }
		       break;

      case GRANDPARENT_DIR:
               ++src;
               from = *((int *) src); src += sizeof(int);
	       length = *((int *) src); src += sizeof(int);
	       mksubstr(grandparent_dir_len,&from,&length);
		       if (length<=NAME_MAX-(dest-fmt))
		       {
		         g_utf8_strncpy(dest,gnome_cmd_file_get_path(finfo)+grandparent_dir_offset+from,length);
                 dest += length;
		       }
		       break;

      case COUNTER:
               ++src;
               precision = *((int *) src); src += sizeof(int);
		       if (precision!=-1)
		         sprintf(custom_counter_fmt,"%%0%ilu",precision);
                       dest += snprintf(dest,NAME_MAX-(dest-fmt),(precision==-1 ? counter_fmt : custom_counter_fmt),counter);
		       break;
      
      default:
               *dest++ = *src++;
               break;
    }

  fmt[NAME_MAX-1] = '\0';

  counter += counter_step;
  
  strftime(new_fname,new_fname_size,fmt,localtime(&finfo->info->mtime));

  free(fmt);
  g_free(fname);

  return new_fname;
}


#undef min
