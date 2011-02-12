/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

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
#ifndef __GNOME_CMD_PATH_H__
#define __GNOME_CMD_PATH_H__

struct GnomeCmdPath
{
  protected:

    virtual GnomeCmdPath *do_clone() const = 0;

  public:

    GnomeCmdPath *clone() const                                 {  return do_clone();  }

    virtual const gchar *get_path() = 0;
    virtual const gchar *get_display_path() = 0;
    virtual GnomeCmdPath *get_parent() = 0;
    virtual GnomeCmdPath *get_child(const gchar *child) = 0;
};

#endif // __GNOME_CMD_PATH_H__
