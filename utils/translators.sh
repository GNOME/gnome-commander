#!/bin/sh

# This script was written by Jonh Wendell <jwendell gnome org>

# usage: translators.sh <LAST_COMMIT_ID>

echo "UI translations:"
git log $1..HEAD --pretty=format:%an --name-only  -- po/*.po | sed -e :a -e '$!N;s|\npo/\(.*\)\.po| \(\1\)|;ta' | sort -u | sed '$!N;s/^\n//'

echo
echo "Doc translations:"
git log $1..HEAD --pretty=format:%an --name-only  -- doc/*/*.po | sed -e :a -e '$!N;s|\ndoc/.*/\(.*\)\.po| \(\1\)|;ta' | sort -u | sed '$!N;s/^\n//'
