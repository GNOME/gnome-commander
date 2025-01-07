# ToDo: Install into >>plugin_dir / 'fileroller'<<
fileroller_plugin_dir = plugin_dir

filerollerplugin_headers = files(
    'file-roller-plugin.h'
)

filerollerplugin_sources = files(
    'file-roller-plugin.cc'
)

config_h = configure_file(output : 'config.h', configuration : conf_data)

filerollerplugin = shared_module('filerollerplugin',
  include_directories: [libgcmd_inc],
  link_with: [libgcmd],
  dependencies: [glib, gio, gobject, gmodule, gtk],
  install_dir: fileroller_plugin_dir,
  install: true,
  sources: [filerollerplugin_headers, filerollerplugin_sources, config_h]
)

pixmaps = [
    'file-roller.xpm',
    'file-roller-small.xpm'
]

install_data(
    pixmaps,
    install_dir: fileroller_plugin_dir
)

install_data(['file-roller.xpm'],
     rename: ['gnome-commander-file-roller.xpm'],
     install_dir: datadir / 'icons' / 'hicolor' / 'scalable' / 'apps')
