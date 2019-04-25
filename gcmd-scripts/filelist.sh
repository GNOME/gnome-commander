#!/bin/sh
#name: File list
#term: true

# Written by puux <puuxmine@gmail.com> 2016
# Part of gnome-commander script plug-in system

echo "Enter file name:"
read list

for n in "$@"
do
  echo "$n" >> "$list"
  echo "echo \"$n\" >> \"$list\""
done

