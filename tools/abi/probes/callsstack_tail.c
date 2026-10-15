/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Tail calls that give the frame back before the jump, as GCC's sibling
   calls would (spec/abi/calling-convention.md, "Tail calls";
   doc/gcc-port-plan.md D4, "Sibling calls").  lcc's own tail calls keep
   the caller's frame (pocplan_tail.c); the functions in
   callsstack_tail.s release theirs with a positive getsp and then jump
   with ct.  This driver checks that the callee then runs exactly as if
   called directly:

   sp      cs_rel_same/gap/none: where the callee's stack pointer is
           against a direct call (expected 0, 0 and -64);
   vla     cs_rel_vla (n): a frame plus an n-byte VLA released by one
           getsp of a register (expected 0);
   round   getsp -a then +a: where the stack pointer ends (shows what a
           release of an amount that is not a multiple of 16 does);
   over    a getsp +a with nothing allocated: is it refused?  (in a
           child);
   movtdp  a movtd into a CTPR under a true and a false predicate (in a
           child): the signal and si_code, or "no trap"; over a disp of
           the same CTPR (p), into a CTPR not prepared before (q), and
           unconditionally over a disp (u, the control);
   args10  cs_rel_args10: the caller rewrites slots 8 and 9 of its own
           incoming area after the release; the callee's checksum and
           local address against a direct call;
   big     cs_rel_big: a 96-byte result written through the caller's
           area by the tail-called callee;
   chain   N mutual tail calls with a 64-byte frame each (release before
           or in the ct's instruction): the final callee's local
           address, the sum, and the stack pointer after the return;
   alloca  the final callee allocates 64 KiB .. 4 MiB with alloca (the
           kernel grows the stack during the chain): sum and stack
           pointer after;
   bt      backtrace () in the final callee of a 10,000-call chain
           (frame count and names; the chain functions do not appear);
   sig     a 20 us SIGALRM timer during CHAINS (default 200) chains of
           100,000 calls: the
           handler (on the thread's own stack) clears 2 KiB of locals
           and every 8th signal records backtrace ()'s depth; results,
           signals taken and the deepest backtrace;
   spin    the final callee spins until 3 timer signals came, checking
           a 512-byte local array after each.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   # or v6.2c3 ...
     $L -O2 -fno-inline -rdynamic tools/abi/probes/callsstack_tail.c \
        tools/abi/probes/callsstack_tail.s -o callsstack_tail
     ./callsstack_tail [CHAINS]

   Prints one line per check, "ok" or "BAD" with the figures.  Also runs
   under MCST's qemu-e2k (-L the sysroot).  */

#define _GNU_SOURCE
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

struct big
{
  long v[12];
};

long cs_sp (long);
long cs_rel_same (long), cs_rel_gap (long), cs_rel_none (long);
long cs_rel_vla (long);
long cs_round (long), cs_over (long);
long cs_rel_args10 (long, long, long, long, long, long, long, long, long,
                    long);
struct big cs_rel_big (long);
typedef long (*fin_t) (long, long, long);
long cs_chain_a (long, long, long), cs_chain_c (long, long, long);
long cs_movtdp (long, long), cs_movtdu (long, long), cs_movtdq (long, long);

char *volatile where;
static int bad;

static void
check (const char *what, int ok, const char *fmt, long a, long b)
{
  printf ("%-44s %s  ", what, ok ? "ok " : "BAD");
  printf (fmt, a, b);
  printf ("\n");
  if (!ok)
    bad++;
}

long __attribute__ ((noinline))
g10c (long a, long b, long c, long d, long e, long f, long g, long h,
      long i, long j)
{
  volatile char k = 0;
  where = (char *) &k;
  return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h + 9 * i
         + 10 * j + k;
}

struct big __attribute__ ((noinline))
gbig (long x)
{
  volatile char k = 0;
  struct big r;
  where = (char *) &k;
  for (int i = 0; i < 12; i++)
    r.v[i] = x + i + k;
  return r;
}

long __attribute__ ((noinline))
fin_where (long acc, long u, long v)
{
  (void) u, (void) v;
  volatile char k = 0;
  where = (char *) &k;
  return acc + k;
}

static long alloca_size;

long __attribute__ ((noinline))
fin_alloca (long acc, long u, long v)
{
  (void) u, (void) v;
  long size = alloca_size;
  volatile char *p = __builtin_alloca (size);
  for (long i = 0; i < size; i += 4096)
    p[i] = 1;
  p[size - 1] = 2;
  where = (char *) p;
  return acc + p[size - 1] - 2;
}

static void *bt_buf[256];
static int bt_n;

long __attribute__ ((noinline))
fin_bt (long acc, long u, long v)
{
  (void) u, (void) v;
  bt_n = backtrace (bt_buf, 256);
  return acc;
}

static volatile int nsig, bt_max, bt_samples;

static void
on_alrm (int sig)
{
  (void) sig;
  volatile char junk[2048];
  memset ((char *) junk, 0x5a, sizeof junk);
  nsig++;
  if ((nsig & 7) == 0)
    {
      void *b[256];
      int n = backtrace (b, 256);
      bt_samples++;
      if (n > bt_max)
        bt_max = n;
    }
}

long __attribute__ ((noinline))
fin_spin (long acc, long u, long v)
{
  (void) u, (void) v;
  volatile long a[64];
  for (int i = 0; i < 64; i++)
    a[i] = acc + i;
  int seen = nsig, rounds = 0, wrong = 0;
  while (rounds < 3)
    {
      if (nsig != seen)
        {
          seen = nsig;
          rounds++;
          for (int i = 0; i < 64; i++)
            if (a[i] != acc + i)
              wrong++;
        }
    }
  where = (char *) &a[0];
  return wrong ? -1 : acc;
}

long __attribute__ ((noinline))
mt_target (long x)
{
  (void) x;
  return 12345;
}

static void
on_ill (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  printf ("signal %d si_code %d\n", sig, si->si_code);
  fflush (stdout);
  _exit (0);
}

static void
timer (long usec)
{
  struct itimerval it = { { 0, usec }, { 0, usec } };
  setitimer (ITIMER_REAL, &it, NULL);
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  int chains = argc > 1 ? atoi (argv[1]) : 200;
  char line[128];

  /* sp */
  long d = cs_sp (0);
  long r = cs_rel_same (0) - d;
  check ("sp: release in the ct's instruction", r == 0, "delta %ld", r, 0);
  r = cs_rel_gap (0) - d;
  check ("sp: release 6 cycles before the ct", r == 0, "delta %ld", r, 0);
  r = cs_rel_none (0) - d;
  check ("sp: no release (control)", r == -64, "delta %ld", r, 0);

  /* vla */
  static const long vn[] = { 1, 16, 100, 4096, 70000 };
  for (unsigned i = 0; i < sizeof vn / sizeof vn[0]; i++)
    {
      d = cs_sp (vn[i]);
      r = cs_rel_vla (vn[i]) - d;
      snprintf (line, sizeof line, "vla: %ld bytes + 32-byte frame", vn[i]);
      check (line, r == 0, "delta %ld", r, 0);
    }

  /* round */
  static const long ra[] = { 8, 16, 24, 32, 40 };
  for (unsigned i = 0; i < sizeof ra / sizeof ra[0]; i++)
    {
      r = cs_round (ra[i]);
      snprintf (line, sizeof line, "round: getsp -%ld then +%ld", ra[i],
                ra[i]);
      printf ("%-44s     sp moved by %ld\n", line, r);
    }

  /* over */
  for (long a = 16; a <= 4096; a *= 16)
    {
      fflush (stdout);
      pid_t pid = fork ();
      if (pid == 0)
        {
          long v = cs_over (a);
          printf ("%-44s     getsp +%ld with no frame: sp moved by %ld\n",
                  "over:", a, v);
          fflush (stdout);
          _exit (0);
        }
      int st;
      waitpid (pid, &st, 0);
      if (WIFSIGNALED (st))
        printf ("%-44s     getsp +%ld with no frame: signal %d\n", "over:",
                a, WTERMSIG (st));
    }

  /* movtdp */
  static const struct
  {
    const char *name;
    long (*f) (long, long);
    int conds;
  } mt[] = { { "p: over a disp", cs_movtdp, 2 },
             { "q: into an unprepared CTPR", cs_movtdq, 2 },
             { "u: unconditional, over a disp", cs_movtdu, 1 } };
  for (unsigned k = 0; k < sizeof mt / sizeof mt[0]; k++)
    for (long v = 0; v < mt[k].conds; v++)
      {
        printf ("%-44s     %s, movtd%s: ", "movtdp:", mt[k].name,
                mt[k].conds == 1 ? "" : v ? " ? false" : " ? true");
        fflush (stdout);
        pid_t pid = fork ();
        if (pid == 0)
          {
            struct sigaction sa;
            memset (&sa, 0, sizeof sa);
            sa.sa_sigaction = on_ill;
            sa.sa_flags = SA_SIGINFO;
            sigaction (SIGILL, &sa, NULL);
            sigaction (SIGSEGV, &sa, NULL);
            sigaction (SIGBUS, &sa, NULL);
            long sp = cs_sp (0);
            long got = mt[k].f (v, (long) mt_target);
            printf ("no trap, %s\n", got == 12345 ? "the ct used the movtd"
                    : got == sp ? "the ct used the disp"
                    : got == 777 ? "the ct was not taken"
                    : "unexpected result");
            fflush (stdout);
            _exit (0);
          }
        int st;
        waitpid (pid, &st, 0);
        if (WIFSIGNALED (st))
          printf ("killed by signal %d\n", WTERMSIG (st));
      }

  /* args10 */
  long e = g10c (2, 3, 4, 5, 6, 7, 8, 9, 10, 1);
  char *dw = where;
  r = cs_rel_args10 (1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  check ("args10: checksum", r == e, "%ld (direct %ld)", r, e);
  check ("args10: callee's local against a direct call", where == dw,
         "delta %ld", (long) (where - dw), 0);

  /* big */
  struct big b0 = gbig (100);
  dw = where;
  struct big b1 = cs_rel_big (100);
  check ("big: 96-byte result", memcmp (&b0, &b1, sizeof b0) == 0,
         "v[0] %ld v[11] %ld", b1.v[0], b1.v[11]);
  check ("big: callee's local against a direct call", where == dw,
         "delta %ld", (long) (where - dw), 0);

  /* chain */
  static const long cn[] = { 0, 1, 10, 10000, 1000000 };
  for (int k = 0; k < 2; k++)
    for (unsigned i = 0; i < sizeof cn / sizeof cn[0]; i++)
      {
        long (*ch) (long, long, long) = k ? cs_chain_c : cs_chain_a;
        long n = cn[i];
        fin_where (0, 0, 0);
        dw = where;
        long sp0 = cs_sp (0);
        r = ch (n, (long) fin_where, 0);
        long sp1 = cs_sp (0);
        snprintf (line, sizeof line, "chain %s: %ld calls, sum",
                  k ? "c/d" : "a/b", n);
        check (line, r == n * (n + 1) / 2, "%ld (expected %ld)", r,
               n * (n + 1) / 2);
        snprintf (line, sizeof line, "chain %s: %ld calls, local, sp after",
                  k ? "c/d" : "a/b", n);
        check (line, where == dw && sp0 == sp1, "delta %ld, sp delta %ld",
               (long) (where - dw), sp1 - sp0);
      }

  /* alloca */
  for (alloca_size = 64 << 10; alloca_size <= 4 << 20; alloca_size *= 4)
    {
      long sp0 = cs_sp (0);
      r = cs_chain_a (1000, (long) fin_alloca, 0);
      long sp1 = cs_sp (0);
      char *got = where;
      fin_alloca (0, 0, 0);
      snprintf (line, sizeof line, "alloca: %ld KiB after 1,000 calls",
                alloca_size >> 10);
      check (line, r == 1000L * 1001 / 2 && got == where && sp0 == sp1,
             "block delta %ld, sp delta %ld", (long) (got - where),
             sp1 - sp0);
    }

  /* bt */
  backtrace (bt_buf, 4); /* loads what backtrace needs before the signals */
  r = cs_chain_a (10000, (long) fin_bt, 0);
  char **names = backtrace_symbols (bt_buf, bt_n);
  printf ("%-44s     %d frames:", "bt: in the final callee of 10,000", bt_n);
  for (int i = 0; i < bt_n; i++)
    {
      const char *s = names ? names[i] : "?";
      const char *p = strrchr (s, '/');
      printf (" %s", p ? p + 1 : s);
    }
  printf ("\n");
  int has_chain = 0;
  for (int i = 0; names && i < bt_n; i++)
    if (strstr (names[i], "cs_chain"))
      has_chain = 1;
  check ("bt: no cs_chain frame, sum", !has_chain && r == 10000L * 10001 / 2,
         "frames %ld, sum %ld", bt_n, r);

  /* sig */
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = on_alrm;
  sa.sa_flags = SA_RESTART;
  sigaction (SIGALRM, &sa, NULL);
  int wrong = 0;
  fin_where (0, 0, 0);
  dw = where;
  long sp0 = cs_sp (0);
  timer (20);
  for (int i = 0; i < chains; i++)
    {
      long n = 100000;
      r = (i & 1 ? cs_chain_c : cs_chain_a) (n, (long) fin_where, i);
      if (r != n * (n + 1) / 2 + i || where != dw || cs_sp (0) != sp0)
        wrong++;
    }
  timer (0);
  snprintf (line, sizeof line, "sig: %d chains of 100,000, wrong results",
            chains);
  check (line, wrong == 0, "%ld wrong, %ld signals", wrong, nsig);
  printf ("%-44s     deepest backtrace in the handler: %d frames "
          "(%d samples)\n", "sig:", bt_max, bt_samples);

  /* spin */
  timer (1000);
  nsig = 0;
  fin_spin (0, 0, 0);
  dw = where;
  r = cs_chain_a (10000, (long) fin_spin, 0);
  timer (0);
  check ("spin: final callee's array across 3 signals",
         r == 10000L * 10001 / 2 && where == dw, "sum %ld, delta %ld", r,
         (long) (where - dw));

  printf ("%s\n", bad ? "SOME BAD" : "all ok");
  return 0;
}
