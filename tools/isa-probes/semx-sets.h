/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The input sets of semx.c, shared with check_semx.c.  */

static const unsigned long semx_pat[] = {
  0x0000000000000000UL, 0x0000000000000001UL, 0xffffffffffffffffUL,
  0x8000000000000000UL, 0x7fffffffffffffffUL, 0x0123456789abcdefUL,
  0xfedcba9876543210UL, 0x3ff0000000000000UL, 0xbff8000000000000UL,
  0x7ff0000000000000UL, 0x7ff8000000000000UL, 0x7ff4000000000000UL,
  0x0000000000000003UL, 0x400921fb54442d18UL,
  0x3f800000c0400000UL, /* floats 1.0, -3.0 */
  0x7f8000007fc00000UL, /* inf, qNaN */
  0x0000000580000000UL, /* 5, -0.0f */
  0x7fa0000000000001UL, /* sNaN, denormal float */
  0x8000800080008000UL, 0x7fff7fff7fff7fffUL, 0x80807f7f01ff00feUL,
  0x0f0e0d0c0b0a0908UL, 0x4142434445464748UL, 0x0000ffff0000ffffUL,
  0x0000000000000004UL, 0x000000000000003fUL, 0x0000000000000010UL,
  0x3c003c004000c000UL, 0x8000000080000000UL, 0x7fffffff80000001UL,
};
#define NPAT (sizeof semx_pat / sizeof semx_pat[0])
#define NSETS 40

/* Three operands per set: a pattern, then patterns chosen by an LCG
   (the third one XORed with the LCG's state from set 30 on).  */
static void
semx_sets (unsigned long in[NSETS][3])
{
  unsigned long x = 1;
  for (int s = 0; s < NSETS; s++)
    for (int k = 0; k < 3; k++)
      {
        x = x * 6364136223846793005UL + 1442695040888963407UL;
        in[s][k] = s < (int) NPAT && k == 0 ? semx_pat[s]
                   : semx_pat[(x >> 33) % NPAT] ^ (k == 2 && s >= 30 ? x : 0);
      }
}
