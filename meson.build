project('gnome-commander', ['C', 'cpp', 'rust'],
  version: '1.19.0',
  license: 'GPLv2',
  meson_version: '>= 0.59',
  default_options: ['cpp_std=c++17', 'build.cpp_std=c++17']
)

# Importing modules
gnome = import('gnome')
i18n = import('i18n')

# Constants
package_url = 'https://gcmd.github.io/'
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

c = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

common_ccflags = [
  '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
  '-DGTK_DISABLE_SINGLE_INCLUDES',
  # '-DGDK_DISABLE_DEPRECATED',
  # '-DGTK_DISABLE_DEPRECATED',
]
test_cflags = [
  '-Wno-deprecated-declarations',
  '-Wformat=2',
  '-Wmissing-declarations',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Wstrict-prototypes'
]

common_ccflags += cpp.get_supported_arguments (test_cflags)

use_no_strict_aliasing = cpp.has_argument('-fno-strict-aliasing')
if use_no_strict_aliasing
  common_ccflags += ['-fno-strict-aliasing']
endif

add_project_arguments(common_ccflags, language: 'cpp')

plugin_dir = libdir / meson.project_name() / 'plugins'
pixmaps_dir = datadir / 'pixmaps' / meson.project_name()
icons_dir = datadir / meson.project_name() / 'icons'

conf_data = configuration_data()

conf_data.set_quoted('DATADIR',           '@0@'.format(datadir), description : '')
conf_data.set_quoted('PACKAGE',           '@0@'.format(meson.project_name()), description : 'Name of package')
conf_data.set_quoted('PACKAGE_NAME',      '@0@'.format(meson.project_name()), description : 'Define to the full name of this package.')
conf_data.set_quoted('PACKAGE_URL',       '@0@'.format(package_url), description : 'Define to the home page for this package.')
conf_data.set_quoted('PACKAGE_BUGREPORT', '@0@'.format(package_bugreport), description : 'Define to the address where bug reports for this package should be sent.')
conf_data.set_quoted('PIXMAPS_DIR',       '@0@'.format(pixmaps_dir), description : 'pixmaps location')
conf_data.set_quoted('ICONS_DIR',         '@0@'.format(icons_dir), description : 'icons location')
conf_data.set_quoted('PLUGIN_DIR',        '@0@'.format(plugin_dir), description : 'location of plugins')
conf_data.set_quoted('PREFIX',            '@0@'.format(prefix), description : 'definition of PREFIX')
conf_data.set_quoted('PACKAGE_VERSION',   '@0@'.format(meson.project_version()), description : 'Define to the version of this package.')

# Compulsory dependencies
GLIB_REQ = '2.78.0'
conf_data.set('GLIB_REQ', GLIB_REQ, description: 'Required version of glib library')
GMODULE_REQ = '2.0.0'
GTK_REQ = '4.12.0'
conf_data.set('GTK_REQ', GTK_REQ, description: 'Required version of gtk library')
GIO_REQ = '2.0'

gobject   = dependency('gobject-2.0', version: '>=' + GLIB_REQ)
glib      = dependency('glib-2.0', version: '>=' + GLIB_REQ)
gmodule   = dependency('gmodule-2.0', version: '>=' + GMODULE_REQ)
gio       = dependency('gio-2.0', version: '>=' + GIO_REQ)
gtk       = dependency('gtk4', version: '>=' + GTK_REQ)
pixbuf    = dependency('gdk-pixbuf-2.0')

common_deps = [
  glib, gmodule, gio, gobject, gtk
]

# Require that gdk-pixbuf can load xpm files
pixbuf_module_dir = pixbuf.get_variable(pkgconfig: 'gdk_pixbuf_moduledir')
cpp.find_library('pixbufloader-xpm', dirs: pixbuf_module_dir, required: true)

##### Optional dependencies #####
EXIV2_REQ = '0.14'
conf_data.set('EXIV2_REQ', EXIV2_REQ, description: 'Required version of exiv2 library')
TAGLIB_REQ = '1.4'
conf_data.set('TAGLIB_REQ', TAGLIB_REQ, description: 'Required version of poppler library')
LIBGSF_REQ = '1.14.46'
conf_data.set('LIBGSF_REQ', LIBGSF_REQ, description: 'Required version of gsf library')

# Check for exiv2 support
exiv2 = dependency('exiv2', version: '>=' + EXIV2_REQ, required: get_option('exiv2'))

# Check for libgsf
gsf = dependency('libgsf-1', version: '>=' + LIBGSF_REQ, required: get_option('libgsf'))

# Check for taglib
taglib = dependency('taglib', version: '>=' + TAGLIB_REQ, required: get_option('taglib'))

# Check for poppler
poppler = dependency('poppler-glib', required: get_option('poppler'))

# Check for gettext
intl_dep = dependency('intl', required: false)
if intl_dep.found()
  conf_data.set('ENABLE_NLS', 1, description : 'Is translation of program messages to the user\'s native language requested.')
endif

#### Check for mmap #####
if cpp.has_function('mmap', prefix : '#include<sys/mman.h>')
  conf_data.set('HAVE_MMAP', 1, description: 'Define to 1 if you have a working `mmap\' system call.')
endif

configure_file(output : 'config.h', configuration : conf_data)

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
    'iconsdir': icons_dir,
    'pixmapsdir': pixmaps_dir,
    'plugindir': plugin_dir
  },
  section: 'Directories'
)

summary(
  {
    'Generate doc': get_option('help'),
    'Use exiv2': exiv2.found(),
    'Use libgsf': gsf.found(),
    'Use poppler': poppler.found(),
    'Use taglib': taglib.found(),
  },
  section: 'User defined options'
)

configuration_inc = include_directories('.')
libgcmd_inc = include_directories('libgcmd')
sources_inc = include_directories('src', 'src/dialogs', 'src/intviewer', 'src/tags')

subdir('libgcmd')
subdir('src')
subdir('data')
subdir('pixmaps')
subdir('plugins')

if get_option('help')
  subdir('doc')
endif
if intl_dep.found()
  subdir('po')
endif

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true
)
