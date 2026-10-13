// SPDX-License-Identifier: GPL-3.0-or-later
// Variadic functions that return a class through the hidden result
// pointer (spec/abi/calling-convention.md; docs/spec-poc-findings.md A24):
// which argument the caller stores to memory as "the last fixed" and
// which ones the callee reads from memory.
//
// Read as assembly:
//   L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/l++
//   $L -O3 -S -DCALLEE pocabi1_va.cc -o va-callee.s
//   $L -O3 -S -DCALLER pocabi1_va.cc -o va-caller.s
// Run (every pair of compilers, under qemu-e2k or on hardware):
//   $L -O3 -c -DCALLEE pocabi1_va.cc -o callee.o
//   $L -O3 -c -DCALLER pocabi1_va.cc -o caller.o
//   $L callee.o caller.o -o va && ./va     (prints "ok N" or "FAIL ...")
// With -DGARBAGE (both files) the caller fills its argument area with a
// pattern before each call, so a callee that reads a slot the caller
// didn't store reads the pattern.

#include <stdarg.h>
#include <stdio.h>

// Not trivially copyable: returned through the hidden pointer in slot 0.
struct S
{
  long v[3];
  S () { v[0] = v[1] = v[2] = 0; }
  S (const S &o) { v[0] = o.v[0]; v[1] = o.v[1]; v[2] = o.v[2]; }
  ~S () { v[0] = -1; }
};

// Trivially copyable control: returned in %dr0..%dr2, no hidden pointer.
struct T
{
  long v[3];
};

S fmt2 (long a, long b, ...);     // b longs follow; a is a tag
S fmt1 (int n, ...);              // n longs
S fmtd (int n, ...);              // n doubles
S fmt9 (long a1, long a2, long a3, long a4, long a5, long a6, long a7,
	long n, ...);             // last fixed in slot 8 (on the stack)
T tmt1 (int n, ...);              // control: no hidden pointer

void junk (long, ...);            // defined with the callees

struct X
{
  long k;
  S m (int n, ...);               // this, hidden pointer, n: who is first?
  S m1 (long a);                  // the same without varargs (read as
				  // assembly only: %r0 pointer, %r1 this)
};

#ifdef CALLEE
void junk (long, ...) {}

static long
sum_longs (va_list ap, int n)
{
  long s = 0;
  for (int i = 0; i < n; i++)
    s = s * 3 + va_arg (ap, long);
  return s;
}

S
fmt2 (long a, long b, ...)
{
  S r;
  va_list ap;
  va_start (ap, b);
  r.v[0] = sum_longs (ap, (int) b);
  va_end (ap);
  r.v[1] = a;
  r.v[2] = b;
  return r;
}

S
fmt1 (int n, ...)
{
  S r;
  va_list ap;
  va_start (ap, n);
  r.v[0] = sum_longs (ap, n);
  va_end (ap);
  r.v[1] = n;
  return r;
}

S
fmtd (int n, ...)
{
  S r;
  va_list ap;
  double s = 0;
  va_start (ap, n);
  for (int i = 0; i < n; i++)
    s = s * 3 + va_arg (ap, double);
  va_end (ap);
  r.v[0] = (long) s;
  r.v[1] = n;
  return r;
}

S
fmt9 (long a1, long a2, long a3, long a4, long a5, long a6, long a7,
      long n, ...)
{
  S r;
  va_list ap;
  va_start (ap, n);
  r.v[0] = sum_longs (ap, (int) n);
  va_end (ap);
  r.v[1] = a1 + a2 + a3 + a4 + a5 + a6 + a7;
  r.v[2] = n;
  return r;
}

T
tmt1 (int n, ...)
{
  T r = { { 0, 0, 0 } };
  va_list ap;
  va_start (ap, n);
  r.v[0] = sum_longs (ap, n);
  va_end (ap);
  r.v[1] = n;
  return r;
}

S
X::m1 (long a)
{
  S r;
  r.v[0] = a;
  r.v[1] = k;
  return r;
}

S
X::m (int n, ...)
{
  S r;
  va_list ap;
  va_start (ap, n);
  r.v[0] = sum_longs (ap, n);
  va_end (ap);
  r.v[1] = n;
  r.v[2] = k;
  return r;
}
#endif

#ifdef CALLER
static int fails, checks;

static void
check (const char *what, long got, long want)
{
  checks++;
  if (got != want)
    {
      fails++;
      printf ("FAIL %s: %ld, expected %ld\n", what, got, want);
    }
}

static long
expect (int n, long first)
{
  long s = 0;
  for (int i = 0; i < n; i++)
    s = s * 3 + (first + i);
  return s;
}

#ifdef GARBAGE
// Fill the caller's argument area with a pattern before each call: a
// variadic call stores every slot from its last fixed argument on, and
// the next call reuses the area, so a slot the next caller doesn't store
// holds the pattern, not a leftover of an earlier call.
#define J 0x5a5a5a5a00000000L
#define DIRTY() junk (J, J, J, J, J, J, J, J, J, J, J, J, J, J)
#else
#define DIRTY() (void) 0
#endif

int
main (void)
{
  DIRTY ();
  S r = fmt2 (77, 9, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 9L);
  check ("fmt2 sum", r.v[0], expect (9, 1));
  check ("fmt2 a", r.v[1], 77);
  check ("fmt2 b", r.v[2], 9);

  DIRTY ();
  r = fmt1 (9, 11L, 12L, 13L, 14L, 15L, 16L, 17L, 18L, 19L);
  check ("fmt1 sum", r.v[0], expect (9, 11));
  check ("fmt1 n", r.v[1], 9);

  DIRTY ();
  r = fmt1 (2, 11L, 12L);
  check ("fmt1 short sum", r.v[0], expect (2, 11));

  DIRTY ();
  r = fmtd (9, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0);
  check ("fmtd sum", r.v[0], expect (9, 1));
  check ("fmtd n", r.v[1], 9);

  DIRTY ();
  r = fmt9 (1, 2, 3, 4, 5, 6, 7, 3, 21L, 22L, 23L);
  check ("fmt9 sum", r.v[0], expect (3, 21));
  check ("fmt9 a", r.v[1], 28);
  check ("fmt9 n", r.v[2], 3);

  DIRTY ();
  T t = tmt1 (9, 31L, 32L, 33L, 34L, 35L, 36L, 37L, 38L, 39L);
  check ("tmt1 sum", t.v[0], expect (9, 31));
  check ("tmt1 n", t.v[1], 9);

  X x;
  x.k = 555;
  DIRTY ();
  r = x.m (9, 41L, 42L, 43L, 44L, 45L, 46L, 47L, 48L, 49L);
  check ("X::m sum", r.v[0], expect (9, 41));
  check ("X::m n", r.v[1], 9);
  check ("X::m k", r.v[2], 555);

#ifdef GARBAGE
  // Control, not counted: called through a type whose last fixed
  // argument is the first long, the caller doesn't store n's slot (slot
  // 1).  A callee that reads n from memory, as lcc's does, gets the
  // pattern's low word, 0; one that reads its register gets 2.
  DIRTY ();
  r = ((S (*) (int, long, ...)) fmt1) (2, 11L, 12L);
  printf ("control: fmt1 read n = %ld (2 from the register, 0 from memory)\n",
	  r.v[1]);
#endif
  printf ("%s %d checks\n", fails ? "FAIL" : "ok", checks);
  return fails != 0;
}
#endif
