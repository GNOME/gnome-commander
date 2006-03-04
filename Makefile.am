ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libgcmd src plugins po pixmaps doc data

distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper

EXTRA_DIST = autogen.sh         \
	intltool-*.in           \
	xmldocs.make 		\
	omf.make		\
	m4			\
	gnome-doc-utils.make

DISTCLEANFILES = 		\
	gnome-doc-utils.make    \
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper

AUTOMAKE_OPTIONS = dist-bzip2
