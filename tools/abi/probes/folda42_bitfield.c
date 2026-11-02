/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * folda42_bitfield.c: the type a bit-field of type unsigned long long
 * has in an expression, by its width (spec/abi/source-compat.md;
 * data-layout.md, "Bit-fields").  C11 6.7.2.1p10 makes
 * a bit-field's type the declared type "consisting of the specified
 * number of bits"; GCC's C front end gives a field of 33-63 bits a type of
 * exactly that width, so arithmetic wraps there, while lcc (EDG), clang
 * and C++ compute in the declared type.  Fields of up to 32 bits behave
 * alike (int, or unsigned int at 32).  MCST's kernel relies on lcc's rule:
 * get_cr0_ip is cr0.Ip << 3 with a 45-bit Ip (the proof-of-concept back
 * end's finding A42).
 *
 * Runs anywhere; the values are the same on every target:
 *
 *   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O2 \
 *       tools/abi/probes/folda42_bitfield.c -o folda42_bitfield
 *   ./folda42_bitfield                       # on hardware (or -O0, l++
 *                                            # with -x c++)
 *   gcc -O2 tools/abi/probes/folda42_bitfield.c && ./a.out    # host
 *   clang -O2 ...; g++ -x c++ -O2 ...
 *
 * One line per expression: its value as unsigned long long and the size
 * of its type.  The fields are volatile, so nothing is folded at compile
 * time.
 *
 * 2026-11-03: lcc 1.29.16 (e2k-v3 at -O0 and -O2, l++ -x c++, on the 4C;
 * e2k-v5.8c2 -O2 on the 8C2) and 1.31.05 (e2k-v3 -O2), host clang 22 and
 * g++ 16.2.1 give cr0.ip << 3 e20000080830, ~cr0.ip ffffe3bffffefef9,
 * cr0.ip + cr0.ip 38800002020c, the compare 1, cr0.ip * 16
 * 1c40000101060, edge.c + 1 200000000, edge.c << 1 3fffffffe, edge.d + 1
 * 8000000000000000, edge.d << 1 fffffffffffffffe, sl.h << 12
 * 7ffffffff000.  Host GCC 16.2.1 C (-O0, -O2) and the proof-of-concept
 * E2K back end (-O2, on the 4C) give 20000080830, 3bffffefef9,
 * 18800002020c, 0, 40000101060, 0, 1fffffffe, 0, 7ffffffffffffffe and
 * fffffffffffff000.  All agree on (u64) cr0.ip << 3, the 20- and 32-bit
 * fields (int and unsigned int: ffffffffffff0000, fffffff0, 0) and the
 * 64-bit one; sizeof is 8 for every 33-63-bit expression in both.
 */

#include <stdio.h>

typedef unsigned long long u64;

static volatile struct
{
  u64 : 3;
  u64 ip : 45;
} cr0;
static volatile struct { u64 a : 20; u64 b : 32; } nar;
static volatile struct { u64 c : 33; u64 : 0; u64 d : 63; } edge;
static volatile struct { u64 full : 64; } fl;
/* A signed field, as e2k-iommu.c's `long h_page_table : 36`: under GCC's
   rule h << 12 overflows the 36-bit type (undefined).  */
static volatile struct { long h : 36; } sl;

#define P(e) printf ("%-34s %016llx  size %zu\n", #e, (u64) (e), sizeof (e))

int
main (void)
{
  cr0.ip = 0xe20000080830ULL >> 3;
  nar.a = 0xfffff;
  nar.b = 0xffffffff;
  edge.c = 0x1ffffffffULL;
  edge.d = 0x7fffffffffffffffULL;
  fl.full = ~0ULL;
  sl.h = 0x7ffffffffL;
  P (cr0.ip << 3);
  P (~cr0.ip);
  P (cr0.ip + cr0.ip);
  P ((cr0.ip << 3) > 0x100000000000ULL);
  P (cr0.ip * 16);
  P ((u64) cr0.ip << 3);
  P (nar.a << 16);
  P (nar.b << 4);
  P (nar.b + 1);
  P (edge.c + 1);
  P (edge.c << 1);
  P (edge.d + 1);
  P (edge.d << 1);
  P (fl.full + 1);
  P (sl.h << 12);
  return 0;
}
