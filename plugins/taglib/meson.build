sources = files(
    'taglib-plugin.cc'
)

config_h = configure_file(output : 'config.h', configuration : conf_data)

taglibplugin = shared_module('taglibplugin',
    include_directories: [libgcmd_inc],
    link_with: libgcmd,
    dependencies: [glib, gio, gobject, gmodule, gtk, taglib],
    install_dir: plugin_dir,
    install: true,
    sources: [sources, config_h]
)
