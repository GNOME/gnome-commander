sources = files(
    'gsf-plugin.cc'
)

config_h = configure_file(output : 'config.h', configuration : conf_data)

gsfplugin = shared_module('gsfplugin',
    include_directories: [libgcmd_inc],
    link_with: libgcmd,
    dependencies: [glib, gio, gobject, gmodule, gtk, gsf],
    install_dir: plugin_dir,
    install: true,
    sources: [sources, config_h]
)
