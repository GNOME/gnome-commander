## Process this file with automake to produce Makefile.in

.l.c:
	$(LEXCOMPILE) $<

AM_CPPFLAGS = -Wall \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

libdir = $(PLUGIN_DIR)

lib_LTLIBRARIES = libcvs.la

libcvs_la_SOURCES = \
	parser.h parser.l \
	cvs-plugin.h cvs-plugin.c \
	interface.h interface.c \
	cvs-plugin.xpm close.xpm

libcvs_la_LIBADD = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS) 
libcvs_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS) 
