## Process this file with automake to produce Makefile.in

AM_CXXFLAGS = $(WARN_CXXFLAGS) -Werror=parentheses -Wno-parentheses -Werror=cast-function-type -Wno-cast-function-type
AM_LDFLAGS = $(WARN_LDFLAGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS) \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES

plugindir = $(pkglibdir)/plugins

noinst_LTLIBRARIES = libtest.la

libtest_la_SOURCES = \
	test-plugin.h test-plugin.cc \
	test-plugin.xpm

libtest_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS) -module -avoid-version

-include $(top_srcdir)/git.mk
