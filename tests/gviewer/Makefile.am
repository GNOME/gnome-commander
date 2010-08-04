## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	-I$(top_builddir)/src

noinst_PROGRAMS = fileops inputmodes datapresentation textrenderer imagerenderer dualrenderer viewerwidget viewerwindow bm_byte_test bm_chartype_test utils_test search_dlg_test

TESTLIBS = $(GLIB_LIBS) $(GTK_LIBS) $(GNOMEUI_LIBS) $(top_builddir)/src/libgviewer/libgviewer.a

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

bm_byte_test_SOURCES = bm_byte_test.c
bm_byte_test_LDADD = $(TESTLIBS)
bm_byte_test_LDFLAGS = $(TESTLIBS)

bm_chartype_test_SOURCES = bm_chartype_test.c
bm_chartype_test_LDADD = $(TESTLIBS)
bm_chartype_test_LDFLAGS = $(TESTLIBS)

utils_test_SOURCES = utils_test.c
utils_test_LDADD = $(TESTLIBS)
utils_test_LDFLAGS = $(TESTLIBS)

search_dlg_test_SOURCES = search_dlg_test.c
search_dlg_test_LDADD = $(TESTLIBS)
search_dlg_test_LDFLAGS = $(TESTLIBS)

-include $(top_srcdir)/git.mk
