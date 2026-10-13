/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which functions after one with nfx = 0 lcc also gives nfx = 0
   (spec/abi/registers.md, "nfx"; docs/spec-poc-findings.md A29, which
   read "every later function"; pocabi2_nfx.c has the long double cases).
   Compile to assembly and read each function's setwd:

     for t in v3 v5.8c2 v6.2c3; do
       /opt/mcst/lcc-1.29.16.e2k-$t.linux-6.1/bin/lcc -O3 -S \
         tools/abi/probes/pocabi1_nfx.c -o - |
       grep -E '^[a-z0-9_]+:|setwd'
     done

   (v3 has no 128-bit registers: build with -DNO_QP.)  lcc 1.29.16 -O1 to
   -O3 and 1.31.05 -O3, v5.8c2 and v6.2c3: lf 1; g 0; c1, c2 0 (they
   inherit it); a1 1, and the functions after it 1 until g2; g2 0, m2 0
   (inherits), m3 1.  The long double half, also on v3: ld 0, ld_c 0,
   ld_pass 0, ld2 0, ld_a 1, ld_pass2 1, ld3 0, ld_m 0, ld_pass3 0, ld4
   0, ld_cp 1, ld_pass4 1.  So a function that returns a constant or one
   loaded value takes nfx = 0 from the function before it, and passes it
   on; one that computes (an adds, an addd of two loads) or stores (a
   copy through memory) gets nfx = 1 and stops it.  */

int lf (void) { return 1; }
#ifndef NO_QP
__int128 g (__int128 *p) { return __atomic_load_n (p, 0); }
#endif
int c1 (void) { return 2; }
int c2 (void) { return 3; }
int a1 (int x) { return x + 3; }
int c3 (void) { return 4; }
long m (long *p) { return *p; }
int c4 (void) { return 5; }
extern int ext (int);
int k (int x) { return ext (x) + 1; }
int c5 (void) { return 6; }
#ifndef NO_QP
__int128 g2 (__int128 *p) { return __atomic_load_n (p, 0); }
#endif
long m2 (long *p) { return *p; }
long m3 (long *p, long *q) { return *p + *q; }

/* The same with an 80-bit value (also v3).  */
long double ld (long double a, long double b) { return a * b + a; }
int ld_c (void) { return 2; }
long double ld_pass (long double x, long double y) { (void) x; return y; }
long double ld2 (long double a, long double b) { return a * b + a; }
int ld_a (int x) { return x + 3; }
long double ld_pass2 (long double x, long double y) { (void) x; return y; }
long double ld3 (long double a, long double b) { return a * b + a; }
long ld_m (long *p) { return *p; }
long double ld_pass3 (long double x, long double y) { (void) x; return y; }
long double ld4 (long double a, long double b) { return a * b + a; }
void ld_cp (long *a, long *b) { *a = *b; }
long double ld_pass4 (long double x, long double y) { (void) x; return y; }
