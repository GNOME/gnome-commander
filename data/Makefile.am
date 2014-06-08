## Process this file with automake to produce Makefile.in.


desktopdir = $(datadir)/applications
desktop_in_files = @PACKAGE@.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

appdatadir = $(datadir)/appdata
appdata_in_files = @PACKAGE@.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

#BUILT_SOURCES = @PACKAGE@-@VERSION@.ebuild

ebuild_files = @PACKAGE@.ebuild
ebuild_DATA_NOINST = @PACKAGE@-@VERSION@.ebuild

@PACKAGE@-@VERSION@.ebuild: $(ebuild_files)
	ln -sf @PACKAGE@.ebuild @PACKAGE@-@VERSION@.ebuild

EXTRA_DIST = $(appdata_in_files) $(desktop_in_files) @PACKAGE@.spec $(ebuild_files) $(ebuild_DATA_NOINST)

DISTCLEANFILES = $(appdata_DATA) $(desktop_DATA)

-include $(top_srcdir)/git.mk
