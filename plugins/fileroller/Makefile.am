## Process this file with automake to produce Makefile.in

AM_CXXFLAGS = $(WARN_CXXFLAGS) -Werror=parentheses -Wno-parentheses -Werror=cast-function-type -Wno-cast-function-type -Wno-deprecated-declarations \
				-Wno-inline
AM_LDFLAGS = $(WARN_LDFLAGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GTK_CFLAGS) \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES

plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = libfileroller.la

libfileroller_la_SOURCES = \
	file-roller-plugin.h file-roller-plugin.cc \
	file-roller.xpm file-roller-small.xpm

libfileroller_la_LDFLAGS = $(GTK_LIBS) -module -avoid-version

-include $(top_srcdir)/git.mk
