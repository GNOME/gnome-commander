#!/bin/sh
#
# SPDX-FileCopyrightText: 2016 puux <puuxmine@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

#name: Move to trash

# Part of gnome-commander script plug-in system

for n in $@
do
  mv $n ~/.local/share/Trash/
done

