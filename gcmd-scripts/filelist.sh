#!/bin/sh
#
# SPDX-FileCopyrightText: 2016 puux <puuxmine@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

#name: File list
#term: true

# Part of gnome-commander script plug-in system

echo "Enter file name:"
read list

for n in "$@"
do
  echo "$n" >> "$list"
  echo "echo \"$n\" >> \"$list\""
done

