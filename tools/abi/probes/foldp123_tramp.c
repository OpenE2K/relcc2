/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stack trampolines as doc/gcc-port-plan.md D1 designs them
   ("Trampolines"; spec/abi/elf.md, "Executable stack"; recheck of
   docs/spec-poc-findings.md P123), run on hardware.

   Part 1, "hand" (any compiler, lcc too): the five doublewords of MCST
   libffi's closure trampoline (D1; execstack/tramp-text.s assembles to
   them) are stored as constants into a 104-byte stack buffer, the
   static chain and the function each rotated by 32 bits, at an entry
   moved off page offsets 0xdc0-0xdff to 0xe00 (spec/isa/hwbugs.md,
   CODE_PLACEMENT), then __builtin___clear_cache, and the entry is
   called.  The target reads %g12 (the static chain) with inline asm and
   returns chain + argument.  This is repeated with the caller's frame
   shifted by 0, 16, ..., 4080 bytes (alloca), so the buffer passes every
   16-byte position of a page.
   Part 2, "nested" (GCC only: GNU C nested functions, built by the
   compiler's TARGET_TRAMPOLINE_INIT and TARGET_TRAMPOLINE_ADJUST_ADDRESS):
   a nested function passed as a pointer, at the same 256 frame shifts,
   and "rec": 64 recursive activations each make a trampoline to a
   nested function reading the activation's own local, and the deepest
   one calls them all.
   Part 3, "cost": rrd %clkr cycles per call of loop_call's loop
   (100,000 calls through a function pointer, minimum of 7 runs) to the
   target directly and through a trampoline.
   "segv" (argument): calls one hand-built trampoline with a SIGSEGV
   handler, which prints si_code and whether si_addr is the entry.
   "noadjust" (argument): part 1 without moving the entry, and 100,000
   calls of loop_call through each trampoline whose entry is at
   0xdc0-0xdff, checking every result (libffi doesn't move its entries,
   D1).
   Every run first prints the [stack] line's permissions from
   /proc/self/maps.

   Build (in tools/abi/probes; the lcc binary needs -z execstack, since
   lcc writes no GNU-stack note and no PT_GNU_STACK means a
   non-executable stack on E2K):

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O3 foldp123_tramp.c -o tramp-lcc -z execstack
     $L -O3 foldp123_tramp.c -o tramp-lcc-noexec
     # G: the proof of concept's compiler with its trampolines
     # (gcc-claude 6d0a5ba760f or later)
     $G -O2 foldp123_tramp.c -o tramp-gcc
     $G -O2 -static foldp123_tramp.c -o tramp-gcc-static
     $G -O2 foldp123_tramp.c -Wl,-z,noexecstack -o tramp-gcc-noexec

   Run on hardware, the cost under the timing lock:
     flock -w 3600 /tmp/relcc2-claude-timing.lock taskset -c 3 ./tramp-lcc
     ./tramp-lcc-noexec segv; ./tramp-gcc-noexec segv

   Output: "stack rwxp" or "rw-p"; per part, "ok" with the count of calls
   that returned the right value, the number of distinct entry offsets
   and how many entries lay in 0xdc0-0xdff (must be 0) or at 0xe00; the
   cost line; for "segv", the signal line (or "returned" if the call
   worked).

   Results (2026-11-03; 4C kernel 6.1.158-alt1.9.1 with v3 code, 8C2
   kernel 6.1.0-1.19 with v3 and 8C2 code; lcc 1.29.16 -O3 and
   gcc-claude 0b79b3fce35 -O2, dynamic and -static): "hand" and "nested"
   ok 256 of 256 everywhere, 252 distinct entry offsets, none in
   0xdc0-0xdff, 5 at 0xe00 (the 4 moved ones and one there anyway); "rec"
   ok 64 of 64; with RWE the [stack] is rwxp.  Cost: a call through the
   trampoline takes 10.0 cycles more than the same indirect call made
   directly (lcc 26.0 -> 36.0, GCC 19.0 -> 29.0 on both machines; 4C
   lcc 26.10 -> 36.12).  "segv" (no PT_GNU_STACK from lcc, RW from GCC
   with -z noexecstack): [stack] rw-p and SIGSEGV with si_code 1,
   SEGV_MAPERR, si_addr the entry, on both.  "noadjust": entries left at
   0xdc0-0xdff gave 400,000 right results of 400,000 on both machines,
   so moving them is a precaution like gas's padding, not shown
   necessary.  The 2C3 (v6) was offline: owed.  */

#define _GNU_SOURCE
#include <alloca.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PADS 256

static uint64_t
rot32 (uint64_t x)
{
  return (x << 32) | (x >> 32);
}

static inline unsigned long
clkr (void)
{
  unsigned long t;
  __asm__ volatile ("rrd %%clkr, %0" : "=r" (t));
  return t;
}

/* The target: the static chain in %g12 plus the argument.  */
static long __attribute__ ((noinline))
chain_plus (long x)
{
  long c;
  __asm__ volatile ("addd 0x0, %%dg12, %0" : "=r" (c));
  return c + x;
}

static void *expected_entry;
static int noadjust;

/* Builds a trampoline in its own frame and calls it.  */
static long __attribute__ ((noinline))
call_hand (long chain, long x, unsigned *off, int timing, long *cyc)
{
  uint64_t buf[13] __attribute__ ((aligned (16)));
  uintptr_t a = (uintptr_t) buf;
  uintptr_t o = a & 0xfff;
  if (o - 0xdc0 < 64 && !noadjust)
    a += 0xe00 - o;
  volatile uint64_t *t = (volatile uint64_t *) a;
  t[0] = 0x61c0dcd10c000032ULL;
  t[1] = 0x11c0deecULL;
  t[2] = rot32 ((uint64_t) chain);
  t[3] = rot32 ((uint64_t) (uintptr_t) chain_plus);
  t[4] = 0x8000042000001001ULL;
  __builtin___clear_cache ((char *) a, (char *) a + 40);
  __asm__ volatile ("" ::: "memory");
  long (*fp) (long) = (long (*) (long)) a;
  expected_entry = (void *) a;
  if (off)
    *off = a & 0xfff;
  if (timing == 2)
    {
      long right = 0;
      for (long i = 0; i < 100000; i++)
	right += fp (i) == chain + i;
      *cyc = right;
    }
  else if (timing)
    {
      extern long loop_call (long (*) (long), long);
      long best = -1;
      for (int r = 0; r < 7; r++)
	{
	  unsigned long t0 = clkr ();
	  loop_call (fp, 100000);
	  long d = (long) (clkr () - t0);
	  if (best < 0 || d < best)
	    best = d;
	}
      *cyc = best;
    }
  return fp (x);
}

long __attribute__ ((noinline))
loop_call (long (*fp) (long), long n)
{
  long s = 0;
  for (long i = 0; i < n; i++)
    s += fp (i);
  return s;
}

/* 100,000 calls through a trampoline built with the frame shifted by
   PAD * 16 bytes; returns how many were right.  */
static long loop_right;

static long __attribute__ ((noinline))
shifted_loop (int pad, long chain, unsigned *off)
{
  volatile char *p = alloca (pad * 16 + 16);
  p[0] = 0;
  call_hand (chain, 0, off, 2, &loop_right);
  return loop_right;
}

static long __attribute__ ((noinline))
shifted_hand (int pad, long chain, long x, unsigned *off)
{
  volatile char *p = alloca (pad * 16 + 16);
  p[0] = 0;
  return call_hand (chain, x, off, 0, 0);
}

static void
report (const char *what, int ok, int n, const unsigned *offs)
{
  int distinct = 0, bad = 0, at_e00 = 0;
  static unsigned char seen[4096];
  memset (seen, 0, sizeof seen);
  for (int i = 0; i < n; i++)
    {
      if (!seen[offs[i]])
	distinct++;
      seen[offs[i]] = 1;
      if (offs[i] - 0xdc0 < 64)
	bad++;
      if (offs[i] == 0xe00)
	at_e00++;
    }
  printf ("%s: ok %d of %d; %d distinct entry offsets, %d in 0xdc0-0xdff,"
	  " %d at 0xe00\n", what, ok, n, distinct, bad, at_e00);
}

#ifndef __LCC__
static long __attribute__ ((noinline))
apply (long (*f) (long), long x)
{
  return f (x);
}

static long __attribute__ ((noinline))
nested_once (long base, long x, unsigned *off)
{
  long add (long y) { return base + y; }
  long (*fp) (long) = add;
  *off = (uintptr_t) fp & 0xfff;
  return apply (fp, x);
}

static long __attribute__ ((noinline))
shifted_nested (int pad, long base, long x, unsigned *off)
{
  volatile char *p = alloca (pad * 16 + 16);
  p[0] = 0;
  return nested_once (base, x, off);
}

#define DEPTH 64
static long (*rec_fps[DEPTH]) (long);

static int __attribute__ ((noinline))
rec (int depth)
{
  long mine = depth * 1000 + 7;
  long get (long y) { return mine + y; }
  rec_fps[depth] = get;
  if (depth + 1 < DEPTH)
    return rec (depth + 1);
  int ok = 0;
  for (int i = 0; i < DEPTH; i++)
    ok += apply (rec_fps[i], i) == i * 1000 + 7 + i;
  return ok;
}
#endif

static void
segv_handler (int sig, siginfo_t *si, void *uc)
{
  char b[160];
  (void) sig;
  (void) uc;
  int n = snprintf (b, sizeof b, "segv: SIGSEGV si_code %d (%s), si_addr %s\n",
		    si->si_code,
		    si->si_code == SEGV_ACCERR ? "SEGV_ACCERR"
		    : si->si_code == SEGV_MAPERR ? "SEGV_MAPERR"
		    : si->si_code == SEGV_BNDERR ? "SEGV_BNDERR" : "other",
		    si->si_addr == expected_entry ? "= the entry" : "elsewhere");
  write (1, b, n);
  _exit (0);
}

static void
print_stack_perms (void)
{
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  while (f && fgets (line, sizeof line, f))
    if (strstr (line, "[stack]"))
      {
	char perms[8] = "";
	sscanf (line, "%*s %7s", perms);
	printf ("stack %s\n", perms);
      }
  if (f)
    fclose (f);
}

int
main (int argc, char **argv)
{
  static unsigned offs[PADS];
  print_stack_perms ();
  fflush (stdout);

  if (argc > 1 && strcmp (argv[1], "segv") == 0)
    {
      struct sigaction sa;
      memset (&sa, 0, sizeof sa);
      sa.sa_sigaction = segv_handler;
      sa.sa_flags = SA_SIGINFO;
      sigaction (SIGSEGV, &sa, 0);
      unsigned off;
      long r = call_hand (0x1000, 1, &off, 0, 0);
      printf ("segv: returned %ld (expected %d), no signal\n", r, 0x1001);
      return 0;
    }

  if (argc > 1 && strcmp (argv[1], "noadjust") == 0)
    {
      noadjust = 1;
      int ok = 0, bad = 0;
      long badok = 0;
      for (int pad = 0; pad < PADS; pad++)
	ok += shifted_hand (pad, 0x1000 + pad, pad, &offs[pad])
	      == 0x1000 + 2 * pad;
      report ("noadjust", ok, PADS, offs);
      for (int pad = 0; pad < PADS; pad++)
	{
	  unsigned off;
	  long right = shifted_loop (pad, 0x3000 + pad, &off);
	  if (off - 0xdc0 < 64)
	    {
	      bad++;
	      badok += right;
	    }
	}
      printf ("noadjust: %ld of %d right results from entries in"
	      " 0xdc0-0xdff\n", badok, bad * 100000);
      return 0;
    }

  int ok = 0;
  for (int pad = 0; pad < PADS; pad++)
    ok += shifted_hand (pad, 0x1000 + pad, pad, &offs[pad]) == 0x1000 + 2 * pad;
  report ("hand", ok, PADS, offs);

#ifndef __LCC__
  ok = 0;
  for (int pad = 0; pad < PADS; pad++)
    ok += shifted_nested (pad, 0x2000 + pad, pad, &offs[pad]) == 0x2000 + 2 * pad;
  report ("nested", ok, PADS, offs);
  printf ("rec: ok %d of %d\n", rec (0), DEPTH);
#endif

  long best = -1;
  for (int r = 0; r < 7; r++)
    {
      unsigned long t0 = clkr ();
      loop_call (chain_plus, 100000);
      long d = (long) (clkr () - t0);
      if (best < 0 || d < best)
	best = d;
    }
  long tcyc;
  call_hand (0, 0, 0, 1, &tcyc);
  printf ("cost: %.2f cycles a call directly, %.2f through a trampoline,"
	  " %.2f more\n", best / 100000.0, tcyc / 100000.0,
	  (tcyc - best) / 100000.0);
  return 0;
}
