// SPDX-License-Identifier: GPL-3.0-or-later
// The C++ ABI beyond calls, across compilers (spec/abi/calling-
// convention.md, "C++ ABI beyond calls"; memorycxx_abi.cc reads the
// same things with -S): member-function and data-member pointers made
// by one compiler and used by the other, a guarded static of an inline
// function initialised once for both, new[] by one and delete[] by the
// other (array cookies, also over-aligned), __float128, long double and
// __int128 values, typeid of E2K's types, a thread_local with a dynamic
// initialiser read through the other side's wrapper, and virtual calls
// through a second base (thunks).  Build each side with its compiler,
// link with l++ and run:
//
//   L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/l++
//   G='<GCC> -O2'                        # any C++ compiler for E2K
//   $L -O2 -DSIDE=1 -c tools/abi/probes/memorycxx_abi_x.cc -o s1.o
//   $G -DSIDE=2 -c tools/abi/probes/memorycxx_abi_x.cc -o s2.o
//   $L s1.o s2.o -o x && ./x             # or qemu-e2k -L <lcc>/fs ./x
//
// main is in side 1.  Each line names a check, the side that made the
// value and the side that used it, and "ok" or what differed; the last
// line counts the failures.  Swap the compilers between the sides for
// the other direction.
//
// Results of 2026-10-16 (l++ 1.29.16 v3 and the proof-of-concept GCC
// snap-v04f, every pairing, linked by l++; MCST's qemu, the 4C, the
// 8C2 and the 2C3): 0 failures.  But with l++'s object first and
// GCC's second the link fails, "multiple definition of `guard
// variable for cx_counter()::c'": l++ puts the guard in its
// variable's COMDAT group (_ZZ10cx_countervE1c, at offset 8), GCC in a
// group of its own (_ZGVZ10cx_countervE1c), and both are
// STB_GNU_UNIQUE.  GCC's object first links; so does either order with
// GCC's side built with -fno-gnu-unique (its guard and variable are
// then weak).  l++ 1.31.05 groups the guard as 1.29.16 does.

#include <stdio.h>
#include <string.h>
#include <new>
#include <typeinfo>

#if SIDE == 1
#define P(n) s1_##n
#define Q(n) s2_##n
#define ME 1
#else
#define P(n) s2_##n
#define Q(n) s1_##n
#define ME 2
#endif

// Shared definitions (identical on both sides).
struct A { long a; virtual ~A () {} virtual long fa (long x) { return x + 1000; } };
struct B { long b; virtual long v1 (long x) { return x + 1; }
           virtual long v2 (long x) { return x + 2; }
           long nv (long x) { return x + 3 + b; } long x; };
struct D : A, B { long d; long v2 (long x) override { return x + 20 + d; }
                  long dn (long x) { return x + 30 + d; } };
typedef long (B::*pmf_t) (long);
typedef long (D::*pmfd_t) (long);
typedef long B::*pdm_t;

extern long cx_inits, cx_dtors;          // defined in side 1
long cx_init (void);
inline long &cx_counter (void) { static long c = cx_init (); return c; }

struct C { long v; ~C () { cx_dtors++; } };
struct alignas (32) C32 { long v; ~C32 () { cx_dtors++; } };

extern thread_local long cx_tl;          // defined in side 1

typedef double v2df __attribute__ ((vector_size (16)));

// What each side exports, under its prefix.
pmf_t s1_get_pmf (int), s2_get_pmf (int);
pmfd_t s1_get_pmfd (int), s2_get_pmfd (int);
pdm_t s1_get_pdm (int), s2_get_pdm (int);
long s1_call_pmf (B *, pmf_t, long), s2_call_pmf (B *, pmf_t, long);
long s1_call_pmfd (D *, pmfd_t, long), s2_call_pmfd (D *, pmfd_t, long);
long s1_read_pdm (B *, pdm_t), s2_read_pdm (B *, pdm_t);
int s1_pmf_eq (pmf_t, pmf_t), s2_pmf_eq (pmf_t, pmf_t);
long s1_counter (void), s2_counter (void);
C *s1_new (long), *s2_new (long);
void s1_del (C *), s2_del (C *);
C32 *s1_new32 (long), *s2_new32 (long);
void s1_del32 (C32 *), s2_del32 (C32 *);
__float128 s1_f128 (long, __float128, long double, __int128, long double *),
  s2_f128 (long, __float128, long double, __int128, long double *);
const std::type_info &s1_ti (int), &s2_ti (int);
long s1_tl (void), s2_tl (void);
B *s1_make_d (void), *s2_make_d (void);
long s1_vcall (B *, long), s2_vcall (B *, long);
v2df s1_vec (v2df, long, v2df), s2_vec (v2df, long, v2df);
int s1_check (void), s2_check (void);

pmf_t
P (get_pmf) (int w)
{
  switch (w)
    {
    case 0: return &B::nv;
    case 1: return &B::v1;
    case 2: return &B::v2;
    default: return nullptr;
    }
}

pmfd_t
P (get_pmfd) (int w)
{
  switch (w)
    {
    case 0: return &D::dn;
    case 1: return &D::v1;     // B's, through the second base
    case 2: return &D::v2;     // D's own slot
    case 3: return &D::fa;     // A's, the primary base
    case 4: return &D::nv;     // B's non-virtual, adjusted
    default: return nullptr;
    }
}

pdm_t
P (get_pdm) (int w)
{
  return w == 0 ? &B::x : w == 1 ? &B::b : nullptr;
}

long P (call_pmf) (B *o, pmf_t f, long a) { return (o->*f) (a); }
long P (call_pmfd) (D *o, pmfd_t f, long a) { return (o->*f) (a); }
long P (read_pdm) (B *o, pdm_t m) { return m ? o->*m : -77; }
int P (pmf_eq) (pmf_t f, pmf_t g) { return f == g; }
long P (counter) (void) { return ++cx_counter (); }
C *P (new) (long n) { C *p = new C[n]; for (long i = 0; i < n; i++)
                        p[i].v = i; return p; }
void P (del) (C *p) { delete[] p; }
C32 *P (new32) (long n) { C32 *p = new C32[n]; for (long i = 0; i < n; i++)
                            p[i].v = i; return p; }
void P (del32) (C32 *p) { delete[] p; }

// Returns the __float128 argument's bits, stores the long double and
// checks the __int128 in a slot after them.
__float128
P (f128) (long k, __float128 q, long double e, __int128 i, long double *out)
{
  *out = e;
  if (i != ((__int128) k << 64 | 0x1234))
    return 0;
  return q;
}

const std::type_info &
P (ti) (int w)
{
  switch (w)
    {
    case 0: return typeid (__float128);
    case 1: return typeid (long double);
    case 2: return typeid (__int128);
    case 3: return typeid (unsigned __int128);
    case 4: return typeid (v2df);
    case 5: return typeid (pmf_t);
    default: return typeid (D);
    }
}

long P (tl) (void) { return cx_tl; }
B *P (make_d) (void) { D *d = new D; d->a = 1; d->b = 2; d->x = 3;
                       d->d = 4; return d; }
long P (vcall) (B *o, long a) { return o->v2 (a) * 1000 + o->v1 (a); }
v2df P (vec) (v2df a, long k, v2df b) { return a * (double) k + b; }

#if SIDE == 1
long cx_inits, cx_dtors;
long cx_init (void) { return ++cx_inits * 100; }
static long tl_inits;
static long tl_init (void) { return ++tl_inits + 500; }
thread_local long cx_tl = tl_init ();
#endif

static int fails;
static void
report (const char *what, int from, int to, int ok, const char *how)
{
  printf ("%-28s made by %d, used by %d: %s%s\n", what, from, to,
          ok ? "ok" : "WRONG ", ok ? "" : how);
  fails += !ok;
}

// Use the other side's values here and this side's there.
int
P (check) (void)
{
  int other = 3 - ME;
  char buf[160];
  D d;
  d.a = 1, d.b = 2, d.x = 3, d.d = 4;
  B *bp = &d;
  // Member-function pointers to B, made there, called here and there.
  static const long want_b[3] = { 10 + 3 + 2, 10 + 1, 10 + 20 + 4 };
  for (int w = 0; w < 3; w++)
    {
      pmf_t f = Q (get_pmf) (w);
      long r1 = P (call_pmf) (bp, f, 10), r2 = Q (call_pmf) (bp, f, 10);
      unsigned long words[2];
      memcpy (words, &f, sizeof words);
      snprintf (buf, sizeof buf, "got %ld, %ld; words %#lx %#lx", r1, r2,
                words[0], words[1]);
      char what[40];
      snprintf (what, sizeof what, "pmf B #%d", w);
      report (what, other, ME, r1 == want_b[w] && r2 == want_b[w]
              && P (pmf_eq) (f, P (get_pmf) (w))
              && Q (pmf_eq) (f, P (get_pmf) (w)), buf);
    }
  report ("pmf null", other, ME, Q (get_pmf) (9) == nullptr
          && !P (pmf_eq) (Q (get_pmf) (9), P (get_pmf) (0)), "");
  // Member-function pointers to D (this adjustments).
  static const long want_d[5] = { 10 + 30 + 4, 10 + 1, 10 + 20 + 4,
                                  10 + 1000, 10 + 3 + 2 };
  for (int w = 0; w < 5; w++)
    {
      pmfd_t f = Q (get_pmfd) (w);
      long r1 = P (call_pmfd) (&d, f, 10), r2 = Q (call_pmfd) (&d, f, 10);
      unsigned long words[2];
      memcpy (words, &f, sizeof words);
      snprintf (buf, sizeof buf, "got %ld, %ld; words %#lx %#lx", r1, r2,
                words[0], words[1]);
      char what[40];
      snprintf (what, sizeof what, "pmf D #%d", w);
      report (what, other, ME, r1 == want_d[w] && r2 == want_d[w], buf);
    }
  // Data-member pointers, null included (-1).
  for (int w = 0; w < 3; w++)
    {
      pdm_t m = Q (get_pdm) (w);
      long want = w == 0 ? 3 : w == 1 ? 2 : -77;
      long r = P (read_pdm) (bp, m);
      long raw;
      memcpy (&raw, &m, sizeof raw);
      snprintf (buf, sizeof buf, "got %ld, raw %ld", r, raw);
      char what[40];
      snprintf (what, sizeof what, "pdm #%d", w);
      report (what, other, ME, r == want, buf);
    }
  // The guarded static: both sides share one counter and one init.
  long before = cx_inits;
  long c1 = P (counter) (), c2 = Q (counter) (), c3 = P (counter) ();
  snprintf (buf, sizeof buf, "counter %ld %ld %ld, inits %ld -> %ld", c1, c2,
            c3, before, cx_inits);
  report ("guarded static", ME, other, c2 == c1 + 1 && c3 == c2 + 1
          && cx_inits == 1, buf);
  // new[] there, delete[] here, and the reverse; the destructors count.
  for (long n = 1; n <= 5; n += 4)
    {
      long d0 = cx_dtors;
      C *p = Q (new) (n);
      int vals = 1;
      for (long i = 0; i < n; i++)
        vals &= p[i].v == i;
      P (del) (p);
      long d1 = cx_dtors;
      C32 *q = Q (new32) (n);
      int al = ((unsigned long) q & 31) == 0;
      for (long i = 0; i < n; i++)
        vals &= q[i].v == i;
      P (del32) (q);
      snprintf (buf, sizeof buf, "dtors %ld and %ld of %ld, aligned %d",
                d1 - d0, cx_dtors - d1, n, al);
      report (n == 1 ? "new[]/delete[] n=1" : "new[]/delete[] n=5", other,
              ME, vals && al && d1 - d0 == n && cx_dtors - d1 == n, buf);
    }
  // __float128, long double, __int128 through the other side.
  {
    __float128 q = 1;
    q /= 3;
    long double e = 2.0L / 3, eo = 0;
    __float128 r = Q (f128) (7, q, e, (__int128) 7 << 64 | 0x1234, &eo);
    report ("__float128/long double", ME, other,
            !memcmp (&r, &q, 16) && !memcmp (&eo, &e, 10), "");
  }
  // typeid of the same types on both sides.
  for (int w = 0; w < 7; w++)
    {
      const std::type_info &a = P (ti) (w), &b = Q (ti) (w);
      snprintf (buf, sizeof buf, "%s %s, same object %d", a.name (),
                b.name (), &a == &b);
      char what[40];
      snprintf (what, sizeof what, "typeid #%d", w);
      report (what, other, ME, a == b && !strcmp (a.name (), b.name ()),
              buf);
    }
  // A thread_local with a dynamic initialiser, through each side.
  {
    long a = Q (tl) (), b = P (tl) ();
    snprintf (buf, sizeof buf, "read %ld and %ld", a, b);
    report ("thread_local init", 1, other, a == 501 && b == 501, buf);
  }
  // A D made there, called here through B (the thunk to D::v2).
  {
    B *o = Q (make_d) ();
    long r1 = P (vcall) (o, 10), r2 = Q (vcall) (o, 10);
    long want = (10 + 20 + 4) * 1000 + 11;
    snprintf (buf, sizeof buf, "got %ld, %ld", r1, r2);
    report ("virtual through base 2", other, ME, r1 == want && r2 == want,
            buf);
    delete static_cast<D *> (o);
  }
  // A 16-byte vector argument and result.
  {
    v2df a = { 1.5, -2.0 }, b = { 0.25, 8.0 };
    v2df r = Q (vec) (a, 3, b);
    report ("vector_size(16) double", ME, other,
            r[0] == 4.75 && r[1] == 2.0, "");
  }
  return fails;
}

#if SIDE == 1
int
main (void)
{
  int f = s1_check ();
  f += s2_check ();
  printf ("%d failures\n", f);
  return f != 0;
}
#endif
