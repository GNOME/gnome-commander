subdir('device-icons')
subdir('file-type-icons')
subdir('mime-icons')

pixmaps = [
  'overlay_symlink.xpm',
  'overlay_umount.xpm',
  'pin.png',
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
  'overlay_symlink.xpm',
  'overlay_umount.xpm',
  'gnome-commander-copy-file-names.xpm',
]

icons_24x24 = [
  'gnome-commander-edit.png',
  'gnome-commander-connect.png',
]

icons_scalable = [
  'gnome-commander.svg',
  'gnome-commander-internal-viewer.svg',
  'gnome-commander-down-symbolic.svg',
  'gnome-commander-bookmark-outline-symbolic.svg',
  'gnome-commander-info-outline-symbolic.svg',
]

install_data(icons_16x16 + icons_24x24 + icons_scalable,
  install_dir: icons_dir)

desktop_icons = [
  'gnome-commander.svg',
  'gnome-commander-symbolic.svg',
  'gnome-commander-internal-viewer.svg',
]

install_data(desktop_icons,
  install_dir: datadir / 'icons' / 'hicolor' / 'scalable' / 'apps')
