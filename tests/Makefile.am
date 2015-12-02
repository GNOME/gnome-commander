SUBDIRS =

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	$(GLIB_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(GTEST_CPPFLAGS) \
	-I$(top_builddir)/src

INTVLIBS = \
	$(GLIB_LIBS) \
	$(GOBJECT_CFLAGS) \
	$(GTK_LIBS) \
	$(GNOMEUI_LIBS) \
	-lgtest

ADDITIONAL_LDADD = \
	$(top_builddir)/src/intviewer/libgviewer.a \
    $(GOBJECT_LIBS) \
    $(GTK_LIBS) \
    $(GNOMEUI_LIBS)

TESTS = \
	iv_viewerwidget \
	iv_fileops \
	iv_bm_search \
	iv_datapresentation \
	iv_imagerenderer \
	iv_inputmodes \
	iv_textrenderer

check_PROGRAMS = $(TESTS)

# *** Internal Viewer Tests *** Most of these only consist of serialised
# function calls for acceptance tests, acutally. Functions of the internal
# viewer library are not fully tested by unit tests. 
iv_imagerenderer_SOURCES = iv_imagerenderer_test.cc gcmd_tests_main.cc
iv_imagerenderer_CXXFLAGS = $(AM_CPPFLAGS)
iv_imagerenderer_LDFLAGS = $(INTVLIBS)
iv_imagerenderer_LDADD = $(ADDITIONAL_LDADD)

iv_inputmodes_SOURCES = iv_inputmodes_test.cc gcmd_tests_main.cc
iv_inputmodes_CXXFLAGS = $(AM_CPPFLAGS)
iv_inputmodes_LDFLAGS = $(INTVLIBS)
iv_inputmodes_LDADD = $(ADDITIONAL_LDADD)

iv_textrenderer_SOURCES = iv_textrenderer_test.cc gcmd_tests_main.cc
iv_textrenderer_CXXFLAGS = $(AM_CPPFLAGS)
iv_textrenderer_LDFLAGS = $(INTVLIBS)
iv_textrenderer_LDADD = $(ADDITIONAL_LDADD)

iv_viewerwidget_SOURCES = iv_viewerwidget_test.cc gcmd_tests_main.cc
iv_viewerwidget_CXXFLAGS = $(AM_CPPFLAGS)
iv_viewerwidget_LDFLAGS = $(INTVLIBS)
iv_viewerwidget_LDADD = $(ADDITIONAL_LDADD)

iv_fileops_SOURCES = iv_fileops_test.cc gcmd_tests_main.cc 
iv_fileops_CXXFLAGS = $(AM_CPPFLAGS)
iv_fileops_LDFLAGS = $(INTVLIBS)
iv_fileops_LDADD = $(ADDITIONAL_LDADD)

iv_bm_search_SOURCES = iv_bm_search_test.cc gcmd_tests_main.cc
iv_bm_search_CXXFLAGS = $(AM_CPPFLAGS)
iv_bm_search_LDFLAGS = $(INTVLIBS)
iv_bm_search_LDADD = $(ADDITIONAL_LDADD)

iv_datapresentation_SOURCES = iv_datapresentation.cc gcmd_tests_main.cc
iv_datapresentation_CXXFLAGS = $(AM_CPPFLAGS)
iv_datapresentation_LDFLAGS = $(INTVLIBS)
iv_datapresentation_LDADD = $(ADDITIONAL_LDADD)

-include $(top_srcdir)/git.mk
