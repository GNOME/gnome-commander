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
