/* console_switch.c -- header file for the console grabbing
 *                     routines for vlock, the VT locking program
 *                     for linux
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

#include <stdbool.h>

/* Is console switching currently disabled? */
extern bool console_switch_locked;

/* Disable virtual console switching in the kernel.  On failure false
 * is returned and errno is set. */
bool lock_console_switch(void);
/* Reenable console switching if it was previously disabled.  On failure false
 * is returned and errno is set. */
bool unlock_console_switch(void);
