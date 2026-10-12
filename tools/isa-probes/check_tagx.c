/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Check tagx.c's model lines ("m OP SET IN0 IN1 IN2 IN3 -> OUT0 OUT1
   flags F") against reference models on an x86-64 host
   (spec/isa/semantics.md, "Other scalar operations").

     cc -O1 tools/isa-probes/check_tagx.c -o check_tagx
     ./check_tagx < out.txt

   The operations run under dbl = 0 with both destination registers
   preset to 0x5555555555555555, so a 32-bit result keeps the upper half
   0x55555555.  muls: the low 32 bits of the product of the low halves;
   muld: the low 64 bits of the product; merges, merged: IN0 when the
   predicate (IN3 == 1) is false, IN1 when true; movts, movtd: IN0;
   movtq: IN0 and IN1; fstoistr: SSE cvttss2si of IN0's low half with
   its MXCSR flags (round to nearest, every exception masked).  The
   integer operations must raise no flag.  Prints every line that
   differs and a count per operation.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long u64;
#define KEEP 0x5555555500000000UL
#define PRESET 0x5555555555555555UL

static void
cvttss2si (unsigned x, unsigned *res, unsigned *flags)
{
  unsigned c = 0x1f80, r;
  __asm__ volatile ("ldmxcsr %2\n\t"
                    "movd %3, %%xmm7\n\t"
                    "cvttss2si %%xmm7, %0\n\t"
                    "stmxcsr %1"
                    : "=r" (r), "=m" (c) : "m" (c), "r" (x) : "xmm7");
  *res = r;
  *flags = c & 0x3f;
}

int
main (void)
{
  char line[512], op[32];
  long n = 0, bad = 0;
  struct { char name[16]; long n, bad; } per[16];
  int nper = 0;
  while (fgets (line, sizeof line, stdin))
    {
      int s;
      u64 in[4], o0, o1, fl;
      if (strncmp (line, "m ", 2) != 0)
        continue;
      if (sscanf (line, "m %31s %d %lx %lx %lx %lx -> %lx %lx flags %lx", op,
                  &s, &in[0], &in[1], &in[2], &in[3], &o0, &o1, &fl) != 9)
        {
          printf ("unparsed: %s", line);
          bad++;
          continue;
        }
      u64 e0 = PRESET, e1 = PRESET, ef = 0;
      int pred = in[3] == 1;
      if (!strcmp (op, "muls"))
        e0 = KEEP | (unsigned) ((unsigned) in[0] * (unsigned) in[1]);
      else if (!strcmp (op, "muld"))
        e0 = in[0] * in[1];
      else if (!strcmp (op, "merges"))
        e0 = KEEP | (unsigned) (pred ? in[1] : in[0]);
      else if (!strcmp (op, "merged"))
        e0 = pred ? in[1] : in[0];
      else if (!strcmp (op, "movts"))
        e0 = KEEP | (unsigned) in[0];
      else if (!strcmp (op, "movtd"))
        e0 = in[0];
      else if (!strcmp (op, "movtq"))
        e0 = in[0], e1 = in[1];
      else if (!strcmp (op, "fstoistr"))
        {
          unsigned r, f;
          cvttss2si ((unsigned) in[0], &r, &f);
          e0 = KEEP | r;
          ef = f;
        }
      else
        {
          printf ("unknown op: %s", line);
          bad++;
          continue;
        }
      int k;
      for (k = 0; k < nper && strcmp (per[k].name, op); k++)
        ;
      if (k == nper && nper < 16)
        {
          strcpy (per[nper].name, op);
          per[nper].n = per[nper].bad = 0;
          nper++;
        }
      n++;
      per[k].n++;
      if (o0 != e0 || o1 != e1 || fl != ef)
        {
          bad++;
          per[k].bad++;
          printf ("differs: %s  expected %016lx %016lx flags %02lx\n",
                  strtok (line, "\n"), e0, e1, ef);
        }
    }
  for (int k = 0; k < nper; k++)
    printf ("%-9s %6ld lines, %ld differ\n", per[k].name, per[k].n,
            per[k].bad);
  printf ("total %ld lines, %ld differ\n", n, bad);
  return bad != 0;
}
