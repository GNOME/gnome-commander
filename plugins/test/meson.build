# ToDo: Install into >>plugin_dir / 'test'<<
testplugin_dir = plugin_dir

testplugin_headers = files(
    'test-plugin.h'
)

testplugin_sources = files(
    'test-plugin.cc'
)

config_h = configure_file(output : 'config.h', configuration : conf_data)

testplugin = shared_module('testplugin',
  include_directories: [libgcmd_inc],
  link_with: libgcmd,
  dependencies: [glib, gio, gobject, gmodule, gtk],
  install_dir: testplugin_dir,
  install: true,
  sources: [testplugin_headers, testplugin_sources, config_h]
)

pixmaps = [
    'test-plugin.xpm'
]

install_data(
    pixmaps,
    install_dir: testplugin_dir
)
