## Process this file with automake to produce Makefile.in.

man1_MANS = gnome-commander.1

EXTRA_DIST = \
	advrename.txt \
	keys.txt \
	tips_tricks.txt \
	internal_viewer_hacking.txt \
        $(man1_MANS)


include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook
 
DOC_MODULE = gnome-commander
DOC_ENTITIES = 
DOC_INCLUDES = legal.xml
DOC_FIGURES = \
	figures/create_archive.png                      \
	figures/dev_cd.png                              \
	figures/extract_archive.png                     \
	figures/mounting.png

#DOC_LINGUAS = se
