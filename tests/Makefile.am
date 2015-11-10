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
	iv_viewerwidget \
	iv_fileops \
	iv_bm_search \
	iv_datapresentation \
	iv_dualrenderer \
	iv_imagerenderer \
	iv_inputmodes \
	iv_textrenderer

check_PROGRAMS = $(TESTS)

# *** Internal Viewer Tests *** Most of these only consist of serialised
# function calls for acceptance tests, acutally. Functions of the internal
# viewer library are not fully tested by unit tests. Currently, I don't
# feel that there is a need of adding unit tests for this nicely working
# library. Some of these tests require a running X session.
iv_dualrenderer_SOURCES = iv_dualrenderer_test.cc gcmd_tests_main.cc
iv_dualrenderer_CXXFLAGS = $(INTVLIBS)
iv_dualrenderer_LDFLAGS = $(INTVLIBS) -lgtest

iv_imagerenderer_SOURCES = iv_imagerenderer_test.cc
iv_imagerenderer_CXXFLAGS = $(INTVLIBS)
iv_imagerenderer_LDFLAGS = $(INTVLIBS) -lgtest

iv_inputmodes_SOURCES = iv_inputmodes_test.cc
iv_inputmodes_CXXFLAGS = $(INTVLIBS)
iv_inputmodes_LDFLAGS = $(INTVLIBS) -lgtest

iv_search_dlg_SOURCES = iv_search_dlg_test.cc
iv_search_dlg_CXXFLAGS = $(INTVLIBS) -fpermissive
iv_search_dlg_LDFLAGS = $(INTVLIBS) -lgtest

iv_textrenderer_SOURCES = iv_textrenderer_test.cc
iv_textrenderer_CXXFLAGS = $(INTVLIBS)
iv_textrenderer_LDFLAGS = $(INTVLIBS) -lgtest

iv_viewerwidget_SOURCES = iv_viewerwidget_test.cc
iv_viewerwidget_CXXFLAGS = $(INTVLIBS)
iv_viewerwidget_LDFLAGS = $(INTVLIBS) -lgtest

iv_viewerwindow_SOURCES = iv_viewerwindow_test.cc
iv_viewerwindow_CXXFLAGS = $(INTVLIBS)
iv_viewerwindow_LDFLAGS = $(INTVLIBS) -lgtest

iv_fileops_SOURCES = iv_fileops_test.cc gcmd_tests_main.cc 
iv_fileops_CXXFLAGS = $(INTVLIBS)
iv_fileops_LDFLAGS = $(INTVLIBS) -lgtest

iv_bm_search_SOURCES = iv_bm_search_test.cc gcmd_tests_main.cc 
iv_bm_search_CXXFLAGS = $(INTVLIBS)
iv_bm_search_LDFLAGS = $(INTVLIBS) -lgtest

iv_datapresentation_SOURCES = iv_datapresentation.cc
iv_datapresentation_CXXFLAGS = $(INTVLIBS)
iv_datapresentation_LDFLAGS = $(INTVLIBS) -lgtest

-include $(top_srcdir)/git.mk
