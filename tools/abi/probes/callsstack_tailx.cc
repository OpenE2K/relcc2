// SPDX-License-Identifier: GPL-3.0-or-later
// C++ exceptions through tail calls that gave their frame back before
// the jump (callsstack_tail.s; spec/abi/calling-convention.md, "Tail
// calls"), with lcc's runtime: MCST's libgcc unwinder, which walks the
// chain stack (spec/abi/stack-and-unwinding.md, "Exceptions").
//
//   throw   cs_chain_a/c (N, fin_throw, 0): N mutual tail calls, each
//           with a released 64-byte frame, end in fin_throw, which has
//           a local with a destructor and calls thrower (), which
//           throws the sum; main catches it.  Checks the value, that the
//           destructor ran once, and main's stack pointer afterwards.
//   direct  the same with fin_throw_here, which throws itself.
//   catch   fin_catch catches thrower ()'s exception itself (a landing
//           pad in a function entered by ct) and returns sum + 1.
//   args10  cs_rel_args10's callee g10c throws (stack arguments
//           rewritten in the caller's incoming area after the release).
//
//     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/l++   # or v6.2c3 ...
//     $L -O2 -fno-inline tools/abi/probes/callsstack_tailx.cc \
//        tools/abi/probes/callsstack_tail.s -o callsstack_tailx
//     ./callsstack_tailx
//
// Prints one line per check, "ok" or "BAD".

#include <cstdio>
#include <cstring>

struct big
{
  long v[12];
};

extern "C" {
long cs_sp (long);
long cs_chain_a (long, long, long), cs_chain_c (long, long, long);
long cs_rel_args10 (long, long, long, long, long, long, long, long, long,
                    long);
long g10c (long, long, long, long, long, long, long, long, long, long);
struct big gbig (long);
long fin_throw (long, long, long), fin_throw_here (long, long, long);
long fin_catch (long, long, long);
}

static int dtors, bad;
static bool throw10;

struct D
{
  ~D () { dtors++; }
};

__attribute__ ((noinline)) void
thrower (long v)
{
  if (v >= 0)
    throw v;
}

long
fin_throw (long acc, long, long)
{
  D d;
  thrower (acc);
  return -1;
}

long
fin_throw_here (long acc, long, long)
{
  D d;
  if (acc >= 0)
    throw acc;
  return -1;
}

long
fin_catch (long acc, long, long)
{
  D d;
  try
    {
      thrower (acc);
    }
  catch (long v)
    {
      return v + 1;
    }
  return -1;
}

long
g10c (long a, long b, long c, long d, long e, long f, long g, long h,
      long i, long j)
{
  long s = a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h
           + 9 * i + 10 * j;
  D x;
  if (throw10)
    throw s;
  return s;
}

struct big
gbig (long x)
{
  struct big r;
  for (int i = 0; i < 12; i++)
    r.v[i] = x + i;
  return r;
}

static void
check (const char *what, bool ok, long a, long b, long c)
{
  std::printf ("%-40s %s  value %ld (expected %ld), sp delta %ld\n", what,
               ok ? "ok " : "BAD", a, b, c);
  if (!ok)
    bad++;
}

int
main ()
{
  std::setvbuf (stdout, NULL, _IOLBF, 0);
  static const long ns[] = { 0, 1, 10, 10000, 1000000 };
  char line[96];
  for (int k = 0; k < 2; k++)
    for (long n : ns)
      {
        long (*ch) (long, long, long) = k ? cs_chain_c : cs_chain_a;
        long want = n * (n + 1) / 2;
        static const struct
        {
          const char *name;
          long (*f) (long, long, long);
        } fins[] = { { "throw", fin_throw }, { "direct", fin_throw_here } };
        for (const auto &fn : fins)
          {
            long got = -2;
            dtors = 0;
            long sp0 = cs_sp (0);
            try
              {
                ch (n, (long) fn.f, 0);
              }
            catch (long v)
              {
                got = v;
              }
            long sp1 = cs_sp (0);
            std::snprintf (line, sizeof line, "%s %s: %ld calls", fn.name,
                           k ? "c/d" : "a/b", n);
            check (line, got == want && dtors == 1 && sp0 == sp1, got, want,
                   sp1 - sp0);
          }
        dtors = 0;
        long sp0 = cs_sp (0);
        long got = ch (n, (long) fin_catch, 0);
        long sp1 = cs_sp (0);
        std::snprintf (line, sizeof line, "catch %s: %ld calls",
                       k ? "c/d" : "a/b", n);
        check (line, got == want + 1 && dtors == 1 && sp0 == sp1, got,
               want + 1, sp1 - sp0);
      }
  throw10 = false;
  long want = g10c (2, 3, 4, 5, 6, 7, 8, 9, 10, 1);
  throw10 = true;
  long got = -2;
  dtors = 0;
  long sp0 = cs_sp (0);
  try
    {
      cs_rel_args10 (1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    }
  catch (long v)
    {
      got = v;
    }
  long sp1 = cs_sp (0);
  check ("args10: g10c throws", got == want && dtors == 1 && sp0 == sp1, got,
         want, sp1 - sp0);
  std::printf ("%s\n", bad ? "SOME BAD" : "all ok");
  return 0;
}
