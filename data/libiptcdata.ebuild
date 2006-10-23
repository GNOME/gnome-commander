# Copyright 1999-2004 Gentoo Technologies, Inc.
# Distributed under the terms of the GNU General Public License, v2 or later
# Maintainer: Dirk Göttel <dgoettel@freenet.de>

DESCRIPTION="library for manipulating the International Press Telecommunications Council (IPTC) metadata"
HOMEPAGE="http://libiptcdata.sourceforge.net/"

SRC_URI="http://download.sourceforge.net/${PN}/${P}.tar.gz"

KEYWORDS="alpha amd64 ia64 ppc ppc64 sparc x86"

LICENSE="GPL-2"

IUSE="doc"
SLOT="0"


src_install () {
	emake DESTDIR=${D} install || die
	dodoc ABOUT-NLS AUTHORS ChangeLog COPYING INSTALL NEWS README TODO
}
