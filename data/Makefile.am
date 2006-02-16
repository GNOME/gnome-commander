## Process this file with automake to produce Makefile.in.


desktopdir = $(datadir)/applications
desktop_in_files = @PACKAGE@.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

#BUILT_SOURCES = @PACKAGE@-@VERSION@.ebuild

ebuild_files = @PACKAGE@.ebuild
ebuild_DATA_NOINST = @PACKAGE@-@VERSION@.ebuild

@PACKAGE@-@VERSION@.ebuild: $(ebuild_files)
	ln -sf @PACKAGE@.ebuild @PACKAGE@-@VERSION@.ebuild

EXTRA_DIST = $(desktop_in_files) @PACKAGE@.spec $(ebuild_files) $(ebuild_DATA_NOINST)

DISTCLEANFILES = $(desktop_DATA)
