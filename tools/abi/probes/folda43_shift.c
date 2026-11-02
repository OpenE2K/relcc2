/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * folda43_shift.c: what a C shift by a count of the operand's width or
 * more gives with lcc and with GCC (spec/abi/source-compat.md;
 * spec/isa/semantics.md, "shift counts are modulo the width").  C makes
 * such a shift undefined; the hardware takes the count modulo 64 (shld,
 * shrd, sard) or 32 (shls, ...).  MCST glibc's strcspn, strpbrk and strspn
 * (sysdeps/e2k/str{cspn,pbrk,spn}.c, the path for a set longer than 8
 * bytes) build a 256-bit table with `1LL << spec` for bytes spec up to 255
 * and look bytes up the same way; with GCC every byte from 0x40 up is
 * missed (the proof-of-concept back end's finding A43).  inset () below is
 * that shape, written anew.
 *
 * Build and run on hardware (also -O0, -O3; e2k-v5.8c2 on the 8C2):
 *
 *   L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
 *   $L -O2 tools/abi/probes/folda43_shift.c -o folda43_shift && ./folda43_shift
 *   $L -O2 -S tools/abi/probes/folda43_shift.c -o -     # the shifts as written
 *   e2k-linux-gnu-gcc -O2 ... (a GCC port), gcc -O2 ... (host GCC)
 *
 * One line per call: the expression and its value.
 *
 * 2026-11-03, the 4C (e2k-v3 builds) and the 8C2 (e2k-v5.8c2): lcc
 * 1.29.16 at -O0, -O2 and -O3 and 1.31.05 at -O3 give bit_if_high 2 and
 * 20, shl64 1 and 2, shr64 1, sar64 fffffffffffffffe, shl32 2 (the
 * hardware's count modulo 64 or 32) and inset 1 1 1 1 0 0; shl_const64
 * 0 (lcc folds the literal 1ULL << 64 to 0, warning #63, while a count
 * that reaches a shift through inlining folds modulo 64: lcc -O3 turns
 * an inline v << n with n = 64, 65 into 1, 2, and n = 70 into scld 1, 6).
 * The proof-of-concept back end at -O2 and host GCC 16.2.1 -O2 (x86-64):
 * bit_if_high 0 and 0, inset 1 0 0 0 0 0 (only bytes below 0x40 found),
 * the rest as lcc; the back end at -O0 as lcc.
 */

#include <stdio.h>

typedef unsigned long long u64;

/* The count reaches the shift only when it is 128 or more.  */
__attribute__ ((noinline)) u64
bit_if_high (unsigned x)
{
  if (x & 0x80)
    return 1ULL << x;
  return 0;
}

/* Counts the compiler can't see.  */
__attribute__ ((noinline)) u64 shl64 (u64 v, unsigned n) { return v << n; }
__attribute__ ((noinline)) u64 shr64 (u64 v, unsigned n) { return v >> n; }
__attribute__ ((noinline)) long long sar64 (long long v, unsigned n) { return v >> n; }
__attribute__ ((noinline)) unsigned shl32 (unsigned v, unsigned n) { return v << n; }

/* A constant count of 64 (both compilers warn).  */
__attribute__ ((noinline)) u64
shl_const64 (void)
{
  return 1ULL << 64;
}

/* glibc's table: bit (c mod 64) of word c / 64, the count a 64-bit
   variable, the word picked by a byte offset.  */
__attribute__ ((noinline)) int
inset (const unsigned char *set, int len, unsigned char c)
{
  u64 table[4], t0 = 0, t1 = 0, t2 = 0, t3 = 0, spec, val;
  for (int i = 0; i < len; i++)
    {
      spec = set[i];
      val = 1LL << spec;
      if (spec & 0x80)
        {
          if (spec & 0x40) t3 |= val;
          else t2 |= val;
        }
      else
        {
          if (spec & 0x40) t1 |= val;
          else t0 |= val;
        }
    }
  table[0] = t0;
  table[1] = t1;
  table[2] = t2;
  table[3] = t3;
  spec = c;
  return (*(u64 *) ((char *) table + ((spec >> 3) & 0x18))
          & (1LL << spec)) != 0;
}

int
main (void)
{
  static const unsigned char set[] = "!abcdefghi\x81\xc5";
  printf ("bit_if_high (0x81)   %llx\n", bit_if_high (0x81));
  printf ("bit_if_high (0xc5)   %llx\n", bit_if_high (0xc5));
  printf ("shl64 (1, 64)        %llx\n", shl64 (1, 64));
  printf ("shl64 (1, 65)        %llx\n", shl64 (1, 65));
  printf ("shr64 (1<<63, 127)   %llx\n", shr64 (1ULL << 63, 127));
  printf ("sar64 (-8, 66)       %llx\n", (u64) sar64 (-8, 66));
  printf ("shl32 (1, 33)        %x\n", shl32 (1, 33));
  printf ("shl_const64 ()       %llx\n", shl_const64 ());
  int n = sizeof set - 1;
  printf ("inset '!' 'c' 0x81 0xc5 'z' 0x82   %d %d %d %d %d %d\n",
          inset (set, n, '!'), inset (set, n, 'c'), inset (set, n, 0x81),
          inset (set, n, 0xc5), inset (set, n, 'z'), inset (set, n, 0x82));
  return 0;
}
