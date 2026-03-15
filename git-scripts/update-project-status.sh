#!/bin/bash
#-------------------------------------------------------------------------------
# SPDX-FileCopyrightText: 2018 Andreas Redmer <ar-appleflinger@abga.be.com>
# SPDX-FileCopyrightText: 2022 Uwe Scholz <u.scholz83@gmx.de>
#
# SPDX-License-Identifier: GPL-3.0-or-later
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
