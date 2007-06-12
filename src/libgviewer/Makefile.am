## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	-I.. \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

noinst_LIBRARIES = libgviewer.a

libgviewer_a_SOURCES = \
	bm_byte.c bm_byte.h \
	bm_chartype.c bm_chartype.h \
	cp437.c cp437.h \
	datapresentation.c datapresentation.h \
	fileops.c fileops.h \
	gvtypes.h \
	image-render.c image-render.h \
	inputmodes.c inputmodes.h \
	libgviewer.c libgviewer.h \
	scroll-box.c scroll-box.h \
	search-dlg.c search-dlg.h \
	search-progress-dlg.c \
	search-progress-dlg.h \
	searcher.c searcher.h \
	text-render.c text-render.h \
	viewer-utils.c viewer-utils.h \
	viewer-widget.c viewer-widget.h \
	viewer-window.c viewer-window.h
