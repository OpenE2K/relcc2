/* SPDX-License-Identifier: GPL-3.0-or-later */
/* getfs and getfd, the field extract (spec/isa/semantics.md, "Field
   extract"): every offset, length, sign bit and byte selector of the
   descriptor over a few values, compared with two models (the community
   qemu's v1 and v5 helpers), plus descriptors with their upper bits
   set.  getf.s is hand-written.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/getf.c tools/isa-probes/getf.s -o getf
     ./getf                               # on hardware

   Model: rotate src1 right by the offset (desc bits 0-5; s: 0-4), keep
   the low LEN bits (bits 6-11; s: 6-10); with bit 12 set, sign-extend
   from a bit that v5 takes from the field's top (sextract; for LEN 0
   the rotated value's top bit) and v1 from bit
   8 * BYTE + ((offset + LEN - 1) & 7) of src1, BYTE = desc bits 13-15
   (s: 13-14).  getfs keeps the upper half
   of its destination (dbl = 0).  */

#include <stdio.h>

typedef unsigned long u64;

u64 getfd_ (u64, u64), getfs_ (u64, u64);

static u64
ror (u64 x, int k, int w)
{
  u64 m = w == 64 ? ~0UL : (1UL << w) - 1;
  x &= m;
  return k ? ((x >> k) | (x << (w - k))) & m : x;
}

/* V5: 1 for the v5 rule, 0 for the v1 rule.  */
static u64
model (u64 x, u64 d, int w, int v5)
{
  int lb = w == 64 ? 6 : 5;
  int off = d & ((1 << lb) - 1), len = (d >> 6) & ((1 << lb) - 1);
  int sign = (d >> 12) & 1, byte = (d >> 13) & (w == 64 ? 7 : 3);
  u64 m = w == 64 ? ~0UL : 0xffffffffUL;
  /* LEN 0 keeps no bits but still sign-extends, from the bit below
     the field (bit -1 of the rotated value, i.e. its top bit).  */
  u64 rot = ror (x, off, w);
  u64 r = len ? rot & ((1UL << len) - 1) : 0;
  int sb = v5 ? (int) (rot >> ((len + w - 1) % w)) & 1
              : (int) (x >> (byte * 8 + ((off + len - 1) & 7))) & 1;
  if (sign && sb)
    r |= ~0UL << len;
  return r & m;
}

int
main (void)
{
  static const u64 xs[] = { 0x0123456789abcdefUL, 0xfedcba9876543210UL,
                            0x8000000000000001UL, 0x5a5a5a5aa5a5a5a5UL,
                            0xffffffffffffffffUL };
  for (int w = 64; w >= 32; w -= 32)
    {
      long n = 0, bad[2] = { 0, 0 }, shown = 0;
      int lb = w == 64 ? 6 : 5, nbyte = w == 64 ? 8 : 4;
      for (unsigned i = 0; i < sizeof xs / sizeof *xs; i++)
        for (int off = 0; off < w; off++)
          for (int len = 0; len < (1 << lb); len++)
            for (int sign = 0; sign < 2; sign++)
              for (int byte = 0; byte < nbyte; byte++)
                {
                  u64 x = xs[i];
                  u64 d = off | (u64) len << 6 | (u64) sign << 12
                          | (u64) byte << 13;
                  u64 r = w == 64 ? getfd_ (x, d) : getfs_ (x, d);
                  if (w == 32)
                    {
                      if (r >> 32 != 0x55555555UL)
                        printf ("getfs upper half changed: %016lx\n", r);
                      r &= 0xffffffffUL;
                    }
                  n++;
                  for (int v = 0; v < 2; v++)
                    if (r != model (x, d, w, v))
                      bad[v]++;
                  if (r != model (x, d, w, 0) && r != model (x, d, w, 1)
                      && shown++ < 8)
                    printf ("getf%c %016lx desc %05lx: %016lx (v1 %016lx, "
                            "v5 %016lx)\n", w == 64 ? 'd' : 's', x, d, r,
                            model (x, d, w, 0), model (x, d, w, 1));
                }
      printf ("getf%c: %ld cases, %ld differ from the v1 model, %ld from "
              "the v5 model\n", w == 64 ? 'd' : 's', n, bad[0], bad[1]);
    }
  /* Upper descriptor bits: getfd field 0x1234 with garbage above.  */
  static const u64 hi[] = { 0, 0x10000, 0xffff0000UL, 0xffffffffffff0000UL };
  for (unsigned i = 0; i < sizeof hi / sizeof *hi; i++)
    printf ("getfd desc %016lx: %016lx, getfs: %08lx\n", 0x1234 | hi[i],
            getfd_ (xs[0], 0x1234 | hi[i]),
            getfs_ (xs[0], 0x1234 | hi[i]) & 0xffffffffUL);
  return 0;
}
