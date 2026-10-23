/* SPDX-License-Identifier: GPL-3.0-or-later */
/* __attribute__ ((aligned (64))) on a function parameter, as MCST's
   kernel writes it on the pointer parameters of xor2_128x16_m12,
   xor3_128x16_m10, xor4_128x16_m8 and xor5_128x16_m6
   (arch/e2k/include/asm/xor.h:688-1193, under __iset__ >= 5; spec/abi/
   kernel-build.md, "What a GCC build meets first";
   docs/spec-poc-findings.md A36).  The attribute aligns the parameter
   itself (the pointer variable), not what it points to.  Read with -S
   and compare, nothing runs:

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   (or e2k-v5.8c2)
     $L -O3 -Wall -Wextra -S folda36_parmalign.c -o with.s
     $L -O3 -Wall -Wextra -S -DATTR= folda36_parmalign.c -o without.s
     diff with.s without.s

   lcc 1.29.16 (e2k-v3 and e2k-v5.8c2, -O2 and -O3) and 1.31.05
   (e2k-v3), 2026-10-24: no diagnostic, and the two files are identical
   (local_* and typedef_* use ALIGN64, not ATTR, so they don't change).
   lcc ignores the attribute on a parameter: taken's slot is
   not realigned (getsp -16), parm_alignof returns 8, low still masks.
   On a local (local_taken, local_alignof) lcc honours it: the frame is
   realigned (getsp of a computed amount) and __alignof__ says 64.
   Through a typedef (typedef_*) __alignof__ says 64 but the slot is
   still not realigned.
   GCC refuses ATTR on a parameter: "alignment may not be specified for
   'p'" (c-family/c-attribs.cc, common_handle_aligned_attribute; host
   GCC 16.2.1, C and C++).  Clang (host 22.1.8, and Unipro's 20.1.8 with
   --target=e2k-linux-gnu -march=elbrus-v3) honours it: taken's slot is
   realigned to 64 and parm_alignof returns 64.  */

#ifndef ATTR
#define ATTR __attribute__ ((aligned (64)))
#endif
#define ALIGN64 __attribute__ ((aligned (64)))

void sink (void *);

/* The parameter's address escapes: is its slot realigned?  */
void taken (unsigned long *p ATTR) { sink (&p); }

/* What __alignof__ says of the parameter.  */
int parm_alignof (unsigned long *p ATTR) { return __alignof__ (p); }

/* The pointer's low bits: the attribute says nothing about them, so
   this must stay a mask.  */
unsigned long low (unsigned long *p ATTR) { return (unsigned long) p & 63; }

/* A copy loop through two such pointers, as in xor.h.  */
void copy (unsigned long *__restrict d ATTR,
	   const unsigned long *__restrict s ATTR)
{
  for (int i = 0; i < 64; i++)
    d[i] = s[i];
}

/* For contrast: the attribute on a local and through a typedef.  */
void local_taken (void) { unsigned long *q ALIGN64; sink (&q); }
int local_alignof (void) { unsigned long *q ALIGN64; return __alignof__ (q); }
typedef unsigned long *aligned_ptr ALIGN64;
int typedef_alignof (aligned_ptr p) { return __alignof__ (p); }
void typedef_taken (aligned_ptr p) { sink (&p); }
