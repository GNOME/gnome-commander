## Process this file with automake to produce Makefile.in

INCLUDES = -Wall \
	$(GLIB_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(GTK_CFLAGS)

pkglib_LTLIBRARIES = libgviewer.la

libgviewer_la_SOURCES = \
	scroll-box.c scroll-box.h \
	image-render.c image-render.h \
	text-render.c text-render.h \
	viewer-widget.c viewer-widget.h \
	viewer-window.c viewer-window.h \
	datapresentation.c datapresentation.h \
	inputmodes.c inputmodes.h \
	fileops.c fileops.h \
	libgviewer.c libgviewer.h \
	gvtypes.h \
	cp437.c

libgviewer_la_LIBADD = 	$(GNOMEUI_LIBS) $(GLIB_LIBS) $(GTK_LIBS)
libgviewer_la_LDFLAGS = $(GNOMEUI_LIBS) $(GLIB_LIBS) $(GTK_LIBS)
