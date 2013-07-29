/* ttyblank.c -- a console blanking plugin for vlock,
 *               the VT locking program for linux
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

#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/tiocl.h>

#include "vlock_plugin.h"

const char *depends[] = { "all", NULL };

bool vlock_save(void __attribute__ ((__unused__)) ** ctx)
{
  char arg[] = { TIOCL_BLANKSCREEN, 0 };
  return ioctl(STDIN_FILENO, TIOCLINUX, arg) == 0;
}

bool vlock_save_abort(void __attribute__ ((__unused__)) ** ctx)
{
  char arg[] = { TIOCL_UNBLANKSCREEN, 0 };
  return ioctl(STDIN_FILENO, TIOCLINUX, arg) == 0;
}
