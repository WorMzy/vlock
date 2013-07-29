#include <stdlib.h>
#include <time.h>

#include <CUnit/CUnit.h>

#include "util.h"

#include "test_util.h"

void test_parse_timespec(void)
{
  struct timespec *t = parse_seconds("123");

  CU_ASSERT_PTR_NOT_NULL(t);
  CU_ASSERT(t->tv_sec == 123);
  CU_ASSERT(t->tv_nsec == 0);

  free(t);

#if 0
  /* Fractions are not supported, yet. */
  t = parse_seconds("123.4");

  CU_ASSERT_PTR_NOT_NULL(t);
  CU_ASSERT(t->tv_sec == 123);
  CU_ASSERT(t->tv_nsec == 400000);

  free(t);
#else
  CU_ASSERT_PTR_NULL(parse_seconds("123.4"));
#endif

  CU_ASSERT_PTR_NULL(parse_seconds("-1"));
  CU_ASSERT_PTR_NULL(parse_seconds("hello"));
}

CU_TestInfo util_tests[] = {
  { "test_parse_timespec", test_parse_timespec },
  CU_TEST_INFO_NULL,
};
