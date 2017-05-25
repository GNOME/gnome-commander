ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libgcmd src plugins po pixmaps doc data tests

distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper

EXTRA_DIST =                \
	BUGS                    \
	autogen.sh              \
	mkinstalldirs           \
	ChangeLog-20090629      \
	xmldocs.make            \
	omf.make                \
	m4                      \
	gnome-doc-utils.make    \
	AUTHORS                 \
	ChangeLog               \
	INSTALL                 \
	NEWS                    \
	README

DISTCLEANFILES =            \
	po/.intltool-merge-cache

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper

AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip

distclean-local:
	if test "$(srcdir)" = "."; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log gnome-commander_1_2_8.. --stat -M -C --name-status  --date=short --no-color) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

#
# Add filenames to this variable if the should be added to .gitignore automatically
#
GITIGNOREFILES = \
	doc/doxygen/ \
	data/gnome-commander.desktop.in \
	data/gnome-commander.ebuild \
	data/gnome-commander.spec \
	po/gnome-commander.pot \
	.geanyprj \
	INSTALL \
	README \
	py-compile \
	translators.sh

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_GETTEXT) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL)

# Remove targets if the command fails
.DELETE_ON_ERROR:

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
