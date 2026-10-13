/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What lcc's -ffast-math link does to a process (spec/abi/registers.md,
   "Flush to zero"; docs/spec-poc-findings.md A28): lcc links MCST's
   crtfastmath.o, whose constructor sets %pfpfr's daz (bit 6) and fz (bit
   15), for -ffast-math, -Ofast and -funsafe-math-optimizations, also
   into shared libraries.  This shows what such a library does to the
   program that loads it.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O2 -fPIC -shared -ffast-math -DLIB pocabi1_fm.c -o libfm.so
     $L -O2 -fPIC -shared -DLIB pocabi1_fm.c -o libplain.so
     $L -O2 pocabi1_fm.c -o fm -ldl -lpthread           # plain program
     $L -O2 -ffast-math pocabi1_fm.c -o fm-fast -ldl -lpthread
     $L -O2 -DLINKED pocabi1_fm.c -o fm-linked -L. -lfm -ldl -lpthread
     ./fm ./libplain.so; ./fm ./libfm.so; ./fm-fast; LD_LIBRARY_PATH=. ./fm-linked

   Each line prints %pfpfr and what 1e-310 * 1.0 (a denormal operand:
   daz) and 1e-300 * 1e-10 (a denormal result: fz) give, in main, in a
   thread created before the dlopen and in threads created after it.  */

#include <stdio.h>

static unsigned
pfpfr (void)
{
  unsigned v;
  __asm__ volatile ("rrs %%pfpfr, %0" : "=r" (v));
  return v;
}

#ifdef LIB
int
lib_fn (void)
{
  return 42;
}
#else
#include <dlfcn.h>
#include <pthread.h>

static volatile double tiny = 1e-310, one = 1.0, a = 1e-300, b = 1e-10;

static void
show (const char *when)
{
  double daz = tiny * one, fz = a * b;
  printf ("%-34s %%pfpfr %#06x  1e-310*1 = %-12g 1e-300*1e-10 = %g\n",
	  when, pfpfr (), daz, fz);
}

static void *
thread (void *arg)
{
  show ((const char *) arg);
  return 0;
}

static void
in_thread (const char *when)
{
  pthread_t t;
  pthread_create (&t, 0, thread, (void *) when);
  pthread_join (t, 0);
}

/* A thread started before the dlopen that reports after it.  */
static volatile int loaded;

static void *
early_thread (void *arg)
{
  (void) arg;
  while (!loaded)
    ;
  show ("thread from before, after dlopen");
  return 0;
}

#ifdef LINKED
int lib_fn (void);
#endif

int
main (int argc, char **argv)
{
  show ("main, start");
#ifdef LINKED
  printf ("(linked against the library: lib_fn () = %d)\n", lib_fn ());
#endif
  in_thread ("new thread, start");
  if (argc > 1)
    {
      pthread_t early;
      pthread_create (&early, 0, early_thread, 0);
      void *h = dlopen (argv[1], RTLD_NOW);
      if (!h)
	{
	  printf ("dlopen: %s\n", dlerror ());
	  return 1;
	}
      show ("main, after dlopen");
      loaded = 1;
      pthread_join (early, 0);
      in_thread ("new thread, after dlopen");
      dlclose (h);
      show ("main, after dlclose");
    }
  return 0;
}
#endif
