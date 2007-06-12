## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = libfileroller.la

libfileroller_la_SOURCES = \
	file-roller-plugin.h file-roller-plugin.c \
	file-roller.xpm file-roller-small.xpm

libfileroller_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS)
