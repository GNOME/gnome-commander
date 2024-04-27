subdir('device-icons')
subdir('file-type-icons')
subdir('mime-icons')

pixmaps = [
  'copy_file_names.xpm',
  'exec_wheel.xpm',
  'gnome_cmd_arrow_blank.xpm',
  'gnome_cmd_arrow_down.xpm',
  'gnome_cmd_arrow_up.xpm',
  'gnome-commander.svg',
  'internal-viewer.svg',
  'mail-send.png',
  'menu_bookmark.xpm',
  'overlay_symlink.xpm',
  'overlay_umount.xpm',
  'pin.png',
  'terminal.svg',
  'unpin.png'
]

install_data(pixmaps,
  install_dir: pixmaps_dir
)

icons_16x16 = [
  'gnome-commander-rotate-180.xpm',
  'gnome-commander-rotate-270.xpm',
  'gnome-commander-rotate-90.xpm',
  'gnome-commander-flip-horizontal.xpm',
  'gnome-commander-flip-vertical.xpm',
]

install_data(icons_16x16,
  install_dir: icons_dir)

desktop_icons = [
  'gnome-commander.svg',
  'gnome-commander-symbolic.svg'
]

install_data(desktop_icons,
  install_dir: datadir / 'icons' / 'hicolor' / 'scalable' / 'apps')
