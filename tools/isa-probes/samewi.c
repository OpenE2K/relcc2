/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Memory accesses to one address in one wide instruction
   (spec/isa/bundle.md).  samewi.s has the instructions.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/isa-probes/samewi.c tools/isa-probes/samewi.s -o samewi
     ./samewi

   Each case runs 1000 times on a fresh doubleword (5, or 0 for the
   stores) and counts what the load returned and what memory held.  gas
   rejects a store in ALC2 with a load in ALC3 and a store in ALC5 with a
   load in ALC0, so those pairs are not here.  */

#include <stdio.h>

#define N 1000

long ldst_0_2 (long *), ldst_3_5 (long *), ldst_2_5 (long *),
  ldst_5_2 (long *), ldstw_5_2 (long *), ldstw_0_2 (long *);
void stst_2_5 (long *), stst_5_2 (long *);

static void
ldst (const char *name, long (*f) (long *), long fresh)
{
  static long cell[8] __attribute__ ((aligned (64)));
  long olds = 0, news = 0, other = 0, mem = 0;
  for (int i = 0; i < N; i++)
    {
      cell[0] = 5;
      long v = f (cell);
      if (v == 5)
        olds++;
      else if (v == fresh)
        news++;
      else
        other++;
      mem = cell[0];
    }
  printf ("%-10s load: old %ld, new %ld, other %ld; memory %#lx\n", name,
          olds, news, other, mem);
}

static void
stst (const char *name, void (*f) (long *))
{
  static long cell[8] __attribute__ ((aligned (64)));
  long n7 = 0, n9 = 0, other = 0;
  for (int i = 0; i < N; i++)
    {
      cell[0] = 0;
      f (cell);
      if (cell[0] == 7)
        n7++;
      else if (cell[0] == 9)
        n9++;
      else
        other++;
    }
  printf ("%-10s memory: first store's 7 %ld, second's 9 %ld, other %ld\n",
          name, n7, n9, other);
}

int
main (void)
{
  ldst ("ld0+st2", ldst_0_2, 7);
  ldst ("ld3+st5", ldst_3_5, 7);
  ldst ("ld2+st5", ldst_2_5, 7);
  ldst ("ld5+st2", ldst_5_2, 7);
  ldst ("ld5+stw2", ldstw_5_2, 0x700000005L);
  ldst ("ld0+stw2", ldstw_0_2, 0x700000005L);
  stst ("st2+st5", stst_2_5);
  stst ("st5+st2", stst_5_2);
  return 0;
}
