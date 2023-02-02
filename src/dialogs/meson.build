libgmcddialogs_headers = files(
  'gnome-cmd-advrename-dialog.h',
  'gnome-cmd-advrename-regex-dialog.h',
  'gnome-cmd-chmod-dialog.h',
  'gnome-cmd-chown-dialog.h',
  'gnome-cmd-con-dialog.h',
  'gnome-cmd-delete-dialog.h',
  'gnome-cmd-edit-bookmark-dialog.h',
  'gnome-cmd-file-props-dialog.h',
  'gnome-cmd-make-copy-dialog.h',
  'gnome-cmd-manage-bookmarks-dialog.h',
  'gnome-cmd-mkdir-dialog.h',
  'gnome-cmd-key-shortcuts-dialog.h',
  'gnome-cmd-edit-profile-dialog.h',
  'gnome-cmd-manage-profiles-dialog.h',
  'gnome-cmd-options-dialog.h',
  'gnome-cmd-patternsel-dialog.h',
  'gnome-cmd-prepare-copy-dialog.h',
  'gnome-cmd-prepare-move-dialog.h',
  'gnome-cmd-prepare-xfer-dialog.h',
  'gnome-cmd-remote-dialog.h',
  'gnome-cmd-rename-dialog.h',
  'gnome-cmd-search-dialog.h',
)

libgmcddialogs_sources = files(
  'gnome-cmd-advrename-dialog.cc',
  'gnome-cmd-advrename-regex-dialog.cc',
  'gnome-cmd-chmod-dialog.cc',
  'gnome-cmd-chown-dialog.cc',
  'gnome-cmd-con-dialog.cc',
  'gnome-cmd-delete-dialog.cc',
  'gnome-cmd-edit-bookmark-dialog.cc',
  'gnome-cmd-file-props-dialog.cc',
  'gnome-cmd-make-copy-dialog.cc',
  'gnome-cmd-manage-bookmarks-dialog.cc',
  'gnome-cmd-mkdir-dialog.cc',
  'gnome-cmd-key-shortcuts-dialog.cc',
  'gnome-cmd-options-dialog.cc',
  'gnome-cmd-patternsel-dialog.cc',
  'gnome-cmd-prepare-copy-dialog.cc',
  'gnome-cmd-prepare-move-dialog.cc',
  'gnome-cmd-prepare-xfer-dialog.cc',
  'gnome-cmd-remote-dialog.cc',
  'gnome-cmd-rename-dialog.cc',
  'gnome-cmd-search-dialog.cc',
)

libgmcddialogs = static_library('dialogs',
  dependencies: [common_deps, sources_dep],
  include_directories: [configuration_inc, sources_inc],
  link_with: libgcmd,
  sources: [libgmcddialogs_headers, libgmcddialogs_sources]
)

libgmcddialogs_dep = declare_dependency(
  include_directories: [include_directories('.')],
)
