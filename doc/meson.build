sources = [
  'index.docbook',
  'legal.xml',
  'releases.xml'
]

media = [
  'figures/create_archive.png',
  'figures/extract_archive.png',
  'figures/gnome-commander_dialog_adv_rename.png',
  'figures/gnome-commander_dialog_application.png',
  'figures/gnome-commander_dialog_bookmark.png',
  'figures/gnome-commander_dialog_file_metadata.png',
  'figures/gnome-commander_dialog_file_permissions.png',
  'figures/gnome-commander_dialog_file_properties.png',
  'figures/gnome-commander_dialog_keyboard_shortcuts.png',
  'figures/gnome-commander_dialog_remote_connections.png',
  'figures/gnome-commander_dialog_remote_server.png',
  'figures/gnome-commander_options_confirmation.png',
  'figures/gnome-commander_options_devices.png',
  'figures/gnome-commander_options_filters.png',
  'figures/gnome-commander_options_format.png',
  'figures/gnome-commander_options_layout_colors.png',
  'figures/gnome-commander_options_layout.png',
  'figures/gnome-commander_options_programs.png',
  'figures/gnome-commander_options_tabs.png',
  'figures/gnome-commander_simple_plugins_example.png',
  'figures/gnome-commander_window.png'
]

install_man('gnome-commander.1')

install_data(
  ['keys.txt', 'internal_viewer_hacking.txt'],
  install_dir : get_option('datadir') / meson.project_name()
)

gnome.yelp(
  meson.project_name(),
  sources: sources,
  media: media
)
