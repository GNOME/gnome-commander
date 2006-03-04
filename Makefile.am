ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libgcmd src plugins po pixmaps doc data

EXTRA_DIST = autogen.sh         \
	intltool-*.in           \
	m4

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper

AUTOMAKE_OPTIONS = dist-bzip2
