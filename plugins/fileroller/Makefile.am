## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -Wall \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

libdir = $(PLUGIN_DIR)

lib_LTLIBRARIES = libfileroller.la

libfileroller_la_SOURCES = \
	file-roller-plugin.h file-roller-plugin.c \
	file-roller.xpm file-roller-small.xpm

libfileroller_la_LIBADD = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS) 
libfileroller_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS) 
