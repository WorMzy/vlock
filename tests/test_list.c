#include <CUnit/CUnit.h>

#include "list.h"

#include "test_list.h"

void test_list_new(void)
{
  struct list *l = list_new();

  CU_ASSERT_PTR_NOT_NULL_FATAL(l);
  CU_ASSERT_PTR_NULL(l->first);
  CU_ASSERT_PTR_NULL(l->last);

  list_free(l);
}

void test_list_copy(void)
{
  struct list *l = list_new();
  struct list *m;

  list_append(l, (void *)1);
  list_append(l, (void *)2);
  list_append(l, (void *)3);

  m = list_copy(l);

  CU_ASSERT_EQUAL(list_length(l), list_length(m));
  CU_ASSERT_PTR_NOT_EQUAL(l, m);

  for (struct list_item *item_l = l->first, *item_m = m->first;
      item_l != NULL && item_m != NULL;
      item_l = item_l->next, item_m = item_m->next) {
    CU_ASSERT_PTR_EQUAL(item_l->data, item_m->data);
    CU_ASSERT_PTR_NOT_EQUAL(item_l, item_m);
  }

  list_free(m);
  list_free(l);
}

void test_list_free(void)
{
  struct list *l = list_new();

  list_append(l, (void *)1);
  list_append(l, (void *)2);
  list_append(l, (void *)3);

  list_free(l);

  CU_PASS("list_free() didn't crash");
}

void test_list_length(void)
{
  struct list *l = list_new();

  CU_ASSERT(list_length(l) == 0);

  list_append(l, (void *)1);
  CU_ASSERT(list_length(l) == 1);

  list_append(l, (void *)2);
  CU_ASSERT(list_length(l) == 2);

  list_append(l, (void *)3);
  CU_ASSERT(list_length(l) == 3);

  list_append(l, (void *)4);
  CU_ASSERT(list_length(l) == 4);

  list_free(l);
}

void test_list_append(void)
{
  struct list *l = list_new();

  list_append(l, (void *)1);

  CU_ASSERT_PTR_EQUAL(l->first, l->last);
  CU_ASSERT_PTR_NULL(l->first->previous);
  CU_ASSERT_PTR_NULL(l->last->next);

  CU_ASSERT_PTR_EQUAL(l->first->data, (void *)1);

  list_append(l, (void *)2);

  CU_ASSERT_PTR_NOT_EQUAL(l->first, l->last);
  CU_ASSERT_PTR_EQUAL(l->first->next, l->last);
  CU_ASSERT_PTR_EQUAL(l->last->previous, l->first);
  CU_ASSERT_PTR_NULL(l->first->previous);
  CU_ASSERT_PTR_NULL(l->last->next);

  CU_ASSERT_PTR_EQUAL(l->last->data, (void *)2);

  list_append(l, (void *)3);

  CU_ASSERT_PTR_EQUAL(l->first->next, l->last->previous);
  CU_ASSERT_PTR_EQUAL(l->last->previous->previous, l->first);
  CU_ASSERT_PTR_NULL(l->first->previous);
  CU_ASSERT_PTR_NULL(l->last->next);

  CU_ASSERT_PTR_EQUAL(l->last->data, (void *)3);

  list_free(l);
}

void test_list_delete_item(void)
{
  struct list *l = list_new();

  list_append(l, (void *)1);

  list_delete_item(l, l->first);

  CU_ASSERT_PTR_NULL(l->first);
  CU_ASSERT_PTR_NULL(l->last);

  list_append(l, (void *)1);
  list_append(l, (void *)2);
  list_append(l, (void *)3);

  list_delete_item(l, l->first->next);

  CU_ASSERT_PTR_EQUAL(l->first->next, l->last);
  CU_ASSERT_PTR_EQUAL(l->last->previous, l->first);

  CU_ASSERT_PTR_EQUAL(l->first->data, (void *)1)
  CU_ASSERT_PTR_EQUAL(l->last->data, (void *)3)

  list_free(l);
}

void test_list_delete(void)
{
  struct list *l = list_new();

  list_append(l, (void *)1);
  list_append(l, (void *)2);
  list_append(l, (void *)3);

  list_delete(l, (void *)2);

  CU_ASSERT_PTR_EQUAL(l->first->next, l->last);
  CU_ASSERT_PTR_EQUAL(l->last->previous, l->first);

  CU_ASSERT_PTR_EQUAL(l->first->data, (void *)1)
  CU_ASSERT_PTR_EQUAL(l->last->data, (void *)3)

  list_delete(l, (void *)1);
  list_delete(l, (void *)3);

  CU_ASSERT(list_length(l) == 0);

  list_delete(l, (void *)4);

  list_free(l);
}

void test_list_find(void)
{
  struct list *l = list_new();

  list_append(l, (void *)1);
  list_append(l, (void *)2);
  list_append(l, (void *)3);

  CU_ASSERT_PTR_EQUAL(list_find(l, (void *)2), l->first->next);
  CU_ASSERT_PTR_NULL(list_find(l, (void *)4));
  CU_ASSERT_PTR_NULL(list_find(l, NULL));

  list_free(l);
}

CU_TestInfo list_tests[] = {
  { "test_list_new", test_list_new },
  { "test_list_copy", test_list_copy },
  { "test_list_free", test_list_free },
  { "test_list_length", test_list_length },
  { "test_list_append", test_list_append },
  { "test_list_delete_item", test_list_delete_item },
  { "test_list_delete", test_list_delete },
  { "test_list_find", test_list_find },
  CU_TEST_INFO_NULL,
};
