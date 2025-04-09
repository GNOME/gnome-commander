libgcmd_headers = files(
  'gnome-cmd-configurable.h',
  'gnome-cmd-file-descriptor.h',
  'gnome-cmd-file-actions.h',
  'gnome-cmd-file-metadata-extractor.h',
  'gnome-cmd-plugin-info.h',
  'gnome-cmd-state.h',
  'libgcmd.h',
)

libgcmd_sources = files(
  'gnome-cmd-configurable.c',
  'gnome-cmd-file-descriptor.c',
  'gnome-cmd-file-actions.c',
  'gnome-cmd-file-metadata-extractor.c',
  'gnome-cmd-state.c',
)

libgcmd_deps = [
  glib, gmodule, gio, gobject, gtk
]

libgcmd = shared_library('gcmd',
  include_directories: [include_directories('.')],
  dependencies: libgcmd_deps,
  sources: [libgcmd_headers, libgcmd_sources],
  install: true,
)

libgcmd_gir = gnome.generate_gir(libgcmd,
  namespace: 'GnomeCmd',
  nsversion: '1.0',
  header: 'libgcmd.h',
  sources: [libgcmd_headers, libgcmd_sources],
  includes: ['Gio-2.0', 'Gtk-4.0'],
  identifier_prefix: 'GnomeCmd',
  symbol_prefix: 'gnome_cmd',
  export_packages: 'gnome_cmd',
  fatal_warnings: true,
  install: true,
)

gidocgen = find_program('gi-docgen', required: false)

if gidocgen.found()
  custom_target('libgcmd-doc',
    input: libgcmd_gir[0],
    output: 'libgcmd-1.0',
    command: [
      gidocgen,
      'generate',
      '--quiet',
      '--output-dir=@OUTPUT@',
      '--no-namespace-dir',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT@',
    ],
    build_by_default: true,
    install: true,
    install_dir: datadir / 'doc',
  )
endif
