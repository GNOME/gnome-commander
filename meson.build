# SPDX-FileCopyrightText: 2022 Uwe Scholz
# SPDX-FileCopyrightText: 2024 Andrey Kutejko
# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

project('gnome-commander', ['rust'],
  version: '2.1.0',
  license: 'GPLv3',
  meson_version: '>= 0.59',
)

# Importing modules
gnome = import('gnome')
i18n = import('i18n')

# Constants
package_url = 'https://gnome.pages.gitlab.gnome.org/gnome-commander/'
package_bugreport = 'https://gitlab.gnome.org/GNOME/gnome-commander/issues'
app_id = 'org.gnome.gnome-commander'

prefix = get_option('prefix')

bindir      = join_paths(prefix, get_option('bindir'))
libdir      = join_paths(prefix, get_option('libdir'))
datadir     = join_paths(prefix, get_option('datadir'))
pkglibdir   = join_paths(libdir, meson.project_name())
pkgdatadir  = join_paths(datadir, meson.project_name())
desktopdir  = join_paths(datadir, 'applications')
metainfodir = join_paths(datadir, 'metainfo')
schemadir   = join_paths(datadir, 'glib-2.0', 'schemas')

# Summary before build starts
summary('prefix', get_option('prefix'))
summary('bindir', get_option('bindir'))
summary('libdir', get_option('libdir'))
summary('datadir', get_option('datadir'))
summary('buildtype', get_option('buildtype'), section: 'Build Configuration')
summary('debug', get_option('debug'), section: 'Build Configuration')

plugin_dir = libdir / meson.project_name() / 'plugins'

conf_data = configuration_data()

conf_data.set_quoted('DATADIR',           '@0@'.format(datadir), description : '')
conf_data.set_quoted('PACKAGE',           '@0@'.format(meson.project_name()), description : 'Name of package')
conf_data.set_quoted('PACKAGE_NAME',      '@0@'.format(meson.project_name()), description : 'Define to the full name of this package.')
conf_data.set_quoted('PACKAGE_URL',       '@0@'.format(package_url), description : 'Define to the home page for this package.')
conf_data.set_quoted('PACKAGE_BUGREPORT', '@0@'.format(package_bugreport), description : 'Define to the address where bug reports for this package should be sent.')
conf_data.set_quoted('PLUGIN_DIR',        '@0@'.format(plugin_dir), description : 'location of plugins')
conf_data.set_quoted('PREFIX',            '@0@'.format(prefix), description : 'definition of PREFIX')
conf_data.set_quoted('PACKAGE_VERSION',   '@0@'.format(meson.project_version()), description : 'Define to the version of this package.')

meson.add_dist_script(
  find_program('check-news.sh').full_path(),
  '@0@'.format(meson.project_version()),
  'data/org.gnome.gnome-commander.metainfo.xml.in'
)

cargo = find_program('cargo', required: true)
cargo_version = run_command(cargo, '--version', check: true).stdout().strip()
message(cargo_version)
rustc_version = run_command('rustc', '--version', check: true).stdout().strip()
message(rustc_version)

if get_option('debug')
    profile = 'debug'
else
    profile = 'release'
endif

dependency('gi-docgen', native: true, required: false)

summary = [
  '',
  '-----------',
  'Gnome Commander @0@'.format(meson.project_version()),
  '-----------',
]

summary(
  {
    'bindir': bindir,
    'libdir': libdir,
    'datadir': datadir,
    'plugindir': plugin_dir
  },
  section: 'Directories'
)

summary(
  {
    'Generate doc': get_option('help'),
  },
  section: 'User defined options'
)

subdir('src')
subdir('data')
subdir('plugins')

if get_option('help')
  subdir('doc')
endif
subdir('po')

gnome.post_install(
  glib_compile_schemas: true,
  update_desktop_database: true
)
