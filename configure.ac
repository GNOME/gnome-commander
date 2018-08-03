AC_INIT([gnome-commander],[1.9.0],[https://gitlab.gnome.org/GNOME/gnome-commander/issues])
AC_CONFIG_SRCDIR(src/gnome-cmd-includes.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE

YELP_HELP_INIT

AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

AC_USE_SYSTEM_EXTENSIONS

GLIB_GSETTINGS

AX_IS_RELEASE([minor-version])
AX_COMPILER_FLAGS
AX_COMPILER_FLAGS_CXXFLAGS
AX_CXX_COMPILE_STDCXX_11

AC_PROG_CC
AC_PROG_CXX
AM_PROG_LEX
LT_INIT

dnl ===================
dnl Gettext stuff
dnl ===================

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])

dnl ===================
dnl Check for libraries
dnl ===================

GLIB_REQ=2.44.0
GMODULE_REQ=2.0.0
GTK_REQ=2.18.0
GNOME_REQ=2.0.0
GNOMEUI_REQ=2.4.0
GNOMEVFS_REQ=2.0.0
GNOME_KEYRING_REQ=2.22
UNIQUE_REQ=0.9.3
PYTHON_REQ=2.5
EXIV2_REQ=0.14
TAGLIB_REQ=1.4
LIBGSF_REQ=1.12.0
POPPLER_REQ=0.18

AC_SUBST(GLIB_REQ)
AC_SUBST(GTK_REQ)
AC_SUBST(GNOME_REQ)
AC_SUBST(GNOMEUI_REQ)
AC_SUBST(GNOMEVFS_REQ)
AC_SUBST(GNOME_KEYRING_REQ)
AC_SUBST(UNIQUE_REQ)
AC_SUBST(PYTHON_REQ)
AC_SUBST(EXIV2_REQ)
AC_SUBST(TAGLIB_REQ)
AC_SUBST(LIBGSF_REQ)
AC_SUBST(POPPLER_REQ)

dnl Google Test library for 'make check'
dnl This variable is currently used only in gentoo ebuild.
GTEST_REQ=1.7.0
AC_SUBST(GTEST_REQ)

dnl Check for glib
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= ${GLIB_REQ}])

dnl Check for gobject
PKG_CHECK_MODULES([GOBJECT],[gobject-2.0])

dnl Check for gmodule
PKG_CHECK_MODULES([GMODULE], [gmodule-2.0 >= ${GMODULE_REQ}])

dnl Check for gtk+
PKG_CHECK_MODULES([GTK], [gtk+-2.0 >= ${GTK_REQ}])

dnl Check for libgnome
PKG_CHECK_MODULES([GNOME], [libgnome-2.0 >= ${GNOME_REQ}])

dnl Check for libgnomeui
PKG_CHECK_MODULES([GNOMEUI], [libgnomeui-2.0 >= ${GNOMEUI_REQ}])

dnl Check for gnome-vfs
PKG_CHECK_MODULES([GNOMEVFS], [gnome-vfs-2.0 >= ${GNOMEVFS_REQ}], have_gnomevfs=yes, have_gnomevfs=no)

dnl Check for gnome-keyring
PKG_CHECK_MODULES(GNOME_KEYRING, [gnome-keyring-1 >= ${GNOME_KEYRING_REQ}])

dnl Check for libUnique
AC_ARG_WITH([unique], AS_HELP_STRING([--without-unique], [disable unique support]))
AS_IF([test "x$with_unique" != "xno"],
   [
      PKG_CHECK_MODULES([UNIQUE], [unique-1.0 >= ${UNIQUE_REQ}], have_unique=yes, have_unique=no)
   ],
   [test "x$with_unique" = "xno"], [have_unique=no])
if test "x$have_unique" = "xyes"; then
   AC_DEFINE(HAVE_UNIQUE, 1, [Define to 1 if you have unique support])
else
   AC_MSG_WARN([unique >= ${UNIQUE_REQ} not found, disabling unique support])
fi

dnl Check for samba support
AC_ARG_WITH(samba, [AS_HELP_STRING([--without-samba], [disable SAMBA support])])
have_samba=no
if test "x$with_samba" != "xno"; then
   if test "x$have_gnomevfs" != "xyes"; then
   have_samba=no
   else
   have_samba=yes
   fi
fi
if test "x$have_samba" = "xyes"; then
   AC_DEFINE(HAVE_SAMBA, 1, [Define to 1 if you have SAMBA support])
fi
AM_CONDITIONAL([HAVE_SAMBA],[test "x$have_samba" = "xyes"])


dnl Check for exiv2 support
AC_ARG_WITH(exiv2, [AS_HELP_STRING([--without-exiv2], [disable EXIF and IPTC support])])
have_exiv2=no
if test x$with_exiv2 != xno; then
    PKG_CHECK_MODULES(EXIV2, exiv2 >= $EXIV2_REQ, have_exiv2=yes, have_exiv2=no)
fi
if test "x$have_exiv2" = "xyes"; then
   AC_DEFINE(HAVE_EXIV2, 1, [Define to 1 if you have EXIF and IPTC support])
fi


dnl Check for taglib support
AC_ARG_WITH(taglib, [AS_HELP_STRING([--without-taglib], [disable taglib (ID3, Vorbis, APE and FLAC) support])])
have_taglib=no
if test x$with_taglib != xno; then
    PKG_CHECK_MODULES(TAGLIB, taglib >= $TAGLIB_REQ, have_taglib=yes, have_taglib=no)
fi
if test "x$have_taglib" = "xyes"; then
   AC_DEFINE(HAVE_ID3, 1, [Define to 1 if you have taglib (ID3, Vorbis, APE and FLAC) support])
fi


dnl Check for libchm support
AC_ARG_WITH(libchm, [AS_HELP_STRING([--without-libchm], [disable CHM support])])
have_chm=no
AC_CHECK_LIB(chm, chm_open, have_chm=yes, have_chm=no)
AM_CONDITIONAL(HAVE_CHM, test "x$have_chm" = "xyes")


dnl Check for libgsf support
AC_ARG_WITH(libgsf, [AS_HELP_STRING([--without-libgsf], [disable OLE and ODF support])])
have_gsf=no
if test x$with_libgsf != xno; then
    PKG_CHECK_MODULES(GSF, libgsf-1 >= $LIBGSF_REQ, have_gsf=yes, have_gsf=no)
fi
if test "x$have_gsf" = "xyes"; then
    PKG_CHECK_MODULES(GSF_1_14_24, libgsf-1 >= 1.14.24,
		      AC_DEFINE(HAVE_GSF_1_14_24,1,[Define to 1 if you have libgsf >= 1.14.24]),
		      have_gsf_1_14_24=no)
    PKG_CHECK_MODULES(GSF_1_14_26, libgsf-1 >= 1.14.26,
		      AC_DEFINE(HAVE_GSF_1_14_26,1,[Define to 1 if you have libgsf >= 1.14.26]),
		      have_gsf_1_14_26=no)
fi
if test "x$have_gsf" = "xyes"; then
   AC_DEFINE(HAVE_GSF, 1, [Define to 1 if you have libgsf (OLE and ODF) support])
fi


dnl Check for poppler support
AC_ARG_WITH([poppler], [AS_HELP_STRING([--without-poppler], [disable PDF support])])
have_pdf=no
if test x$with_poppler != xno; then
    PKG_CHECK_MODULES(POPPLER, poppler-glib >= $POPPLER_REQ, have_pdf=yes, have_pdf=no)
fi
if test "x$have_pdf" = "xyes"; then
   AC_DEFINE(HAVE_PDF, 1, [Define to 1 if you have PDF support])
fi


dnl =====================
dnl     Google Test
dnl =====================

# Configure pthreads.
# ACX_PTHREAD([have_pthread=yes])

# Define gtest variables
if test "x$have_pthread" = "xyes"; then
GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1"
GTEST_CXXFLAGS="$PTHREAD_CFLAGS"
GTEST_LDFLAGS=
GTEST_LIBS="$PTHREAD_LIBS"
else
GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0"
GTEST_CXXFLAGS=
GTEST_LDFLAGS=
GTEST_LIBS=
fi
AC_SUBST([GTEST_CPPFLAGS])
AC_SUBST([GTEST_CXXFLAGS])
AC_SUBST([GTEST_LDFLAGS])
AC_SUBST([GTEST_LIBS])

AC_MSG_RESULT([
Google Test is now configured (for \"make check\")

    GTEST_CPPFLAGS  : $GTEST_CPPFLAGS
    GTEST_CXXFLAGS  : $GTEST_CXXFLAGS
    GTEST_LDFLAGS   : $GTEST_LDFLAGS
    GTEST_LIBS      : $GTEST_LIBS
])

dnl =============================
dnl Checks for library functions
dnl =============================

AC_FUNC_MMAP

dnl =====================
dnl Set stuff in config.h
dnl =====================

dnl Get the prefix
if test "x${prefix}" = "xNONE"; then
  PREFIX="${ac_default_prefix}"
  AC_DEFINE_UNQUOTED(PREFIX, "${ac_default_prefix}", [definition of PREFIX])
else
  PREFIX="${prefix}"
  AC_DEFINE_UNQUOTED(PREFIX, "${prefix}", [definition of PREFIX])
fi
AC_SUBST(PREFIX)


dnl Set PIXMAPS_DIR in config.h.
AC_DEFINE_UNQUOTED(PIXMAPS_DIR, "${PREFIX}/share/pixmaps/${PACKAGE}", [pixmaps location])


dnl Set GNOME_PREFIX in config.h.
GNOME_PREFIX_TMP=`pkg-config --variable=prefix libgnome-2.0`
AC_DEFINE_UNQUOTED(GNOME_PREFIX, "$GNOME_PREFIX_TMP", "")


PLUGIN_LIBTOOL_FLAGS="-module -avoid-version"
AC_SUBST(PLUGIN_LIBTOOL_FLAGS)

dnl =====================
dnl Check for binary
dnl =====================
AC_CHECK_PROG(HAVE_UPDATE_MIME_DATABASE, update-mime-database, "yes", "no")
AC_SUBST(HAVE_UPDATE_MIME_DATABASE)


dnl =====================
dnl Write some output
dnl =====================

AC_CONFIG_FILES([
Makefile
data/gnome-commander.ebuild
data/gnome-commander.spec
data/gnome-commander.desktop.in
data/Makefile
doc/Makefile
libgcmd/Makefile
pixmaps/Makefile
pixmaps/device-icons/Makefile
pixmaps/file-type-icons/Makefile
pixmaps/mime-icons/Makefile
plugins/Makefile
plugins/fileroller/Makefile
plugins/test/Makefile
po/Makefile.in
src/Makefile
src/dialogs/Makefile
src/tags/Makefile
src/intviewer/Makefile
tests/Makefile
README
])
AC_OUTPUT



echo ""
echo ""
echo "=========================================="
echo " Ready to build GNOME Commander ${VERSION}"
echo "=========================================="
echo ""
echo "The following settings will be used:"
echo ""
echo "  host             : ${host}"
echo "  prefix           : ${prefix}"
echo "  compiler         : ${CC}"
echo "  CXXFLAGS         : ${CXXFLAGS}"
echo "  LDFLAGS          : ${LDFLAGS}"
echo ""
echo "  libunique support:      ${have_unique}"
echo "  Python plugins support: ${enable_python}"
echo "  Samba support:          ${have_samba}"
echo ""
echo "Optional file metadata support:"
echo ""
echo "  Exif support   : ${have_exiv2}"
echo "  IPTC support   : ${have_exiv2}"
echo "  ID3 support    : ${have_taglib}"
echo "  Vorbis support : ${have_taglib}"
echo "  APE support    : ${have_taglib}"
echo "  FLAC support   : ${have_taglib}"
echo "  OLE2 support   : ${have_gsf}"
echo "  ODF support    : ${have_gsf}"
echo "  PDF support    : ${have_pdf}"
echo ""
echo "Type 'make' to build $PACKAGE-$VERSION and then 'make install' to install"
echo ""
