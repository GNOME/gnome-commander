libgcmd_headers = files(
  'gnome-cmd-file-descriptor.h',
  'gnome-cmd-plugin.h',
  'gnome-cmd-plugin-info.h',
  'libgcmd.h',
)

libgcmd_sources = files(
  'gnome-cmd-file-descriptor.c',
  'gnome-cmd-plugin.c',
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
