SUBDIRS = libgcmd src plugins po pixmaps doc data

desktopdir = $(datadir)/applications
desktop_in_files = @PACKAGE@.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = autogen.sh $(desktop_in_files) @PACKAGE@.spec intltool-*.in

DISTCLEANFILES = $(desktop_DATA) intltool-*
