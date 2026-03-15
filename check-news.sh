#!/bin/sh
#
# SPDX-FileCopyrightText: 2019 Red Hat, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Author: Bastien Nocera <hadess@hadess.net>

# Add to your top-level meson.build to check for an updated NEWS file
# when doing a "dist" release, similarly to automake's check-news:
# https://www.gnu.org/software/automake/manual/html_node/List-of-Automake-options.html
#
# Checks NEWS for the version number:
# meson.add_dist_script(
#   find_program('check-news.sh').path(),
#   '@0@'.format(meson.project_version())
# )
#
# Checks NEWS and data/foo.metainfo.xml for the version number:
# meson.add_dist_script(
#   find_program('check-news.sh').path(),
#   '@0@'.format(meson.project_version()),
#   'NEWS',
#   'data/foo.metainfo.xml'
# )

usage()
{
	echo "$0 VERSION [FILES...]"
	exit 1
}

check_version()
{
	VERSION=$1
	# Look in the first 15 lines for NEWS files, but look
	# everywhere for other types of files
	if [ "$2" = "NEWS" ]; then
		DATA=`sed 15q $SRC_ROOT/"$2"`
	else
		DATA=`cat $SRC_ROOT/"$2"`
	fi
	case "$DATA" in
	*"$VERSION"*)
		:
		;;
	*)
		echo "$2 not updated; not releasing" 1>&2;
		exit 1
		;;
	esac
}

SRC_ROOT=${MESON_DIST_ROOT:-"./"}

if [ $# -lt 1 ] ; then usage ; fi

VERSION=$1
shift

if [ $# -eq 0 ] ; then
	check_version $VERSION 'NEWS'
	exit 0
fi

for i in $@ ; do
	check_version $VERSION "$i"
done

exit 0
