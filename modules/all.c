/* all.c -- console grabbing plugin for vlock,
 *          the VT locking program for linux
 *
 * This program is copyright (C) 2007 Frank Benkstein, and is free
 * software which is freely distributable under the terms of the
 * GNU General Public License version 2, included as the file COPYING in this
 * distribution.  It is NOT public domain software, and any
 * redistribution not permitted by the GNU General Public License is
 * expressly forbidden without prior written permission from
 * the author.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>

#include "vlock_plugin.h"
#include "console_switch.h"

bool vlock_start(void __attribute__((unused)) **ctx_ptr)
{
  return lock_console_switch();
}

bool vlock_end(void __attribute__((unused)) **ctx_ptr)
{
  return unlock_console_switch();
}
