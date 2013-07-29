/* nosysrq.c -- SysRq protection plugin for vlock,
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "vlock_plugin.h"

const char *preceeds[] = { "new", "all", NULL };
const char *depends[] = { "all", NULL };

#define SYSRQ_PATH "/proc/sys/kernel/sysrq"
#define SYSRQ_DISABLE_VALUE "0\n"

struct sysrq_context {
  FILE *file;
  char value[32];
};

/* Disable SysRq and save old value in context. */
bool vlock_start(void **ctx_ptr)
{
  struct sysrq_context *ctx;

  /* Allocate the context. */
  if ((ctx = malloc(sizeof *ctx)) == NULL)
    return false;

  /* XXX: add optional PAM check here */

  /* Open the SysRq sysctl file for reading and writing. */
  if ((ctx->file = fopen(SYSRQ_PATH, "r+")) == NULL) {
    perror("vlock-nosysrq: could not open '" SYSRQ_PATH "'");
    if (errno == ENOENT)
      goto nothing_to_do;
    else
      goto err;
  }

  /* Read the old value. */
  if (fgets(ctx->value, sizeof ctx->value, ctx->file) == NULL) {
    perror("vlock-nosysrq: could not read from '" SYSRQ_PATH "'");
    goto err;
  }

  /* Check whether all data was read. */
  if (feof(ctx->file) != 0) {
    fprintf(stderr, "vlock-nosysrq: sysrq buffer to small: %zu\n",
            sizeof ctx->value);
    goto err;
  }

  /* Check if SysRq was already disabled. */
  if (strcmp(SYSRQ_DISABLE_VALUE, ctx->value) == 0)
    goto nothing_to_do;

  /* Disable SysRq. */
  if (fseek(ctx->file, 0, SEEK_SET) < 0
      || ftruncate(fileno(ctx->file), 0) < 0
      || fputs(SYSRQ_DISABLE_VALUE, ctx->file) < 0
      || fflush(ctx->file) < 0) {
    perror("vlock-nosysrq: could not write disable value to '" SYSRQ_PATH "'");
    goto err;
  }

  *ctx_ptr = ctx;
  return true;

nothing_to_do:
  free(ctx);
  *ctx_ptr = NULL;
  return true;

err:
  errno = 0;
  free(ctx);
  return false;
}


/* Restore old SysRq value. */
bool vlock_end(void **ctx_ptr)
{
  struct sysrq_context *ctx = *ctx_ptr;

  if (ctx == NULL)
    return true;

  /* Restore SysRq. */
  if (fseek(ctx->file, 0, SEEK_SET) < 0
      || ftruncate(fileno(ctx->file), 0) < 0
      || fputs(ctx->value, ctx->file) < 0 || fflush(ctx->file) < 0)
    perror("vlock-nosysrq: could not write old value to '" SYSRQ_PATH "'");

  free(ctx);
  return true;
}
