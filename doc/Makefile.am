## Process this file with automake to produce Makefile.in.

man1_MANS = gnome-commander.1

EXTRA_DIST = \
	advrename.txt \
	keys.txt \
	internal_viewer_hacking.txt \
        $(man1_MANS)


include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook
 
DOC_MODULE = gnome-commander
DOC_ENTITIES = 
DOC_AM_CPPFLAGS = legal.xml
DOC_FIGURES = \
	figures/create_archive.png                           \
	figures/dev_cd.png                                   \
	figures/extract_archive.png                          \
	figures/gnome-commander_dialog_file_permissions.png  \
	figures/gnome-commander_dialog_file_properties.png   \
	figures/gnome-commander_dialog_ftp.png               \
	figures/gnome-commander_dialog_bookmark.png          \
	figures/gnome-commander_dialog_search.png            \
	figures/gnome-commander_options_dev.png              \
	figures/gnome-commander_options_prog.png             \
	figures/gnome-commander_options_filter.png           \
	figures/gnome-commander_options_layout.png           \
	figures/gnome-commander_options_data.png             \
	figures/gnome-commander_options_general.png          \
	figures/gnome-commander_window.png                   \
	figures/mounting.png

#DOC_LINGUAS = se
