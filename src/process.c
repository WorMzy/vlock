/* process.c -- child process routines for vlock,
 *              the VT locking program for linux
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
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

#include "process.h"

/* Do nothing. */
static void ignore_sigalarm(int __attribute__((unused)) signum)
{
}

bool wait_for_death(pid_t pid, long sec, long usec)
{
  int status;
  struct sigaction act;
  struct sigaction oldact;
  struct itimerval timer;
  struct itimerval otimer;
  bool result;

  /* Ignore SIGALRM.  The handler must be a real function instead of SIG_IGN
   * otherwise waitpid() would not get interrupted.
   *
   * There is a small window here where a previously set alarm might be
   * ignored.  */
  sigemptyset(&act.sa_mask);
  act.sa_handler = ignore_sigalarm;
  act.sa_flags = 0;
  sigaction(SIGALRM, &act, &oldact);

  /* Initialize the timer. */
  timer.it_value.tv_sec = sec;
  timer.it_value.tv_usec = usec;
  /* No repetition. */
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;

  /* Set the timer. */
  setitimer(ITIMER_REAL, &timer, &otimer);

  /* Wait until the child exits or the timer fires. */
  result = (waitpid(pid, &status, 0) == pid);

  /* Possible race condition.  If an alarm was set before it may get ignored.
   * This is probably better than getting killed by our own alarm. */

  /* Restore the timer. */
  setitimer(ITIMER_REAL, &otimer, NULL);

  /* Restore signal handler for SIGALRM. */
  sigaction(SIGALRM, &oldact, NULL);

  return result;
}

/* Try hard to kill the given child process. */
void ensure_death(pid_t pid)
{
  int status;

  switch (waitpid(pid, &status, WNOHANG)) {
    case -1:
      /* Not your child? */
      return;
    case 0:
      /* Not dead yet.  Continue. */
      break;
    default:
      /* Already dead.  Nothing to do. */
      return;
  }

  /* Send SIGTERM. */
  (void) kill(pid, SIGTERM);

  /* SIGTERM handler (if any) has 500ms to finish. */
  if (wait_for_death(pid, 0, 500000L))
    return;

  // Send SIGKILL. */
  (void) kill(pid, SIGKILL);
  /* Child may be stopped.  Send SIGCONT just to be sure. */
  (void) kill(pid, SIGCONT);

  /* Wait until dead.  Shouldn't take long. */
  (void) waitpid(pid, &status, 0);
}

/* Close all possibly open file descriptors except the ones specified in the
 * given set. */
static void close_fds(fd_set *except_fds)
{
  struct rlimit r;
  int maxfd;

  /* Get the maximum number of file descriptors. */
  if (getrlimit(RLIMIT_NOFILE, &r) == 0)
    maxfd = r.rlim_cur;
  else
    /* Hopefully safe default. */
    maxfd = 1024;

  /* Close all possibly open file descriptors except STDIN_FILENO,
   * STDOUT_FILENO and STDERR_FILENO. */
  for (int fd = 0; fd < maxfd; fd++)
    if (!FD_ISSET(fd, except_fds))
      (void) close(fd);
}

static int open_devnull(void)
{
  static int devnull_fd = -1;

  if (devnull_fd < 0)
    devnull_fd = open("/dev/null", O_RDWR);

  return devnull_fd;
}

bool create_child(struct child_process *child)
{
  int errsv;
  int child_errno = 0;
  int status_pipe[2];
  int stdin_pipe[2];
  int stdout_pipe[2];
  int stderr_pipe[2];

  if (pipe(status_pipe) < 0)
    return false;

  (void) fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);

  if (child->stdin_fd == REDIRECT_PIPE) {
    if (pipe(stdin_pipe) < 0) {
      errsv = errno;
      goto stdin_pipe_failed;
    }
  } 

  if (child->stdout_fd == REDIRECT_PIPE) {
    if (pipe(stdout_pipe) < 0) {
      errsv = errno;
      goto stdout_pipe_failed;
    }
  }

  if (child->stderr_fd == REDIRECT_PIPE) {
    if (pipe(stderr_pipe) < 0) {
      errsv = errno;
      goto stderr_pipe_failed;
    }
  }

  child->pid = fork();

  if (child->pid == 0) {
    /* Child. */
    fd_set except_fds;

    if (child->stdin_fd == REDIRECT_PIPE)
      (void) dup2(stdin_pipe[0], STDIN_FILENO);
    else if (child->stdin_fd == REDIRECT_DEV_NULL)
      (void) dup2(open_devnull(), STDIN_FILENO);
    else if (child->stdin_fd != NO_REDIRECT)
      (void) dup2(child->stdin_fd, STDIN_FILENO);

    if (child->stdout_fd == REDIRECT_PIPE)
      (void) dup2(stdout_pipe[1], STDOUT_FILENO);
    else if (child->stdout_fd == REDIRECT_DEV_NULL)
      (void) dup2(open_devnull(), STDOUT_FILENO);
    else if (child->stdout_fd != NO_REDIRECT)
      (void) dup2(child->stdout_fd, STDOUT_FILENO);

    if (child->stderr_fd == REDIRECT_PIPE)
      (void) dup2(stderr_pipe[1], STDERR_FILENO);
    else if (child->stderr_fd == REDIRECT_DEV_NULL)
      (void) dup2(open_devnull(), STDERR_FILENO);
    else if (child->stderr_fd != NO_REDIRECT)
      (void) dup2(child->stderr_fd, STDERR_FILENO);

    FD_ZERO(&except_fds);
    FD_SET(STDIN_FILENO, &except_fds);
    FD_SET(STDOUT_FILENO, &except_fds);
    FD_SET(STDERR_FILENO, &except_fds);
    FD_SET(status_pipe[1], &except_fds);

    (void) close_fds(&except_fds);

    (void) setgid(getgid());
    (void) setuid(getuid());

    if (child->function != NULL) {
      (void) close(status_pipe[1]);
      _exit(child->function(child->argument));
    } else {
      execv(child->path, (char *const*)child->argv);
      (void) write(status_pipe[1], &errno, sizeof errno);
    }

    _exit(1);
  }

  if (child->pid < 0) {
    errsv = errno;
    goto fork_failed;
  }

  (void) close(status_pipe[1]);

  /* Get the error status from the child, if any. */
  if (read(status_pipe[0], &child_errno, sizeof child_errno) == sizeof child_errno) {
    errsv = child_errno;
    goto child_failed;
  }

  (void) close(status_pipe[0]);

  if (child->stdin_fd == REDIRECT_PIPE) {
    /* Write end. */
    child->stdin_fd = stdin_pipe[1];
    /* Read end. */
    (void) close(stdin_pipe[0]);
  }

  if (child->stdout_fd == REDIRECT_PIPE) {
    /* Read end. */
    child->stdout_fd = stdout_pipe[0];
    /* Write end. */
    (void) close(stdout_pipe[1]);
  }

  if (child->stderr_fd == REDIRECT_PIPE) {
    /* Read end. */
    child->stderr_fd = stderr_pipe[0];
    /* Write end. */
    (void) close(stderr_pipe[1]);
  }

  return true;

child_failed:
fork_failed:
  if (child->stderr_fd == REDIRECT_PIPE) {
    (void) close(stderr_pipe[0]);
    (void) close(stderr_pipe[1]);
  }

stderr_pipe_failed:
  if (child->stdout_fd == REDIRECT_PIPE) {
    (void) close(stdout_pipe[0]);
    (void) close(stdout_pipe[1]);
  }

stdout_pipe_failed:
  if (child->stdin_fd == REDIRECT_PIPE) {
    (void) close(stdin_pipe[0]);
    (void) close(stdin_pipe[1]);
  }

stdin_pipe_failed:
  (void) close(status_pipe[0]);
  (void) close(status_pipe[1]);

  errno = errsv;

  return false;
}
