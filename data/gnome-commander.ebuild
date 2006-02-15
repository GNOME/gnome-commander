# Copyright 1999-2003 Gentoo Technologies, Inc.
# Distributed under the terms of the GNU General Public License, v2 or later
# Maintainer: Nuno Araujo <araujo_n@russo79.com>
# $Header$

S="${WORKDIR}/${P}"

DESCRIPTION="File Manager for Gnome"

SRC_URI="http://ftp.gnome.org/pub/GNOME/sources/gnome-commander/1.2/${P}.tar.bz2";

HOMEPAGE="http://www.nongnu.org/gcmd/"

LICENSE="GPL-2"

KEYWORDS="alpha amd64 ia64 ppc ppc64 sparc x86"

DEPEND="app-admin/gamin
	gnome-base/gnome-libs
	>=gnome-base/gconf-1.0.8
	<gnome-base/gconf-2
	<gnome-base/gnome-vfs-2"

src_compile() {
	./configure \
		--host=${CHOST} \
		--prefix=/usr \
		--infodir=/usr/share/info \
		--mandir=/usr/share/man || die "./configure failed"
	emake || die
}

src_install () {
	make DESTDIR=${D} install || die

	dodoc AUTHORS ChangeLog COPYING INSTALL NEWS README TODO
}
