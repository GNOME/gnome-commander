#!/bin/bash
#-------------------------------------------------------------------------------
# Copyright (C) 2018-2020 Andreas Redmer <ar-appleflinger@abga.be.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#-------------------------------------------------------------------------------

# This script is copied from https://gitlab.com/ar-/apple-flinger/-/blob/master/scripts/update-project-status.sh
# and adapted to collect information about the currently available translations.

translations=`ls po/*.po -1 | wc -l`

echo "{\"translations\":\"$translations\"}" > /tmp/status.json

diff /tmp/status.json status.json
DIFF_RETURN=$?

if [ $DIFF_RETURN != "0" ]
then
  echo "New translation updates found. Updating status file. Commit again"
  cat /tmp/status.json > status.json
  exit 1
fi
