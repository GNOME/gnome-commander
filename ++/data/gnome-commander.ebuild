# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License, v2 or later
# Maintainer: Dirk Göttel <dgoettel@freenet.de>
# $Header:

inherit gnome2

DESCRIPTION="A full featured, dual-pane file manager for Gnome2"
HOMEPAGE="http://www.nongnu.org/gcmd/"

SRC_URI="http://ftp.gnome.org/pub/GNOME/sources/${PN}/1.2/${P}.tar.bz2";

KEYWORDS="alpha amd64 ia64 ppc ppc64 sparc x86"

LICENSE="GPL-2"

IUSE="doc exif iptc id3"
SLOT="0"

RDEPEND=">=x11-libs/gtk+-2.6.0
	>=gnome-base/gnome-vfs-2.0
	>=dev-libs/glib-2.0.0
	>=gnome-base/libgnomeui-2.0
	>=gnome-base/gconf-2.0
	|| (
		app-admin/gamin
		app-admin/fam
	)
	exif? ( media-libs/libexif )
	iptc? ( media-libs/libiptcdata )
	id3?  ( media-libs/id3lib )"

DEPEND="${RDEPEND}
	dev-util/intltool
	dev-util/pkgconfig"

src_compile() {
	./configure \
		--host=${CHOST} \
		--prefix=/usr \
		--infodir=/usr/share/info \
		--mandir=/usr/share/man || die "./configure failed"
	emake || die
}

DOCS="AUTHORS BUGS ChangeLog NEWS README TODO"

src_install () {
	emake DESTDIR=${D} install || die
}
