#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "list.h"
#include "tsort.h"

#include "test_tsort.h"

#define A ((void *)1)
#define B ((void *)2)
#define C ((void *)3)
#define D ((void *)4)
#define E ((void *)5)
#define F ((void *)6)
#define G ((void *)7)
#define H ((void *)8)

struct edge *make_edge(void *p, void *s)
{
  struct edge *e = malloc(sizeof *e);

  e->predecessor = p;
  e->successor = s;

  return e;
}

bool item_preceeds(struct list_item *first, struct list_item *second)
{
  for (struct list_item *item = first->next; item != NULL; item = item->next)
    if (item == second)
      return true;

  return false;
}

void test_tsort(void)
{
  struct list *list = list_new();
  struct list *edges = list_new();
  struct list *faulty_edges = list_new();
  struct list *sorted_list;

  list_append(list, A);
  list_append(list, B);
  list_append(list, C);
  list_append(list, D);
  list_append(list, E);
  list_append(list, F);
  list_append(list, G);
  list_append(list, H);

  /* Check item_preceeds: */
  CU_ASSERT(item_preceeds(list_find(list, A), list_find(list, H)));

  /* Edges:
   *
   *  E
   *  |
   *  B C D   H
   *   \|/    |
   *    A   F G
   */
  list_append(edges, make_edge(A, B));
  list_append(edges, make_edge(A, C));
  list_append(edges, make_edge(A, D));
  list_append(edges, make_edge(B, E));
  list_append(edges, make_edge(G, H));

  sorted_list = tsort(list, edges);

  CU_ASSERT(list_length(edges) == 0);

  CU_ASSERT_PTR_NOT_NULL(sorted_list);

  CU_ASSERT_EQUAL(list_length(list), list_length(sorted_list));

  /* Check that all items from the original list are in the
   * sorted list. */
  list_for_each(list, item)
    CU_ASSERT_PTR_NOT_NULL(list_find(sorted_list, item->data));

  CU_ASSERT(item_preceeds(list_find(list, A), list_find(list, B)));
  CU_ASSERT(item_preceeds(list_find(list, A), list_find(list, C)));
  CU_ASSERT(item_preceeds(list_find(list, A), list_find(list, D)));

  CU_ASSERT(item_preceeds(list_find(list, B), list_find(list, E)));

  CU_ASSERT(item_preceeds(list_find(list, G), list_find(list, H)));

  /* Faulty edges: same as above but F wants to be below A and above E. */
  list_append(faulty_edges, make_edge(A, B));
  list_append(faulty_edges, make_edge(A, C));
  list_append(faulty_edges, make_edge(A, D));
  list_append(faulty_edges, make_edge(B, E));
  list_append(faulty_edges, make_edge(E, F));
  list_append(faulty_edges, make_edge(F, A));
  list_append(faulty_edges, make_edge(G, H));

  CU_ASSERT_PTR_NULL(tsort(list, faulty_edges));

  CU_ASSERT(list_length(faulty_edges) > 0);

  list_delete_for_each(faulty_edges, edge_item)
    free(edge_item->data);

  list_free(sorted_list);
  list_free(edges);
  list_free(faulty_edges);
  list_free(list);
}

CU_TestInfo tsort_tests[] = {
  { "test_tsort", test_tsort },
  CU_TEST_INFO_NULL,
};
