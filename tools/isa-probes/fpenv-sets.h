/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The input sets of fpenv.c, shared with check_fpenv.c.  A set is ten
   64-bit words: doubles a, b, c; floats a, b, c (low 32 bits); a 64-bit
   and a 32-bit integer; two packed-single operands.  Sets 0..NEDGE-1
   are chosen around the denormal range and run under all 16 control
   settings; the NRAND random sets after them run only without fz and
   daz, in the four rounding modes.  */

typedef unsigned long u64;

static const u64 fe_dpat[] = {
  0x0000000000000000UL, 0x8000000000000000UL, /* +-0 */
  0x3ff0000000000000UL, 0xbff0000000000000UL, /* +-1 */
  0x3ff8000000000000UL,                       /* 1.5 */
  0x0000000000000001UL, 0x8000000000000001UL, /* +-smallest denormal */
  0x000fffffffffffffUL, 0x800fffffffffffffUL, /* +-largest denormal */
  0x0008000000000001UL,                       /* a middle denormal */
  0x0010000000000000UL, 0x8010000000000000UL, /* +-smallest normal */
  0x0010000000000001UL, 0x0018000000000000UL, /* 1.5 * 2^-1022 */
  0x3fe0000000000000UL, 0x3fe8000000000000UL, /* 0.5, 0.75 */
  0x4000000000000000UL, 0x3fe5555555555555UL, /* 2, 2/3 */
  0x2000000000000000UL, 0x1e30000000000000UL, /* 2^-511, 2^-540 */
  0x7ff0000000000000UL, 0x7ff8000000000000UL, /* inf, qNaN */
  0x7ff4000000000000UL, 0x7fefffffffffffffUL, /* sNaN, max */
  0x3ff0000000000001UL, 0x3fefffffffffffffUL, /* 1 + ulp, 1 - ulp/2 */
  0x41e0000000000000UL, 0xc1e0000000200000UL, /* 2^31, -(2^31 + 0.5) */
  0x4330000000000001UL, 0x3cb0000000000000UL, /* 2^52 + 1, 2^-52 */
};
static const unsigned fe_spat[] = {
  0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x3fc00000u,
  0x00000001u, 0x80000001u, 0x007fffffu, 0x807fffffu, 0x00400001u,
  0x00800000u, 0x80800000u, 0x00800001u, 0x00c00000u, 0x3f000000u,
  0x3f400000u, 0x40000000u, 0x3f2aaaabu, 0x1f800000u, 0x1e000000u,
  0x7f800000u, 0x7fc00000u, 0x7fa00000u, 0x7f7fffffu, 0x3f800001u,
  0x3f7fffffu, 0x4f000000u, 0xcf000001u, 0x4b000001u, 0x34000000u,
};
static const u64 fe_ipat[] = {
  0, 1, (u64) -1, 0x8000000000000000UL, 0x7fffffffffffffffUL,
  0x0020000000000001UL, 0x0020000000000003UL, 0xffdffffffffffffdUL,
  0x0000000001000001UL, 0x0000000001000003UL, 0x00000000ff000001UL,
  0x0000000080000001UL, 0x7ffffffeUL, 0x123456789abcdef1UL,
};
#define FE_ND (sizeof fe_dpat / sizeof fe_dpat[0])
#define FE_NS (sizeof fe_spat / sizeof fe_spat[0])
#define FE_NI (sizeof fe_ipat / sizeof fe_ipat[0])
#define NEDGE 120
#define NRAND 1500
#define NSETS (NEDGE + NRAND)

static u64 fe_x = 88172645463325252UL;

static u64
fe_rand (void)
{
  fe_x ^= fe_x << 13;
  fe_x ^= fe_x >> 7;
  fe_x ^= fe_x << 17;
  return fe_x;
}

/* A random double: random sign and significand, exponent within E of
   1.0, or (bit 63 of a draw) near the denormal range.  */
static u64
fe_rdouble (int e)
{
  u64 r = fe_rand (), s = fe_rand ();
  long ex = (r >> 63) && (s & 1) ? (long) (r % 60) - 10 + 1
                                 : 1023 + (long) (r % (2 * e + 1)) - e;
  if (ex < 0)
    ex = 0;
  return (s & 0x8000000000000000UL) | ((u64) ex << 52)
         | (fe_rand () & 0xfffffffffffffUL);
}

static unsigned
fe_rsingle (int e)
{
  u64 r = fe_rand (), s = fe_rand ();
  long ex = (r >> 63) && (s & 1) ? (long) (r % 30) - 5 + 1
                                 : 127 + (long) (r % (2 * e + 1)) - e;
  if (ex < 0)
    ex = 0;
  return (unsigned) ((s >> 32) & 0x80000000u) | (unsigned) (ex << 23)
         | (unsigned) (fe_rand () & 0x7fffffu);
}

static void
fe_sets (u64 in[NSETS][10])
{
  for (int s = 0; s < NSETS; s++)
    {
      u64 *v = in[s];
      if (s < NEDGE)
        {
          /* Pairs of patterns: a cycles, b strides through the list.  */
          v[0] = fe_dpat[s % FE_ND];
          v[1] = fe_dpat[(s / FE_ND * 7 + s * 3 + 1) % FE_ND];
          v[2] = fe_dpat[(s * 5 + 2) % FE_ND];
          v[3] = fe_spat[s % FE_NS];
          v[4] = fe_spat[(s / FE_NS * 7 + s * 3 + 1) % FE_NS];
          v[5] = fe_spat[(s * 5 + 2) % FE_NS];
          v[6] = fe_ipat[s % FE_NI];
          v[7] = (unsigned) fe_ipat[(s * 3 + 1) % FE_NI];
        }
      else
        {
          int e = s % 3 == 0 ? 2 : s % 3 == 1 ? 30 : 300;
          v[0] = fe_rdouble (e);
          v[1] = fe_rdouble (e);
          v[2] = fe_rdouble (e);
          v[3] = fe_rsingle (e > 100 ? 60 : e);
          v[4] = fe_rsingle (e > 100 ? 60 : e);
          v[5] = fe_rsingle (e > 100 ? 60 : e);
          /* Integers with a random number of significant bits.  */
          v[6] = fe_rand () >> (fe_rand () % 64);
          if (fe_rand () & 1)
            v[6] = -v[6];
          v[7] = (unsigned) (fe_rand () >> (32 + fe_rand () % 32));
          if (fe_rand () & 1)
            v[7] = (unsigned) -(unsigned) v[7];
        }
      v[8] = v[3] | v[4] << 32;
      v[9] = v[5] | v[3] << 32;
    }
}

/* The control settings: all exceptions masked, then rounding (bits
   13-14), daz (bit 6), fz (bit 15).  */
static unsigned
fe_ctl (int k)
{
  return 0x1f80u | (unsigned) (k & 3) << 13 | (k & 4 ? 0x40u : 0)
         | (k & 8 ? 0x8000u : 0);
}
