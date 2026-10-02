/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which functions lcc protects with -fstack-protector*, and how (elf.md,
   "Stack protector"). Read the assembly; nothing is run.

     LCC=toolchains/lcc-1.31.05-e2k-v3/lcc     # 1.31 implements it
     for o in -fstack-protector -fstack-protector-strong \
              -fstack-protector-all; do
       $LCC -O2 $o -S tools/abi/probes/stackprot.c -o - | grep -c stack_chk
     done

   lcc 1.31.05: -fstack-protector checks every function, no_array too;
   -strong and -all check none. lcc 1.29 emits no checks at all.  */

void use (char *);
void usei (int *);

int big_char (int n) { char b[64]; use (b); return b[n]; }
int small_char (int n) { char b[4]; use (b); return b[n]; }
int int_array (int n) { int v[32]; usei (v); return v[n]; }
int no_array (int n) { int x = n * 3; return x; }
