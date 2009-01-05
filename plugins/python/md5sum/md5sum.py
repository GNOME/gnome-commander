#       md5sum python plugin
#       Copyright (C) 2007-2009 Piotr Eljasiak
#
#    Part of
#       GNOME Commander - A GNOME based file manager
#       Copyright (C) 2001-2006 Marcus Bjurman
#       Copyright (C) 2007-2009 Piotr Eljasiak
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

try:
    import gnomevfs
except ImportError:
    import gnome.vfs as gnomevfs

import os
import string
import md5


def main(main_wnd_xid, active_cwd, inactive_cwd, selected_files):
    parent_dir = string.split(active_cwd, os.sep)[-1]
    if parent_dir=='':
        parent_dir = 'root'
    f_md5sum = file(inactive_cwd+os.sep+parent_dir+'.md5sum', 'w')
    for uri in selected_files:
        if gnomevfs.get_file_info(uri).type==gnomevfs.FILE_TYPE_REGULAR:
            f = file(active_cwd+os.sep+uri.short_name, 'rb')
            file_content = f.read()
            f.close()
            md5sum = md5.new(file_content).hexdigest()
            f_md5sum.write('%s  %s\n' % (md5sum, uri.short_name))
    f_md5sum.close()
    return True
