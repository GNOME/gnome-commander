## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

pkglib_LTLIBRARIES = libgcmd.la

libgcmd_la_SOURCES = \
	gnome-cmd-plugin.h gnome-cmd-plugin.c \
	gnome-cmd-file-info.h gnome-cmd-file-info.c \
	gnome-cmd-dialog.h gnome-cmd-dialog.c \
	gnome-cmd-string-dialog.h gnome-cmd-string-dialog.c \
	gnome-cmd-state.h \
	libgcmd-utils.h libgcmd-utils.c \
	libgcmd-data.h libgcmd-data.c \
	libgcmd-widget-factory.h libgcmd-widget-factory.c \
	plugin-info.h \
	libgcmd-deps.h \
	libgcmd.h


libgcmd_la_LIBADD = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS)
libgcmd_la_LDFLAGS = $(GNOMEUI_LIBS) $(GNOMEVFS_LIBS)
