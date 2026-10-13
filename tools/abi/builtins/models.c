/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Reference models of lcc's builtins and <e2kintrin.h> helpers that are
   not one machine operation (spec/abi/lcc-builtins.md), checked against
   the compiler's own on random inputs; plus what the CPU-detection
   builtins report on this machine.  Prints "FAIL ..." for a mismatch,
   "info ..." lines that differ between machines, and a summary.

     lcc -O2 models.c -o models && ./models     (on hardware)

   A compiler that lacks some of these (a port adds them one at a
   time) still builds this file: each check is compiled only if the
   compiler has one builtin or helper of each family the check tests,
   and otherwise prints "missing CHECK".  A builtin counts if
   __has_builtin knows it or a macro of its name exists (lcc's
   <e2kintrin.h> defines some builtins as macros).

   The 128-bit parts need __iset__ >= 5.  Nothing here writes to memory
   through a MAS other than byte swapping and cache hints, and nothing
   locks: __builtin_e2k_clflush (a MAS 0xf store) and lock loads are
   not run.  */

#include <stdio.h>
#include <string.h>
#if defined __has_include
# if __has_include (<e2kintrin.h>)
#  include <e2kintrin.h>
#  define HDR 1
# else
#  define HDR 0
# endif
#else
# include <e2kintrin.h>
# define HDR 1
#endif
#ifndef __has_builtin
# define __has_builtin(x) 1
#endif

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef long long v2di __attribute__ ((vector_size (16)));

static int fails, checks, missed;

static void __attribute__ ((unused))
missing (const char *what)
{
  printf ("missing %s\n", what);
  missed++;
}

#define CHECK(name, cond, ...)                                          \
  do {                                                                  \
    checks++;                                                           \
    if (!(cond) && fails++ < 60)                                        \
      {                                                                 \
        printf ("FAIL %s: ", name);                                     \
        printf (__VA_ARGS__);                                           \
        printf ("\n");                                                  \
      }                                                                 \
  } while (0)

static u64 seed = 0x2545f4914f6cdd1dULL;

static u64
rnd (void)
{
  seed ^= seed << 13;
  seed ^= seed >> 7;
  seed ^= seed << 17;
  return seed;
}

/* Byte, halfword and word K of X, and the inverse.  */
static u64 lane (u64 x, int size, int k)
{
  return size == 8 ? x : (x >> (8 * size * k)) & ((1ULL << (8 * size)) - 1);
}

static u64 setlane (u64 x, int size, int k, u64 v)
{
  if (size == 8)
    return v;
  u64 m = ((1ULL << (8 * size)) - 1) << (8 * size * k);
  return (x & ~m) | ((v << (8 * size * k)) & m);
}

static u64 bswap64 (u64 x) { return __builtin_bswap64 (x); }
static u32 bswap32 (u32 x) { return __builtin_bswap32 (x); }

/* punpck{l,h}: interleave the low (high) halves' lanes, B's at the even
   positions (semantics.md).  */
static u64
m_punpck (u64 a, u64 b, int size, int high)
{
  int n = 8 / size, base = high ? n / 2 : 0;
  u64 r = 0;
  for (int k = 0; k < n / 2; k++)
    {
      r = setlane (r, size, 2 * k, lane (b, size, base + k));
      r = setlane (r, size, 2 * k + 1, lane (a, size, base + k));
    }
  return r;
}

static void
check_punpck (void)
{
#if (defined __builtin_e2k_punpcklbh || __has_builtin (__builtin_e2k_punpcklbh))
  for (int i = 0; i < 200; i++)
    {
      u64 a = rnd (), b = rnd ();
#define P(name, size, high)                                             \
      CHECK (#name, __builtin_e2k_##name (a, b) == m_punpck (a, b, size, high), \
             "%016llx %016llx -> %016llx", a, b, __builtin_e2k_##name (a, b));
      P (punpcklbh, 1, 0) P (punpckhbh, 1, 1) P (punpcklhw, 2, 0)
      P (punpckhhw, 2, 1) P (punpcklwd, 4, 0) P (punpckhwd, 4, 1)
#undef P
    }
#else
  missing ("check_punpck");
#endif
}

/* pshufh: halfword K of the result is halfword (imm >> 2K) & 3 of A, as
   x86 pshufw; the immediate counts modulo 256.  */
static u64
m_pshufh (u64 a, int imm)
{
  u64 r = 0;
  for (int k = 0; k < 4; k++)
    r = setlane (r, 2, k, lane (a, 2, (imm >> (2 * k)) & 3));
  return r;
}

static void
check_pshufh (void)
{
#if (defined __builtin_e2k_pshufh || __has_builtin (__builtin_e2k_pshufh))
  for (int i = 0; i < 50; i++)
    {
      u64 a = rnd ();
#define S(imm) CHECK ("pshufh", __builtin_e2k_pshufh (a, imm) == m_pshufh (a, imm & 255), \
                      "%016llx, %d", a, imm);
      S (0) S (1) S (0x1b) S (0x4e) S (0xe4) S (0xff) S (0x93) S (0x100) S (0x1e4)
#undef S
    }
#else
  missing ("check_pshufh");
#endif
}

static void
check_align64 (void)
{
#if defined E2K_ALIGN_DATA
  for (int i = 0; i < 400; i++)
    {
      u64 s1 = rnd (), s2 = rnd ();
      int al = i & 7;
      u64 spec, d, want;
      /* E2K_ALIGN_DATA: bytes al..al+7 of s2:s1 (s1 the low 8 bytes).  */
      E2K_PREPARE_ALIGN (al, spec);
      E2K_ALIGN_DATA (s1, s2, d, spec);
      want = al ? (s1 >> (8 * al)) | (s2 << (64 - 8 * al)) : s1;
      CHECK ("E2K_ALIGN_DATA", d == want, "al %d %016llx %016llx -> %016llx",
             al, s1, s2, d);
      /* _BE: big-endian data, s1 the earlier 8 bytes.  */
      E2K_PREPARE_ALIGN_BE (al, spec);
      E2K_ALIGN_DATA_BE (s1, s2, d, spec);
      want = al ? (s1 << (8 * al)) | (s2 >> (64 - 8 * al)) : s1;
      CHECK ("E2K_ALIGN_DATA_BE", d == want, "al %d %016llx %016llx -> %016llx",
             al, s1, s2, d);
      /* _ARITH: the preparation turns a byte offset into bits.  */
      u64 a = al;
      E2K_PREPARE_ALIGN_ARITH ((void *) 0, a);
      CHECK ("E2K_PREPARE_ALIGN_ARITH", a == 8ULL * al, "%d -> %llu", al, a);
      E2K_ALIGN_DATA_ARITH (s1, s2, d, a);
      want = al ? (s1 >> (8 * al)) | (s2 << (64 - 8 * al)) : s1;
      CHECK ("E2K_ALIGN_DATA_ARITH", d == want, "al %d -> %016llx", al, d);
      a = al;
      E2K_PREPARE_ALIGN_ARITH_BE ((void *) 0, a);
      CHECK ("E2K_PREPARE_ALIGN_ARITH_BE", a == 8ULL * al, "%d -> %llu", al, a);
      E2K_ALIGN_DATA_ARITH_BE (s1, s2, d, a);
      want = al ? (s1 << (8 * al)) | (s2 >> (64 - 8 * al)) : s1;
      CHECK ("E2K_ALIGN_DATA_ARITH_BE", d == want, "al %d -> %016llx", al, d);
      /* 32-bit forms: byte offsets 0..3 of 4-byte words.  */
      u64 w1 = s1 & 0xffffffff, w2 = s2 & 0xffffffff;
      int al4 = i & 3;
      a = al4;
      E2K_PREPARE_ALIGN_ARITH32 ((void *) 0, a);
      CHECK ("E2K_PREPARE_ALIGN_ARITH32", a == 8ULL * al4, "%d -> %llu", al4, a);
      E2K_ALIGN_DATA_ARITH32 (w1, w2, d, a);
      want = al4 ? (u32) ((w1 >> (8 * al4)) | (w2 << (32 - 8 * al4))) : w1;
      CHECK ("E2K_ALIGN_DATA_ARITH32", (u32) d == want,
             "al %d %08llx %08llx -> %016llx", al4, w1, w2, d);
      a = al4;
      E2K_PREPARE_ALIGN_INSF32 ((void *) 0, a);
      CHECK ("E2K_PREPARE_ALIGN_INSF32", a == 512ULL * al4, "%d -> %llu", al4, a);
      E2K_ALIGN_DATA_INSF32 (w1, w2, d, a);
      CHECK ("E2K_ALIGN_DATA_INSF32", (u32) d == want,
             "al %d %08llx %08llx -> %016llx", al4, w1, w2, d);
    }
#else
  missing ("check_align64");
#endif
}

static void
check_ptr (void)
{
#if defined E2K_ALIGN_PTR
  for (int i = 0; i < 200; i++)
    {
      char *p = (char *) (rnd () & 0xffffffffffffULL);
      u64 x = (u64) p;
#define A(al)                                                           \
      CHECK ("E2K_ALIGN_PTR", (u64) E2K_ALIGN_PTR (p, al) == ((x + al - 1) & -(u64) al), "%llx", x); \
      CHECK ("E2K_ALIGN_PTR_BACK", (u64) E2K_ALIGN_PTR_BACK (p, al) == (x & -(u64) al), "%llx", x); \
      CHECK ("E2K_BYTES_TO_ALIGN", (u64) E2K_BYTES_TO_ALIGN (p, al) == (-x & (al - 1)), "%llx", x); \
      CHECK ("E2K_BYTES_FROM_ALIGN", (u64) E2K_BYTES_FROM_ALIGN (p, al) == (x & (al - 1)), "%llx", x);
      A (4) A (8) A (16) A (64) A (4096)
#undef A
    }
#else
  missing ("check_ptr");
#endif
}

static int clz64 (u64 x) { int n = 0; while (n < 64 && !(x >> (63 - n) & 1)) n++; return n; }
static int ctz64 (u64 x) { int n = 0; while (n < 64 && !(x >> n & 1)) n++; return n; }

static void
check_clzb (void)
{
#if defined __CLZB && defined __CTZB
  for (int i = 0; i < 300; i++)
    {
      u64 x = rnd () >> (i % 65 == 64 ? 63 : i % 64);
      x = i % 7 == 0 ? x << (i % 64) : x;
      if (i == 0)
        x = 0;
      int n, m __attribute__ ((unused));
      __CLZB (n, x);
      CHECK ("__CLZB", n == clz64 (x) / 8, "%016llx -> %d", x, n);
      __CTZB (n, x);
      CHECK ("__CTZB", n == ctz64 (x) / 8, "%016llx -> %d", x, n);
      /* The E2K_ names are not in lcc 1.29's header.  */
#if defined E2K_CLZB && defined E2K_CTZB
      E2K_CLZB (m, x);
      __CLZB (n, x);
      CHECK ("E2K_CLZB", m == n, "%016llx -> %d", x, m);
      E2K_CTZB (m, x);
      __CTZB (n, x);
      CHECK ("E2K_CTZB", m == n, "%016llx -> %d", x, m);
#endif
    }
#if !(defined E2K_CLZB && defined E2K_CTZB)
  missing ("check_clzb E2K_CLZB, E2K_CTZB");
#endif
#else
  missing ("check_clzb");
#endif
}

/* Memory helpers on a buffer.  */
static union { u64 d[64]; u8 b[512]; v2di q[32]; } buf __attribute__ ((aligned (64)));

static void
fill (void)
{
  for (int i = 0; i < 64; i++)
    buf.d[i] = rnd ();
}

static void
check_mem64 (void)
{
#if HDR \
    && (defined __builtin_e2k_ld_64s_be || __has_builtin (__builtin_e2k_ld_64s_be)) \
    && __has_builtin (__builtin_loadmas_64u)
  for (int i = 0; i < 100; i++)
    {
      fill ();
      int k = i % 32;
      u64 *p = &buf.d[k];
      u32 *w = (u32 *) p;
      u64 v = *p;
      CHECK ("ld_32u_be", __builtin_e2k_ld_32u_be (w) == bswap32 (*w), "");
      CHECK ("ld_32u_t1", __builtin_e2k_ld_32u_t1 (w) == *w, "");
      CHECK ("ld_32u_t2", __builtin_e2k_ld_32u_t2 (w) == *w, "");
      CHECK ("ld_32u_nta", __builtin_e2k_ld_32u_nta (w) == *w, "");
      CHECK ("ld_64s_be", (u64) __builtin_e2k_ld_64s_be (p) == bswap64 (v), "");
      CHECK ("ld_64s_t1", (u64) __builtin_e2k_ld_64s_t1 (p) == v, "");
      CHECK ("ld_64s_t2", (u64) __builtin_e2k_ld_64s_t2 (p) == v, "");
      CHECK ("ld_64s_nta", (u64) __builtin_e2k_ld_64s_nta (p) == v, "");
      /* _sm and _cleartag take a byte offset.  */
      CHECK ("ld_64s_sm", (u64) __builtin_e2k_ld_64s_sm (buf.d, 8 * k) == v, "");
      CHECK ("ld_64s_cleartag", (u64) __builtin_e2k_ld_64s_cleartag (buf.d, 8 * k) == v, "");
      u64 x = rnd ();
      u32 y = (u32) rnd ();
      __builtin_e2k_st_64s_be (x, p);
      CHECK ("st_64s_be", *p == bswap64 (x), "");
      __builtin_e2k_st_64s_nt (x, p);
      CHECK ("st_64s_nt", *p == x, "");
      __builtin_e2k_st_32u_be (y, w);
      CHECK ("st_32u_be", *w == bswap32 (y), "");
      __builtin_e2k_st_32u_nt (y, w);
      CHECK ("st_32u_nt", *w == y, "");
      __builtin_e2k_prefetch (p, E2K_HINT_T0);
      __builtin_e2k_prefetch (p, E2K_HINT_NTA);
      CHECK ("puttagd", __builtin_e2k_puttagd (x, 0) == x, "");
      /* loadmas/storemas with the flags that don't lock.  */
      CHECK ("loadmas_64u", __builtin_loadmas_64u (p, __LCC_MAS_NO, __LCC_CHAN_ANY) == *p, "");
      CHECK ("loadmas_64u be", __builtin_loadmas_64u (p, 0x8, 0) == bswap64 (*p), "");
      CHECK ("loadmas_64u spec", __builtin_loadmas_64u (p, __LCC_MAS_SPEC, 2) == *p, "");
      CHECK ("loadmas_64u volatile", __builtin_loadmas_64u (p, __LCC_MAS_VOLATILE, 5) == *p, "");
      CHECK ("loadmas_64u spec cleartag",
             __builtin_loadmas_64u (p, __LCC_MAS_SPEC | __LCC_MAS_CLEARTAG, 3) == *p, "");
      CHECK ("loadmas_32u be", __builtin_loadmas_32u (w, 0x8, 3) == bswap32 (*w), "");
      CHECK ("loadmas_16u be", __builtin_loadmas_16u (w, 0x8, 3) == (u16) (bswap32 (*w) >> 16), "");
      CHECK ("loadmas_8u", __builtin_loadmas_8u (w, 0x20, 0) == (u8) *w, "");
      __builtin_storemas_64u (x, p, 0x8, __LCC_CHAN_ANY);
      CHECK ("storemas_64u be", *p == bswap64 (x), "");
      __builtin_storemas_32u (y, w, 0x8, 2);
      CHECK ("storemas_32u be", *w == bswap32 (y), "");
      __builtin_storemas_16u ((u16) y, w, 0x8, 5);
      CHECK ("storemas_16u be", *(u16 *) w == (u16) (((y & 0xff) << 8) | ((y >> 8) & 0xff)), "");
      __builtin_storemas_8u ((u8) y, w, __LCC_MAS_SPEC, 2);
      CHECK ("storemas_8u spec", *(u8 *) w == (u8) y, "");
    }
#else
  missing ("check_mem64");
#endif
}

#if __iset__ >= 5
static u64 q0 (v2di x) { return (u64) x[0]; }
static u64 q1 (v2di x) { return (u64) x[1]; }
static int qeq (v2di a, v2di b) { return q0 (a) == q0 (b) && q1 (a) == q1 (b); }
static v2di qmk (u64 lo, u64 hi) { v2di r = { (long long) lo, (long long) hi }; return r; }
static v2di qbswap (v2di x) { return qmk (bswap64 (q1 (x)), bswap64 (q0 (x))); }

static u64
qlane (v2di x, int size, int k)
{
  int per = 8 / size;
  return lane (k < per ? q0 (x) : q1 (x), size, k % per);
}

static v2di
qsetlane (v2di x, int size, int k, u64 v)
{
  int per = 8 / size;
  return k < per ? qmk (setlane (q0 (x), size, k, v), q1 (x))
                 : qmk (q0 (x), setlane (q1 (x), size, k - per, v));
}

static v2di
m_qpunpck (v2di a, v2di b, int size, int high)
{
  int n = 16 / size, base = high ? n / 2 : 0;
  v2di r = qmk (0, 0);
  for (int k = 0; k < n / 2; k++)
    {
      r = qsetlane (r, size, 2 * k, qlane (b, size, base + k));
      r = qsetlane (r, size, 2 * k + 1, qlane (a, size, base + k));
    }
  return r;
}

/* Byte I of the 32-byte s2:s1 (s1 the low 16).  */
static u8
qbyte2 (v2di s1, v2di s2, int i)
{
  return i < 16 ? qlane (s1, 1, i) : qlane (s2, 1, i - 16);
}

static void
check_q (void)
{
#if (defined __builtin_e2k_qppackdl || __has_builtin (__builtin_e2k_qppackdl)) \
    && defined E2K_ALIGN_DATA128
  for (int i = 0; i < 200; i++)
    {
      v2di a = qmk (rnd (), rnd ()), b = qmk (rnd (), rnd ());
#define P(name, size, high)                                             \
      CHECK (#name, qeq (__builtin_e2k_##name (a, b), m_qpunpck (a, b, size, high)), "");
      P (qpunpcklbh, 1, 0) P (qpunpckhbh, 1, 1) P (qpunpcklhw, 2, 0)
      P (qpunpckhhw, 2, 1) P (qpunpcklwd, 4, 0) P (qpunpckhwd, 4, 1)
      P (qpunpckldq, 8, 0) P (qpunpckhdq, 8, 1)
#undef P
      u64 hi = rnd (), lo = rnd ();
      CHECK ("qppackdl", qeq (__builtin_e2k_qppackdl (hi, lo), qmk (lo, hi)), "");
      CHECK ("qpswitchd", qeq (__builtin_e2k_qpswitchd (a), qmk (q1 (a), q0 (a))), "");
      CHECK ("puttagqp", qeq (__builtin_e2k_puttagqp (a, 0), a), "");
      /* 128-bit alignment: bytes al..al+15 of b:a (a the low 16).  */
      int al = i & 15;
      v2di spec, d, want = a;
      E2K_PREPARE_ALIGN128 (al, spec);
      E2K_ALIGN_DATA128 (a, b, d, spec);
      for (int k = 0; k < 16; k++)
        want = qsetlane (want, 1, k, qbyte2 (a, b, al + k));
      CHECK ("E2K_ALIGN_DATA128", qeq (d, want), "al %d", al);
      E2K_PREPARE_ALIGN128_BE (al, spec);
      E2K_ALIGN_DATA128_BE (a, b, d, spec);
      for (int k = 0; k < 16; k++)
        want = qsetlane (want, 1, k, k >= al ? qlane (a, 1, k - al)
                                              : qlane (b, 1, 16 + k - al));
      CHECK ("E2K_ALIGN_DATA128_BE", qeq (d, want), "al %d", al);
#if defined E2K_TRANSPOSE2_64F && defined E2K_TRANSPOSE4_32F
      /* Transposes: rows in, columns out (not in lcc 1.29's header).  */
      v2di t[4], r[4];
      for (int k = 0; k < 4; k++)
        r[k] = qmk (rnd (), rnd ());
      E2K_TRANSPOSE2_64F (r[0], r[1], t[0], t[1]);
      CHECK ("E2K_TRANSPOSE2_64F", qeq (t[0], qmk (q0 (r[0]), q0 (r[1])))
             && qeq (t[1], qmk (q1 (r[0]), q1 (r[1]))), "");
      E2K_TRANSPOSE4_32F (r[0], r[1], r[2], r[3], t[0], t[1], t[2], t[3]);
      int ok = 1;
      for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
          ok &= qlane (t[col], 4, row) == qlane (r[row], 4, col);
      CHECK ("E2K_TRANSPOSE4_32F", ok, "");
#endif
    }
#if !(defined E2K_TRANSPOSE2_64F && defined E2K_TRANSPOSE4_32F)
  missing ("check_q transposes");
#endif
#else
  missing ("check_q");
#endif
}

static void
check_mem128 (void)
{
#if (defined __builtin_e2k_ld_128_be || __has_builtin (__builtin_e2k_ld_128_be)) \
    && __has_builtin (__builtin_loadmas_128v)
  for (int i = 0; i < 100; i++)
    {
      fill ();
      int k = i % 16;
      v2di *p = &buf.q[k], x = *p;
      CHECK ("ld_128_be", qeq (__builtin_e2k_ld_128_be (p), qbswap (x)),
             "%016llx %016llx -> %016llx %016llx", q0 (x), q1 (x),
             q0 (__builtin_e2k_ld_128_be (p)), q1 (__builtin_e2k_ld_128_be (p)));
      CHECK ("ld_128_t1", qeq (__builtin_e2k_ld_128_t1 (p), x), "");
      CHECK ("ld_128_t2", qeq (__builtin_e2k_ld_128_t2 (p), x), "");
      CHECK ("ld_128_nta", qeq (__builtin_e2k_ld_128_nta (p), x), "");
      CHECK ("ld_128_sm", qeq (__builtin_e2k_ld_128_sm (buf.q, 16 * k), x), "");
      CHECK ("ld_128_cleartag", qeq (__builtin_e2k_ld_128_cleartag (buf.q, 16 * k), x), "");
      CHECK ("loadmas_128v", qeq (__builtin_loadmas_128v (p, __LCC_MAS_NO, __LCC_CHAN_ANY), x), "");
      CHECK ("loadmas_128v be", qeq (__builtin_loadmas_128v (p, 0x8, 0), qbswap (x)), "");
      v2di y = qmk (rnd (), rnd ());
      __builtin_e2k_st_128_be (y, p);
      CHECK ("st_128_be", qeq (*p, qbswap (y)), "");
      __builtin_e2k_st_128_nt (y, p);
      CHECK ("st_128_nt", qeq (*p, y), "");
      __builtin_storemas_128v (y, p, 0x8, 5);
      CHECK ("storemas_128v be", qeq (*p, qbswap (y)), "");
      /* Byte-masked stores: byte j is written when mask bit j is set.  */
      unsigned mask = (unsigned) rnd () & 0xffff;
      v2di old = *p, z = qmk (rnd (), rnd ()), want = old;
      for (int j = 0; j < 16; j++)
        if (mask >> j & 1)
          want = qsetlane (want, 1, j, qlane (z, 1, j));
      __builtin_e2k_pst_128 (z, p, mask);
      CHECK ("pst_128", qeq (*p, want), "mask %04x", mask);
      *p = old;
      __builtin_e2k_pst_128_sm (z, p, mask);
      CHECK ("pst_128_sm", qeq (*p, want), "mask %04x", mask);
      *p = old;
      __builtin_pstoremas_128v (z, p, mask, __LCC_MAS_NO, 2);
      CHECK ("pstoremas_128v", qeq (*p, want), "mask %04x", mask);
      /* pst_128_be: the 16 bytes reversed, the mask applied before
         the reversal: bit j guards data byte j, stored at byte 15 - j.  */
      *p = old;
      want = old;
      for (int j = 0; j < 16; j++)
        if (mask >> (15 - j) & 1)
          want = qsetlane (want, 1, j, qlane (qbswap (z), 1, j));
      __builtin_e2k_pst_128_be (z, p, mask);
      CHECK ("pst_128_be", qeq (*p, want), "mask %04x: got %016llx %016llx",
             mask, q0 (*p), q1 (*p));
    }
#else
  missing ("check_mem128");
#endif
}
#endif

extern const u32 __cpu_info[2] __attribute__ ((visibility ("hidden")));

static void
info (void)
{
#if __has_builtin (__builtin_cpu_name) \
    && (defined __builtin_e2k_get_cpu_iset || __has_builtin (__builtin_e2k_get_cpu_iset)) \
    && defined __GETPFPFR
  static const char *const cpus[] = {
    "elbrus", "elbrus-2c+", "elbrus-4c", "elbrus-8c", "elbrus-1c+",
    "elbrus-8c2", "elbrus-12c", "elbrus-16c", "elbrus-2c3", "elbrus-48c",
    "elbrus-8v7" };
  printf ("info cpu_name %s cpu_arch %s __cpu_info %u %u get_cpu_iset %u\n",
          __builtin_cpu_name (), __builtin_cpu_arch (), __cpu_info[0],
          __cpu_info[1], __builtin_e2k_get_cpu_iset ());
  printf ("info cpu_is");
#define I(s) if (__builtin_cpu_is (s)) printf (" %s", s);
  I ("elbrus") I ("elbrus-2c+") I ("elbrus-4c") I ("elbrus-8c") I ("elbrus-1c+")
  I ("elbrus-8c2") I ("elbrus-12c") I ("elbrus-16c") I ("elbrus-2c3")
  I ("elbrus-48c") I ("elbrus-8v7") I ("elbrus-v1") I ("elbrus-v2")
  I ("elbrus-v3") I ("elbrus-v4") I ("elbrus-v5") I ("elbrus-v6") I ("elbrus-v7")
#undef I
  printf ("\n");
  (void) cpus;
  printf ("info core_mode.descr_v7 %u\n",
          __builtin_e2k_get_cpu_mode ("core_mode.descr_v7"));
  unsigned w0, w1;
  __GETPFPFR (w0);
  __SETPFPFR (w0 ^ 0x6000);
  __GETPFPFR (w1);
  __SETPFPFR (w0);
  printf ("info pfpfr %#x, after writing %#x: %#x\n", w0, w0 ^ 0x6000, w1);
  unsigned __int128 usd = __builtin_e2k_rrq ("usd");
  u64 sp;
  __asm__ volatile ("getsp,0 0, %0" : "=r" (sp));
  CHECK ("rrq usd", (u64) usd >> 32 == 0 || ((u64) usd & 0xffffffffffffULL) <= sp + 4096,
         "usd.lo %016llx, sp %016llx", (u64) usd, sp);
#else
  missing ("info");
#endif
}

/* (In info () itself lcc 1.31.05 for v3 stops with "internal error:
   Cannot expand HW_BARRIER oper".)  */
static void __attribute__ ((noinline))
barriers (void)
{
#if (defined __builtin_e2k_barrier || __has_builtin (__builtin_e2k_barrier))
  __builtin_e2k_read_barrier (0x30);
  __builtin_e2k_write_barrier (0x30);
  __builtin_e2k_mem_barrier (0x30);
  __builtin_e2k_barrier (0x30);
#else
  missing ("barriers");
#endif
}

int
main (void)
{
  check_punpck ();
  check_pshufh ();
  check_align64 ();
  check_ptr ();
  check_clzb ();
  check_mem64 ();
#if __iset__ >= 5
  check_q ();
  check_mem128 ();
#endif
  info ();
  barriers ();
  printf ("%d checks, %d failed", checks, fails);
  if (missed)
    printf (", %d missing", missed);
  printf ("\n");
  return fails != 0 || missed != 0;
}
