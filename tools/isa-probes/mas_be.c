/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Byte-swapped ("big-endian") memory accesses: MAS bit 3, as lcc's
   __builtin_e2k_{ld,st}_*_be emit them (spec/isa/semantics.md).
   mas_be.s and, for v5, mas_be-v5.s are hand-written.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O1 tools/isa-probes/mas_be.c tools/isa-probes/mas_be.s -o mas_be
     $LCC5 -O1 -DMBE_V5 tools/isa-probes/mas_be.c tools/isa-probes/mas_be.s \
       tools/isa-probes/mas_be-v5.s -o mas_be      # v5: also ldqp/stqp
     ./mas_be                             # on hardware  */

#include <stdio.h>
#include <string.h>

void mbe_loads (unsigned long *, const void *);
void mbe_stores (void *, unsigned long);
long chase_plain (void *, long, long);
long chase_be (void *, long, long);
#ifdef MBE_V5
void mbe_quad (unsigned long *, const void *);
#endif

static unsigned char buf[64] __attribute__ ((aligned (16)));

int
main (void)
{
  unsigned long out[8] __attribute__ ((aligned (16)));
  for (int i = 0; i < 64; i++)
    buf[i] = i + 1;
  memset (out, 0xee, sizeof out);
  mbe_loads (out, buf);
  printf ("buf bytes 01 02 03 ... 10\n");
  printf ("ldb mas 8  %016lx\nldh mas 8  %016lx\nldw mas 8  %016lx\n"
          "ldd mas 8  %016lx\nldd,sm b   %016lx\nldd        %016lx\n"
          "ldd+8 mas 8 %016lx\n", out[0], out[1], out[2], out[3], out[4],
          out[5], out[6]);
  unsigned char st[32];
  memset (st, 0, sizeof st);
  mbe_stores (st, 0x0102030405060708UL);
  printf ("stores of 0x0102030405060708, mas 8:");
  for (int i = 0; i < 32; i++)
    printf ("%s%02x", i % 8 ? "" : "  ", st[i]);
  printf ("  (sth, stw, std, stb)\n");
#ifdef MBE_V5
  unsigned long q[4] __attribute__ ((aligned (16)));
  memset (q, 0xee, sizeof q);
  mbe_quad (q, buf);
  printf ("ldqp mas 8  high %016lx low %016lx\n", q[1], q[0]);
  printf ("stqp mas 8 bytes:");
  for (int i = 16; i < 32; i++)
    printf (" %02x", ((unsigned char *) q)[i]);
  printf ("\n");
#endif
  /* A self-pointer, stored plain and byte-swapped.  */
  static unsigned long cell, cellbe;
  cell = (unsigned long) &cell;
  cellbe = __builtin_bswap64 ((unsigned long) &cellbe);
  long best[2] = { 1L << 60, 1L << 60 };
  for (int r = 0; r < 20; r++)
    {
      long a = chase_plain (&cell, 0, 0), b = chase_be (&cellbe, 0, 0);
      best[0] = a < best[0] ? a : best[0];
      best[1] = b < best[1] ? b : best[1];
    }
  printf ("64 dependent loads: plain %ld cycles, mas 8 %ld cycles\n",
          best[0], best[1]);
  return 0;
}
