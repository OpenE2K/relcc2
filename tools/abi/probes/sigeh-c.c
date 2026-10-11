/* SPDX-License-Identifier: GPL-3.0-or-later */
/* sigeh.cc's thread in plain C (no unwind tables needed): blocks in a
   cancellation point with a cleanup handler.  */
#include <pthread.h>
#include <unistd.h>

extern volatile int dtors;

static void
cleanup (void *arg)
{
  (void) arg;
  dtors++;
}

void *
sigeh_plain_thread (void *arg)
{
  pthread_cleanup_push (cleanup, arg);
  for (;;)
    pause ();
  pthread_cleanup_pop (0);
  return 0;
}
