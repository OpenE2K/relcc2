/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Inline assembly operands and register variables (source-compat.md).
   Read the assembly; nothing is run.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O2 -S tools/abi/probes/asmops.c -o - | grep '# '
     $LCC -O2 -mno-old-asm-constraint-r -S tools/abi/probes/asmops.c -o - | grep '# '

   The first warns that "r" on long double is read as "x".  */

long double ld;
__int128 i;
long l;

void r_ld (void) { asm volatile ("# r-ld %0" : : "r" (ld)); }
void x_ld (void) { asm volatile ("# x-ld %0" : : "x" (ld)); }
void r_i128 (void) { asm volatile ("# r-i128 %0" : : "r" (i)); }
void r_long (void) { asm volatile ("# r-long %0" : : "r" (l)); }

void
reg_xg9 (void)
{
  register long double v asm ("%xg9") = ld;
  asm volatile ("# xg9 %0" : : "x" (v));
}
