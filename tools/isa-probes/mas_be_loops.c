/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Loops with byte swaps as lcc compiles them: through __builtin_bswap64
   (ldd + pshufb, std after pshufb) or through its MAS bit 3 builtins
   (ldd/std mas=0x8); timed by mas_be.c.  Compiled separately at -O3.  */

#include <e2kintrin.h>

unsigned long
sum_shuf (const unsigned long *a, long n)
{
  unsigned long s = 0;
  for (long i = 0; i < n; i++)
    s += __builtin_bswap64 (a[i]);
  return s;
}

unsigned long
sum_be (const unsigned long *a, long n)
{
  unsigned long s = 0;
  for (long i = 0; i < n; i++)
    s += __builtin_e2k_ld_64s_be (a + i);
  return s;
}

void
copy_shuf (unsigned long *restrict d, const unsigned long *restrict a, long n)
{
  for (long i = 0; i < n; i++)
    d[i] = __builtin_bswap64 (a[i]);
}

void
copy_be_st (unsigned long *restrict d, const unsigned long *restrict a, long n)
{
  for (long i = 0; i < n; i++)
    __builtin_e2k_st_64s_be (a[i], d + i);
}

void
copy_be_ld (unsigned long *restrict d, const unsigned long *restrict a, long n)
{
  for (long i = 0; i < n; i++)
    d[i] = __builtin_e2k_ld_64s_be (a + i);
}
