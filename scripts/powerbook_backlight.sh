#!/bin/sh
###########################################################################
# powerbook_backlight.sh, version 0.1.1
# 2007-10-15
#
# This vlock plugin script switches off the backlight on Apple Powerbook 
# laptops while locking. It requires the fblevel utility to be installed.
#
# Copyright (C) 2007 Rene Kuettner <rene@bitkanal.net>
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; version 2.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307  USA
#
###########################################################################

set -e

DEPENDS="all"

hooks() {
  while read hook_name ; do
    case "${hook_name}" in
      vlock_save)
        sudo fblevel off >/dev/null
      ;;
      vlock_save_abort)
        sudo fblevel on >/dev/null
      ;;
    esac
  done
}

if [ $# -ne 1 ] ; then
  echo >&2 "Usage: $0 <command>"
  exit 1
fi

case "$1" in
  hooks)
    hooks
  ;;
  preceeds)
    echo "${PRECEEDS}"
  ;;
  succeeds)
    echo "${SUCCEEDS}"
  ;;
  requires)
    echo "${REQUIRES}"
  ;;
  needs)
    echo "${NEEDS}"
  ;;
  depends)
    echo "${DEPENDS}"
  ;;
  conflicts)
    echo "${CONFLICTS}"
  ;;
  *)
    echo >&2 "$0: unknown command '$1'"
    exit 1
  ;;
esac
