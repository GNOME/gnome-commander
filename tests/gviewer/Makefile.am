## Process this file with automake to produce Makefile.in
AM_CPPFLAGS = -Wall \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	-I$(top_builddir)/src

noinst_PROGRAMS = fileops inputmodes datapresentation textrenderer imagerenderer dualrenderer viewerwidget viewerwindow

TESTLIBS = $(GLIB_LIBS) $(GTK_LIBS) $(GNOMEUI_LIBS) $(top_builddir)/src/libgviewer/libgviewer.la

fileops_SOURCES = fileops.c
fileops_LDADD = $(TESTLIBS)
fileops_LDFLAGS = $(TESTLIBS)

inputmodes_SOURCES = inputmodes.c
inputmodes_LDADD = $(TESTLIBS)
inputmodes_LDFLAGS = $(TESTLIBS)

datapresentation_SOURCES = datapresentation.c
datapresentation_LDADD = $(TESTLIBS)
datapresentation_LDFLAGS = $(TESTLIBS)

textrenderer_SOURCES = textrenderer.c
textrenderer_LDADD = $(TESTLIBS)
textrenderer_LDFLAGS = $(TESTLIBS)

imagerenderer_SOURCES = imagerenderer.c
imagerenderer_LDADD = $(TESTLIBS)
imagerenderer_LDFLAGS = $(TESTLIBS)

dualrenderer_SOURCES = dualrenderer.c
dualrenderer_LDADD = $(TESTLIBS)
dualrenderer_LDFLAGS = $(TESTLIBS)

viewerwidget_SOURCES = viewerwidget.c
viewerwidget_LDADD = $(TESTLIBS)
viewerwidget_LDFLAGS = $(TESTLIBS)

viewerwindow_SOURCES = viewerwindow.c
viewerwindow_LDADD = $(TESTLIBS)
viewerwindow_LDFLAGS = $(TESTLIBS)
