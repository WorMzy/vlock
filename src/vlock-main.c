/* vlock-main.c -- main routine for vlock,
 *                    the VT locking program for linux
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <pwd.h>

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include "prompt.h"
#include "auth.h"
#include "console_switch.h"
#include "util.h"

#ifdef USE_PLUGINS
#include "plugins.h"
#endif

int vlock_debug = 0;

#define ensure_atexit(func) \
  do { \
    if (atexit(func) != 0) \
      fatal_perror("vlock: atexit() failed"); \
  } while (0)

static char *get_username(void)
{
  uid_t uid = getuid();
  char *username = NULL;

  /* Get the user name from the environment if started as root. */
  if (uid == 0)
    username = getenv("USER");

  if (username == NULL) {
    struct passwd *pw;

    /* Get the password entry. */
    pw = getpwuid(uid);

    if (pw == NULL)
      return NULL;

    username = pw->pw_name;
  }

  return strdup(username);
}

static void terminate(int signum)
{
  fprintf(stderr, "vlock: Terminated!\n");
  /* Call exit here to ensure atexit handlers are called. */
  exit(1);
}

static void block_signals(void)
{
  struct sigaction sa;

  /* Ignore some signals. */
  /* These signals shouldn't be delivered anyway, because terminal signals are
   * disabled below. */
  (void) sigemptyset(&(sa.sa_mask));
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = SIG_IGN;
  (void) sigaction(SIGINT, &sa, NULL);
  (void) sigaction(SIGQUIT, &sa, NULL);
  (void) sigaction(SIGTSTP, &sa, NULL);

  /* Install special handler for SIGTERM. */
  sa.sa_flags = SA_RESETHAND;
  sa.sa_handler = terminate;
  (void) sigaction(SIGTERM, &sa, NULL);
}

static struct termios old_term;

static void setup_terminal(void)
{
  (void) tcgetattr(STDIN_FILENO, &old_term);
  struct termios new_term = old_term;
  /* Pressing enter must yield line feed. */
  new_term.c_iflag &= ~INLCR;
  new_term.c_iflag |= ICRNL;
  /* Disable terminal echoing and signals. */
  new_term.c_lflag &= ~(ECHO | ISIG);
  (void) tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
}

static void restore_terminal(void)
{
  /* Restore the terminal. */
  (void) tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
}

static int auth_tries;

static void auth_loop(const char *username)
{
  struct timespec *prompt_timeout;
  struct timespec *wait_timeout;
  char *vlock_message;

  /* Get the vlock message from the environment. */
  vlock_message = getenv("VLOCK_MESSAGE");

  if (vlock_message == NULL) {
    if (console_switch_locked)
      vlock_message = getenv("VLOCK_ALL_MESSAGE");
    else
      vlock_message = getenv("VLOCK_CURRENT_MESSAGE");
  }

  /* Get the timeouts from the environment. */
  prompt_timeout = parse_seconds(getenv("VLOCK_PROMPT_TIMEOUT"));
#ifdef USE_PLUGINS
  wait_timeout = parse_seconds(getenv("VLOCK_TIMEOUT"));
#else
  wait_timeout = NULL;
#endif

  for (;;) {
    char c;

    /* Print vlock message if there is one. */
    if (vlock_message && *vlock_message) {
      fputs(vlock_message, stderr);
      fputc('\n', stderr);
    }

    /* Wait for enter or escape to be pressed. */
    c = wait_for_character("\n\033", wait_timeout);

    /* Escape was pressed or the timeout occurred. */
    if (c == '\033' || c == 0) {
#ifdef USE_PLUGINS
      plugin_hook("vlock_save");
      /* Wait for any key to be pressed. */
      c = wait_for_character(NULL, NULL);
      plugin_hook("vlock_save_abort");

      /* Do not require enter to be pressed twice. */
      if (c != '\n')
        continue;
#else
      continue;
#endif
    }

    /* Try authentication as user. */
    if (auth(username, prompt_timeout))
      break;
    else
      sleep(1);

#ifndef NO_ROOT_PASS
    if (strcmp(username, "root") != 0) {
      /* Try authentication as root. */
      if (auth("root", prompt_timeout))
        break;
      else
        sleep(1);
    }
#endif

    auth_tries++;
  }

  /* Free timeouts memory. */
  free(wait_timeout);
  free(prompt_timeout);
}

void display_auth_tries(void)
{
  if (auth_tries > 0)
    fprintf(stderr, "%d failed authentication %s.\n", auth_tries, auth_tries > 1 ? "tries" : "try");
}

#ifdef USE_PLUGINS
static void call_end_hook(void)
{
  (void) plugin_hook("vlock_end");
}
#endif

/* Lock the current terminal until proper authentication is received. */
int main(int argc, char *const argv[])
{
  char *username;

  vlock_debug = (getenv("VLOCK_DEBUG") != NULL);

  block_signals();

  username = get_username();

  if (username == NULL)
    fatal_perror("vlock: could not get username");

  ensure_atexit(display_auth_tries);

#ifdef USE_PLUGINS
  for (int i = 1; i < argc; i++)
    if (!load_plugin(argv[i]))
      fatal_error("vlock: loading plugin '%s' failed: %s", argv[i], STRERROR);

  ensure_atexit(unload_plugins);

  if (!resolve_dependencies()) {
    if (errno == 0)
      exit(EXIT_FAILURE);
    else
      fatal_error("vlock: error resolving plugin dependencies: %s", STRERROR);
  }

  plugin_hook("vlock_start");
  ensure_atexit(call_end_hook);
#else /* !USE_PLUGINS */
  /* Emulate pseudo plugin "all". */
  if (argc == 2 && (strcmp(argv[1], "all") == 0)) {
    if (!lock_console_switch()) {
      if (errno)
        perror("vlock: could not disable console switching");

      exit(EXIT_FAILURE);
    }

    ensure_atexit((void (*)(void))unlock_console_switch);
  } else if (argc > 1) {
    fatal_error("vlock: plugin support disabled");
  }
#endif

  if (!isatty(STDIN_FILENO))
    fatal_error("vlock: stdin is not a terminal");

  setup_terminal();
  ensure_atexit(restore_terminal);

  auth_loop(username);

  free(username);

  exit(0);
}
