/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which shapes lcc's v5 code turns into getfz, and where it uses scalar
   plog: a recheck of the proof-of-concept back end's findings I53 and
   I54 in docs/spec-poc-findings.md, for spec/isa/semantics.md ("Field
   extract") and operations-v5.yaml.  Read the assembly, nothing runs:

     for t in v5.8c2 v6.2c3 v3; do
       /opt/mcst/lcc-1.29.16.e2k-$t.linux-6.1/bin/lcc -O3 -S \
         tools/isa-probes/foldisa_getfz.c -o foldisa_getfz-$t.s
     done          # and lcc 1.31.05 (toolchains/lcc-1.31.05-e2k-v5.8c2)

   What lcc 1.29.16 -O3 printed for v5.8c2 and v6.2c3 alike (1.31.05 for
   v5.8c2 the same, but for tidx_uc2 and the loop majv, below), with
   v3's code after the semicolon: */

struct tv { long a, b; };

/* shrd x, 8 + andd 0xff0 (v5 too) */
unsigned long mask_shr (unsigned long x) { return (x >> 8) & 0xff0; }
/* andd x, 0xfff0 (v5 too) */
unsigned long mask_only (unsigned long x) { return x & 0xfff0; }
/* getfzd x, 0x8303 (offset 3, len 12, 4 zeros); shrd x, 3 + andd 0xff0 */
unsigned long shl_field (unsigned long x) { return ((x >> 7) & 0xff) << 4; }
/* shrs + ands (v5 too) */
unsigned mask_shr32 (unsigned x) { return (x >> 8) & 0xff0; }
/* getfzs x, 0x8303; shrs x, 3 + ands 0xff0 */
unsigned shl_field32 (unsigned x) { return ((x >> 7) & 0xff) << 4; }
/* sxt 6 + getfzd 0x62c4 (3 zeros) + ldd; sxt + shrd + andd + ldd */
long idx8 (long *p, unsigned i) { return p[(i >> 7) & 0xff]; }
/* sxt 6 + getfzd 0x8303 + ldd */
long idx16 (struct tv *p, unsigned i) { return p[(i >> 7) & 0xff].a; }
/* sxt 6 + getfzd 0x8303 + addd */
struct tv *addr16 (struct tv *p, unsigned i) { return &p[(i >> 7) & 0xff]; }
/* getfzd 0x8303 + ldd */
long idx16l (struct tv *p, unsigned long i) { return p[(i >> 7) & 0xff].a; }
/* getfzd 0x44cd (offset 13, len 19, 2 zeros) + ldw */
int idx4 (int *p, unsigned long i) { return p[(i >> 15) & 0x1ffff]; }
/* getfzd 0x2257 (1 zero) + ldh */
short idx2 (short *p, unsigned long i) { return p[(i >> 24) & 0xff]; }
/* getfzd 0x2246 */
unsigned long k1 (unsigned long x) { return ((x >> 7) & 0xff) << 1; }
/* getfzd 0xe3c0 (7 zeros) */
unsigned long k7 (unsigned long x) { return ((x >> 7) & 0xff) << 7; }
/* shld x, 1 + andd 0xff00: no getfz beyond 7 zeros */
unsigned long k8 (unsigned long x) { return ((x >> 7) & 0xff) << 8; }
/* getfzd 0x8303 */
unsigned long mul16 (unsigned long x) { return ((x >> 7) & 0xff) * 16; }
/* getfzd 0x7447: a signed field (bit 12, v5's sign rule), 3 zeros */
long sfield3 (long x) { return ((x << 40) >> 50) << 3; }
struct bf { unsigned a : 7, b : 9; };
/* sxt 6 + getfzd 0x8343 */
unsigned long bfield (struct bf s) { return (unsigned long) s.b << 4; }
/* andd 0xfff + shld 4: a field at bit 0 is mostly not turned into getfz */
unsigned long low4 (unsigned long x) { return (x & 0xfff) << 4; }

/* The values above written as masks with their zeros stay as written:
   shrd x, 3 + andd 0xff0 (shl_field's value) */
unsigned long same_mask (unsigned long x) { return (x >> 3) & 0xff0; }
/* andd 0x7f80 (k7's value) */
unsigned long same_k7 (unsigned long x) { return x & 0x7f80; }
/* getfzd 0x8400 (offset 0, len 16, 4 zeros): mask_only's value as a
   shifted field; andd 0xfff0 */
unsigned long low_k4b (unsigned long x) { return ((x >> 4) & 0xfff) << 4; }
/* shrd x, 8 + andd 0xff00: k8's limit (at most 7 zeros) */
unsigned long k8r (unsigned long x) { return ((x >> 16) & 0xff) << 8; }
/* andd 0xff + shld 1 */
unsigned long low1 (unsigned long x) { return (x & 0xff) << 1; }
/* andd 0xff + shld 2 + ldw */
int tidx_and (const int *t, unsigned long x) { return t[x & 0xff]; }
/* sxt 4 + shld 2 + ldw */
int tidx_uc (const int *t, unsigned long x) { return t[(unsigned char) x]; }
/* shrd 8 + sxt 4 + shld 2 + ldw; lcc 1.31.05: getfzd 0x4286 + ldw */
int tidx_uc2 (const int *t, unsigned long x)
{ return t[(unsigned char) (x >> 8)]; }
/* But a byte passed to an inline function that indexes a table, as
   glibc's sysdeps/e2k/strcasecmp.c does (ch1 = (a0) & 0xff; TOLOWER
   (ch1)): getfzd a0, 0x42be (offset 62, the field wrapping round to
   bit 0, len 10, 2 zeros) + ldw; shld 2 + andd 0x3fc + ldw.  */
static inline int tl (const int *t, int c) { return t[c]; }
int tbyte (const int *t, unsigned long a0)
{ unsigned char ch1 = a0 & 0xff; return tl (t, ch1); }

/* Scalar three-input logic stays ordinary logic operations (no plog):
   ord, andd, andd, ord; andd, andnd, ord; xord, xord; andd, ord.  */
unsigned long maj (unsigned long a, unsigned long b, unsigned long c)
{ return (a & b) | (a & c) | (b & c); }
unsigned long sel (unsigned long a, unsigned long b, unsigned long c)
{ return (a & b) | (~a & c); }
unsigned long x3 (unsigned long a, unsigned long b, unsigned long c)
{ return a ^ b ^ c; }
unsigned long aoc (unsigned long a, unsigned long b, unsigned long c)
{ return a | (b & c); }
/* Vectorized: qplog_0xa8 and qplog_0xea (c & (a | b), then c | (a & b));
   lcc 1.31.05 also chains plog_0xa8 and plog_0xea into the 64-bit
   part's stores.  */
void majv (unsigned long *r, const unsigned long *a, const unsigned long *b,
           const unsigned long *c, int n)
{
  for (int i = 0; i < n; i++)
    r[i] = (a[i] & b[i]) | (a[i] & c[i]) | (b[i] & c[i]);
}
