/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Byte-swapped ("big-endian") memory accesses: MAS bit 3, as lcc's
   __builtin_e2k_{ld,st}_*_be emit them (spec/isa/semantics.md).
   mas_be.s and, for v5, mas_be-v5.s are hand-written.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/mas_be.c tools/isa-probes/mas_be.s -o mas_be
     $LCC5 -O3 -DMBE_V5 tools/isa-probes/mas_be.c tools/isa-probes/mas_be.s \
       tools/isa-probes/mas_be-v5.s -o mas_be      # v5: also ldqp/stqp
     ./mas_be                             # on hardware

   Add tools/isa-probes/mas_be_loops.c, compiled separately at -O3, for
   the loop timings (sum and copy through __builtin_bswap64 or through
   lcc's _be builtins).  */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static sigjmp_buf env;

static void
handler (int sig)
{
  siglongjmp (env, sig);
}

void mbe_loads (unsigned long *, const void *);
void mbe_stores (void *, unsigned long);
long chase_plain (void *, long, long);
long chase_be (void *, long, long);
long chase_shuf (void *, long, long);
long chase_st_lcc (long, void *);
/* One misaligned access each, plain and with mas=0x8:
   f (out, buf + 0, value).  */
#define MIS(n) void mis_##n (unsigned long *, void *, unsigned long);
#define MISCASES \
  MIS (ldh1) MIS (ldh1_be) MIS (ldw1) MIS (ldw1_be) MIS (ldw2) \
  MIS (ldw2_be) MIS (ldd3) MIS (ldd3_be) MIS (ldd4) MIS (ldd4_be) \
  MIS (sth1) MIS (sth1_be) MIS (stw2) MIS (stw2_be) MIS (std4) MIS (std4_be)
MISCASES
#undef MIS
long chase_st_mas (long, void *);
unsigned long sum_shuf (const unsigned long *, long);
unsigned long sum_be (const unsigned long *, long);
void copy_shuf (unsigned long *, const unsigned long *, long);
void copy_be_st (unsigned long *, const unsigned long *, long);
void copy_be_ld (unsigned long *, const unsigned long *, long);
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
  /* Misaligned accesses, plain and swapped (the offsets are from an
     8-byte aligned buffer).  */
  signal (SIGBUS, handler);
  signal (SIGSEGV, handler);
  {
    static const struct { const char *n; void (*f) (unsigned long *, void *,
                                                    unsigned long); } mc[] = {
#define MIS(n) { #n, mis_##n },
      MISCASES
#undef MIS
    };
    printf ("misaligned:");
    for (unsigned i = 0; i < sizeof mc / sizeof *mc; i++)
      {
        unsigned long m = 0;
        unsigned char sb[16] __attribute__ ((aligned (16)));
        memset (sb, 0, sizeof sb);
        int sig = sigsetjmp (env, 1);
        if (sig)
          printf (" %s:sig%d", mc[i].n, sig);
        else
          {
            int store = mc[i].n[0] == 's';
            mc[i].f (&m, store ? sb : (void *) buf, 0x0102030405060708UL);
            if (store)
              {
                printf (" %s:", mc[i].n);
                for (int k = 0; k < 16; k++)
                  printf ("%02x", sb[k]);
              }
            else
              printf (" %s:%lx", mc[i].n, m);
          }
        if (i % 4 == 3)
          printf ("\n           ");
      }
    printf ("\n");
  }
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
  long best[3] = { 1L << 60, 1L << 60, 1L << 60 };
  for (int r = 0; r < 20; r++)
    {
      long a = chase_plain (&cell, 0, 0), b = chase_be (&cellbe, 0, 0),
           c = chase_shuf (&cellbe, 0, 0);
      best[0] = a < best[0] ? a : best[0];
      best[1] = b < best[1] ? b : best[1];
      best[2] = c < best[2] ? c : best[2];
    }
  printf ("64 dependent loads: plain %ld cycles, mas 8 %ld, ldd + pshufb "
          "%ld\n", best[0], best[1], best[2]);
  static unsigned int word __attribute__ ((aligned (8)));
  long bs[2] = { 1L << 60, 1L << 60 };
  for (int r = 0; r < 20; r++)
    {
      long a = chase_st_lcc (0x11223344, &word),
           b = chase_st_mas (0x11223344, &word);
      bs[0] = a < bs[0] ? a : bs[0];
      bs[1] = b < bs[1] ? b : bs[1];
    }
  printf ("32 x (swap + stw, ldw mas 8): sxt + pshufb + stw %ld cycles, "
          "stw mas 8 %ld\n", bs[0], bs[1]);

  /* Loops over 2,048 elements (16 KiB, in L1 after the first run):
     best of 2,000 runs, in ns per element.  */
  enum { N = 2048 };
  static unsigned long a[N] __attribute__ ((aligned (64))),
    d[N] __attribute__ ((aligned (64)));
  for (int i = 0; i < N; i++)
    a[i] = 0x0102030405060708UL * (i + 1);
  struct { const char *name; int kind; } loops[] = {
    { "sum, bswap64 (ldd + pshufb)", 0 }, { "sum, ld_64s_be (mas 8)", 1 },
    { "copy, bswap64", 2 }, { "copy, st_64s_be (mas 8)", 3 },
    { "copy, ld_64s_be (mas 8)", 4 } };
  unsigned long check[2] = { 0, 0 };
  for (int k = 0; k < 5; k++)
    {
      double b = 1e30;
      for (int r = 0; r < 2000; r++)
        {
          struct timespec t0, t1;
          clock_gettime (CLOCK_MONOTONIC, &t0);
          switch (loops[k].kind)
            {
            case 0: check[0] = sum_shuf (a, N); break;
            case 1: check[1] = sum_be (a, N); break;
            case 2: copy_shuf (d, a, N); break;
            case 3: copy_be_st (d, a, N); break;
            case 4: copy_be_ld (d, a, N); break;
            }
          clock_gettime (CLOCK_MONOTONIC, &t1);
          double ns = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
          b = ns < b ? ns : b;
        }
      if (k >= 2 && d[5] != __builtin_bswap64 (a[5]))
        printf ("  wrong copy\n");
      printf ("%-30s %.3f ns/element\n", loops[k].name, b / N);
    }
  if (check[0] != check[1])
    printf ("  sums differ\n");
  return 0;
}
