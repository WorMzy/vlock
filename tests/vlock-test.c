#include <stdlib.h>
#include <stdio.h>

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "test_list.h"
#include "test_tsort.h"
#include "test_util.h"
#include "test_process.h"

CU_SuiteInfo vlock_test_suites[] = {
  { "test_list" , NULL, NULL, list_tests },
  { "test_tsort", NULL, NULL, tsort_tests },
  { "test_util", NULL, NULL, util_tests },
  { "test_process", NULL, NULL, process_tests },
  CU_SUITE_INFO_NULL,
};

int main(int __attribute__((unused)) argc, const char *argv[])
{
  char *output_mode = getenv("VLOCK_TEST_OUTPUT_MODE");

  if (CU_initialize_registry() != CUE_SUCCESS) {
    fprintf(stderr, "%s: CUnit initialization failed\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (CU_register_suites(vlock_test_suites) != CUE_SUCCESS) {
    fprintf(stderr, "%s: registering test suites failed: %s\n", argv[0], CU_get_error_msg());
    goto error;
  }

  if (output_mode != NULL) {
    if (strcmp(output_mode, "verbose") == 0)
      CU_basic_set_mode(CU_BRM_VERBOSE);
    else if (strcmp(output_mode, "normal") == 0)
      CU_basic_set_mode(CU_BRM_NORMAL);
    else if (strcmp(output_mode, "silent") == 0)
      CU_basic_set_mode(CU_BRM_SILENT);
  }

  if (CU_basic_run_tests() != CUE_SUCCESS) {
    fprintf(stderr, "%s: running tests failed\n", argv[0]);
    goto error;
  }

  if (CU_get_number_of_tests_failed() > 0)
    goto error;

  CU_cleanup_registry();
  exit(EXIT_SUCCESS);

error:
  CU_cleanup_registry();
  exit(EXIT_FAILURE);
}
