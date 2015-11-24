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
	iv_imagerenderer \
	iv_inputmodes \
	iv_textrenderer

if HAVE_X11
TESTS += \
	iv_dualrenderer
endif


check_PROGRAMS = $(TESTS)

# *** Internal Viewer Tests *** Most of these only consist of serialised
# function calls for acceptance tests, acutally. Functions of the internal
# viewer library are not fully tested by unit tests! Currently, I don't
# feel that there is a need of adding unit tests for this nicely working
# library. Some of these tests require a running X session. It is checked
# if a session is running in the binaries. If not, the test is skipped.
if HAVE_X11
iv_dualrenderer_SOURCES = iv_dualrenderer_test.cc
iv_dualrenderer_CXXFLAGS = $(INTVLIBS)
iv_dualrenderer_LDFLAGS = $(INTVLIBS) -lX11 -lgtest
endif

iv_imagerenderer_SOURCES = iv_imagerenderer_test.cc gcmd_tests_main.cc
iv_imagerenderer_CXXFLAGS = $(INTVLIBS)
iv_imagerenderer_LDFLAGS = $(INTVLIBS) -lgtest

iv_inputmodes_SOURCES = iv_inputmodes_test.cc
iv_inputmodes_CXXFLAGS = $(INTVLIBS)
iv_inputmodes_LDFLAGS = $(INTVLIBS) -lgtest

iv_textrenderer_SOURCES = iv_textrenderer_test.cc
iv_textrenderer_CXXFLAGS = $(INTVLIBS)
iv_textrenderer_LDFLAGS = $(INTVLIBS) -lgtest

iv_viewerwidget_SOURCES = iv_viewerwidget_test.cc gcmd_tests_main.cc
iv_viewerwidget_CXXFLAGS = $(INTVLIBS)
iv_viewerwidget_LDFLAGS = $(INTVLIBS) -lgtest

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
