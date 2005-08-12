SUBDIRS = libgcmd src po pixmaps doc plugins

desktopdir = $(datadir)/applications
desktop_in_files = @PACKAGE@.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = autogen.sh mkinstalldirs @PACKAGE@.desktop @PACKAGE@.spec intltool-*.in

DISTCLEANFILES = intltool-*
