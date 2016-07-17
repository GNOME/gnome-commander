#!/bin/sh
#name: Duplicate

# Written by puux <puuxmine@gmail.com> 2016
# Part of gnome-commander script plug-in system

for n in $@
do
  cp $n "1_$n"
done

