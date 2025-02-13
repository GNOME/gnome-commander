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

install_data(icons_16x16,
  install_dir: icons_dir / 'hicolor' / '16x16' / 'actions')

icons_scalable = [
  'gnome-commander.svg',
  'gnome-commander-internal-viewer.svg',
  'gnome-commander-down-symbolic.svg',
  'gnome-commander-bookmark-outline-symbolic.svg',
  'gnome-commander-info-outline-symbolic.svg',
  'gnome-commander-settings-symbolic.svg',
  'gnome-commander-recycling-bin-symbolic.svg',
  'gnome-commander-edit-symbolic.svg',
  'gnome-commander-folder-remote-symbolic.svg',
  'gnome-commander-folder-remote-disconnect-symbolic.svg',
]

install_data(icons_scalable,
  install_dir: icons_dir / 'hicolor' / 'scalable' / 'actions')

desktop_icons = [
  'gnome-commander.svg',
  'gnome-commander-symbolic.svg',
  'gnome-commander-internal-viewer.svg',
]

install_data(desktop_icons,
  install_dir: datadir / 'icons' / 'hicolor' / 'scalable' / 'apps')
