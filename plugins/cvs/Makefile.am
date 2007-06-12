## Process this file with automake to produce Makefile.in

.l.c:
	$(LEXCOMPILE) $<

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = libcvs.la

libcvs_la_SOURCES = \
	parser.h parser.l \
	cvs-plugin.h cvs-plugin.c \
	interface.h interface.c \
	cvs-plugin.xpm close.xpm

libcvs_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS)
