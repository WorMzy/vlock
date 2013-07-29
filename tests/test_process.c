#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#include <CUnit/CUnit.h>

#include "process.h"

#include "test_process.h"

void test_wait_for_death(void)
{
  pid_t pid = fork();

  if (pid == 0) {
    usleep(20000);
    execl("/bin/true", "/bin/true", NULL);
    _exit(1);
  }

  CU_ASSERT(!wait_for_death(pid, 0, 2000));
  CU_ASSERT(wait_for_death(pid, 0, 20000));
}

void test_ensure_death(void)
{
  pid_t pid = fork();

  if (pid == 0) {
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    execl("/bin/true", "/bin/true", NULL);
    _exit(0);
  }

  ensure_death(pid);

  CU_ASSERT(waitpid(pid, NULL, WNOHANG) < 0);
  CU_ASSERT(errno == ECHILD);
}

int child_function(void *a)
{
  char *s = a;
  ssize_t l = strlen(s);
  char buffer[LINE_MAX];
  ssize_t l_in;
  ssize_t l_out;

  if (write(STDOUT_FILENO, s, l) < l)
    return 1;

  l_in = read(STDIN_FILENO, buffer, sizeof buffer);

  if (l_in <= 0)
    return 1;

  l_out = write(STDOUT_FILENO, buffer, l_in);

  if (l_out != l_in)
    return 1;

  return 0;
}

void test_create_child_function(void)
{
  char *s1 = "hello";
  char *s2 = "world";
  ssize_t l1 = strlen(s1);
  ssize_t l2 = strlen(s2);
  struct child_process child = {
    .function = child_function,
    .argument = s1,
    .stdin_fd = REDIRECT_PIPE,
    .stdout_fd = REDIRECT_PIPE,
    .stderr_fd = REDIRECT_DEV_NULL,
  };
  int status;
  char buffer[LINE_MAX];

  CU_ASSERT(create_child(&child));

  CU_ASSERT(child.pid > 0);

  CU_ASSERT(read(child.stdout_fd, buffer, sizeof buffer) == l1);
  CU_ASSERT(strncmp(buffer, s1, l1) == 0);

  CU_ASSERT(waitpid(child.pid, NULL, WNOHANG) == 0);

  CU_ASSERT(write(child.stdin_fd, s2, l2) == l2);
  CU_ASSERT(read(child.stdout_fd, buffer, sizeof buffer) == l2);

  CU_ASSERT(strncmp(buffer, s2, l2) == 0);

  CU_ASSERT(waitpid(child.pid, &status, 0) == child.pid);

  CU_ASSERT(WIFEXITED(status));
  CU_ASSERT(WEXITSTATUS(status) == 0);

  (void) close(child.stdin_fd);
  (void) close(child.stdout_fd);
}

void test_create_child_process(void)
{
  const char *s1 = "hello\n";
  const char *s2 = "olleh\n";
  ssize_t l1 = strlen(s1);
  ssize_t l2 = strlen(s2);
  const char *argv[] = { "sh", "-c", "rev", NULL };
  struct child_process child = {
    .path = "/bin/sh",
    .argv = argv,
    .stdin_fd = REDIRECT_PIPE,
    .stdout_fd = REDIRECT_PIPE,
    .stderr_fd = REDIRECT_DEV_NULL,
    .function = NULL,
  };
  char buffer[LINE_MAX];

  CU_ASSERT(create_child(&child));

  CU_ASSERT(write(child.stdin_fd, s1, l1) == l1);
  (void) close(child.stdin_fd);

  CU_ASSERT(read(child.stdout_fd, buffer, sizeof buffer) == l2);
  (void) close(child.stdout_fd);

  CU_ASSERT(strncmp(buffer, s2, l2) == 0);

  CU_ASSERT(wait_for_death(child.pid, 0, 0));
}

CU_TestInfo process_tests[] = {
  { "test_wait_for_death", test_wait_for_death },
  { "test_ensure_death", test_ensure_death },
  { "test_create_child_function", test_create_child_function },
  { "test_create_child_process", test_create_child_process },
  CU_TEST_INFO_NULL,
};
