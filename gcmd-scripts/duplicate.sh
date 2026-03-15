#!/bin/sh
#
# SPDX-FileCopyrightText: 2016 puux <puuxmine@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

#name: Duplicate
#term: true

# Part of gnome-commander script plug-in system

for n in "$@"
do
  cp "$n" "1_$n"
  echo "cp \"$n\" \"1_$n\""
done

