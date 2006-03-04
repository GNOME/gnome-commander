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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   Author: Assaf Gordon  <agordon88@gmail.com>
*/

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgviewer/libgviewer.h>
#include <libgviewer/gvtypes.h>
#include <libgviewer/fileops.h>

void usage()
{
    fprintf(stderr,"This program tests the file operations module in 'libgviewer'.\n\n" ) ;

    fprintf(stderr,"Usage: test-fileops filename [start_offset] [end_offset]\n" ) ;
    fprintf(stderr,"\t'filename' will be printed to STDOUT.\n" ) ;
    fprintf(stderr,"\t'start_offset' and 'end_offset' are optinal.\n\n" ) ;
    exit(0);
}

int main(int argc, char* argv[])
{
    ViewerFileOps *fops;
    offset_type start = 0 ;
    offset_type end = 0 ;
    offset_type current ;
    int value ;
    
    if (argc<=1)
        usage() ;
    
    fops = gv_fileops_new();
    
    if (gv_file_open(fops, argv[1])==-1) {
        fprintf(stderr,"Failed to open \"%s\"\n", argv[1]) ;
        goto error;
    }
    
    if (argc>=3)
        start = atol(argv[2]);    
    else
        start = 0 ;
    
    if (argc>=4)
        end = atol(argv[3]);    
    else
        end = gv_file_get_max_offset(fops);

    current = start ;
    while (current < end) {
        value = gv_file_get_byte(fops, current);
        current++ ;
        
        if (value==-1)
            break ;
        
        printf("%c",(unsigned char)value);
    }
    
error:    
    gv_file_free(fops);
    g_free(fops);
    return 0;
}
