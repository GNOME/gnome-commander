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
	intviewer_fileops
check_PROGRAMS = $(TESTS)

intviewer_fileops_SOURCES = iv_fileops.cc gcmd_tests_main.cc 
intviewer_fileops_CXXFLAGS = $(INTVLIBS)
intviewer_fileops_LDFLAGS = $(INTVLIBS) -lgtest

-include $(top_srcdir)/git.mk
