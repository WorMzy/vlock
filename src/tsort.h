/* tsort.h -- header file for topological sort for vlock,
 *            the VT locking program for linux
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

/* An edge of the graph, specifying that predecessor must come before
 * successor. */
struct edge {
  void *predecessor;
  void *successor;
};

struct list;

/* For the given directed graph, generate a topological sort of the nodes.
 *
 * Sorts the list and deletes all edges.  If there are circles found in the
 * graph or there are edges that have no corresponding nodes the erroneous
 * edges are left. */
struct list *tsort(struct list *nodes, struct list *edges);
