## Process this file with automake to produce Makefile.in.

man1_MANS = gnome-commander.1

EXTRA_DIST = \
	keys.txt \
	internal_viewer_hacking.txt \
        $(man1_MANS)


include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook

DOC_MODULE = gnome-commander
DOC_ENTITIES =
DOC_INCLUDES = legal.xml
DOC_FIGURES = \
	figures/create_archive.png                              \
	figures/dev_cd.png                                      \
	figures/extract_archive.png                             \
	figures/gnome-commander_dialog_adv_rename.png           \
	figures/gnome-commander_dialog_bookmark.png             \
	figures/gnome-commander_dialog_file_metadata.png        \
	figures/gnome-commander_dialog_file_permissions.png     \
	figures/gnome-commander_dialog_file_properties.png      \
	figures/gnome-commander_dialog_keyboard_shortcuts.png   \
	figures/gnome-commander_dialog_remote_connections.png   \
	figures/gnome-commander_dialog_remote_server.png        \
	figures/gnome-commander_dialog_search.png               \
	figures/gnome-commander_options_confirmation.png        \
	figures/gnome-commander_options_devices.png             \
	figures/gnome-commander_options_filters.png             \
	figures/gnome-commander_options_format.png              \
	figures/gnome-commander_options_general.png             \
	figures/gnome-commander_options_layout.png              \
	figures/gnome-commander_options_network.png             \
	figures/gnome-commander_options_programs.png            \
	figures/gnome-commander_window.png                      \
	figures/mounting.png

DOC_LINGUAS = de es

