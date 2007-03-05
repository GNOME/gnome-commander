SUBDIRS = id3lib

noinst_LIBRARIES = libgcmd-tags.a

AM_CPPFLAGS = \
	-Wall \
	-I$(top_srcdir) \
	-I.. \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS) \
	$(EXIF_CFLAGS) \
	$(IPTCDATA_CFLAGS) \
	$(LCMS_CFLAGS) \
	$(CHM_CFLAGS) \
	$(GSF_CFLAGS) \
	-DPLUGIN_DIR=\""$(libdir)/$(PACKAGE)/plugins"\"

libgcmd_tags_a_SOURCES = \
	gnome-cmd-tags.h gnome-cmd-tags.c \
	gnome-cmd-tags-libs.h \
	gnome-cmd-tags-audio.h gnome-cmd-tags-audio.c \
	gnome-cmd-tags-doc.h gnome-cmd-tags-doc.c \
	gnome-cmd-tags-exif.h gnome-cmd-tags-exif.c \
	gnome-cmd-tags-icc.h gnome-cmd-tags-icc.c \
	gnome-cmd-tags-id3.h gnome-cmd-tags-id3.c \
	gnome-cmd-tags-iptcdata.h gnome-cmd-tags-iptcdata.c
