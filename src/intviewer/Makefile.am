## Process this file with automake to produce Makefile.in

AM_CXXFLAGS = $(WARN_CXXFLAGS) -Werror=parentheses -Wno-parentheses \
				-Werror=cast-function-type -Wno-cast-function-type -Wno-deprecated-declarations \
				-Wno-inline
AM_LDFLAGS = $(WARN_LDFLAGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(srcdir)/.. \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GNOMEVFS_CFLAGS) \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES

noinst_LIBRARIES = libgviewer.a

libgviewer_a_SOURCES = \
	bm_byte.cc bm_byte.h \
	bm_chartype.cc bm_chartype.h \
	cp437.cc cp437.h \
	datapresentation.cc datapresentation.h \
	fileops.cc fileops.h \
	gvtypes.h \
	image-render.cc image-render.h \
	inputmodes.cc inputmodes.h \
	libgviewer.h \
	scroll-box.cc scroll-box.h \
	search-dlg.cc search-dlg.h \
	search-progress-dlg.cc \
	search-progress-dlg.h \
	searcher.cc searcher.h \
	text-render.cc text-render.h \
	viewer-utils.cc viewer-utils.h \
	viewer-widget.cc viewer-widget.h \
	viewer-window.cc viewer-window.h

-include $(top_srcdir)/git.mk
