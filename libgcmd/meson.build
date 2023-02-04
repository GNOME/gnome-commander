libgcmd_headers = files(
  'gnome-cmd-dialog.h',
  'gnome-cmd-file-base.h',
  'gnome-cmd-plugin.h',
  'gnome-cmd-state.h',
  'gnome-cmd-string-dialog.h',
  'libgcmd-deps.h',
  'libgcmd.h',
  'libgcmd-utils.h',
  'libgcmd-widget-factory.h'
)

libgcmd_sources = files(
  'libgcmd-widget-factory.cc',
  'gnome-cmd-dialog.cc',
  'gnome-cmd-file-base.cc',
  'gnome-cmd-plugin.cc',
  'gnome-cmd-string-dialog.cc',
  'libgcmd-utils.cc'
)

libgcmd_includes = [configuration_inc, sources_inc, include_directories('.')]

libgcmd_dep = declare_dependency(
  dependencies: common_deps,
  include_directories: libgcmd_includes,
)

libgcmd = static_library('gcmd',
  include_directories: [libgcmd_includes],
  dependencies: common_deps,
  sources: [libgcmd_headers, libgcmd_sources],
)
