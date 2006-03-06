## Process this file with automake to produce Makefile.in

SUBDIRS = libgviewer

AM_CPPFLAGS = -Wall \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

bin_PROGRAMS = gnome-commander gcmd-block

gnome_commander_SOURCES = \
	main.c \
	handle.h handle.c \
	plugin_manager.h plugin_manager.c \
	dirlist.h dirlist.c \
	widget-factory.h widget-factory.c \
	gnome-cmd-about-plugin.h gnome-cmd-about-plugin.c \
	gnome-cmd-smb-net.h gnome-cmd-smb-net.c \
	gnome-cmd-con.h gnome-cmd-con.c \
	gnome-cmd-con-home.h gnome-cmd-con-home.c \
	gnome-cmd-con-ftp.h gnome-cmd-con-ftp.c \
	gnome-cmd-con-device.h gnome-cmd-con-device.c \
	gnome-cmd-con-smb.h gnome-cmd-con-smb.c \
	gnome-cmd-con-list.h gnome-cmd-con-list.c \
	gnome-cmd-path.h gnome-cmd-path.c \
	gnome-cmd-plain-path.h gnome-cmd-plain-path.c \
	gnome-cmd-smb-path.h gnome-cmd-smb-path.c \
	gnome-cmd-dir-pool.h gnome-cmd-dir-pool.c \
	gnome-cmd-file.h gnome-cmd-file.c \
	gnome-cmd-file-collection.h gnome-cmd-file-collection.c \
	gnome-cmd-dir.h gnome-cmd-dir.c \
	gnome-cmd-includes.h \
	gnome-cmd-types.h \
	cap.c cap.h \
	utils.h utils.c \
	owner.h owner.c \
	history.h history.c \
	filter.h filter.c \
	ls_colors.h ls_colors.c \
	useractions.h useractions.c \
	imageloader.c imageloader.h \
	gnome-cmd-dir-indicator.h gnome-cmd-dir-indicator.c \
	gnome-cmd-advrename-dialog.h gnome-cmd-advrename-dialog.c \
	gnome-cmd-advrename-lexer.h gnome-cmd-advrename-lexer.l \
	gnome-cmd-app.h gnome-cmd-app.c \
	gnome-cmd-bookmark-dialog.h gnome-cmd-bookmark-dialog.c \
	gnome-cmd-chmod-component.h gnome-cmd-chmod-component.c \
	gnome-cmd-chmod-dialog.h gnome-cmd-chmod-dialog.c \
	gnome-cmd-chown-component.h gnome-cmd-chown-component.c \
	gnome-cmd-chown-dialog.h gnome-cmd-chown-dialog.c \
	gnome-cmd-clist.h gnome-cmd-clist.c \
	gnome-cmd-cmdline.h gnome-cmd-cmdline.c \
	gnome-cmd-combo.h gnome-cmd-combo.c \
	gnome-cmd-data.h gnome-cmd-data.c \
	gnome-cmd-delete-dialog.h gnome-cmd-delete-dialog.c \
	gnome-cmd-file-list.h gnome-cmd-file-list.c \
	gnome-cmd-file-popmenu.h gnome-cmd-file-popmenu.c \
	gnome-cmd-file-props-dialog.h gnome-cmd-file-props-dialog.c \
	gnome-cmd-file-selector.h gnome-cmd-file-selector.c \
	gnome-cmd-ftp-dialog.h gnome-cmd-ftp-dialog.c \
	gnome-cmd-list-popmenu.h gnome-cmd-list-popmenu.c \
	gnome-cmd-main-win.h gnome-cmd-main-win.c \
	gnome-cmd-main-menu.h gnome-cmd-main-menu.c \
	gnome-cmd-mkdir-dialog.h gnome-cmd-mkdir-dialog.c \
	gnome-cmd-options-dialog.h gnome-cmd-options-dialog.c \
	gnome-cmd-patternsel-dialog.h gnome-cmd-patternsel-dialog.c \
	gnome-cmd-pixmap.h gnome-cmd-pixmap.c \
	gnome-cmd-prepare-copy-dialog.h gnome-cmd-prepare-copy-dialog.c \
	gnome-cmd-prepare-move-dialog.h gnome-cmd-prepare-move-dialog.c \
	gnome-cmd-prepare-xfer-dialog.h gnome-cmd-prepare-xfer-dialog.c \
	gnome-cmd-quicksearch-popup.h gnome-cmd-quicksearch-popup.c \
	gnome-cmd-rename-dialog.h gnome-cmd-rename-dialog.c \
	gnome-cmd-make-copy-dialog.h gnome-cmd-make-copy-dialog.c \
	gnome-cmd-search-dialog.h gnome-cmd-search-dialog.c \
	gnome-cmd-style.h gnome-cmd-style.c \
	gnome-cmd-xfer.h gnome-cmd-xfer.c \
	gnome-cmd-xfer-progress-win.h gnome-cmd-xfer-progress-win.c

gnome_commander_LDADD = \
	$(GNOMEUI_LIBS) \
	$(GNOMEVFS_LIBS) \
	$(top_builddir)/libgcmd/libgcmd.la \
	$(top_builddir)/src/libgviewer/libgviewer.la

gnome_commander_LDFLAGS = \
	$(GNOMEUI_LIBS) \
	$(GNOMEVFS_LIBS)

gcmd_block_SOURCES = block.c
