sources = files(
    'poppler-plugin.cc'
)

config_h = configure_file(output : 'config.h', configuration : conf_data)

popplerplugin = shared_module('popplerplugin',
    include_directories: [libgcmd_inc],
    link_with: libgcmd,
    dependencies: [glib, gio, gobject, gmodule, gtk, poppler],
    install_dir: plugin_dir,
    install: true,
    sources: [sources, config_h]
)
