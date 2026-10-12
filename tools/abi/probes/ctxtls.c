/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What a compiler keeps of thread-local storage across a call that may
   switch threads (spec/abi/kernel-interface.md, "Contexts and threads"):
   a context switched out in one thread can be resumed by another, whose
   %g13 (thread pointer) differs.  Each function reads a __thread
   variable, its address or errno before and after swapcontext; read the
   assembly.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O3 -S ctxtls.c -o ctxtls.s           # local exec
     $L -O3 -fPIC -S ctxtls.c -o ctxtls-pic.s  # general dynamic
     gcc -O2 [-fPIC] -S ctxtls.c               # any GCC, for comparison

   value: the second read of tv must load through the new thread
     pointer (or call __tls_get_addr again), not through the address
     computed before the call.
   addr, errno_addr: the XOR is folded to 0 when the compiler keeps the
     address.
   errno_val: __errno_location is declared const (glibc stdlib/errno.h),
     so a compiler may call it once.
   via_addr, via_val: the same through an out-of-line helper that returns
     the address or the value (libgo's runtime_g pattern); GCC finds an
     address-returning helper const unless it is noipa.  */

#include <errno.h>
#include <ucontext.h>

__thread long tv;
extern ucontext_t a, b;

#ifdef __LCC__
#define OUT_OF_LINE __attribute__ ((noinline))  /* lcc has no noipa */
#else
#define OUT_OF_LINE __attribute__ ((noipa))
#endif

static long *__attribute__ ((noinline)) tv_addr (void) { return &tv; }
static long *OUT_OF_LINE tv_addr_opaque (void) { return &tv; }
static long __attribute__ ((noinline)) tv_val (void) { return tv; }

long
value (void)
{
  long x = tv;
  swapcontext (&a, &b);
  long y = tv;
  return x * 1000 + y;
}

long
addr (void)
{
  long *p = &tv;
  swapcontext (&a, &b);
  long *q = &tv;
  return (long) p ^ (long) q;
}

int
errno_val (void)
{
  int e1 = errno;
  swapcontext (&a, &b);
  int e2 = errno;
  return e1 + e2;
}

long
errno_addr (void)
{
  int *p = &errno;
  swapcontext (&a, &b);
  int *q = &errno;
  return (long) p ^ (long) q;
}

long
via_addr (void)
{
  long *p = tv_addr ();
  swapcontext (&a, &b);
  long *q = tv_addr ();
  return (long) p ^ (long) q;
}

long
via_addr_opaque (void)
{
  long *p = tv_addr_opaque ();
  swapcontext (&a, &b);
  long *q = tv_addr_opaque ();
  return (long) p ^ (long) q;
}

long
via_val (void)
{
  long x = tv_val ();
  swapcontext (&a, &b);
  return x * 1000 + tv_val ();
}
