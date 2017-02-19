/** 
 * @file gnome-cmd-plain-path.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifndef __GNOME_CMD_PLAIN_PATH_H__
#define __GNOME_CMD_PLAIN_PATH_H__

#include "gnome-cmd-path.h"

struct GnomeCmdPlainPathPrivate;


class GnomeCmdPlainPath: public GnomeCmdPath
{

    gchar *path;

  protected:

    virtual GnomeCmdPlainPath *do_clone() const {  return new GnomeCmdPlainPath(*this);  }

  public:

    GnomeCmdPlainPath(const GnomeCmdPlainPath &thePath);
    explicit GnomeCmdPlainPath(const gchar *path) {  this->path = g_strdup (path);  }
    virtual ~GnomeCmdPlainPath()                  {  g_free (path);                 }

    virtual const gchar *get_path()               {  return path;                   }
    virtual const gchar *get_display_path()       {  return path;                   }
    virtual GnomeCmdPath *get_parent();
    virtual GnomeCmdPath *get_child(const gchar *child);
};

inline GnomeCmdPlainPath::GnomeCmdPlainPath(const GnomeCmdPlainPath &thePath)
{
    path = g_strdup (thePath.path);
}

#endif // __GNOME_CMD_PLAIN_PATH_H__
