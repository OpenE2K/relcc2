/* SPDX-License-Identifier: GPL-3.0-or-later */
/* lcc's sanitizers on hardware (spec/abi/kernel-interface.md,
   "Sanitizers"): does MCST's compiler-rt start on the Linux 6.1 layout
   (and on the 2C3's lower layout), and does it report what it should.
   One case per run, named by argv[1]:

     ok         nothing wrong: prints where things are and exits 0
     heap       reads one byte past a 10-byte malloc block
     uaf        reads a freed block
     stack      reads past a local array
     global     reads past a global array
     leak       drops the only pointer to a malloc block, then exits
     reach      __lsan_do_recoverable_leak_check while blocks are held by
                a global, by an address-taken local (data stack) and only
                by a register of a calling function; all freed after
     uninit     branches on an uninitialised malloc'ed int
     uninitstk  branches on an uninitialised local
     thread     the heap case in a new thread
     threadu    the uninit case in a new thread
     threadreg  main returns while a blocked thread holds 224 bytes only in
                a register and 192 in a data-stack slot

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc  # or the machine's
     for s in address leak memory; do
       $L -O2 -g -fsanitize=$s abileft-san.c -o san-$s -lpthread
       $L -O2 -g -fsanitize=$s -fPIE -pie abileft-san.c -o san-$s-pie \
          -lpthread
     done
     ./san-address heap; ...                     # on hardware  */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int __lsan_do_recoverable_leak_check (void) __attribute__ ((weak));

volatile long sink;
char garr[10];
void *gptr;

static void __attribute__ ((noinline))
opaque (void *p)
{
  __asm__ volatile ("" : : "r" (p) : "memory");
}

static void
show (void)
{
  int local;
  void *h = malloc (32);
  void *big = malloc (1 << 20);
  printf ("main %p, global %p, local %p, malloc(32) %p, malloc(1 MiB) %p\n",
          (void *) show, (void *) garr, (void *) &local, h, big);
  free (h);
  free (big);
}

static void __attribute__ ((noinline))
case_heap (void)
{
  char *p = malloc (10);
  opaque (p);
  sink = p[10];
  free (p);
}

static void __attribute__ ((noinline))
case_uaf (void)
{
  char *p = malloc (10);
  opaque (p);
  free (p);
  sink = p[0];
}

static void __attribute__ ((noinline))
case_stack (int i)
{
  char a[10];
  memset (a, 1, sizeof a);
  opaque (a);
  sink = a[i];
}

static void __attribute__ ((noinline))
case_global (int i)
{
  sink = garr[i];
}

static void __attribute__ ((noinline))
case_leak (void)
{
  void *p = malloc (100);
  opaque (p);
  p = NULL;
}

static int __attribute__ ((noinline))
check_now (void)
{
  if (!__lsan_do_recoverable_leak_check)
    {
      printf ("no __lsan_do_recoverable_leak_check in this build\n");
      return -1;
    }
  return __lsan_do_recoverable_leak_check ();
}

static int __attribute__ ((noinline))
deeper (int n)
{
  if (n > 0)
    return deeper (n - 1) + 0;
  return check_now ();
}

/* Three blocks of different sizes, so that a report names which one it
   thinks lost: 64 bytes held by a global, 96 by an address-taken local
   (a data-stack slot) and 160 only by a register of this function (no
   variadic call ever sees it, since those pass arguments in memory).  */
static void __attribute__ ((noinline))
case_reach (void)
{
  void *in_reg = malloc (160);
  void *volatile slot = malloc (96);
  gptr = malloc (64);
  memset (in_reg, 1, 160);
  opaque ((void *) &slot);
  printf ("held by a global (64 bytes), a data-stack slot at %p (96) and "
          "only a register (160)\n", (void *) &slot);
  fflush (stdout);
  int r = deeper (3);
  printf ("__lsan_do_recoverable_leak_check () = %d\n", r);
  fflush (stdout);
  opaque (in_reg);
  free (in_reg);
  free (slot);
  free (gptr);
}

static void __attribute__ ((noinline))
case_uninit (void)
{
  int *p = malloc (sizeof *p);
  opaque (p);
  if (*p == 42)
    puts ("42");
  free (p);
}

static void __attribute__ ((noinline))
case_uninitstk (void)
{
  int x;
  opaque (&x);
  if (x == 42)
    puts ("42");
}

/* A thread that holds 224 bytes only in a register and 192 in a
   data-stack slot, and stays blocked while main returns: the exit-time
   leak check stops it and scans its stack and registers.  */
static volatile int t_ready, t_stop;

static void *
reg_holder (void *arg)
{
  (void) arg;
  void *in_reg = malloc (224);
  void *volatile slot = malloc (192);
  memset (in_reg, 2, 224);
  opaque ((void *) &slot);
  t_ready = 1;
  while (!t_stop)
    usleep (1000);
  opaque (in_reg);
  free (in_reg);
  free (slot);
  return NULL;
}

static void *
thread_fn (void *arg)
{
  if (arg)
    case_uninit ();
  else
    case_heap ();
  return NULL;
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  const char *c = argc > 1 ? argv[1] : "ok";
  show ();
  if (!strcmp (c, "heap"))
    case_heap ();
  else if (!strcmp (c, "uaf"))
    case_uaf ();
  else if (!strcmp (c, "stack"))
    case_stack (argc + 9);
  else if (!strcmp (c, "global"))
    case_global (argc + 9);
  else if (!strcmp (c, "leak"))
    case_leak ();
  else if (!strcmp (c, "reach"))
    case_reach ();
  else if (!strcmp (c, "uninit"))
    case_uninit ();
  else if (!strcmp (c, "uninitstk"))
    case_uninitstk ();
  else if (!strcmp (c, "thread") || !strcmp (c, "threadu"))
    {
      pthread_t t;
      pthread_create (&t, NULL, thread_fn, c[6] ? (void *) 1 : NULL);
      pthread_join (t, NULL);
    }
  else if (!strcmp (c, "threadreg"))
    {
      pthread_t t;
      pthread_create (&t, NULL, reg_holder, NULL);
      while (!t_ready)
        usleep (1000);
      usleep (10000);
    }
  printf ("end of main (%s)\n", c);
  return 0;
}
