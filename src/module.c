/* module.c -- module routines for vlock, the VT locking program for linux
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

/* Modules are shared objects that are loaded into vlock's address space. */
/* They can define certain functions that are called through vlock's plugin
 * mechanism.  They should also define dependencies if they depend on other
 * plugins of have to be called before or after other plugins. */

#if !defined(__FreeBSD__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>

#include <sys/types.h>

#include "list.h"
#include "util.h"

#include "plugin.h"

static bool init_module(struct plugin *p);
static void destroy_module(struct plugin *p);
static bool call_module_hook(struct plugin *p, const char *hook_name);

struct plugin_type *module = &(struct plugin_type){
  .init = init_module,
  .destroy = destroy_module,
  .call_hook = call_module_hook,
};

/* A hook function as defined by a module. */
typedef bool (*module_hook_function)(void **);

struct module_context
{
  /* Handle returned by dlopen(). */
  void *dl_handle;
  /* Pointer to be used by the modules. */
  void *module_data;
  /* Array of hook functions befined by a single module.  Stored in the same
   * order as the global hooks. */
  module_hook_function hooks[nr_hooks];
};


/* Initialize a new plugin as a module. */
bool init_module(struct plugin *p)
{
  char *path;
  struct module_context *context;

  if (asprintf(&path, "%s/%s.so", VLOCK_MODULE_DIR, p->name) < 0) {
    errno = ENOMEM;
    return false;
  }

  /* Test for access.  This must be done manually because vlock most likely
   * runs as a setuid executable and would otherwise override restrictions.
   * Also dlopen doesn't set errno on error. */
  if (access(path, R_OK) < 0) {
    int errsv = errno;
    free(path);
    errno = errsv;
    return false;
  }

  context = malloc(sizeof *context);

  if (context == NULL) {
    int errsv = errno;
    free(path);
    errno = errsv;
    return false;
  }

  /* Open the module as a shared library. */
  context->dl_handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);

  free(path);

  if (context->dl_handle == NULL) {
    errno = 0;
    free(context);
    return false;
  }

  /* Initialisation complete.  From now on cleanup is handled by destroy_module(). */
  p->context = context;

  /* Load all the hooks.  Unimplemented hooks are NULL and will not be called later. */
  for (size_t i = 0; i < nr_hooks; i++)
    *(void **) (&context->hooks[i]) = dlsym(context->dl_handle, hooks[i].name);

  /* Load all dependencies.  Unspecified dependencies are NULL. */
  for (size_t i = 0; i < nr_dependencies; i++) {
    const char *(*dependency)[] = dlsym(context->dl_handle, dependency_names[i]);

    /* Append array elements to list. */
    for (size_t j = 0; dependency != NULL && (*dependency)[j] != NULL; j++) {
      char *s = strdup((*dependency)[j]);

      if (s == NULL)
        return false;

      list_append(p->dependencies[i], s);
    }
  }

  return true;
}

static void destroy_module(struct plugin *p)
{
  struct module_context *context = p->context;

  if (context != NULL) {
    dlclose(context->dl_handle);
    free(context);
  }
}

static bool call_module_hook(struct plugin *p, const char *hook_name)
{
  struct module_context *context = p->context;

  /* Find the right hook index. */
  for (size_t i = 0; i < nr_hooks; i++)
    if (strcmp(hooks[i].name, hook_name) == 0) {
      module_hook_function hook = context->hooks[i];

      if (hook != NULL)
        return hook(&context->module_data);
    }

  return true;
}
