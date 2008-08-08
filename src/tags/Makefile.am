## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libgcmd-tags.a

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	-I.. \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS) \
	$(EXIV2_CFLAGS) \
	$(TAGLIB_CFLAGS) \
	$(CHM_CFLAGS) \
	$(GSF_CFLAGS) \
	$(POPPLER_CFLAGS) \
	-DPLUGIN_DIR=\""$(libdir)/$(PACKAGE)/plugins"\"

libgcmd_tags_a_SOURCES = \
	gnome-cmd-tags.h gnome-cmd-tags.cc \
	gnome-cmd-tags-doc.h gnome-cmd-tags-doc.cc \
	gnome-cmd-tags-exiv2.h gnome-cmd-tags-exiv2.cc \
	gnome-cmd-tags-file.h gnome-cmd-tags-file.cc \
	gnome-cmd-tags-poppler.h gnome-cmd-tags-poppler.cc \
	gnome-cmd-tags-taglib.h gnome-cmd-tags-taglib.cc
