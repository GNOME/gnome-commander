# ToDo: Install into >>plugin_dir / 'test'<<
testplugin_dir = plugin_dir

testplugin_headers = files(
	'test-plugin.h'
)

install_headers(
    testplugin_headers,
    subdir: meson.project_name() / 'src' / 'plugins' / 'test'
)

testplugin_sources = files(
    'test-plugin.cc'
)

testplugin = shared_module('testplugin',
  dependencies: libgcmd_dep,
  include_directories: configuration_inc,
  link_with: libgcmd,
  install_dir: testplugin_dir,
  install: true, # Change to "true" to install the test plugin,
  sources: [testplugin_headers, testplugin_sources]
)

pixmaps = [
    'test-plugin.xpm'
]

install_data(
    pixmaps,
    install_dir: testplugin_dir
)
