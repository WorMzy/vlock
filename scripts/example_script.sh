#!/bin/sh
# example_script.sh -- example script for vlock,
#                      the VT locking program for linux
# 
# This program is copyright (C) 2007 Frank Benkstein, and is free software.  It
# comes without any warranty, to the extent permitted by applicable law.  You
# can redistribute it and/or modify it under the terms of the Do What The Fuck
# You Want To Public License, Version 2, as published by Sam Hocevar.  See
# http://sam.zoy.org/wtfpl/COPYING for more details.

set -e

# Declare dependencies.  Please see PLUGINS for their meaning.  Empty
# dependencies can be left out.
PRECEEDS="new all"
# SUCCEEDS=""
# REQUIRES=""
# NEEDS=""
DEPENDS="all"
# CONFLICTS=""


hooks() {
  # The name of the hook that should be executed is read as a string from
  # stdin.  This function should only exit when stdin hits end-of-file.

  while read hook_name ; do
    case "${hook_name}" in
      vlock_start)
        # do something here that should happen at the start of vlock
      ;;
      vlock_end)
        # do something here that should happen at the end of vlock
      ;;
      vlock_save)
        # start a screensaver type action here
      ;;
      vlock_save_abort)
        # abort a screensaver type action here
      ;;
    esac
  done
}

# Everything below is boilerplate code that shouldn't need to be changed.

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
