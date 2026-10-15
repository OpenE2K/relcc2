/* SPDX-License-Identifier: GPL-3.0-or-later */
/* How MCST's gdb reads debug information on E2K (spec/abi/registers.md,
   "DWARF register numbers"; stack-and-unwinding.md, "Debug
   information"; elf.md, TLS).  The hand-written DWARF in debuginfo.s
   places variables in %r and %b[] registers, under setbn and abn, in
   an outer frame below and above the call's wbs, as 16-byte values
   (pairs, one extended register, a v5 128-bit register) and in TLS;
   this file adds an lcc -O0 function with stack arguments (frame base
   before and after getsp), lcc's own TLS variables in a second thread,
   and _Unwind_GetCFA against gdb's $sp and $fp.  debuginfo.gdb is the gdb
   script; debuginfo.sh builds, copies and runs everything:

     tools/abi/probes/debuginfo.sh claude@ringo.lan v3      # 4C
     tools/abi/probes/debuginfo.sh claude@geofront.lan v5.8c2

   or by hand:

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O0 -g -pthread tools/abi/probes/debuginfo.c \
         tools/abi/probes/debuginfo.s -o debuginfo
     (for v5 and later: -Wa,--defsym,V5=1 with the v5.8c2 lcc)
     gdb -batch -x debuginfo.gdb ./debuginfo     # on hardware; -x stops
                                                 # at the first error, so
                                                 # debuginfo.sh passes each
                                                 # line as -ex instead

   The program prints what it computed and, from _Unwind_Backtrace,
   "cfa N ip ... cfa ..." for the frames above cfa_leaf; gdb prints
   each variable at each stop (the expected values are in
   debuginfo.gdb's echo lines).  Results: debuginfo.sh's header.  */

#include <pthread.h>
#include <stdio.h>
#include <unwind.h>

extern long di_rot (long n);
extern long di_outer (void);
extern long di_wcall (void);
extern __thread long di_tls;

__thread int t = 5;
static __thread long st = 7;

void
di_tls_stop (void)
{
  __asm__ volatile ("" ::: "memory");
}

static void *
thr (void *a)
{
  (void) a;
  t = 42;
  st = 43;
  di_tls = 44;
  di_tls_stop ();
  return 0;
}

/* Arguments 9 and 10 are in the caller's stack area (slots 8 and 9);
   lcc -O0 describes them as DW_OP_fbreg with a frame base of DW_OP_reg8,
   which getsp's instruction sets.  */
long
f10 (long a1, long a2, long a3, long a4, long a5, long a6, long a7,
     long a8, long a9, long a10)
{
  long loc = a1 + a10;
  return loc * a9 + a2 + a3 + a4 + a5 + a6 + a7 + a8;
}

static _Unwind_Reason_Code
cfa_cb (struct _Unwind_Context *c, void *p)
{
  int *n = p;
  printf ("cfa %d ip %#lx cfa %#lx\n", *n, (unsigned long) _Unwind_GetIP (c),
          (unsigned long) _Unwind_GetCFA (c));
  return ++*n < 4 ? _URC_NO_REASON : _URC_END_OF_STACK;
}

void
di_cfa_stop (void)
{
  __asm__ volatile ("" ::: "memory");
}

__attribute__ ((noinline)) void
cfa_leaf (void)
{
  volatile char pad[64];
  int n = 0;
  pad[0] = 1;
  printf ("cfa_leaf local %p\n", (void *) pad);
  _Unwind_Backtrace (cfa_cb, &n);
  di_cfa_stop ();
}

__attribute__ ((noinline)) void
cfa_mid (void)
{
  volatile char pad[256];
  pad[0] = 1;
  printf ("cfa_mid local %p\n", (void *) pad);
  cfa_leaf ();
}

int
main (void)
{
  pthread_t th;

  printf ("di_rot %#lx\n", di_rot (3));
  printf ("di_outer %#lx\n", di_outer ());
  printf ("di_wcall %#lx\n", di_wcall ());
  printf ("f10 %ld\n", f10 (1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
  cfa_mid ();
  pthread_create (&th, 0, thr, 0);
  pthread_join (th, 0);
  di_tls_stop ();
  printf ("t %d st %ld di_tls %#lx\n", t, st, di_tls);
  return 0;
}
