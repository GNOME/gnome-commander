## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

plugindir = $(pkglibdir)/plugins

noinst_LTLIBRARIES = libtest.la

libtest_la_SOURCES = \
	test-plugin.h test-plugin.c \
	test-plugin.xpm

libtest_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS)
