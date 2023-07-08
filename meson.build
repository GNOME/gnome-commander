project('gnome-commander', 'cpp',
  version: '1.16.2',
  license: 'GPLv2',
  meson_version: '>= 0.59'
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
appdatadir  = join_paths(datadir, 'appdata')
icondir     = join_paths(datadir, 'icons')
metainfodir = join_paths(datadir, 'metainfo')
schemadir   = join_paths(datadir, 'glib-2.0', 'schemas')

# Summary before build starts
summary('prefix', get_option('prefix'))
summary('bindir', get_option('bindir'))
summary('libdir', get_option('libdir'))
summary('datadir', get_option('datadir'))
summary('buildtype', get_option('buildtype'), section: 'Build Configuration')
summary('debug', get_option('debug'), section: 'Build Configuration')

glib_ver = 'GLIB_VERSION_2_66'

cpp = meson.get_compiler('cpp')

common_ccflags = [
  '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
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

conf_data = configuration_data()

conf_data.set_quoted('DATADIR',           '@0@'.format(datadir), description : '')
conf_data.set_quoted('PACKAGE',           '@0@'.format(meson.project_name()), description : 'Name of package')
conf_data.set_quoted('PACKAGE_NAME',      '@0@'.format(meson.project_name()), description : 'Define to the full name of this package.')
conf_data.set_quoted('PACKAGE_URL',       '@0@'.format(package_url), description : 'Define to the home page for this package.')
conf_data.set_quoted('PACKAGE_BUGREPORT', '@0@'.format(package_bugreport), description : 'Define to the address where bug reports for this package should be sent.')
conf_data.set_quoted('PIXMAPS_DIR',       '@0@'.format(pixmaps_dir), description : 'pixmaps location')
conf_data.set_quoted('PLUGIN_DIR',        '@0@'.format(plugin_dir), description : 'location of plugins')
conf_data.set_quoted('PREFIX',            '@0@'.format(prefix), description : 'definition of PREFIX')
conf_data.set_quoted('PACKAGE_VERSION',   '@0@'.format(meson.project_version()), description : 'Define to the version of this package.')

# Compulsory dependencies
GLIB_REQ = '2.66.0'
conf_data.set('GLIB_REQ', GLIB_REQ, description: 'Required version of glib library')
GMODULE_REQ = '2.0.0'
GTK_REQ = '2.24.0'
conf_data.set('GTK_REQ', GTK_REQ, description: 'Required version of gtk library')
GIO_REQ = '2.0'

gobject   = dependency('gobject-2.0', version: '>=' + GLIB_REQ)
glib      = dependency('glib-2.0', version: '>=' + GLIB_REQ)
glib_2_70 = dependency('glib-2.0', version: '>= 2.70.0', required: false)
gmodule   = dependency('gmodule-2.0', version: '>=' + GMODULE_REQ)
gio       = dependency('gio-2.0', version: '>=' + GIO_REQ)
gtk       = dependency('gtk+-2.0', version: '>=' + GTK_REQ)

common_deps = [
  glib, gmodule, gio, gobject, gtk
]

if glib_2_70.found()
  conf_data.set('GLIB_2_70', '@0@'.format(glib_2_70.found().to_int()), description : 'Define to 1 if you have glib >= 2.70')
endif

##### Optional dependencies #####
UNIQUE_REQ = '0.9.3'
conf_data.set('UNIQUE_REQ', UNIQUE_REQ, description: 'Required version of poppler library')
EXIV2_REQ = '0.14'
conf_data.set('EXIV2_REQ', EXIV2_REQ, description: 'Required version of exiv2 library')
TAGLIB_REQ = '1.4'
conf_data.set('TAGLIB_REQ', TAGLIB_REQ, description: 'Required version of poppler library')
TAGLIB_1_14_24 = '1.14.24'
TAGLIB_1_14_26 = '1.14.26'
LIBGSF_REQ = '1.12.0'
conf_data.set('LIBGSF_REQ', LIBGSF_REQ, description: 'Required version of gsf library')
POPPLER_REQ = '0.18'
conf_data.set('POPPLER_REQ', POPPLER_REQ, description: 'Required version of poppler library')

# Check for exiv2 support
exiv2 = dependency('exiv2', version: '>=' + EXIV2_REQ, required: get_option('exiv2'))
have_exif2 = exiv2.found()
if have_exif2
  conf_data.set('HAVE_EXIV2', '@0@'.format(have_exif2.to_int()), description : 'Defined if you have EXIF and IPTC support')
endif

# ToDo: Since latest libgsf is already version 1.14.50, reduce this to one flag instead of three.
# Check for libgsf
libgsf = dependency('libgsf-1', version: '>=' + LIBGSF_REQ, required: get_option('libgsf'))
have_gsf = libgsf.found()
if have_gsf
  conf_data.set('HAVE_GSF', '@0@'.format(have_gsf.to_int()), description : 'Define to 1 if you have libgsf (OLE and ODF) support')
endif
if have_gsf and libgsf.version().version_compare('>=' + TAGLIB_1_14_24)
  have_gsf_1_14_24 = have_gsf == false ? false : dependency('libgsf-1', version: '>=' + TAGLIB_1_14_24, required: get_option('libgsf')).found()
  conf_data.set('HAVE_GSF_1_14_24', '@0@'.format(have_gsf_1_14_24.to_int()), description : 'Defined if you have libgsf >= 1.14.24')
  have_gsf_1_14_26 = have_gsf == false ? false : dependency('libgsf-1', version: '>=' + TAGLIB_1_14_26, required: get_option('libgsf')).found()
  conf_data.set('HAVE_GSF_1_14_26', '@0@'.format(have_gsf_1_14_26.to_int()), description : 'Defined if you have libgsf >= 1.14.26')
endif

# Check for taglib
taglib = dependency('taglib', version: '>=' + TAGLIB_REQ, required: get_option('taglib'))
have_taglib = taglib.found()
if have_taglib
  conf_data.set('HAVE_ID3', '@0@'.format(have_taglib.to_int()), description : 'Defined if you have taglib (ID3, Vorbis, APE and FLAC) support')
endif

# Check for poppler
pdf = dependency('poppler-glib', version: '>=' + POPPLER_REQ, required: get_option('poppler'))
have_pdf = pdf.found()
if have_pdf
  conf_data.set('HAVE_PDF', '@0@'.format(have_pdf.to_int()), description : 'Defined if you have PDF support')
endif

# Check for samba
samba = dependency('smbclient', required: get_option('samba'))
have_samba = samba.found()
if have_samba
  conf_data.set('HAVE_SAMBA', '@0@'.format(have_samba.to_int()), description : 'Defined if you have smbclient')
endif

# Check for libunique
unique = dependency('unique-1.0', version: '>=' + UNIQUE_REQ, required: get_option('unique'))
have_unique = unique.found()
if have_unique
  conf_data.set('HAVE_UNIQUE', '@0@'.format(have_unique.to_int()), description : 'Defined if you have unique support')
endif

# Check for gettext
intl_dep = dependency('intl', required: false)
if intl_dep.found()
  conf_data.set('ENABLE_NLS', 1, description : 'Is translation of program messages to the user\'s native language requested.')
endif

#### Check for mmap #####
if cpp.has_function('mmap', prefix : '#include<sys/mman.h>')
  conf_data.set('HAVE_MMAP', 1, description: 'Define to 1 if you have a working `mmap\' system call.')
endif

#Google Test library for 'meson test -C builddir'
GTEST_REQ='1.7.0'
conf_data.set('GTEST_REQ', GTEST_REQ, description: 'Required version of gtest library')
gtest = dependency('gtest', version: '>=' + GTEST_REQ, required: get_option('tests'))

configure_file(output : 'config.h', configuration : conf_data)

meson.add_dist_script(
  find_program('check-news.sh').full_path(),
  '@0@'.format(meson.project_version()),
  'data/org.gnome.gnome-commander.appdata.xml.in'
)

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
    'pixmapsdir': pixmaps_dir,
    'plugindir': plugin_dir
  },
  section: 'Directories'
)

summary(
  {
    'Generate doc': get_option('help'),
    'Use exiv2': have_exif2,
    'Use libgsf': have_gsf,
    'Use poppler': have_pdf,
    'Use samba': have_samba,
    'Use taglib': have_taglib,
    'Use unique': have_unique,
    'Tests': gtest.found(),
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
if gtest.found()
  subdir('tests')
endif

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true
)
