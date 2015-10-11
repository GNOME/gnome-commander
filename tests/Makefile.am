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

TESTS =

TESTS += func_test
check_PROGRAMS += func_test
func_test_SOURCES = run_tests.cpp fileops.cpp

func_test_CXXFLAGS = $(INTVLIBS)
func_test_LDFLAGS = $(INTVLIBS) -lgtest

-include $(top_srcdir)/git.mk
