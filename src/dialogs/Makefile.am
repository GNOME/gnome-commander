## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libgcmd-dialogs.a

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	-I$(srcdir)/.. \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS) \
	$(EXIV2_CFLAGS) \
	$(TAGLIB_CFLAGS) \
	$(CHM_CFLAGS) \
	$(GSF_CFLAGS) \
	$(POPPLER_CFLAGS) \
	-DPLUGIN_DIR=\""$(libdir)/$(PACKAGE)/plugins"\"

libgcmd_dialogs_a_SOURCES = \
	gnome-cmd-advrename-dialog.h gnome-cmd-advrename-dialog.cc \
	gnome-cmd-advrename-regex-dialog.h gnome-cmd-advrename-regex-dialog.cc \
	gnome-cmd-edit-bookmark-dialog.h gnome-cmd-edit-bookmark-dialog.cc \
	gnome-cmd-manage-bookmarks-dialog.h gnome-cmd-manage-bookmarks-dialog.cc \
	gnome-cmd-mkdir-dialog.h gnome-cmd-mkdir-dialog.cc \
	gnome-cmd-key-shortcuts-dialog.h gnome-cmd-key-shortcuts-dialog.cc \
	gnome-cmd-edit-profile-dialog.h gnome-cmd-edit-profile-dialog.cc \
	gnome-cmd-manage-profiles-dialog.h gnome-cmd-manage-profiles-dialog.cc

-include $(top_srcdir)/git.mk
