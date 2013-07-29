/* list.h -- doubly linked list header file for vlock,
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

#include <stdbool.h>
#include <stddef.h>

/* Single list item. */
struct list_item
{
  void *data;
  struct list_item *next;
  struct list_item *previous;
};

/* Whole list. */
struct list
{
  struct list_item *first;
  struct list_item *last;
};

/* Create a new, empty list. */
struct list *list_new(void);
/* Create a (shallow) copy of the given list. */
struct list *list_copy(struct list *l);

/* Deallocate the given list and all items. */
void list_free(struct list *l);

/* Calculate the number of items in the given list. */
size_t list_length(struct list *l);

/* Create a new list item with the given data and add it to the end of the
 * list. */
bool list_append(struct list *l, void *data);

/* Remove the given item from the list.  Returns the item following the deleted
 * one or NULL if the given item was the last. */
struct list_item *list_delete_item(struct list *l, struct list_item *item);

/* Remove the first item with the given data.  Does nothing if no item has this
 * data. */
void list_delete(struct list *l, void *data);

/* Find the first item with the given data.  Returns NULL if no item has this
 * data. */
struct list_item *list_find(struct list *l, void *data);

/* Generic list iteration macro. */
#define list_for_each_from_increment(list, item, start, increment) \
  for (struct list_item *item = (start); item != NULL; (increment))

/* Iterate over the whole list. */
#define list_for_each(list, item) \
  list_for_each_from_increment((list), item, (list)->first, item = item->next)

/* Iterate over the list while deleting every item after the iteration. */
#define list_delete_for_each(list, item) \
  list_for_each_from_increment((list), item, (list)->first, item = list_delete_item((list), item))

/* Iterate over the list.  Incrementation must be done manually. */
#define list_for_each_manual(list, item) \
  for (struct list_item *item = (list)->first; item != NULL;)

/* Iterate backwards over list from the given start. */
#define list_for_each_reverse_from(list, item, start) \
  list_for_each_from_increment((list), item, (start), item = item->previous)

/* Iterate backwards over the whole list. */
#define list_for_each_reverse(list, item) \
  list_for_each_reverse_from((list), item, (list)->last)

static inline bool list_is_empty(struct list *l)
{
  return l->first == NULL;
}
