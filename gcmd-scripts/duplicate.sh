#!/bin/sh
#name: Duplicate
#term: true

# Written by puux <puuxmine@gmail.com> 2016
# Part of gnome-commander script plug-in system

for n in "$@"
do
  cp "$n" "1_$n"
  echo "cp \"$n\" \"1_$n\""
done

