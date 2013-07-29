/* new.c -- console allocation plugin for vlock,
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
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/consio.h>
#else
#include <sys/vt.h>
#endif

#include "vlock_plugin.h"

const char *preceeds[] = { "all", NULL };
const char *requires[] = { "all", NULL };

/* name of the virtual console device */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define CONSOLE "/dev/ttyv0"
#else
#define CONSOLE "/dev/tty0"
#endif
/* template for the device of a given virtual console */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define VTNAME "/dev/ttyv%x"
#else
#define VTNAME "/dev/tty%d"
#endif

/* Get the currently active console from the given
 * console file descriptor.  Returns console number
 * (starting from 1) on success, -1 on error. */
#if defined(__FreeBSD__) || defined (__FreeBSD_kernel__)
static int get_active_console(int consfd)
{
  int n;

  if (ioctl(consfd, VT_GETACTIVE, &n) == 0)
    return n;
  else
    return -1;
}
#else
static int get_active_console(int consfd)
{
  struct vt_stat vtstate;

  /* get the virtual console status */
  if (ioctl(consfd, VT_GETSTATE, &vtstate) == 0)
    return vtstate.v_active;
  else
    return -1;
}
#endif

/* Get the device name for the given console number.
 * Returns the device name or NULL on error. */
static char *get_console_name(int n)
{
  static char name[sizeof VTNAME + 2];
  ssize_t namelen;

  if (n <= 0)
    return NULL;

  /* format the virtual terminal filename from the number */
#if defined(__FreeBSD__) || defined (__FreeBSD_kernel__)
  namelen = snprintf(name, sizeof name, VTNAME, n - 1);
#else
  namelen = snprintf(name, sizeof name, VTNAME, n);
#endif

  if (namelen > (ssize_t) sizeof name) {
    fprintf(stderr, "vlock-new: virtual terminal number too large\n");
    return NULL;
  } else if (namelen < 0) {
    fprintf(stderr, "vlock-new: error calculating terminal device name: %s\n", strerror(errno));
    return NULL;
  } else {
    return name;
  }
}

/* Change to the given console number using the given console
 * file descriptor. */
static int activate_console(int consfd, int vtno)
{
  int c = ioctl(consfd, VT_ACTIVATE, vtno);

  return c < 0 ? c : ioctl(consfd, VT_WAITACTIVE, vtno);
}

struct new_console_context {
  int consfd;
  int old_vtno;
  int new_vtno;
  int saved_stdin;
  int saved_stdout;
  int saved_stderr;
};

/* Run switch to a new console and redirect stdio there. */
bool vlock_start(void **ctx_ptr)
{
  struct new_console_context *ctx;
  int vtfd;
  char *vtname;

  /* Allocate the context. */
  if ((ctx = malloc(sizeof *ctx)) == NULL)
    return false;

  /* Try stdin first. */
  ctx->consfd = dup(STDIN_FILENO);

  /* Get the number of the currently active console. */
  ctx->old_vtno = get_active_console(ctx->consfd);

  if (ctx->old_vtno < 0) {
    /* stdin is does not a virtual console. */
    (void) close(ctx->consfd);

    /* XXX: add optional PAM check here */

    /* Open the virtual console directly. */
    if ((ctx->consfd = open(CONSOLE, O_RDWR)) < 0) {
      perror("vlock-new: cannot open virtual console");
      goto err;
    }

    /* Get the number of the currently active console, again. */
    ctx->old_vtno = get_active_console(ctx->consfd);

    if (ctx->old_vtno < 0) {
      perror("vlock-new: could not get the currently active console");
      goto err;
    }
  }

  /* Get a free virtual terminal number. */
  if (ioctl(ctx->consfd, VT_OPENQRY, &ctx->new_vtno) < 0) {
    perror("vlock-new: could not find a free virtual terminal");
    goto err;
  }

  /* Get the device name for the new virtual console. */
  vtname = get_console_name(ctx->new_vtno);

  /* Open the free virtual terminal. */
  if ((vtfd = open(vtname, O_RDWR)) < 0) {
    perror("vlock-new: cannot open new console");
    goto err;
  }

  /* Work around stupid X11 bug:  When switching immediately after the command
   * is entered, the enter button may get stuck. */
  if (getenv("DISPLAY") != NULL)
    sleep(1);

  /* Switch to the new virtual terminal. */
  if (activate_console(ctx->consfd, ctx->new_vtno) < 0) {
    perror("vlock-new: could not activate new terminal");
    goto err;
  }

  /* Save the stdio file descriptors. */
  ctx->saved_stdin = dup(STDIN_FILENO);
  ctx->saved_stdout = dup(STDOUT_FILENO);
  ctx->saved_stderr = dup(STDERR_FILENO);

  /* Redirect stdio to virtual terminal. */
  (void) dup2(vtfd, STDIN_FILENO);
  (void) dup2(vtfd, STDOUT_FILENO);
  (void) dup2(vtfd, STDERR_FILENO);

  /* Close virtual terminal file descriptor. */
  (void) close(vtfd);

  *ctx_ptr = ctx;
  return true;

err:
  errno = 0;
  free(ctx);
  return false;
}

/* Redirect stdio back und switch to the previous console. */
bool vlock_end(void **ctx_ptr)
{
  struct new_console_context *ctx = *ctx_ptr;

  if (ctx == NULL)
    return true;

  /* Restore saved stdio file descriptors. */
  (void) dup2(ctx->saved_stdin, STDIN_FILENO);
  (void) dup2(ctx->saved_stdout, STDOUT_FILENO);
  (void) dup2(ctx->saved_stderr, STDERR_FILENO);

  /* Switch back to previous virtual terminal. */
  if (activate_console(ctx->consfd, ctx->old_vtno) < 0)
    perror("vlock-new: could not activate previous console");

#if !defined(__FreeBSD__) && !defined(__FreeBSD_kernel__)
  /* Deallocate virtual terminal. */
  if (ioctl(ctx->consfd, VT_DISALLOCATE, ctx->new_vtno) < 0)
    perror("vlock-new: could not disallocate console");
#endif

  (void) close(ctx->consfd);
  free(ctx);

  return true;
}
