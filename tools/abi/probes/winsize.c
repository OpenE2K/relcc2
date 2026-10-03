/* SPDX-License-Identifier: GPL-3.0-or-later */
/* How large must a callee's register window be? (registers.md, "Window
   size limits").

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/abi/probes/winsize.c tools/abi/probes/winsize.s -o winsize
     ./winsize                     # on hardware (MCST qemu aborts on the trap)

   Elbrus 4C: setwd traps (SIGILL) when the callee's wsz is smaller than
   the part of the caller's window it inherits (caller wsz - wbs).  */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
static sigjmp_buf env; static volatile int sig;
static void h(int s){ sig = s; siglongjmp(env, 1); }
long w_4_1(void);
long w_4_2(void);
long w_4_3(void);
long w_4_4(void);
long w_4_5(void);
long w_6_1(void);
long w_6_2(void);
long w_6_3(void);
long w_6_4(void);
long w_6_5(void);
long w_7_1(void);
long w_7_2(void);
long w_7_3(void);
long w_7_4(void);
long w_7_5(void);
int main(void){ signal(SIGILL, h); setvbuf(stdout,0,_IOLBF,0);
  sig=0; if (sigsetjmp(env,1)==0) { w_4_1(); printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 1: ok\n"); } else printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 1: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_4_2(); printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 2: ok\n"); } else printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 2: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_4_3(); printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 3: ok\n"); } else printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 3: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_4_4(); printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 4: ok\n"); } else printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 4: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_4_5(); printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 5: ok\n"); } else printf("caller wsz 8, wbs 4 (inherits 4), callee wsz 5: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_6_1(); printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 1: ok\n"); } else printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 1: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_6_2(); printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 2: ok\n"); } else printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 2: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_6_3(); printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 3: ok\n"); } else printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 3: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_6_4(); printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 4: ok\n"); } else printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 4: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_6_5(); printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 5: ok\n"); } else printf("caller wsz 8, wbs 6 (inherits 2), callee wsz 5: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_7_1(); printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 1: ok\n"); } else printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 1: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_7_2(); printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 2: ok\n"); } else printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 2: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_7_3(); printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 3: ok\n"); } else printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 3: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_7_4(); printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 4: ok\n"); } else printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 4: signal %d\n", sig);
  sig=0; if (sigsetjmp(env,1)==0) { w_7_5(); printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 5: ok\n"); } else printf("caller wsz 8, wbs 7 (inherits 1), callee wsz 5: signal %d\n", sig);
  return 0; }
