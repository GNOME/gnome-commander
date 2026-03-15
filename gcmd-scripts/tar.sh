#!/bin/sh
#
# SPDX-FileCopyrightText: 2016 puux <puuxmine@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

#name: *.tar.gz

# Part of gnome-commander script plug-in system

tar -cf archive.tar.gz "$@" -z

