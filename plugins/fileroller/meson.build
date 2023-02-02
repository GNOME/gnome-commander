# ToDo: Install into >>plugin_dir / 'fileroller'<<
fileroller_plugin_dir = plugin_dir

filerollerplugin_headers = files(
    'file-roller-plugin.h'
)

filerollerplugin_sources = files(
    'file-roller-plugin.cc'
)

filerollerplugin = shared_module('filerollerplugin',
  dependencies: libgcmd_dep,
  link_with: [libgcmd],
  install_dir: fileroller_plugin_dir,
  install: true,
  sources: [filerollerplugin_headers, filerollerplugin_sources]
)

pixmaps = [
    'file-roller.xpm',
    'file-roller-small.xpm'
]

install_data(
    pixmaps,
    install_dir: fileroller_plugin_dir
)
