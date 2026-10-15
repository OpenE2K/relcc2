/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Three gdb questions left over from debuginfo.c: does MCST's gdb read a
   TLS variable in a program that creates no threads (dynamic and
   static); does "next" over a call that ends in longjmp stop where
   setjmp returns (gdb's longjmp breakpoint reads the jmp_buf through
   e2k_get_longjmp_target); and how does it show lcc's own 16-byte
   parameters (fq: x 1.25, f 1.5, q 7)?

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O0 -g tools/abi/probes/debuginfo_misc.c -o misc
     $LCC -O0 -g -static tools/abi/probes/debuginfo_misc.c -o misc-static
     gdb -batch -ex 'break misc_stop' -ex 'break fq' -ex run -ex 'p tv' \
         -ex 'p &tv' -ex continue -ex 'info args' \
         -ex 'break debuginfo_misc.c:JUMP_LINE' -ex continue \
         -ex next -ex 'p $pc' -ex 'info line *$pc' -ex next ./misc

   (debuginfo.sh runs it with JUMP_LINE filled in.)  The program prints
   "after 6 2" when longjmp has returned to main.  The call's return
   address is in the "x = 1" code, which never runs, so gdb's
   step-resume breakpoint there can't stop "next" by chance.  Results
   (4C, 8C2 and 2C3, MCST's gdb 15.2): see debuginfo.sh's header.  */

#include <setjmp.h>
#include <stdio.h>

__thread int tv = 5;
static jmp_buf jb;

void
misc_stop (void)
{
  __asm__ volatile ("" ::: "memory");
}

/* lcc -O0 keeps a __float128 or __int128 parameter in its pair of
   argument registers (DW_OP_reg2, DW_OP_reg4 here) and a long double in
   memory (DW_OP_fbreg).  */
__attribute__ ((noinline)) long double
fq (long double x, __float128 f, __int128 q)
{
  return x + (long double) f + (long double) q;
}

__attribute__ ((noinline)) void
jump (int v)
{
  longjmp (jb, v);
}

int
main (void)
{
  int x;

  tv = 6;
  misc_stop ();
  printf ("fq %Lg\n", fq (1.25L, (__float128) 1.5, 7));
  if (setjmp (jb) == 0)
    {
      jump (1); /* JUMP_LINE */
      x = 1;
    }
  else
    x = 2;
  printf ("after %d %d\n", tv, x);
  return 0;
}
