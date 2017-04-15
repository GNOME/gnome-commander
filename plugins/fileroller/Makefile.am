## Process this file with automake to produce Makefile.in

AM_CXXFLAGS = $(WARN_CXXFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = libfileroller.la

libfileroller_la_SOURCES = \
	file-roller-plugin.h file-roller-plugin.cc \
	file-roller.xpm file-roller-small.xpm

libfileroller_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS) -module -avoid-version

-include $(top_srcdir)/git.mk
