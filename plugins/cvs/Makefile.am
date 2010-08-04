## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

plugindir = $(pkglibdir)/plugins

noinst_LTLIBRARIES = libcvs.la

libcvs_la_SOURCES = \
	parser.h parser.ll \
	cvs-plugin.h cvs-plugin.cc \
	interface.h interface.cc \
	cvs-plugin.xpm close.xpm

libcvs_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS) -module -avoid-version

-include $(top_srcdir)/git.mk
