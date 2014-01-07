/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2014 Uwe Scholz

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

#ifndef __GNOME_CMD_SMB_PATH_H__
#define __GNOME_CMD_SMB_PATH_H__

#include "gnome-cmd-path.h"

class GnomeCmdSmbPath: public GnomeCmdPath
{
    gchar *workgroup;
    gchar *resource;
    gchar *resource_path;
    gchar *path;
    gchar *display_path;

    void set_resources(const gchar *workgroup, const gchar *resource, const gchar *path);

  protected:

    virtual GnomeCmdSmbPath *do_clone() const       {  return new GnomeCmdSmbPath(*this);  }

  public:

    GnomeCmdSmbPath(const GnomeCmdSmbPath &thePath);
    GnomeCmdSmbPath(const gchar *workgroup, const gchar *resource, const gchar *path);
    GnomeCmdSmbPath(const gchar *path_str);
    virtual ~GnomeCmdSmbPath();

    virtual const gchar *get_path()                 {  return path;                   }
    virtual const gchar *get_display_path()         {  return display_path;           }
    virtual GnomeCmdPath *get_parent();
    virtual GnomeCmdPath *get_child(const gchar *child);
};

inline GnomeCmdSmbPath::GnomeCmdSmbPath(const GnomeCmdSmbPath &thePath)
{
    workgroup = g_strdup (thePath.workgroup);
    resource = g_strdup (thePath.resource);
    resource_path = g_strdup (thePath.resource_path);
    path = g_strdup (thePath.path);
    display_path = g_strdup (thePath.display_path);
}

inline GnomeCmdSmbPath::~GnomeCmdSmbPath()
{
    g_free (workgroup);
    g_free (resource);
    g_free (resource_path);
    g_free (path);
    g_free (display_path);
}

#endif // __GNOME_CMD_SMB_PATH_H__
