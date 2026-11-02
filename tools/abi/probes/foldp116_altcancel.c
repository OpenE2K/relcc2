/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Cancellation unwound from a signal handler on an alternate stack
   (recheck of docs/spec-poc-findings.md P116; doc/gcc-port-plan.md 3.3;
   spec/abi/stack-and-unwinding.md, "Signal frames, cancellation").

   glibc's nptl/tst-cancel21 cut down to one case: the main thread's
   SIGHUP handler runs on a sigaltstack of 2 * SIGSTKSZ bytes (16 KiB:
   with _GNU_SOURCE SIGSTKSZ is sysconf (_SC_SIGSTKSZ), 8,192 on E2K,
   spec/abi/kernel-interface.md, "Alternate stacks") and blocks in read
   inside a pthread_cleanup_push region; another thread cancels it.
   SIGCANCEL's handler then runs below the SIGHUP frame on the same
   alternate stack and calls __pthread_unwind, which dlopens libgcc_s and
   runs _Unwind_ForcedUnwind there.  The "main" case does the same with
   the SIGHUP handler on the ordinary stack (sa_flags 0), as a control.

   Build (any C compiler; the result depends on the libgcc_s.so.1 that
   glibc's unwinder loads, not on the program's code):

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O2 foldp116_altcancel.c -o altcancel -lpthread

   Run on hardware, once per libgcc_s:
     ./altcancel                          # the system's (MCST's)
     LD_LIBRARY_PATH=DIR ./altcancel      # DIR holding another libgcc_s.so.1
   ./altcancel N runs only the "alt" case with an N-byte alternate stack
   (sigaltstack refuses less than 4,096); stepping N finds the smallest
   alternate stack the cancellation survives with each libgcc_s:
     for n in $(seq 4096 128 16384); do ./altcancel $n; done

   Output per case: "alt: alternate stack 16384 bytes; canceled 1,
   cleanups 12" when both cleanup handlers ran, inner first, and the
   join saw PTHREAD_CANCELED; a death is printed by the parent as "alt:
   killed by signal N".

   Results (2026-11-03; lcc 1.29.16 -O2 for v3, the 4C's ALT Linux glibc
   and the 8C2's Elbrus Linux 9.5.5 glibc): at 2 * SIGSTKSZ both cases
   pass with every libgcc_s below.  The smallest alternate stack the
   "alt" case survives, in 128-byte steps, the same on both machines:
   MCST's system libgcc_s.so.1 11,392 bytes; gcc-claude 0.5's (rows for
   all 261 hard registers) 16,256, which leaves 128 of the 16,384 bytes
   that 2 * SIGSTKSZ gives; gcc-claude 0.6's (DWARF_FRAME_REGISTERS 48)
   12,416.  Below each, SIGSEGV.  The 2C3 was offline: owed.  */

#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int fd[2];
static volatile int in_sh_body;
static unsigned long cleanups;

static void
cl (void *arg)
{
  cleanups = (cleanups << 4) | (long) arg;
}

static void __attribute__ ((noinline))
sh_body (void)
{
  char c;
  pthread_cleanup_push (cl, (void *) 1L);
  in_sh_body = 1;
  if (read (fd[0], &c, 1) == 1)
    {
      puts ("read succeeded");
      exit (1);
    }
  pthread_cleanup_pop (0);
}

static void
sh (int sig)
{
  (void) sig;
  pthread_cleanup_push (cl, (void *) 2L);
  sh_body ();
  in_sh_body = 0;
  pthread_cleanup_pop (0);
}

static pthread_t main_th;

static void *
canceller (void *arg)
{
  (void) arg;
  pthread_kill (main_th, SIGHUP);
  while (!in_sh_body)
    usleep (1000);
  usleep (10000);
  pthread_cancel (main_th);
  return 0;
}

/* Runs in the child: the main thread is the one cancelled, so its exit
   status is what pthread_exit gives; a helper thread reports.  */
static void *
reporter (void *arg)
{
  void *r;
  pthread_join ((pthread_t) arg, &r);
  printf ("canceled %d, cleanups %lx\n", r == PTHREAD_CANCELED, cleanups);
  fflush (stdout);
  _exit (0);
}

static size_t alt_size;

static void
one_case (const char *name, int onstack)
{
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      if (pipe (fd) != 0)
	_exit (2);
      size_t size = alt_size ? alt_size : 2 * SIGSTKSZ;
      stack_t ss = { .ss_sp = malloc (size), .ss_flags = 0, .ss_size = size };
      if (sigaltstack (&ss, 0) != 0)
	_exit (3);
      struct sigaction sa;
      memset (&sa, 0, sizeof sa);
      sa.sa_handler = sh;
      sa.sa_flags = onstack ? SA_ONSTACK : 0;
      sigaction (SIGHUP, &sa, 0);
      main_th = pthread_self ();
      printf ("%s: alternate stack %zu bytes; ", name, size);
      fflush (stdout);
      pthread_t t1, t2;
      pthread_create (&t2, 0, reporter, (void *) main_th);
      pthread_create (&t1, 0, canceller, 0);
      char c;
      read (fd[0], &c, 1);
      puts ("main thread's read returned");
      _exit (1);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("%s: killed by signal %d\n", name, WTERMSIG (st));
  else if (WEXITSTATUS (st) != 0)
    printf ("%s: exit %d\n", name, WEXITSTATUS (st));
}

int
main (int argc, char **argv)
{
  if (argc > 1)
    {
      alt_size = strtoul (argv[1], 0, 0);
      one_case ("alt", 1);
      return 0;
    }
  one_case ("main", 0);
  one_case ("alt", 1);
  return 0;
}
