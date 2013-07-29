#!/bin/sh
# mplayer.sh -- mplayer pausing script for vlock,
#               the VT locking program for linux
#
# To use this script do run the following command
# $ mkfifo ~/.mplayer/control
# and add
# input=file=/home/<user>/.mplayer/control
# to your ~/.mplayer/config.
# 
# This program is copyright (C) 2007 Frank Benkstein, and is free software.  It
# comes without any warranty, to the extent permitted by applicable law.  You
# can redistribute it and/or modify it under the terms of the Do What The Fuck
# You Want To Public License, Version 2, as published by Sam Hocevar.  See
# http://sam.zoy.org/wtfpl/COPYING for more details.

set -e

DEPENDS="all"

hooks() {
  while read hook_name ; do
    case "${hook_name}" in
      vlock_start)
        if fuser "${HOME}/.mplayer/control" > /dev/null 2>&1 ; then
          echo "pausing seek -4" > "${HOME}/.mplayer/control"
        fi
      ;;
      vlock_end)
        if fuser "${HOME}/.mplayer/control" > /dev/null 2>&1 ; then
          echo "pause" > "${HOME}/.mplayer/control"
        fi
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
