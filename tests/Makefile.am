SUBDIRS =

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(GTEST_CPPFLAGS) \
	-I$(top_builddir)/src

INTVLIBS = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GNOMEUI_LIBS) \
	$(top_builddir)/src/intviewer/libgviewer.a

TESTS = \
	intviewer_viewerwidget \
	intviewer_fileops \
	intviewer_bm_search \
	intviewer_datapresentation \
	intviewer_dualrenderer \
	intviewer_imagerenderer \
	intviewer_inputmodes \
	intviewer_textrenderer

check_PROGRAMS = $(TESTS)

intviewer_dualrenderer_SOURCES = iv_dualrenderer_test.cc gcmd_tests_main.cc
intviewer_dualrenderer_CXXFLAGS = $(INTVLIBS)
intviewer_dualrenderer_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_imagerenderer_SOURCES = iv_imagerenderer_test.cc
intviewer_imagerenderer_CXXFLAGS = $(INTVLIBS)
intviewer_imagerenderer_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_inputmodes_SOURCES = iv_inputmodes_test.cc
intviewer_inputmodes_CXXFLAGS = $(INTVLIBS)
intviewer_inputmodes_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_search_dlg_SOURCES = iv_search_dlg_test.cc
intviewer_search_dlg_CXXFLAGS = $(INTVLIBS) -fpermissive
intviewer_search_dlg_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_textrenderer_SOURCES = iv_textrenderer_test.cc
intviewer_textrenderer_CXXFLAGS = $(INTVLIBS)
intviewer_textrenderer_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_viewerwidget_SOURCES = iv_viewerwidget_test.cc
intviewer_viewerwidget_CXXFLAGS = $(INTVLIBS)
intviewer_viewerwidget_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_viewerwindow_SOURCES = iv_viewerwindow_test.cc
intviewer_viewerwindow_CXXFLAGS = $(INTVLIBS)
intviewer_viewerwindow_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_fileops_SOURCES = iv_fileops_test.cc gcmd_tests_main.cc 
intviewer_fileops_CXXFLAGS = $(INTVLIBS)
intviewer_fileops_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_bm_search_SOURCES = iv_bm_search_test.cc gcmd_tests_main.cc 
intviewer_bm_search_CXXFLAGS = $(INTVLIBS)
intviewer_bm_search_LDFLAGS = $(INTVLIBS) -lgtest

intviewer_datapresentation_SOURCES = iv_datapresentation.cc
intviewer_datapresentation_CXXFLAGS = $(INTVLIBS)
intviewer_datapresentation_LDFLAGS = $(INTVLIBS) -lgtest

-include $(top_srcdir)/git.mk
