/* process.h -- header for child process routines for vlock,
 *              the VT locking program for linux
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
#include <sys/types.h>

/* Wait for the given amount of time for the death of the given child process.
 * If the child process dies in the given amount of time or already was dead
 * true is returned and false otherwise. */
bool wait_for_death(pid_t pid, long sec, long usec);

/* Try hard to kill the given child process. */
void ensure_death(pid_t pid);

#define NO_REDIRECT (-2)
#define REDIRECT_DEV_NULL (-3)
#define REDIRECT_PIPE (-4)

struct child_process {
  /* Function that will be run in the child. */
  int (*function)(void *argument);
  /* Argument for the function. */
  void *argument;
  /* First argument to execv. */
  const char *path;
  /* Second argument to execv. */
  const char *const *argv;
  /* The child's stdin. */
  int stdin_fd;
  /* The child's stdout. */
  int stdout_fd;
  /* The child's stderr. */
  int stderr_fd;
  /* The child's PID. */
  pid_t pid;
};

/* Create a new child process.  All file descriptors except stdin, stdout and
 * stderr are closed and privileges are dropped.  All fields of the child
 * struct except pid must be set.  If a stdio file descriptor field has the
 * special value of REDIRECT_DEV_NULL it is redirected from or to /dev/null.
 * If it has the value REDIRECT_PIPE a pipe will be created and one end will be
 * connected to the respective descriptor of the child.  The file descriptor of
 * the other end is stored in the field after the call.  It is up to the caller
 * to close the pipe descriptor(s). */
bool create_child(struct child_process *child);
