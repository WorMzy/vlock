/* vlock_plugin.h -- header file for plugins for vlock,
 *                   the VT locking program for linux
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

extern const char *preceeds[];
extern const char *succeeds[];
extern const char *requires[];
extern const char *needs[];
extern const char *depends[];
extern const char *conflicts[];

bool vlock_start(void **);
bool vlock_end(void **);
bool vlock_save(void **);
bool vlock_save_abort(void **);
