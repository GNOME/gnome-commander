# SPDX-FileCopyrightText: 2022 Uwe Scholz <u.scholz83@gmx.de>
# SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

plugindir = pkglibdir / 'plugins'

subdir('fileroller')
subdir('test')

plugin_files = [
  'document-metadata.py',
  'image-metadata.py',
  'multimedia-metadata.py',
  'plugin_helper.py',
]

install_data(plugin_files, install_dir: plugin_dir)
