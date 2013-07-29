/* plugin.c -- generic plugin routines for vlock,
 *             the VT locking program for linux
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "list.h"

#include "plugin.h"
#include "util.h"

/* Allocate a new plugin struct. */
struct plugin *new_plugin(const char *name, struct plugin_type *type)
{
  struct plugin *p = malloc(sizeof *p);
  char *last_slash;

  if (p == NULL)
    return NULL;

  /* For security plugin names must not contain a slash. */
  last_slash = strrchr(name, '/');

  if (last_slash != NULL)
    name = last_slash+1;

  p->name = strdup(name);

  if (p->name == NULL) {
    free(p);
    return NULL;
  }

  p->context = NULL;
  p->save_disabled = false;

  for (size_t i = 0; i < nr_dependencies; i++)
    p->dependencies[i] = list_new();

  p->type = type;

  if (p->type->init(p)) {
    return p;
  } else {
    destroy_plugin(p);
    return NULL;
  }
}

/* Destroy the given plugin. */
void destroy_plugin(struct plugin *p)
{
  /* Call destroy method. */
  p->type->destroy(p);

  /* Destroy dependency lists. */
  for (size_t i = 0; i < nr_dependencies; i++) {
    list_delete_for_each(p->dependencies[i], dependency_item)
      free(dependency_item->data);

    list_free(p->dependencies[i]);
  }

  free(p->name);
  free(p);
}

bool call_hook(struct plugin *p, const char *hook_name)
{
  return p->type->call_hook(p, hook_name);
}
