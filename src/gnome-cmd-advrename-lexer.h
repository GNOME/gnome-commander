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

#ifndef __GNOME_CMD_ADVRENAME_LEXER_H__
#define __GNOME_CMD_ADVRENAME_LEXER_H__

#include "gnome-cmd-file.h"

void gnome_cmd_advrename_reset_counter(long start=1, int precision=-1, int step=1);
void gnome_cmd_advrename_parse_template(const char *template_string, gboolean &has_counters);
char *gnome_cmd_advrename_gen_fname(GnomeCmdFile *f, size_t new_fname_size=NAME_MAX);

#endif // __GNOME_CMD_ADVRENAME_LEXER_H__
