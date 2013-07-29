/* script.c -- script routines for vlock, the VT locking program for linux
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

/* Scripts are executables that are run as unprivileged child processes of
 * vlock.  They communicate with vlock through stdin and stdout.
 *
 * When dependencies are retrieved they are launched once for each dependency
 * and should print the names of the plugins they depend on on stdout one per
 * line.  The dependency requested is given as a single command line argument.
 *
 * In hook mode the script is called once with "hooks" as a single command line
 * argument.  It should not exit until its stdin closes.  The hook that should
 * be executed is written to its stdin on a single line.
 *
 * Currently there is no way for a script to communicate errors or even success
 * to vlock.  If it exits it will linger as a zombie until the plugin is
 * destroyed.
 */

#if !defined(__FreeBSD__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "list.h"
#include "process.h"
#include "util.h"

#include "plugin.h"

static bool init_script(struct plugin *p);
static void destroy_script(struct plugin *p);
static bool call_script_hook(struct plugin *p, const char *hook_name);

struct plugin_type *script = &(struct plugin_type){
  .init = init_script,
  .destroy = destroy_script,
  .call_hook = call_script_hook,
};

struct script_context 
{
  /* The path to the script. */
  char *path;
  /* Was the script launched? */
  bool launched;
  /* Did the script die? */
  bool dead;
  /* The pipe file descriptor that is connected to the script's stdin. */
  int fd;
  /* The PID of the script. */
  pid_t pid;
};

/* Get the dependency from the script. */ 
static bool get_dependency(const char *path, const char *dependency_name,
    struct list *dependency_list);
/* Launch the script creating a new script_context. */
static bool launch_script(struct script_context *script);

bool init_script(struct plugin *p)
{
  int errsv;
  struct script_context *context = malloc(sizeof *context);

  if (context == NULL)
    return false;

  context->dead = false;
  context->launched = false;

  if (asprintf(&context->path, "%s/%s", VLOCK_SCRIPT_DIR, p->name) < 0) {
    free(context);
    errno = ENOMEM;
    return false;
  }

  /* Get the dependency information.  Whether the script is executable or not
   * is also detected here. */
  for (size_t i = 0; i < nr_dependencies; i++)
    if (!get_dependency(context->path, dependency_names[i], p->dependencies[i]))
      goto error;

  p->context = context;
  return true;

error:
  errsv = errno;
  free(context->path);
  free(context);
  errno = errsv;
  return false;
}

static void destroy_script(struct plugin *p)
{
  struct script_context *context = p->context;

  if (context != NULL) {
    free(context->path);

    if (context->launched) {
      /* Close the pipe. */
      (void) close(context->fd);

      /* Kill the child process. */
      if (!wait_for_death(context->pid, 0, 500000L))
        ensure_death(context->pid);
    }

    free(context);
  }
}

/* Invoke the hook by writing it on a single line to the scripts stdin. */
static bool call_script_hook(struct plugin *s, const char *hook_name)
{
  static const char newline = '\n';
  struct script_context *context = s->context;
  ssize_t hook_name_length = strlen(hook_name);
  ssize_t length;
  struct sigaction act;
  struct sigaction oldact;

  if (!context->launched) {
    /* Launch script. */
    context->launched = launch_script(context);

    if (!context->launched)
      return false;
  }

  if (context->dead)
    /* Nothing to do. */
    return false;

  /* When writing to a pipe when the read end is closed the kernel invariably
   * sends SIGPIPE.   Ignore it. */
  (void) sigemptyset(&(act.sa_mask));
  act.sa_flags = SA_RESTART;
  act.sa_handler = SIG_IGN;
  (void) sigaction(SIGPIPE, &act, &oldact);

  /* Send hook name and a newline through the pipe. */
  length = write(context->fd, hook_name, hook_name_length);

  if (length > 0)
    length += write(context->fd, &newline, sizeof newline); 

  /* Restore the previous SIGPIPE handler. */
  (void) sigaction(SIGPIPE, &oldact, NULL);

  /* If write fails the script is considered dead. */
  context->dead = (length != hook_name_length + 1);

  return !context->dead;
}

static bool launch_script(struct script_context *script)
{
  int fd_flags;
  const char *argv[] = { script->path, "hooks", NULL };
  struct child_process child = {
    .path = script->path,
    .argv = argv,
    .stdin_fd = REDIRECT_PIPE,
    .stdout_fd = REDIRECT_DEV_NULL,
    .stderr_fd = REDIRECT_DEV_NULL,
    .function = NULL,
  };

  if (!create_child(&child))
    return false;

  script->fd = child.stdin_fd;
  script->pid = child.pid;

  fd_flags = fcntl(script->fd, F_GETFL, &fd_flags);

  if (fd_flags != -1) {
    fd_flags |= O_NONBLOCK;
    (void) fcntl(script->fd, F_SETFL, fd_flags);
  }

  return true;
}

static char *read_dependency(const char *path, const char *dependency_name);
static bool parse_dependency(char *data, struct list *dependency_list);

/* Get the dependency from the script. */
static bool get_dependency(const char *path, const char *dependency_name,
    struct list *dependency_list)
{
  /* Read the dependency data. */
  char *data;
  errno = 0;
  data = read_dependency(path, dependency_name);

  if (data == NULL)  {
    return errno == 0;
  } else {
    /* Parse the dependency data. */
    bool result = parse_dependency(data, dependency_list);
    int errsv = errno;
    free(data);
    errno = errsv;
    return result;
  }
}

/* Read the dependency data by starting the script with the name of the
 * dependency as a single command line argument.  The script should then print
 * the dependencies to its stdout one on per line. */
static char *read_dependency(const char *path, const char *dependency_name)
{
  const char *argv[] = { path, dependency_name, NULL };
  struct child_process child = {
    .path = path,
    .argv = argv,
    .stdin_fd = REDIRECT_DEV_NULL,
    .stdout_fd = REDIRECT_PIPE,
    .stderr_fd = REDIRECT_DEV_NULL,
    .function = NULL,
  };
  struct timeval timeout = {1, 0};
  char *data = calloc(1, sizeof *data);
  size_t data_length = 0;

  if (data == NULL)
    return NULL;

  if (!create_child(&child)) {
    int errsv = errno;
    free(data);
    errno = errsv;
    return NULL;
  }

  /* Read the dependency from the child.  Reading fails if either the timeout
   * elapses or more that LINE_MAX bytes are read. */
  for (;;) {
    struct timeval t = timeout;
    struct timeval t1;
    struct timeval t2;
    char buffer[LINE_MAX];
    ssize_t length;

    fd_set read_fds;

    FD_ZERO(&read_fds);
    FD_SET(child.stdout_fd, &read_fds);

    /* t1 is before select. */
    (void) gettimeofday(&t1, NULL);

    if (select(child.stdout_fd+1, &read_fds, NULL, NULL, &t) != 1) {
timeout:
      errno = ETIMEDOUT;
      goto error;
    }

    /* t2 is after select. */
    (void) gettimeofday(&t2, NULL);

    /* Get the time that during select. */
    timersub(&t2, &t1, &t2);

    /* This is very unlikely. */
    if (timercmp(&t2, &timeout, >))
      goto timeout;

    /* Reduce the timeout. */
    timersub(&timeout, &t2, &timeout);

    /* Read dependency data from the script. */
    length = read(child.stdout_fd, buffer, sizeof buffer - 1);

    /* Did the script close its stdin or exit? */
    if (length <= 0)
      break;

    if (data_length+length+1 > LINE_MAX) {
      errno = EFBIG;
      goto error;
    }

    /* Grow the data string. */
    data = realloc(data, data_length+length);

    if (data == NULL)
      goto error;

    /* Append the buffer to the data string. */
    strncpy(data+data_length, buffer, length);
    data_length += length;
  }

  /* Terminate the data string. */
  data[data_length] = '\0';

  /* Close the read end of the pipe. */
  (void) close(child.stdout_fd);
  /* Kill the script. */
  if (!wait_for_death(child.pid, 0, 500000L))
    ensure_death(child.pid);

  return data;

error:
  {
    int errsv = errno;
    free(data);
    (void) close(child.stdout_fd);
    if (!wait_for_death(child.pid, 0, 500000L))
      ensure_death(child.pid);
    errno = errsv;
    return NULL;
  }
}

static bool parse_dependency(char *data, struct list *dependency_list)
{
  for (char *saveptr, *token = strtok_r(data, " \r\n", &saveptr);
      token != NULL;
      token = strtok_r(NULL, " \r\n", &saveptr)) {
    char *s = strdup(token);

    if (s == NULL || !list_append(dependency_list, s))
      return false;
  }

  return true;
}
