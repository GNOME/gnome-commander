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

#ifndef __CAP_H__
#define __CAP_H__

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-dir.h"

void cap_cut_files (GnomeCmdFileList *fl, GList *files);
void cap_copy_files (GnomeCmdFileList *fl, GList *files);
void cap_paste_files (GnomeCmdDir *dir);

#endif // __CAP_H__
