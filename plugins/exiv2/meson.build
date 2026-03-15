# SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

sources = files(
    'exiv2-plugin.cc'
)

config_h = configure_file(output : 'config.h', configuration : conf_data)

exiv2plugin = shared_module('exiv2plugin',
    include_directories: [libgcmd_inc],
    link_with: libgcmd,
    dependencies: [glib, gio, gobject, gmodule, gtk, exiv2],
    install_dir: plugin_dir,
    install: true,
    sources: [sources, config_h]
)
