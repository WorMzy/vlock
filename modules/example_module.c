/* example_module.c -- example module for vlock,
 *                     the VT locking program for linux
 *
 * This program is copyright (C) 2007 Frank Benkstein, and is free
 * software.  It comes without any warranty, to the extent permitted by
 * applicable law.  You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar.  See http://sam.zoy.org/wtfpl/COPYING
 * for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* Do not use any vlock specific headers here unless you intend to
 * submit your module for inclusion. */

/* Include this header file to make sure the types of the dependencies
 * and hooks are correct. */
#include "vlock_plugin.h"

/* Declare dependencies.  Please see PLUGINS for their meaning.  Empty
 * dependencies can be left out. */
const char *preceeds[] = { "new", "all", NULL };
/* const char *succeeds[]; */
/* const char *requires[]; */
/* const char *needs[]; */
const char *depends[] = { "all", NULL };
/* const char *conflicts[]; */

/* Every hook has a void** argument ctx_ptr.  When they are called
 * ctx_ptr points to the same location and *ctx_ptr is initially set to
 * NULL.  Hook functions should pass state by defining a context struct
 * and saving the pointer to it in *ctx_ptr instead of using global
 * variables.
 */

struct example_context {
  int a;
  int b;
};

/* Do something that should happen at vlock's start here.  An error in
 * this hook aborts vlock. */
bool vlock_start(void **ctx_ptr)
{
  struct example_context *ctx = malloc(sizeof *ctx);

  if (ctx == NULL)
    return false;

  ctx->a = 23;
  ctx->b = 42;

  /* Save the context for use by the other hooks. */
  *ctx_ptr = ctx;

  return true;
}

/* Hooks that are not implemented should not be defined. */

/* Start a screensaver type action before the password prompt after a
 * timeout.  This hook must not block! */
/* bool vlock_save(void **); */

/* Abort a screensaver type action before the password prompt after a
 * timeout.  This hook must not block! */
/* bool vlock_save_abort(void **); */

/* Do something at the end of vlock.  Error returns are ignored here. */
bool vlock_end(void **ctx_ptr)
{
  struct example_context *ctx = *ctx_ptr;
  bool result = true;

  if (ctx != NULL) {
    result = (ctx->a == 23 && ctx->b == 42);

    free(ctx);

    if (!result)
      fprintf(stderr, "vlock-example_module: Whoops!\n");
  }

  return result;
}
