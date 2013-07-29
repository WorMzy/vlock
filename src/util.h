/* util.h -- header for utility routines for vlock,
 *           the VT locking program for linux
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

#include <stddef.h>

struct timespec;

/* Parse the given string (interpreted as seconds) into a
 * timespec.  On error NULL is returned.  The caller is responsible
 * to free the result.   The string may be NULL, in which case NULL
 * is returned, too. */
struct timespec *parse_seconds(const char *s);

void fatal_error(const char *format, ...)
  __attribute__((noreturn, format(printf, 1, 2)));
void fatal_error_free(char *errmsg)
  __attribute__((noreturn));
void fatal_perror(const char *errmsg)
  __attribute__((noreturn));

#define STRERROR (errno ? strerror(errno) : "Unknown error")

#define GUARD_ERRNO(expr) \
  do { \
    int __errsv = errno; expr; errno = __errsv; \
  } while (0)
