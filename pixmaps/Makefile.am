## Process this file with automake to produce Makefile.in

SUBDIRS = device-icons file-type-icons mime-icons

## desktop icon
dtidir = $(datadir)/pixmaps
dti_DATA = gnome-commander.png

## application pixmaps
pixmapsdir = $(datadir)/pixmaps/@PACKAGE@
pixmaps_DATA = \
	copy_file_names.xpm \
	exec_wheel.xpm \
	flip-horizontal-16.xpm \
	flip-vertical-16.xpm \
	gnome_cmd_arrow_blank.xpm \
	gnome_cmd_arrow_down.xpm \
	gnome_cmd_arrow_up.xpm \
	gnome-commander.xpm \
	internal-viewer.xpm \
	menu_bookmark.xpm \
	nautilus.svg \
	overlay_symlink.xpm \
	overlay_umount.xpm \
	python.svg \
	rotate-180-16.xpm \
	rotate-270-16.xpm \
	rotate-90-16.xpm \
	toggle_horizontal.xpm \
	toggle_vertical.xpm \
	terminal.svg

EXTRA_DIST = $(dti_DATA) $(pixmaps_DATA)
