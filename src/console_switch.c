/* console_switch.c -- console grabbing routines for vlock,
 *                     the VT locking program for linux
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

#if !defined(__FreeBSD__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/consio.h>
#else
#include <sys/vt.h>
#endif

#include "console_switch.h"

/* Is console switching currently disabled? */
bool console_switch_locked = false;

/* This handler is called whenever a user tries to
 * switch away from this virtual console. */
static void release_vt(int __attribute__ ((__unused__)) signum)
{
  /* Deny console switch. */
  (void) ioctl(STDIN_FILENO, VT_RELDISP, 0);
}

/* This handler is called whenever a user switches to this
 * virtual console. */
static void acquire_vt(int __attribute__ ((__unused__)) signum)
{
  /* Acknowledge console switch. */
  (void) ioctl(STDIN_FILENO, VT_RELDISP, VT_ACKACQ);
}

/* Console mode before switching was disabled. */
static struct vt_mode vtm;
/* Signal actions before console handling was disabled. */
static struct sigaction sa_usr1;
static struct sigaction sa_usr2;

/* Disable virtual console switching in the kernel.  If disabling fails false
 * is returned and errno is set. */
bool lock_console_switch(void)
{
  /* Console mode when switching is disabled. */
  struct vt_mode lock_vtm;
  struct sigaction sa;

  /* Get the virtual console mode. */
  if (ioctl(STDIN_FILENO, VT_GETMODE, &vtm) < 0) {
    if (errno == ENOTTY || errno == EINVAL)
      fprintf(stderr, "vlock: this terminal is not a virtual console\n");
    else
      perror("vlock: could not get virtual console mode");

    errno = 0;
    return false;
  }

  /* Copy the current virtual console mode. */
  lock_vtm = vtm;

  (void) sigemptyset(&(sa.sa_mask));
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = release_vt;
  (void) sigaction(SIGUSR1, &sa, &sa_usr1);
  sa.sa_handler = acquire_vt;
  (void) sigaction(SIGUSR2, &sa, &sa_usr2);

  /* Set terminal switching to be process governed. */
  lock_vtm.mode = VT_PROCESS;
  /* Set terminal release signal, i.e. sent when switching away. */
  lock_vtm.relsig = SIGUSR1;
  /* Set terminal acquire signal, i.e. sent when switching here. */
  lock_vtm.acqsig = SIGUSR2;
  /* Set terminal free signal, not implemented on either FreeBSD or Linux. */
  /* Linux ignores this but FreeBSD wants a valid signal number here. */
  lock_vtm.frsig = SIGHUP;

  /* Set virtual console mode to be process governed thus disabling console
   * switching through the signal handlers above. */
  if (ioctl(STDIN_FILENO, VT_SETMODE, &lock_vtm) < 0) {
    perror("vlock: disabling console switching failed");

    /* Reset signal handlers. */
    (void) sigaction(SIGUSR1, &sa_usr1, NULL);
    (void) sigaction(SIGUSR2, &sa_usr2, NULL);
    errno = 0;
    return false;
  }

  console_switch_locked = true;
  return true;
}

/* Reenable console switching if it was previously disabled. */
bool unlock_console_switch(void)
{
  if (ioctl(STDIN_FILENO, VT_SETMODE, &vtm) == 0) {
    /* Reset signal handlers. */
    (void) sigaction(SIGUSR1, &sa_usr1, NULL);
    (void) sigaction(SIGUSR2, &sa_usr2, NULL);

    return true;
  } else {
    perror("vlock: reenabling console switch failed");
    errno = 0;
    return false;
  }
}
