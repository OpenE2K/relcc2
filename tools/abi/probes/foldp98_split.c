/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GCC PR 91663's shape: ipa-split moves excise's tail into
   excise.part.0, called with no location; with a high inlining limit
   the part is inlined back into excise, and the inlined block's call
   location defaults to excise's declaration, so gdb sees excise
   inlined into itself (doc/gcc-port-plan.md, D13, "Partial inlining";
   docs/spec-poc-findings.md P98).  Build with -O2 -g and count gdb's
   breakpoint locations and stops for the two calls:

     gcc -O2 -g [--param=max-inline-insns-auto=100]
         [-fno-partial-inlining] tools/abi/probes/foldp98_split.c -o s
     gdb -q -batch -ex 'break excise' -ex run -ex bt -ex continue \
         -ex bt -ex continue -ex continue -ex 'info breakpoints' ./s
     (and the same with a GCC for E2K, gdb on the machine)

   GCC 16.2.1 and gdb 18.1 on x86-64 (2026-10-24): with
   max-inline-insns-auto=100, excise.part.0 is inlined back into
   excise, a DW_TAG_inlined_subroutine of excise at excise's
   declaration line; break excise gets 5 locations and 3 stops for 2
   calls, and the backtrace shows excise inside excise.  With -fno-partial-inlining, 3 locations
   and one stop per call.  At the defaults the part stays out of line,
   its DIE still names excise, and break excise gets 4 locations and 4
   stops for 2 calls (excise "calling" excise).  The proof-of-concept
   GCC on the 4C behaves the same without its fix, and with it (a plain
   lexical block for a re-inlined split part) gives 3 locations and one
   stop per call, with the same code.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ent { int kind; const char *name; int depth; };
int counter;

__attribute__((noinline)) int
work (const char *s)
{
  counter += strlen (s);
  return counter & 1;
}

int
excise (struct ent *e, int x)
{
  if (e->kind == 0)
    return 0;
  if (work (e->name))
    {
      printf ("removed %s\n", e->name);
      e->depth += x;
      if (e->depth > 10)
	abort ();
      return 2;
    }
  fprintf (stderr, "cannot remove %s at depth %d\n", e->name, e->depth);
  e->depth -= x;
  return 3;
}

int
main (int argc, char **argv)
{
  struct ent a = { argc, "a", 0 }, b = { argc, "bb", 1 };
  int r = excise (&a, 1);
  r += excise (&b, 2);
  return r;
}
