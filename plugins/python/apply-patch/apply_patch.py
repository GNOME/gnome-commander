#       apply_patch python plugin
#       Copyright (C) 2012 Piotr Eljasiak
#
#    Part of
#       GNOME Commander - A GNOME based file manager
#       Copyright (C) 2001-2006 Marcus Bjurman
#       Copyright (C) 2007-2012 Piotr Eljasiak
#       Copyright (C) 2013-2016 Uwe Scholz
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.

# Description:
#   the plugin applies selected patches to the opposite pane using -p1 param

try:
    import gnomevfs
except ImportError:
    import gnome.vfs as gnomevfs

import os
import urllib


def main(main_wnd_xid, active_cwd, inactive_cwd, selected_files):
    for uri in selected_files:
        if gnomevfs.get_file_info(uri,gnomevfs.FILE_INFO_GET_MIME_TYPE|gnomevfs.FILE_INFO_FORCE_FAST_MIME_TYPE).mime_type=='text/x-patch':
            cmd = "patch -p1 --directory='%s' --input='%s' --quiet" % (inactive_cwd, urllib.unquote(uri.path))
            os.system(cmd)

    return True
