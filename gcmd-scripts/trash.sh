#!/bin/sh
#name: Move to trash

# Written by puux <puuxmine@gmail.com> 2016
# Part of gnome-commander script plug-in system

for n in $@
do
  mv $n ~/.local/share/Trash/
done

