/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The high half of a v5+ 128-bit window register whose last write was
   64 bits or narrower, and whether a store of the whole register (stqp)
   can trap: the proof-of-concept back end's I55 (spec/abi/registers.md,
   "nfx" under the window flags; calling-convention.md, "Unused parts of
   a slot").  The PoC saved a pointer that lived in %b[5] with stqp (its
   caller-save mode was the register's widest), and now and then the
   stqp died with SIGILL ILL_ILLOPN on the 8C2 under forced context
   switches, with a high half of 0x4afafafa4afafaf8, tags 1/1, in an
   nfx = 1 frame.

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc     # the 8C2
     L=/opt/mcst/lcc-1.29.16.e2k-v6.2c3.linux-6.1/bin/lcc     # the 2C3
     $L -O1 tools/abi/probes/foldi55.c tools/abi/probes/foldi55.s -o foldi55
     ./foldi55 wr|gain|hand|spill|sig|sys    # on hardware, v5 or later
     ./foldi55 ctx [HOLDS [SPIN [CPU [WAKE_NS]]]]

   Not for MCST's qemu, which hangs on stores of empty halves
   (calling-convention.md, "Unused parts of a slot").

   Every case stores the registers with stqp,sm (no trap; memory keeps
   the tags) and reads each doubleword's tags back with the fill MAS
   (ldd mas=0x4, gettagd).  VALUE/T is a half and its tags (gettagd:
   the low word's in bits 0-1, the high word's in 2-3; 5 = tag 1 on
   both); "diag" marks words with tag 1 and bit 30 set (diagnostic: a
   non-speculative use traps), "empty" tag 1 with bit 30 clear.

     wr     %qpr6 = {hi, hi} (qppackdl), then one write of %dr6 (addd,
            adds under dbl 0 and 1, ldd, ldw, faddd, ldd,sm of a
            PROT_NONE page, sxt): what is left in the high half.  Then
            registers a window gains (setwd 8, 4, 8; a second call).
     gain   fi_gset gives %qpr10..13 a numeric or a diagnostic high half,
            calls a leaf or a 1000-deep recursion and returns; fi_gget_K,
            called next from the same frame, writes them with 64 bits.
            They lie beyond the C caller's window, so the call gains
            them.  Variants: a delay after the setwd, a bigger window, a
            128-bit write of the pair partner, fi_t's order.
     hand   an asm caller (wsz 16 or 20 pairs) calls fi_hset (diagnostic
            high halves in %qpr0..15) and then fi_hget, both with wbs 12:
            fi_hget's %r0..%r7 (wsz 16) or all 16 (wsz 20) are pairs the
            caller hands over, the others pairs the call gains.  fi_hget
            writes %dr1..%dr15 with addd and stores them, then a plain
            stqp of %qpr3 or %qpr9, or a plain std of %dr3.
     spill  fi_t_call: %qpr6 = {hi, hi}, then an addd into %dr6; %dr7
            written with addd only; %qpr8 = {hi, val + 8} (a 128-bit
            write); %qpr9 not written (%r0..%r7 are pairs the C caller
            hands over, %r8 up pairs the window gains); then a call 1000
            deep (lcc's C, or fi_deepq, which leaves 0d0d... in its high
            halves) or none; setwd nfx = 1 and 0.  "prime" first runs
            fi_prime from the same frame: its %qpr0..23 get diagnostic
            high halves 4a0000KK4afafaf8 (KK the register), spilled by a
            deep call.  Each rep then stores %qpr7, %qpr6 or %qpr8 with a
            plain stqp ("ok" or the signal; code 2 is ILL_ILLOPN).
     sig    the same across a SIGSEGV whose handler fixes the page (also
            one whose handler first recurses 1000 deep).
     sys    across getpid, sched_yield and nanosleep (1 ms).
     ctx    HOLDS holds (default 3000) of SPIN loop iterations (default
            100000, about 0.4 ms on the 8C2) on CPU (default 1), with a
            process pinned to it that wakes every WAKE_NS ns (default
            50000, as the PoC's reproduction; 0: none); nfx 1 and 0, with
            nothing, fi_prime ("pre prime") or a 1000-deep call ("pre
            deep") before each hold.  %dr10 holds a diagnostic
            4afafafa4afafaf8 (ldd,sm of a PROT_NONE page, andd,sm ~7)
            during the hold and is then written with val + 10 (the PoC's
            picture: a pointer in the low half); the other low halves
            are val + N.  Classifies each high half (as before, 0/0, its
            own low half, ...) and counts the plain-stqp traps of %qpr7,
            %qpr6, %qpr8 and %qpr10 (one a hold, in turn).

   Elbrus-8C2 (v5), 2026-10-24, the machine otherwise loaded (load 8):
     wr: every write of 64 bits or less kept the high half and its tags.
       Registers a window gains read 0 with tags 1/1 (empty) in both
       halves, and after a 64-bit write the high half was still empty.
     hand: the pairs a caller hands over kept both halves as the earlier
       callee had left them.  fi_hget's 64-bit-written %r1..%r7 (wsz 16;
       all 15 with wsz 20) had 4a0000aa4afafaf8/5 high halves, and a
       plain stqp of %qpr3 was SIGILL ILL_ILLOPN; the gained %r8..%r15
       read empty, and a plain stqp of %qpr9 stored.  A plain std of
       %dr3 stored (it reads the low half only).
     gain: no variant showed an older high half in a gained register.
     spill, sig: nfx = 1: after a 1000-deep call or a signal handler
       every high half read 0, tags 0 (written, unwritten and primed
       ones alike); nfx = 0: all kept.  sys: getpid and sched_yield
       kept them under either nfx; nanosleep did too, except after
       prime with nfx = 1: 0, or (2 runs of 3) a copy of the register's
       own low half.
     ctx (four runs of 3000 holds a case): nfx = 0 kept every high
       half T wrote, in every case.  nfx = 1 kept them in every hold
       with nothing before it (about 10500 involuntary switches with
       the waker); after a 1000-deep call had returned they were lost
       in 2983-2999 of 3000 holds with the waker and in about 300 of
       3000 without it (timer interrupts; 0-56 switches): to 0/0, or to
       a copy of the register's own low half with its tags (1-15 holds
       a case with the waker, 0-39 without).  %qpr10's high half then
       became 4afafafa4afafaf8/5, diagnostic, and after val + 10 was
       written over the low half a plain stqp of %qpr10 was SIGILL
       ILL_ILLOPN: 2 and 3 of 750 holds with the waker, 2 of 750
       without (the PoC's picture: the pointer in the low half, tags 0,
       4afafafa4afafaf8 tags 1/1 in the high half).  A plain stqp of
       %qpr7 after prime was SIGILL in every hold whose primed high half
       survived (676-838 of 750-1000 with nfx 1 and no waker, all with
       nfx 0).  */

#define _GNU_SOURCE
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

typedef unsigned long u64;
typedef u64 (*tfn) (void *, u64, u64, long, long, long);

u64 fi_tags (const void *);
#define WR(k) void fi_wr_##k (void *, u64, u64, long *);
WR (none) WR (addd) WR (adds0) WR (adds1) WR (ldd) WR (ldw) WR (faddd)
WR (lddsm) WR (sxt)
#undef WR
void fi_gain_setwd_nfx0 (void *), fi_gain_setwd_nfx1 (void *);
void fi_gain_set (void), fi_gain_get (void *);
#define T(a)                                                                \
  u64 fi_t_##a##_nfx0 (void *, u64, u64, long, long, long);                \
  u64 fi_t_##a##_nfx1 (void *, u64, u64, long, long, long);                \
  extern char fi_t_##a##_nfx0_st[], fi_t_##a##_nfx1_st[];
T (none) T (call) T (spin) T (fault) T (sys)
#undef T
void fi_prime (long (*) (long), long);
long fi_deepq (long);
void fi_hrun_0x10 (void *, u64, long), fi_hrun_0x14 (void *, u64, long);
extern char fi_hget_st[];
void fi_gset_nfx0 (u64, long, long (*) (long), long);
void fi_gset_nfx1 (u64, long, long (*) (long), long);
#define GG(k) void fi_gget_##k (void *, long *);
GG (0) GG (d) GG (w) GG (q) GG (t)
#undef GG
long fi_leaf (long);

#define M48 0xffffffffffffUL
static const u64 HI = 0x0123456789abcdefUL;
static u64 out[32] __attribute__ ((aligned (16)));
static long pg;
static char *fix_page;
static int segv_deep;
static volatile int nsegv;
static sigjmp_buf jb;
static volatile int trap_sig, trap_code;
static volatile u64 trap_ip;

__attribute__ ((noinline)) static long
deep (long n)
{
  volatile char pad[16];
  pad[0] = (char) n;
  if (n > 0)
    deep (n - 1);
  return pad[0];
}

static void
on_segv (int sig, siginfo_t *si, void *uc)
{
  (void) sig;
  (void) uc;
  if (fix_page && (char *) si->si_addr >= fix_page
      && (char *) si->si_addr < fix_page + pg)
    {
      if (segv_deep)
        deep (1000);
      mprotect (fix_page, pg, PROT_READ | PROT_WRITE);
      nsegv++;
      return;
    }
  static const char m[] = "unexpected SIGSEGV\n";
  ssize_t r = write (1, m, sizeof m - 1);
  (void) r;
  _exit (3);
}

static void
on_ill (int sig, siginfo_t *si, void *ucv)
{
  ucontext_t *uc = ucv;
  trap_sig = sig;
  trap_code = si->si_code;
  trap_ip = uc->uc_mcontext.cr0_hi & M48 & ~7UL;
  siglongjmp (jb, 1);
}

/* A doubleword V with tags T (gettagd: low word bits 0-1, high 2-3).  */
static const char *
kind (u64 v, unsigned t)
{
  static char buf[8][24];
  static int k;
  char *b = buf[k++ & 7];
  const char *w[2];
  for (int i = 0; i < 2; i++)
    {
      unsigned tg = (t >> (2 * i)) & 3;
      u64 word = (v >> (32 * i)) & 0xffffffffUL;
      w[i] = tg == 0 ? "num" : tg != 1 ? "ptr?" : (word & 0x40000000UL)
                                                      ? "diag" : "empty";
    }
  if (strcmp (w[0], w[1]) == 0)
    snprintf (b, 24, "%s", w[0]);
  else
    snprintf (b, 24, "%s/%s", w[1], w[0]);
  return b;
}

static int
isdiag (u64 v, unsigned t)
{
  for (int i = 0; i < 2; i++)
    if (((t >> (2 * i)) & 3) == 1 && ((v >> (32 * i)) & 0x40000000UL))
      return 1;
  return 0;
}

/* Slots of out: r6, r7, r8, r9, the plain stqp, r10, r11.  */
#define NSLOT 7
struct res
{
  u64 v[2 * NSLOT];
  unsigned tg[2 * NSLOT];
  int trapped, code;
  long off;
  u64 cyc;
};

static void
collect (struct res *r)
{
  for (int i = 0; i < 2 * NSLOT; i++)
    r->v[i] = out[i];
  for (int i = 0; i < NSLOT; i++)
    {
      unsigned t = fi_tags (&out[2 * i]);
      r->tg[2 * i] = t & 15;
      r->tg[2 * i + 1] = (t >> 4) & 15;
    }
}

/* One call of F (its plain-store label ST), first PRE: 0 nothing, 1
   fi_prime (deep, 1000), 2 deep (1000); SEL picks the plain stqp (7, 6,
   8, or 0).  */
static void
one_t (tfn f, const char *st, long x, long n, long sel, int pre,
       struct res *r)
{
  memset (out, 0, sizeof out);
  memset (r, 0, sizeof *r);
  trap_sig = 0;
  if (sigsetjmp (jb, 1) == 0)
    {
      if (pre == 1)
        fi_prime (deep, 1000);
      else if (pre == 2)
        deep (1000);
      r->cyc = f (out, HI, (u64) out, x, n, sel);
    }
  else
    {
      r->trapped = trap_sig;
      r->code = trap_code;
      r->off = (long) (trap_ip - (u64) st);
    }
  collect (r);
}

static void
print_res (const char *name, const struct res *r, long sel)
{
  char lo[64] = "";
  if (r->v[0] != (u64) out + 6 || r->v[2] != (u64) out + 7
      || r->v[4] != (u64) out + 8)
    snprintf (lo, sizeof lo, "  LOW CHANGED %lx %lx %lx", r->v[0], r->v[2],
              r->v[4]);
  printf ("%-28s r6 %016lx/%x %-5s r7 %016lx/%x %-5s r8 %016lx/%x %-5s "
          "r9 %016lx/%x %-5s",
          name, r->v[1], r->tg[1], kind (r->v[1], r->tg[1]), r->v[3],
          r->tg[3], kind (r->v[3], r->tg[3]), r->v[5], r->tg[5],
          kind (r->v[5], r->tg[5]), r->v[7], r->tg[7],
          kind (r->v[7], r->tg[7]));
  if (sel)
    {
      if (r->trapped)
        printf ("  plain qpr%ld: signal %d code %d at +%#lx", sel,
                r->trapped, r->code, r->off);
      else
        printf ("  plain qpr%ld: ok", sel);
    }
  printf ("%s\n", lo);
}

static void
setup_signals (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = on_segv;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGSEGV, &sa, NULL);
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = on_ill;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
}

/* --- wr ---------------------------------------------------------------- */

static void
wr (void)
{
  static const struct
  {
    const char *name;
    void (*f) (void *, u64, u64, long *);
    int bad;
  } ws[] = {
    { "none (qppackdl only)", fi_wr_none, 0 },
    { "addd", fi_wr_addd, 0 },
    { "adds, dbl = 0", fi_wr_adds0, 0 },
    { "adds, dbl = 1", fi_wr_adds1, 0 },
    { "ldd", fi_wr_ldd, 0 },
    { "ldw", fi_wr_ldw, 0 },
    { "faddd", fi_wr_faddd, 0 },
    { "ldd,sm of PROT_NONE", fi_wr_lddsm, 1 },
    { "sxt 2 (sign-extend 32)", fi_wr_sxt, 0 },
  };
  printf ("== wr: %%qpr6 = {%016lx, same}, then one write of %%dr6 "
          "(val 0x1122334455667788)\n", HI);
  for (unsigned i = 0; i < sizeof ws / sizeof *ws; i++)
    {
      long mem = 0x3ff0000000000000L;
      long *p = &mem;
      char *bad = NULL;
      if (ws[i].bad)
        p = (long *) (bad = mmap (NULL, pg, PROT_NONE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
      memset (out, 0, sizeof out);
      ws[i].f (out, HI, 0x1122334455667788UL, p);
      unsigned t = fi_tags (out);
      printf ("%-24s low %016lx/%x %-5s high %016lx/%x %-5s\n", ws[i].name,
              out[0], t & 15, kind (out[0], t & 15), out[1], t >> 4,
              kind (out[1], t >> 4));
      if (bad)
        munmap (bad, pg);
    }

  printf ("== registers the window gains (%%qpr13 = 6666..., %%qpr12 = "
          "7777... before; then %%dr13 = 5 only)\n");
  for (int k = 0; k < 4; k++)
    {
      memset (out, 0, sizeof out);
      const char *name;
      if (k == 0)
        name = "setwd 8, 4, 8 (nfx 0)", fi_gain_setwd_nfx0 (out);
      else if (k == 1)
        name = "setwd 8, 4, 8 (nfx 1)", fi_gain_setwd_nfx1 (out);
      else
        {
          name = k == 2 ? "next call, same wbs" : "next call, again";
          fi_gain_set ();
          fi_gain_get (out);
        }
      unsigned t13 = fi_tags (&out[0]), t12 = fi_tags (&out[2]);
      printf ("%-24s qpr13 low %016lx/%x %-5s high %016lx/%x %-5s  qpr12 "
              "low %016lx/%x %-5s high %016lx/%x %-5s\n",
              name, out[0], t13 & 15, kind (out[0], t13 & 15), out[1],
              t13 >> 4, kind (out[1], t13 >> 4), out[2], t12 & 15,
              kind (out[2], t12 & 15), out[3], t12 >> 4,
              kind (out[3], t12 >> 4));
    }
}

/* --- spill, sig, sys --------------------------------------------------- */

static const long sels[3] = { 7, 6, 8 };

static void
spill (void)
{
  printf ("== spill: r6 = {hi %016lx} + addd, r7 addd only, r8 = {hi, "
          "val}, r9 unwritten; low halves = val\n", HI);
  static const struct
  {
    const char *name;
    int call;
    long (*f) (long);
  } ks[] = { { "no call", 0, NULL }, { "C deep 1000", 1, deep },
             { "fi_deepq 1000", 1, fi_deepq } };
  for (int nfx = 1; nfx >= 0; nfx--)
    for (unsigned k = 0; k < 3; k++)
      for (int prime = 0; prime <= 1; prime++)
        for (int rep = 0; rep < 3; rep++)
          {
            struct res r;
            char name[64];
            tfn f = ks[k].call ? (nfx ? fi_t_call_nfx1 : fi_t_call_nfx0)
                               : (nfx ? fi_t_none_nfx1 : fi_t_none_nfx0);
            const char *st = ks[k].call ? (nfx ? fi_t_call_nfx1_st
                                               : fi_t_call_nfx0_st)
                                        : (nfx ? fi_t_none_nfx1_st
                                               : fi_t_none_nfx0_st);
            one_t (f, st, (long) ks[k].f, 1000, sels[rep], prime, &r);
            snprintf (name, sizeof name, "nfx %d %s%s", nfx, ks[k].name,
                      prime ? " prime" : "");
            print_res (name, &r, sels[rep]);
          }
}

static void
sig (void)
{
  printf ("== sig: the same registers across a SIGSEGV whose handler fixes "
          "the page\n");
  for (int nfx = 1; nfx >= 0; nfx--)
    for (int d = 0; d <= 1; d++)
      for (int prime = 0; prime <= 1; prime++)
        for (int rep = 0; rep < 3; rep++)
          {
            struct res r;
            char name[64];
            fix_page = mmap (NULL, pg, PROT_NONE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            segv_deep = d;
            nsegv = 0;
            one_t (nfx ? fi_t_fault_nfx1 : fi_t_fault_nfx0,
                   nfx ? fi_t_fault_nfx1_st : fi_t_fault_nfx0_st,
                   (long) fix_page, 0, sels[rep], prime, &r);
            munmap (fix_page, pg);
            fix_page = NULL;
            snprintf (name, sizeof name, "nfx %d %s%s (%d sig)", nfx,
                      d ? "handler deep" : "handler", prime ? " prime" : "",
                      nsegv);
            print_res (name, &r, sels[rep]);
          }
}

static void
sys (void)
{
  printf ("== sys: the same registers across system calls (entry 3)\n");
  static const struct timespec ms = { 0, 1000000 };
  static const struct
  {
    const char *name;
    long nr;
    long a1;
  } cs[] = { { "getpid", 20, 0 }, { "sched_yield", 158, 0 },
             { "nanosleep 1 ms", 162, (long) &ms } };
  for (int nfx = 1; nfx >= 0; nfx--)
    for (unsigned c = 0; c < 3; c++)
      for (int prime = 0; prime <= 1; prime++)
        for (int rep = 0; rep < 3; rep++)
          {
            struct res r;
            char name[64];
            one_t (nfx ? fi_t_sys_nfx1 : fi_t_sys_nfx0,
                   nfx ? fi_t_sys_nfx1_st : fi_t_sys_nfx0_st, cs[c].a1,
                   cs[c].nr, sels[rep], prime, &r);
            snprintf (name, sizeof name, "nfx %d %s%s", nfx, cs[c].name,
                      prime ? " prime" : "");
            print_res (name, &r, sels[rep]);
          }
}

/* --- ctx --------------------------------------------------------------- */

struct hist
{
  u64 v;
  unsigned t;
  long n;
};

static void
hadd (struct hist *h, int *nh, u64 v, unsigned t)
{
  for (int i = 0; i < *nh; i++)
    if (h[i].v == v && h[i].t == t)
      {
        h[i].n++;
        return;
      }
  if (*nh < 64)
    h[(*nh)++] = (struct hist) { v, t, 1 };
}

static void
hprint (const char *name, struct hist *h, int nh)
{
  /* the most frequent first */
  for (int i = 0; i < nh; i++)
    for (int j = i + 1; j < nh; j++)
      if (h[j].n > h[i].n)
        {
          struct hist x = h[i];
          h[i] = h[j];
          h[j] = x;
        }
  printf ("    %s high halves:", name);
  for (int i = 0; i < nh && i < 8; i++)
    printf (" %016lx/%x %s x%ld;", h[i].v, h[i].t, kind (h[i].v, h[i].t),
            h[i].n);
  if (nh > 8)
    printf (" (%d distinct)", nh);
  printf ("\n");
}

static void
pin (int cpu)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (cpu, &s);
  if (sched_setaffinity (0, sizeof s, &s) != 0)
    perror ("sched_setaffinity");
}

/* Where a high half V/T of register R (6..11) came from.  */
enum { C_KEPT, C_EMPTY, C_ZERO, C_OWNLO, C_LO, C_DIAGLO, C_PRIME, C_OTHER,
       C_N };
static const char *const cname[C_N] = {
  "as before", "empty 0/5", "0/0", "its own low half", "another's low half",
  "r10's diag low half", "a prime marker", "other"
};

static int
classify (int reg, u64 v, unsigned t, int pre)
{
  u64 val = (u64) out;
  if ((reg == 6 || reg == 8) && v == HI && t == 0)
    return C_KEPT;
  if (v == 0 && t == 5)
    return (reg == 6 || reg == 8) ? C_EMPTY
           : pre == 1 && reg <= 7 ? C_EMPTY : C_KEPT;
  if (v == 0 && t == 0)
    return C_ZERO;
  if (v == val + reg && t == 0)
    return C_OWNLO;
  if (v >= val + 6 && v <= val + 11 && t == 0)
    return C_LO;
  if (v == 0x4afafafa4afafaf8UL && t == 5)
    return C_DIAGLO;
  if ((v & 0xffffff00ffffffffUL) == 0x4a0000004afafaf8UL)
    return pre == 1 && reg <= 7 && v == (0x4a0000004afafaf8UL
                                         | ((u64) reg << 32))
           ? C_KEPT : C_PRIME;
  return C_OTHER;
}

static void
ctx (int holds, long spin, int cpu, long wake_ns)
{
  pid_t waker = 0;
  char *bad = mmap (NULL, pg, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  pin (cpu);
  if (wake_ns > 0)
    {
      waker = fork ();
      if (waker == 0)
        {
          struct timespec ts = { 0, wake_ns };
          pin (cpu);
          for (;;)
            nanosleep (&ts, NULL);
        }
    }
  printf ("== ctx: %d holds of %ld iterations on cpu %d, ", holds, spin, cpu);
  if (waker)
    printf ("a process on the same cpu waking every %ld ns\n", wake_ns);
  else
    printf ("alone\n");
  printf ("   before each hold: nothing, fi_prime (pre 1) or a 1000-deep "
          "call (pre 2); r6..r11's high halves classified\n");
  static const char *const pren[3] = { "", " pre prime", " pre deep" };
  for (int nfx = 1; nfx >= 0; nfx--)
    for (int pre = 0; pre <= 2; pre++)
      {
        struct hist h[6][64];
        int nh[6] = { 0 };
        long cls[6][C_N], dg[6] = { 0 };
        long lo = 0, trap[4] = { 0 }, tries[4] = { 0 };
        long other = 0, cyc = 0;
        struct rusage ru0, ru1;
        memset (cls, 0, sizeof cls);
        getrusage (RUSAGE_SELF, &ru0);
        for (int i = 0; i < holds; i++)
          {
            struct res r;
            static const long csel[4] = { 7, 6, 8, 10 };
            int s = i % 4;
            one_t (nfx ? fi_t_spin_nfx1 : fi_t_spin_nfx0,
                   nfx ? fi_t_spin_nfx1_st : fi_t_spin_nfx0_st, (long) bad,
                   spin, csel[s], pre, &r);
            cyc += r.cyc;
            tries[s]++;
            if (r.trapped)
              {
                if (r.trapped == SIGILL && r.code == ILL_ILLOPN)
                  trap[s]++;
                else
                  other++;
              }
            if (r.v[0] != (u64) out + 6 || r.v[2] != (u64) out + 7
                || r.v[4] != (u64) out + 8 || r.v[10] != (u64) out + 10
                || r.v[12] != (u64) out + 11)
              lo++;
            /* slot of register 6 + k */
            static const int slot[6] = { 0, 1, 2, 3, 5, 6 };
            for (int k = 0; k < 6; k++)
              {
                u64 v = r.v[2 * slot[k] + 1];
                unsigned t = r.tg[2 * slot[k] + 1];
                cls[k][classify (6 + k, v, t, pre)]++;
                dg[k] += isdiag (v, t);
                hadd (h[k], &nh[k], v, t);
              }
          }
        getrusage (RUSAGE_SELF, &ru1);
        printf ("nfx %d%s: %d holds, %ld cycles a hold, %ld involuntary "
                "and %ld voluntary switches, low halves changed %ld\n",
                nfx, pren[pre], holds, cyc / (holds ? holds : 1),
                ru1.ru_nivcsw - ru0.ru_nivcsw, ru1.ru_nvcsw - ru0.ru_nvcsw,
                lo);
        printf ("    plain stqp SIGILL ILL_ILLOPN: qpr7 %ld/%ld, qpr6 "
                "%ld/%ld, qpr8 %ld/%ld, qpr10 %ld/%ld; other signals %ld\n",
                trap[0], tries[0], trap[1], tries[1], trap[2], tries[2],
                trap[3], tries[3], other);
        for (int k = 0; k < 6; k++)
          {
            printf ("    r%d high (diagnostic %ld):", 6 + k, dg[k]);
            for (int c = 0; c < C_N; c++)
              if (cls[k][c])
                printf (" %s %ld;", cname[c], cls[k][c]);
            printf ("\n");
          }
        for (int k = 0; k < 6; k++)
          if (nh[k] > 1)
            {
              char nm[8];
              snprintf (nm, sizeof nm, "r%d", 6 + k);
              hprint (nm, h[k], nh[k]);
            }
      }
  if (waker)
    {
      kill (waker, SIGKILL);
      waitpid (waker, NULL, 0);
    }
  munmap (bad, pg);
}

/* --- hand -------------------------------------------------------------- */

static void
hand (void)
{
  printf ("== hand: a caller with wsz W calls fi_hset (%%qpr0..15 = "
          "{4a0000aa4afafaf8 diag, 2222...}), then fi_hget,\n   both with "
          "wbs 12; fi_hget writes %%drK = K (64 bits) and stores %%qprK; "
          "pairs the caller hands over: %%r0..r(2W-25)\n");
  for (int w = 0x10; w <= 0x14; w += 4)
    for (int rep = 0; rep < 3; rep++)
      {
        static const long hsel[3] = { 3, 9, 13 };
        long sel = hsel[rep];
        memset (out, 0, sizeof out);
        trap_sig = 0;
        long off = 0;
        if (sigsetjmp (jb, 1) == 0)
          (w == 0x10 ? fi_hrun_0x10 : fi_hrun_0x14) (out, 0x4a0000aa4afafaf8UL,
                                                     sel);
        else
          off = (long) (trap_ip - (u64) fi_hget_st);
        printf ("W %#x:", w);
        for (int k = 1; k < 16; k++)
          {
            unsigned t = fi_tags (&out[2 * k]) & 0xff;
            printf (" r%d%s %lx/%x %s;", k, k < 2 * w - 24 ? "(h)" : "",
                    out[2 * k + 1], t >> 4, kind (out[2 * k + 1], t >> 4));
            if (out[2 * k] != (u64) k || (t & 15))
              printf (" LOW %lx/%x;", out[2 * k], t & 15);
          }
        const char *what = sel == 13 ? "plain std %dr3" : sel == 9
                                                         ? "plain stqp %qpr9"
                                                         : "plain stqp %qpr3";
        if (trap_sig)
          printf ("  %s: signal %d code %d at +%#lx\n", what, trap_sig,
                  trap_code, off);
        else
          printf ("  %s: ok (stored %016lx)\n", what, out[0]);
      }
}

/* --- gain -------------------------------------------------------------- */

static void
gain (int all)
{
  printf ("== gain: fi_gset gives %%qpr10..13 a high half (and low "
          "2222222222222222), calls f, returns;\n   fi_gget_K, next from the "
          "same frame, writes r13 addd 5, r12 (K: ldd, q: qppackdl, t: "
          "qppackdl then addd), r11 adds 7, not r10\n");
  static const struct
  {
    const char *name;
    u64 hi;
    long tags;
  } hs[] = { { "num 6666...", 0x6666666666666666UL, 0 },
             { "diag 4a0000aa4afafaf8", 0x4a0000aa4afafaf8UL, 0x50 } };
  static const struct
  {
    const char *name;
    long (*f) (long);
  } fs[] = { { "leaf", fi_leaf }, { "C deep 1000", deep } };
  static const struct
  {
    const char *name;
    void (*g) (void *, long *);
  } gs[] = { { "0", fi_gget_0 }, { "d", fi_gget_d }, { "w", fi_gget_w },
             { "q", fi_gget_q }, { "t", fi_gget_t } };
  for (unsigned g = 0; g < 5; g++)
    for (int nfx = 0; nfx <= 1; nfx++)
      for (unsigned h = 0; h < 2; h++)
        for (unsigned k = 0; k < 2; k++)
          {
            long mem = 0x3ff0000000000000L;
            unsigned t[4];
            if (!all && (nfx || !h))
              continue;
            memset (out, 0, sizeof out);
            (nfx ? fi_gset_nfx1 : fi_gset_nfx0) (hs[h].hi, hs[h].tags,
                                                  fs[k].f, 1000);
            gs[g].g (out, &mem);
            for (int i = 0; i < 4; i++)
              t[i] = fi_tags (&out[2 * i]) & 0xff;
            printf ("get %s set nfx %d %-21s %-11s:", gs[g].name, nfx,
                    hs[h].name, fs[k].name);
            static const char *const rn[4] = { "r13", "r12", "r11", "r10" };
            for (int i = 0; i < 4; i++)
              printf ("  %s %016lx/%x %016lx/%x %-5s", rn[i], out[2 * i + 1],
                      t[i] >> 4, out[2 * i], t[i] & 15,
                      kind (out[2 * i + 1], t[i] >> 4));
            printf ("\n");
          }
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  pg = sysconf (_SC_PAGESIZE);
  setup_signals ();
  if (argc < 2)
    {
      fprintf (stderr, "usage: %s wr|gain|hand|spill|sig|sys|ctx [HOLDS [SPIN [CPU "
               "[WAKE_NS]]]]\n", argv[0]);
      return 2;
    }
  if (strcmp (argv[1], "wr") == 0)
    wr ();
  else if (strcmp (argv[1], "spill") == 0)
    spill ();
  else if (strcmp (argv[1], "sig") == 0)
    sig ();
  else if (strcmp (argv[1], "sys") == 0)
    sys ();
  else if (strcmp (argv[1], "gain") == 0)
    gain (argc > 2);
  else if (strcmp (argv[1], "hand") == 0)
    hand ();
  else if (strcmp (argv[1], "ctx") == 0)
    ctx (argc > 2 ? atoi (argv[2]) : 3000,
         argc > 3 ? atol (argv[3]) : 100000, argc > 4 ? atoi (argv[4]) : 1,
         argc > 5 ? atol (argv[5]) : 50000);
  else
    return 2;
  return 0;
}
