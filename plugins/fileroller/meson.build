# ToDo: Install into >>plugin_dir / 'fileroller'<<
fileroller_plugin_dir = plugin_dir

gresource = gnome.compile_resources(
  'file-roller-plugin-resources',
  'file-roller-plugin.gresource.xml',
  source_dir: meson.current_source_dir(),
  c_name: 'file_roller_plugin'
)

filerollerplugin_headers = files(
    'file-roller-plugin.h'
)

filerollerplugin_sources = [
    'file-roller-plugin.cc',
    gresource
    ]

config_h = configure_file(output : 'config.h', configuration : conf_data)

filerollerplugin = shared_module('filerollerplugin',
  include_directories: [libgcmd_inc],
  link_with: [libgcmd],
  dependencies: [glib, gio, gobject, gmodule, gtk],
  install_dir: fileroller_plugin_dir,
  install: true,
  sources: [filerollerplugin_headers, filerollerplugin_sources, config_h]
)
