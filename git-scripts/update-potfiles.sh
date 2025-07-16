#!/bin/bash

cat << EOF > po/POTFILES.in
# List of source files containing translatable strings.
# Please keep this file sorted alphabetically.
data/org.gnome.gnome-commander.desktop.in
data/org.gnome.gnome-commander.gschema.xml
data/org.gnome.gnome-commander.metainfo.xml.in
EOF

{
  grep -RHo 'gettextrs' src/ | cut -d: -f1 && \
  find src/ plugins/ -regex '.*\.\(cc\|\c\)$' -exec grep -lE "\bN?_\s*\(" '{}' \;
} | sort -u >> po/POTFILES.in
