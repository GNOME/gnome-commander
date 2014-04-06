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
	gnome-cmd-chmod-dialog.h gnome-cmd-chmod-dialog.cc \
	gnome-cmd-chown-dialog.h gnome-cmd-chown-dialog.cc \
	gnome-cmd-con-dialog.h gnome-cmd-con-dialog.cc \
	gnome-cmd-delete-dialog.h gnome-cmd-delete-dialog.cc \
	gnome-cmd-edit-bookmark-dialog.h gnome-cmd-edit-bookmark-dialog.cc \
	gnome-cmd-file-props-dialog.h gnome-cmd-file-props-dialog.cc \
	gnome-cmd-make-copy-dialog.h gnome-cmd-make-copy-dialog.cc \
	gnome-cmd-manage-bookmarks-dialog.h gnome-cmd-manage-bookmarks-dialog.cc \
	gnome-cmd-mkdir-dialog.h gnome-cmd-mkdir-dialog.cc \
	gnome-cmd-key-shortcuts-dialog.h gnome-cmd-key-shortcuts-dialog.cc \
	gnome-cmd-edit-profile-dialog.h \
	gnome-cmd-manage-profiles-dialog.h \
	gnome-cmd-options-dialog.h gnome-cmd-options-dialog.cc \
	gnome-cmd-patternsel-dialog.h gnome-cmd-patternsel-dialog.cc \
	gnome-cmd-prepare-copy-dialog.h gnome-cmd-prepare-copy-dialog.cc \
	gnome-cmd-prepare-move-dialog.h gnome-cmd-prepare-move-dialog.cc \
	gnome-cmd-prepare-xfer-dialog.h gnome-cmd-prepare-xfer-dialog.cc \
	gnome-cmd-search-dialog.h gnome-cmd-search-dialog.cc \
	gnome-cmd-remote-dialog.h gnome-cmd-remote-dialog.cc \
	gnome-cmd-rename-dialog.h gnome-cmd-rename-dialog.cc

-include $(top_srcdir)/git.mk
