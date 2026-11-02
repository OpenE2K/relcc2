/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Local register variables read outside asm, as MCST's LibreOffice UNO
   bridge does (OS Elbrus's 0003-bridges_port.patch,
   bridges/source/cpp_uno/gcc3_linux_e2k): cpp_vtable_call reads the
   method's index from `register unsigned long g16 asm ("g16")', which
   MCST's code snippet sets before it jumps (ct, not call) there;
   callVirtualMethod calls a method through a pointer to a void function
   and then reads the result from `register long bN asm ("b[N]")', N 0 to
   7, and puts stack arguments in an alloca block it expects at the
   call's slot 8.  Both bridge functions are __attribute__ ((optimize
   (0))).  spec/abi/source-compat.md, "Register variables";
   docs/spec-poc-findings.md A53.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc    (or v5.8c2, ...)
     $L -O0 folda53_regvar.c -o a53-O0          # also -O3, -O3 -DOPT0
     ./a53-O0                                   # on hardware (4C, 8C2)
     $L -O3 -S folda53_regvar.c -o -            # read the reads
     P=.../gcc-e2k-0.6+33-8c2/bin/e2k-linux-gnu-gcc      # the PoC
     $P -O2 -S folda53_regvar.c -o -

   Each case prints ok or BAD with what it read.  The helpers are
   assembly at file scope (below): fa53_set_g16 writes %g16 and returns,
   fa53_get_g17 returns %g17, and fa53_snippet (a, v, fn) sets %g16 to v
   and jumps to fn (a) as the bridge's snippet does.

     entry      g16_at_entry: %g16 read at entry (cpp_vtable_call)
     calls      g16_after_calls: %g16 read after each of two calls that
                set it to different values
     write      g17_write: a plain C store to a %g17 variable, then a
                call to fa53_get_g17
     results    b_after_call: %b[0]-%b[7] read after a call through a
                pointer to a void function (callVirtualMethod), the
                callee returning a 64-byte struct
     asm-out    g16_asm_out: GCC's spelling of the entry read, an empty
                asm with the variable as its output
     struct     b_struct_call: GCC's spelling of the results, a call
                through a pointer to a function returning the struct
     stack8     stack_args: alloca (2 * 8) filled, then an 8-slot call
                to a 10-parameter callee, which reads its slots 8 and 9
     stack8+10  stack_args_big: the same in a function that also has a
                10-slot call (not taken)

   2026-11-03, lcc 1.29.16 (v3 on the 4C, v5.8c2 on the 8C2; -O0, -O3,
   -O3 -DOPT0) and 1.31.05 (v3 -O0 and -O3 on the 4C, v5.8c2 -O3 on
   the 8C2): every case ok but stack8+10.  -S shows the register read
   where the variable is: `addd,0,sm 0x0, %g16, %r3' at -O0 and
   `addd,3 %g16, %r0, %r0' at -O3 at entry, %g16 again after each call
   (-O3: shld of %g16 after the first, addd with %g16 after the
   second), `addd,0 0x0, %r0, %g17' kept up to the call at -O3, and
   eight std of %b[0]-%b[7] after the call.  lcc reserves one argument
   area for the largest call (getsp -64; -80 with the 10-slot call) and
   puts the alloca block right above it, so the block is the 8-slot
   call's slot 8 only without the bigger call (stack8+10 reads two
   other words).
   The proof-of-concept GCC 0.6+33 on the 8C2: entry, calls and results
   read 0 at -O0 and -O2 (it zeroes registers read before they are
   written: `addd 0x0, 0, %g16' at entry, `addd 0x0, 0, %b[N]' after
   the call), -O2 reads %g16 once for both calls (`muld %g16, 1001'),
   drops the %g17 store at -O2 (write BAD there), and at -O2 deletes the
   alloca block and its memcpy as dead stores (stack8 BAD; ok at -O0
   and with -DOPT0, the bridge's optimize (0)).  asm-out and struct are
   ok everywhere, with either compiler.  */

#include <stdio.h>
#include <string.h>

#ifdef OPT0
# define O0 __attribute__ ((optimize (0)))
#else
# define O0
#endif
#define NOINLINE __attribute__ ((noinline))

extern void fa53_set_g16 (unsigned long);
extern unsigned long fa53_get_g17 (void);
extern long fa53_snippet (long a, unsigned long g16, void *fn);

O0 NOINLINE long g16_at_entry (long a)
{
  register unsigned long g16 asm ("g16");
  unsigned long v = g16;
  return v + a;
}

O0 NOINLINE long g16_asm_out (long a)
{
  register unsigned long g16 asm ("g16");
  asm volatile ("" : "=r" (g16));
  return g16 + a;
}

O0 NOINLINE unsigned long g16_after_calls (unsigned long x, unsigned long y)
{
  register unsigned long g16 asm ("g16");
  fa53_set_g16 (x);
  unsigned long a = g16;
  fa53_set_g16 (y);
  unsigned long b = g16;
  return a * 1000 + b;
}

O0 NOINLINE unsigned long g17_write (unsigned long x)
{
  register unsigned long g17 asm ("g17") = x;
  return fa53_get_g17 ();
}

struct ret8 { long r[8]; };

NOINLINE struct ret8 eight (long k)
{
  struct ret8 s;
  for (int i = 0; i < 8; i++)
    s.r[i] = k * 10 + i;
  return s;
}

O0 NOINLINE void b_after_call (void *fn, long k, long *out)
{
  ((void (*) (long)) fn) (k);
  {
    register long b0 asm ("b[0]"); out[0] = b0;
    register long b1 asm ("b[1]"); out[1] = b1;
    register long b2 asm ("b[2]"); out[2] = b2;
    register long b3 asm ("b[3]"); out[3] = b3;
    register long b4 asm ("b[4]"); out[4] = b4;
    register long b5 asm ("b[5]"); out[5] = b5;
    register long b6 asm ("b[6]"); out[6] = b6;
    register long b7 asm ("b[7]"); out[7] = b7;
  }
}

O0 NOINLINE void b_struct_call (void *fn, long k, long *out)
{
  struct ret8 s = ((struct ret8 (*) (long)) fn) (k);
  memcpy (out, s.r, sizeof s.r);
}

NOINLINE long ten (long a0, long a1, long a2, long a3, long a4, long a5,
                   long a6, long a7, long a8, long a9)
{
  return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 * 1000 + a9 * 100000;
}

typedef long (*call8) (long, long, long, long, long, long, long, long);
typedef long (*call10) (long, long, long, long, long, long, long, long,
                        long, long);

O0 NOINLINE long stack_args (void *fn, const long *vals, int n)
{
  long *stack = __builtin_alloca (n * 8);
  memcpy (stack, vals, n * 8);
  return ((call8) fn) (0, 1, 2, 3, 4, 5, 6, 7);
}

O0 NOINLINE long stack_args_big (void *fn, const long *vals, int n)
{
  if (n < 0)
    return ((call10) fn) (0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  long *stack = __builtin_alloca (n * 8);
  memcpy (stack, vals, n * 8);
  return ((call8) fn) (0, 1, 2, 3, 4, 5, 6, 7);
}

__asm__ (
"	.text\n"
"	.global	fa53_set_g16\n"
"	.type	fa53_set_g16, #function\n"
"	.align	8\n"
"fa53_set_g16:\n"
"	{\n"
"	  setwd	wsz = 0x4, nfx = 0x1\n"
"	  return	%ctpr3\n"
"	  addd,0	%dr0, 0x0, %dg16\n"
"	}\n"
"	{\n"
"	  ct	%ctpr3\n"
"	}\n"
"	.size	fa53_set_g16, .-fa53_set_g16\n"
"	.global	fa53_get_g17\n"
"	.type	fa53_get_g17, #function\n"
"	.align	8\n"
"fa53_get_g17:\n"
"	{\n"
"	  setwd	wsz = 0x4, nfx = 0x1\n"
"	  return	%ctpr3\n"
"	  addd,0	%dg17, 0x0, %dr0\n"
"	}\n"
"	{\n"
"	  ct	%ctpr3\n"
"	}\n"
"	.size	fa53_get_g17, .-fa53_get_g17\n"
/* As the bridge's snippet: a window, %g16, then a jump to fn, which
   returns to fa53_snippet's caller.  */
"	.global	fa53_snippet\n"
"	.type	fa53_snippet, #function\n"
"	.align	8\n"
"fa53_snippet:\n"
"	{\n"
"	  setwd	wsz = 0x4, nfx = 0x1\n"
"	  movtd,0	%dr2, %ctpr1\n"
"	  addd,1	%dr1, 0x0, %dg16\n"
"	}\n"
"	{\n"
"	  ct	%ctpr1\n"
"	}\n"
"	.size	fa53_snippet, .-fa53_snippet\n");

static int bad;

static void report (const char *name, int ok, unsigned long got,
                    unsigned long want)
{
  printf ("%-10s %s (read %#lx, want %#lx)\n", name, ok ? "ok" : "BAD",
          got, want);
  bad |= !ok;
}

int main (void)
{
  volatile unsigned long v = 0x1234567890abcdefUL;
  void *volatile fn8 = (void *) eight;
  void *volatile fn10 = (void *) ten;
  long r, out[8];
  int i, ok;

  r = fa53_snippet (5, v, (void *) g16_at_entry);
  report ("entry", r == (long) (v + 5), r - 5, v);

  r = g16_after_calls (11, 22);
  report ("calls", r == 11022, r, 11022);

  r = g17_write (v);
  report ("write", r == (long) v, r, v);

  memset (out, 0, sizeof out);
  b_after_call (fn8, 4, out);
  for (ok = 1, i = 0; i < 8; i++)
    ok &= out[i] == 40 + i;
  report ("results", ok, out[7], 47);

  r = fa53_snippet (5, v, (void *) g16_asm_out);
  report ("asm-out", r == (long) (v + 5), r - 5, v);

  memset (out, 0, sizeof out);
  b_struct_call (fn8, 4, out);
  for (ok = 1, i = 0; i < 8; i++)
    ok &= out[i] == 40 + i;
  report ("struct", ok, out[7], 47);

  static const long vals[2] = { 3, 4 };
  r = stack_args (fn10, vals, 2);
  report ("stack8", r == 28 + 3000 + 400000, r, 28 + 3000 + 400000);
  r = stack_args_big (fn10, vals, 2);
  report ("stack8+10", r == 28 + 3000 + 400000, r, 28 + 3000 + 400000);
  return bad;
}
