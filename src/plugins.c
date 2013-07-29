/* plugins.c -- plugins for vlock, the VT locking program for linux
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
#include <errno.h>

#include "plugins.h"

#include "list.h"
#include "tsort.h"

#include "plugin.h"
#include "util.h"

/* the list of plugins */
static struct list *plugins = &(struct list){ NULL, NULL };

/****************/
/* dependencies */
/****************/

#define SUCCEEDS 0
#define PRECEEDS 1
#define REQUIRES 2
#define NEEDS 3
#define DEPENDS 4
#define CONFLICTS 5

const char *dependency_names[nr_dependencies] = {
  "succeeds",
  "preceeds",
  "requires",
  "needs",
  "depends",
  "conflicts",
};

/*********/
/* hooks */
/*********/

static void handle_vlock_start(const char * hook_name);
static void handle_vlock_end(const char * hook_name);
static void handle_vlock_save(const char * hook_name);
static void handle_vlock_save_abort(const char * hook_name);

const struct hook hooks[nr_hooks] = {
  { "vlock_start", handle_vlock_start },
  { "vlock_end", handle_vlock_end },
  { "vlock_save", handle_vlock_save },
  { "vlock_save_abort", handle_vlock_save_abort },
};

/**********************/
/* exported functions */
/**********************/

/* helper declarations */
static struct plugin *__load_plugin(const char *name);
static bool __resolve_depedencies(void);
static bool sort_plugins(void);

bool load_plugin(const char *name)
{
  return __load_plugin(name) != NULL;
}

bool resolve_dependencies(void)
{
  return __resolve_depedencies() && sort_plugins();
}

void unload_plugins(void)
{
  list_delete_for_each(plugins, plugin_item)
    destroy_plugin(plugin_item->data);
}

void plugin_hook(const char *hook_name)
{
  for (size_t i = 0; i < nr_hooks; i++)
    /* Get the handler and call it. */
    if (strcmp(hook_name, hooks[i].name) == 0) {
      hooks[i].handler(hook_name);
      break;
    }
}

/********************/
/* helper functions */
/********************/

static struct plugin *get_plugin(const char *name)
{
  list_for_each(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;
    if (strcmp(name, p->name) == 0)
      return p;
  }

  return NULL;
}

/* Load and return the named plugin. */
static struct plugin *__load_plugin(const char *name)
{
  struct plugin *p = get_plugin(name);

  if (p != NULL)
    return p;

  /* Try to open a module first. */
  p = new_plugin(name, module);

  if (p != NULL)
    goto success;

  if (errno != ENOENT)
    return NULL;

  /* Now try to open a script. */
  p = new_plugin(name, script);

  if (p == NULL)
    return NULL;

success:
  if (!list_append(plugins, p)) {
    GUARD_ERRNO(destroy_plugin(p));
    return NULL;
  }

  return p;
}

/* Resolve the dependencies of the plugins. */
static bool __resolve_depedencies(void)
{
  struct list *required_plugins = list_new();

  /* Load plugins that are required.  This automagically takes care of plugins
   * that are required by the plugins loaded here because they are appended to
   * the end of the list. */
  list_for_each(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;

    list_for_each(p->dependencies[REQUIRES], dependency_item) {
      const char *d = dependency_item->data;
      struct plugin *q = __load_plugin(d);

      if (q == NULL) {
        int errsv = errno;
        fprintf(stderr, "vlock-plugins: '%s' requires '%s' which could not be loaded\n", p->name, d);
        list_free(required_plugins);
        errno = errsv;
        return false;
      }

      if (!list_append(required_plugins, q)) {
        GUARD_ERRNO(list_free(required_plugins));
        return false;
      }
    }
  }

  /* Fail if a plugins that is needed is not loaded. */
  list_for_each(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;

    list_for_each(p->dependencies[NEEDS], dependency_item) {
      const char *d = dependency_item->data;
      struct plugin *q = get_plugin(d);

      if (q == NULL) {
        fprintf(stderr, "vlock-plugins: '%s' needs '%s' which is not loaded\n", p->name, d);
        list_free(required_plugins);
        errno = 0;
        return false;
      }

      if (!list_append(required_plugins, q)) {
        GUARD_ERRNO(list_free(required_plugins));
        return false;
      }
    }
  }

  /* Unload plugins whose prerequisites are not present, fail if those plugins
   * are required. */
  list_for_each_manual(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;
    bool dependencies_loaded = true;

    list_for_each(p->dependencies[DEPENDS], dependency_item) {
      const char *d = dependency_item->data;
      struct plugin *q = get_plugin(d);

      if (q == NULL) {
        dependencies_loaded = false;

        /* Abort if dependencies not met and plugin is required. */
        if (list_find(required_plugins, p) != NULL) {
          fprintf(stderr, 
              "vlock-plugins: '%s' is required by some other plugin\n"
               "              but depends on '%s' which is not loaded",
               p->name, d);
          list_free(required_plugins);
          errno = 0;
          return false;
        }

        break;
      }
    }

    if (dependencies_loaded) {
      plugin_item = plugin_item->next;
    } else {
      plugin_item = list_delete_item(plugins, plugin_item);
      destroy_plugin(p);
    }
  }

  list_free(required_plugins);

  /* Fail if conflicting plugins are loaded. */
  list_for_each(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;

    list_for_each(p->dependencies[CONFLICTS], dependency_item) {
      const char *d = dependency_item->data;
      if (get_plugin(d) != NULL) {
        fprintf(stderr, "vlock-plugins: '%s' and '%s' cannot be loaded at the same time\n", p->name, d);
        errno = 0;
        return false;
      }
    }
  }

  return true;
}

static struct list *get_edges(void);

/* Sort the list of plugins according to their "preceeds" and "succeeds"
 * dependencies.  Fails if sorting is not possible because of circles. */
static bool sort_plugins(void)
{
  struct list *edges = get_edges();
  struct list *sorted_plugins;

  if (edges == NULL)
    return false;

  /* Topological sort. */
  sorted_plugins = tsort(plugins, edges);

  if (sorted_plugins == NULL && errno != 0)
    return false;

  list_delete_for_each(edges, edge_item) {
    struct edge *e = edge_item->data;
    struct plugin *p = e->predecessor;
    struct plugin *s = e->successor;

    fprintf(stderr, "\t%s\tmust come before\t%s\n", p->name, s->name);
    free(e);
  }

  if (sorted_plugins != NULL) {
    /* Switch the global list of plugins for the sorted list.  The global list
     * is static and cannot be freed. */
    struct list_item *first = sorted_plugins->first;
    struct list_item *last = sorted_plugins->last;

    sorted_plugins->first = plugins->first;
    sorted_plugins->last = plugins->last;

    plugins->first = first;
    plugins->last = last;

    list_free(sorted_plugins);

    return true;
  } else {
    fprintf(stderr, "vlock-plugins: circular dependencies detected\n");
    return false;
  }
}

static bool append_edge(struct list *edges, struct plugin *p, struct plugin *s)
{
  struct edge *e = malloc(sizeof *e);

  if (e == NULL)
    return false;

  e->predecessor = p;
  e->successor = s;

  if (!list_append(edges, e)) {
    GUARD_ERRNO(free(e));
    return false;
  }

  return true;
}

/* Get the edges of the plugin graph specified by each plugin's "preceeds" and
 * "succeeds" dependencies. */
static struct list *get_edges(void)
{
  struct list *edges = list_new();

  if (edges == NULL)
    return NULL;

  list_for_each(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;
    /* p must come after these */
    list_for_each(p->dependencies[SUCCEEDS], predecessor_item) {
      struct plugin *q = get_plugin(predecessor_item->data);

      if (q != NULL)
        if (!append_edge(edges, q, p))
          goto error;
    }

    /* p must come before these */
    list_for_each(p->dependencies[PRECEEDS], successor_item) {
      struct plugin *q = get_plugin(successor_item->data);

      if (q != NULL)
        if (!append_edge(edges, p, q))
          goto error;
    }
  }

  return edges;

error:
  GUARD_ERRNO(list_free(edges));
  return NULL;
}

/************/
/* handlers */
/************/

/* Call the "vlock_start" hook of each plugin.  Fails if the hook of one of the
 * plugins fails.  In this case the "vlock_end" hooks of all plugins that were
 * called before are called in reverse order. */
void handle_vlock_start(const char *hook_name)
{
  list_for_each(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;

    if (!call_hook(p, hook_name)) {
      int errsv = errno;

      list_for_each_reverse_from(plugins, reverse_item, plugin_item->previous) {
        struct plugin *r = reverse_item->data;
        (void) call_hook(r, "vlock_end");
      }

      if (errsv)
        fprintf(stderr, "vlock: plugin '%s' failed: %s\n", p->name, strerror(errsv));

      exit(EXIT_FAILURE);
    }
  }
}

/* Call the "vlock_end" hook of each plugin in reverse order.  Never fails. */
void handle_vlock_end(const char *hook_name)
{
  list_for_each_reverse(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;
    (void) call_hook(p, hook_name);
  }
}

/* Call the "vlock_save" hook of each plugin.  Never fails.  If the hook of a
 * plugin fails its "vlock_save_abort" hook is called and both hooks are never
 * called again afterwards. */
void handle_vlock_save(const char *hook_name)
{
  list_for_each(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;

    if (p->save_disabled)
      continue;

    if (!call_hook(p, hook_name)) {
      p->save_disabled = true;
      (void) call_hook(p, "vlock_save_abort");
    }
  }
}

/* Call the "vlock_save" hook of each plugin.  Never fails.  If the hook of a
 * plugin fails both hooks "vlock_save" and "vlock_save_abort" are never called
 * again afterwards. */
void handle_vlock_save_abort(const char *hook_name)
{
  list_for_each_reverse(plugins, plugin_item) {
    struct plugin *p = plugin_item->data;

    if (p->save_disabled)
      continue;

    if (!call_hook(p, hook_name))
      p->save_disabled = true;
  }
}
