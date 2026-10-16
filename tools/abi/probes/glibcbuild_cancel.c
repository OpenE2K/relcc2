/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Thread cancellation through __attribute__ ((cleanup)) regions in C
   (spec/abi/stack-and-unwinding.md, "C cleanups and cancellation").

   glibc's internal __libc_cleanup_push (sysdeps/nptl/libc-lockP.h) is a
   cleanup attribute whenever __EXCEPTIONS is defined, while the public
   pthread_cleanup_push of MCST's <pthread.h> always takes the jump-buffer
   path on E2K.  This probe cancels threads that sit in either kind of
   region, blocked in a system call (cancellation from SIGCANCEL's
   handler, so the unwind starts on the far side of a signal frame) or
   polling pthread_testcancel (no signal frame), and one that calls
   pthread_exit.  The dliter cases cancel a thread inside a
   dl_iterate_phdr callback: if the C library's own cleanup (which
   releases dl_load_write_lock) does not run, a second dl_iterate_phdr
   in the main thread deadlocks (reported as SIGALRM).

   Build in tools/abi/probes (any compiler; the cleanups run only if it
   emits unwind tables for them, as GCC does with -fexceptions):

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O2 glibcbuild_cancel.c -o cancel-lcc -lpthread
     $L -O2 -fexceptions glibcbuild_cancel.c -o cancel-lcc-eh -lpthread
     # G: the proof of concept's cross toolchain (tools/gcc-native)
     G=../../../toolchains/gcc-native/cross-0.4+native-build.7/gcc-e2k-0.4+native-build.7
     $G/bin/e2k-linux-gnu-gcc -O2 glibcbuild_cancel.c -o cancel-gcc -lpthread
     $G/bin/e2k-linux-gnu-gcc -O2 -fexceptions glibcbuild_cancel.c \
        -o cancel-gcc-eh -lpthread
     $G/bin/e2k-linux-gnu-g++ -O2 -x c++ -fpermissive -w glibcbuild_cancel.c \
        -o cancel-gxx -lpthread -static-libstdc++

   Run on hardware: ./cancel-lcc (and so on).  A GCC-built program uses
   the system's libgcc_s.so.1 (MCST's) unless LD_LIBRARY_PATH names a
   directory holding the GCC build's libgcc_s.so.1.

   Results, the same on the 4C, 8C2 and 2C3 (2026-10-16): lcc never runs
   an attribute cleanup on cancellation or pthread_exit (cleanups=0,
   with or without -fexceptions; it emits no landing pad), and neither
   does GCC without -fexceptions; pthread_cleanup_push handlers always
   run.  GCC -fexceptions code under MCST's libgcc_s dies with SIGABRT
   in every attribute case, also without a signal frame: MCST's
   __gcc_personality_v0 calls _Unwind_SetGR with index 24, which its
   _Unwind_SetGR rejects with abort ().  With the proof of concept's
   libgcc_s (or g++ and libstdc++'s personality under either libgcc_s)
   attr_poll, attr_exit and mixed_poll run every cleanup, but attr_read
   loses the cancellation (the thread goes back into read; after the
   byte it ends with 0, and neither its cleanup nor the code after read
   and after block_read runs: "read returned=0 after=0") and mixed_read
   runs the inner cleanup after the byte and dies with SIGSEGV.  In
   every build the second dl_iterate_phdr of the dliter cases hangs:
   MCST's libc never releases dl_load_write_lock.

   Output, one line per case: the case name, "[cleanup ran]" each time
   a cleanup handler runs, then what the main thread saw ("joined
   canceled=1 cleanups=N", where "expect" says how many handlers the
   region holds, and the value the thread returned; "read returned=1"
   when block_read's read() came back normally and the rest of
   block_read ran, "after=1" when the code after the call to block_read
   in the region's function ran).  When the thread has not ended 2 s
   after pthread_cancel, "[no join after 2 s]" follows and the main
   thread writes a byte into the pipe the thread reads: "[joined after a
   byte was written]" means the cancellation was lost and the thread
   went on from its read, "[still not joined]" that it stayed stuck.
   "[killed by signal S]" means the case's child process died (14,
   SIGALRM: it hung for 10 s, or 3 s in the second dl_iterate_phdr).
   The first line names the libgcc_s.so.1 the process loaded.
   "./prog CASE" runs one case without forking (for gdb).  */

#define _GNU_SOURCE
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

static int pfd[2];
static volatile int cleanups, reached, quiet, read_returned, after;

/* glibc's __libc_cleanup_push shape (libc-lockP.h).  */
struct frame
{
  void (*routine) (void *);
  void *arg;
  int do_it;
};

static void
frame_routine (struct frame *f)
{
  if (f->do_it)
    f->routine (f->arg);
}

static void
count (void *arg)
{
  (void) arg;
  cleanups++;
  /* Say so at once: the process may die later in the same unwind.  */
  if (!quiet && write (1, " [cleanup ran]", 14) < 0)
    _exit (3);
}

#define REGION(name) \
  struct frame name __attribute__ ((cleanup (frame_routine))) \
    = { count, 0, 1 }

static void __attribute__ ((noinline))
block_read (void)
{
  char c;
  reached = 1;
  if (read (pfd[0], &c, 1) < 0)
    perror ("read");
  read_returned = 1;
}

static void __attribute__ ((noinline))
poll_cancel (void)
{
  reached = 1;
  for (;;)
    pthread_testcancel ();
}

/* 1. Cleanup attribute, blocked in read.  */
static void *
t_attr_read (void *arg)
{
  REGION (fr);
  block_read ();
  after = 1;
  fr.do_it = 0;
  return arg;
}

/* 2. Cleanup attribute, pthread_testcancel loop.  */
static void *
t_attr_poll (void *arg)
{
  REGION (fr);
  poll_cancel ();
  fr.do_it = 0;
  return arg;
}

/* 3. Cleanup attribute, pthread_exit from a nested call.  */
static void __attribute__ ((noinline))
do_exit (void)
{
  reached = 1;
  pthread_exit (0);
}

static void *
t_attr_exit (void *arg)
{
  REGION (fr);
  do_exit ();
  fr.do_it = 0;
  return arg;
}

/* 4. pthread_cleanup_push (MCST's jump-buffer path), blocked in read.  */
static void *
t_push_read (void *arg)
{
  pthread_cleanup_push (count, 0);
  block_read ();
  pthread_cleanup_pop (0);
  return arg;
}

/* 5. pthread_cleanup_push outside, cleanup attribute inside, blocked in
   read.  */
static void __attribute__ ((noinline))
inner_attr_read (void)
{
  REGION (fr);
  block_read ();
  after = 1;
  fr.do_it = 0;
}

static void *
t_mixed_read (void *arg)
{
  pthread_cleanup_push (count, 0);
  inner_attr_read ();
  pthread_cleanup_pop (0);
  return arg;
}

/* 6. Same, pthread_testcancel loop.  */
static void __attribute__ ((noinline))
inner_attr_poll (void)
{
  REGION (fr);
  poll_cancel ();
  fr.do_it = 0;
}

static void *
t_mixed_poll (void *arg)
{
  pthread_cleanup_push (count, 0);
  inner_attr_poll ();
  pthread_cleanup_pop (0);
  return arg;
}

/* 7, 8. Inside dl_iterate_phdr (the C library's own region).  */
static int
cb_read (struct dl_phdr_info *info, size_t size, void *data)
{
  (void) info, (void) size, (void) data;
  block_read ();
  return 1;
}

static int
cb_poll (struct dl_phdr_info *info, size_t size, void *data)
{
  (void) info, (void) size, (void) data;
  poll_cancel ();
  return 1;
}

static void *
t_dliter_read (void *arg)
{
  dl_iterate_phdr (cb_read, 0);
  return arg;
}

static void *
t_dliter_poll (void *arg)
{
  dl_iterate_phdr (cb_poll, 0);
  return arg;
}

static int
cb_nothing (struct dl_phdr_info *info, size_t size, void *data)
{
  (void) info, (void) size, (void) data;
  return 0;
}

static int
cb_find_libgcc (struct dl_phdr_info *info, size_t size, void *data)
{
  (void) size, (void) data;
  if (info->dlpi_name && strstr (info->dlpi_name, "libgcc_s"))
    {
      printf ("libgcc_s loaded: %s\n", info->dlpi_name);
      return 1;
    }
  return 0;
}

struct test
{
  const char *name;
  void *(*fn) (void *);
  int expect;			/* cleanup handlers in the region */
  int dliter;			/* call dl_iterate_phdr again afterwards */
};

static const struct test tests[] = {
  { "attr_read", t_attr_read, 1, 0 },
  { "attr_poll", t_attr_poll, 1, 0 },
  { "attr_exit", t_attr_exit, 1, 0 },
  { "push_read", t_push_read, 1, 0 },
  { "mixed_read", t_mixed_read, 2, 0 },
  { "mixed_poll", t_mixed_poll, 2, 0 },
  { "dliter_read", t_dliter_read, 0, 1 },
  { "dliter_poll", t_dliter_poll, 0, 1 },
};

static int
join_within (pthread_t th, void **ret, int seconds)
{
  struct timespec ts;

  clock_gettime (CLOCK_REALTIME, &ts);
  ts.tv_sec += seconds;
  return pthread_timedjoin_np (th, ret, &ts);
}

static void
run (const struct test *t)
{
  pthread_t th;
  void *ret = 0;
  int i;

  printf ("%-12s", t->name);
  alarm (10);
  if (pipe (pfd))
    _exit (2);
  cleanups = reached = read_returned = after = 0;
  pthread_create (&th, 0, t->fn, 0);
  for (i = 0; i < 1000 && !reached; i++)
    usleep (1000);
  usleep (100000);
  if (t->fn != t_attr_exit)
    pthread_cancel (th);
  if (join_within (th, &ret, 2) == ETIMEDOUT)
    {
      printf (" [no join after 2 s]");
      if (write (pfd[1], "x", 1) != 1)
	_exit (2);
      if (join_within (th, &ret, 2) == ETIMEDOUT)
	{
	  printf (" [still not joined] read returned=%d after=%d cleanups=%d\n",
		  read_returned, after, cleanups);
	  _exit (4);
	}
      printf (" [joined after a byte was written]");
    }
  printf (" joined canceled=%d (%ld) cleanups=%d expect=%d"
	  " read returned=%d after=%d\n", ret == PTHREAD_CANCELED, (long) ret,
	  cleanups, t->expect, read_returned, after);
  if (t->dliter)
    {
      printf ("%-12s second dl_iterate_phdr:", t->name);
      alarm (3);
      dl_iterate_phdr (cb_nothing, 0);
      printf (" returned\n");
    }
}

/* pthread_cancel loads libgcc_s (glibc's __libc_unwind_link_get, a
   dlopen) the first time it runs, and that dlopen waits for
   dl_load_write_lock, which a thread inside dl_iterate_phdr holds: load
   it first with a cancellation the jump-buffer way, then name it.  */
static void
load_libgcc (void)
{
  pthread_t th;

  if (pipe (pfd))
    _exit (2);
  quiet = 1;
  reached = 0;
  pthread_create (&th, 0, t_push_read, 0);
  while (!reached)
    usleep (1000);
  usleep (100000);
  pthread_cancel (th);
  pthread_join (th, 0);
  quiet = 0;
  close (pfd[0]);
  close (pfd[1]);
  if (!dl_iterate_phdr (cb_find_libgcc, 0))
    printf ("libgcc_s loaded: none found\n");
}

int
main (int argc, char **argv)
{
  unsigned i;
  int st;

  setvbuf (stdout, 0, _IONBF, 0);
  load_libgcc ();
  /* ./prog CASE runs one case in this process (for gdb).  */
  if (argc > 1)
    {
      for (i = 0; i < sizeof tests / sizeof tests[0]; i++)
	if (strcmp (argv[1], tests[i].name) == 0)
	  run (&tests[i]);
      return 0;
    }
  for (i = 0; i < sizeof tests / sizeof tests[0]; i++)
    {
      pid_t pid = fork ();
      if (pid == 0)
	{
	  run (&tests[i]);
	  _exit (0);
	}
      waitpid (pid, &st, 0);
      if (WIFSIGNALED (st))
	printf (" [killed by signal %d]\n", WTERMSIG (st));
      else if (WEXITSTATUS (st) && WEXITSTATUS (st) != 4)
	printf (" [exit %d]\n", WEXITSTATUS (st));
    }
  return 0;
}
