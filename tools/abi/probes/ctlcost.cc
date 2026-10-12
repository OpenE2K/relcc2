// SPDX-License-Identifier: GPL-3.0-or-later
// What kernel-backed control flow costs with lcc's runtime: setjmp and
// longjmp (glibc: the e2k_longjmp2 system call), C++ throw and rethrow
// (MCST libgcc's unwinder, which reads the hardware stacks with
// access_hw_stacks and lands with set_backtrace), _Unwind_Backtrace,
// glibc's backtrace, access_hw_stacks and get_backtrace
// (spec/abi/stack-and-unwinding.md, kernel-interface.md).  Mode 8
// (E2K_WRITE_PROCEDURE_STACK_EX) writes 8 bytes 1 KiB below the top of
// the procedure stack, in a frame of rec, with the value read from there
// first, as libgcc writes a landing pad's %db[0] and %db[1].  The reads
// are timed at the top of the stacks and also from offset 0 (the bottom
// 32 chain frames, the bottom 1 KiB), since the kernel walks the chain
// stack down from the top to the range it copies.
//
//   L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/l++  # 4C; v5.8c2, v6.2c3
//   $L -O2 ctlcost.cc -o ctlcost
//   ./ctlcost [CPU]           # %clkr cycles per operation, pinned to CPU
//                             # (default 0), median of 7 runs
//   ./ctlcost once CASE DEPTH # one operation between two getppid calls:
//   strace -o tr ./ctlcost once throw-d 10   # count its system calls
//
// "DEPTH frames up": the catching (or setjmp) function calls rec (DEPTH),
// which recurses down to rec (1), and rec (1) throws, longjmps or reads
// the stacks; so DEPTH is the number of frames the operation leaves.
// Variants with -d have a local with a destructor in every rec frame.

#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <execinfo.h>
#include <sched.h>
#include <setjmp.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unwind.h>
#include <asm/e2k_syswork.h>

static inline unsigned long
clkr ()
{
  unsigned long t;
  asm volatile ("rrd %%clkr, %0" : "=r" (t));
  return t;
}

enum
{
  M_RET, M_THROW, M_LONGJMP, M_SIGLONGJMP, M_ULONGJMP, M_INNER
};

enum
{
  I_UWBT, I_BT, I_BT10, I_GETBT10, I_HW4, I_HW5, I_HW6_32, I_HW6_ALL,
  I_HW7_1K, I_HW7_ALL, I_HW8, I_GETPPID, I_HW6_TOP1, I_HW6_BOT32,
  I_HW7_BOT1K
};

static volatile long sink;
static int g_mode, g_inner, g_once;
static long g_n;
static unsigned long g_cycles;
static long g_result;
static jmp_buf jb;
static sigjmp_buf sjb;

static long
hw (unsigned long mode, unsigned long long *off, void *buf,
    unsigned long size, void *real)
{
  return syscall (__NR_access_hw_stacks, mode, off, buf, size, real);
}

static _Unwind_Reason_Code
count_frame (struct _Unwind_Context *, void *arg)
{
  ++*(int *) arg;
  return _URC_NO_REASON;
}

static void *bt_buf[100000];
static char hw_buf[4 << 20];
static unsigned long long hw8_val;   // the word at PS - 1024 (I_HW8)

// One inner operation; returns something to print once.  No tail calls
// (lcc makes "return syscall (...)" one), so that every system call is
// made from a frame of inner_op called from rec (1).
static long __attribute__ ((noinline))
inner_op (int op, unsigned long long cs, unsigned long long ps)
{
  unsigned long long off, sz;
  long r = -1;
  int n = 0;
  switch (op)
    {
    case I_UWBT:
      _Unwind_Backtrace (count_frame, &n);
      r = n;
      break;
    case I_BT:
      r = backtrace (bt_buf, 100000);
      break;
    case I_BT10:
      r = backtrace (bt_buf, 10);
      break;
    case I_GETBT10:
      r = syscall (__NR_get_backtrace, bt_buf, 10UL, 0UL, 0UL);
      break;
    case I_HW4:
      hw (E2K_GET_CHAIN_STACK_SIZE, 0, 0, 0, &sz);
      r = sz;
      break;
    case I_HW5:
      hw (E2K_GET_PROCEDURE_STACK_SIZE, 0, 0, 0, &sz);
      r = sz;
      break;
    case I_HW6_32:
      sz = cs < 1024 ? cs : 1024;
      off = cs - sz;
      r = hw (E2K_READ_CHAIN_STACK_EX, &off, hw_buf, sz, 0);
      break;
    case I_HW6_ALL:
      off = 0;
      r = hw (E2K_READ_CHAIN_STACK_EX, &off, hw_buf, cs, 0);
      break;
    case I_HW7_1K:
      sz = ps < 1024 ? ps : 1024;
      off = ps - sz;
      r = hw (E2K_READ_PROCEDURE_STACK_EX, &off, hw_buf, sz, 0);
      break;
    case I_HW7_ALL:
      off = 0;
      r = hw (E2K_READ_PROCEDURE_STACK_EX, &off, hw_buf, ps, 0);
      break;
    case I_HW8:
      // Read once (cs == 1), then write the same value back.
      off = (ps - 1024) & ~7ULL;
      if (cs == 1)
        r = hw (E2K_READ_PROCEDURE_STACK_EX, &off, &hw8_val, 8, 0);
      else
        r = hw (E2K_WRITE_PROCEDURE_STACK_EX, &off, &hw8_val, 8, 0);
      break;
    case I_GETPPID:
      r = getppid ();
      break;
    case I_HW6_TOP1:
      // The top frame alone: the fixed cost of a chain read.
      off = cs - 32;
      r = hw (E2K_READ_CHAIN_STACK_EX, &off, hw_buf, 32, 0);
      break;
    case I_HW6_BOT32:
      // The bottom 32 frames: the kernel walks down from the top, so
      // this also passes every frame above them.
      sz = cs < 1024 ? cs : 1024;
      off = 0;
      r = hw (E2K_READ_CHAIN_STACK_EX, &off, hw_buf, sz, 0);
      break;
    case I_HW7_BOT1K:
      sz = ps < 1024 ? ps : 1024;
      off = 0;
      r = hw (E2K_READ_PROCEDURE_STACK_EX, &off, hw_buf, sz, 0);
      break;
    }
  sink++;
  return r;
}

// Inlined into rec, so that rec (1) itself throws or longjmps.
static inline __attribute__ ((always_inline)) void
act ()
{
  switch (g_mode)
    {
    case M_RET:
      return;
    case M_THROW:
      throw 1;
    case M_LONGJMP:
      longjmp (jb, 1);
    case M_SIGLONGJMP:
      siglongjmp (sjb, 1);
    case M_ULONGJMP:
      _longjmp (jb, 1);
    case M_INNER:
      if (g_once)
        {
          g_result = inner_op (g_inner, 0, 0);
          return;
        }
      {
        // The sizes as inner_op's system calls see them (same depth).
        unsigned long long cs = inner_op (I_HW4, 0, 0);
        unsigned long long ps = inner_op (I_HW5, 0, 0);
        if (g_inner == I_HW8)
          inner_op (I_HW8, 1, ps);   // read the word it writes back
        g_result = inner_op (g_inner, cs, ps);
        unsigned long t0 = clkr ();
        for (long i = 0; i < g_n; i++)
          inner_op (g_inner, cs, ps);
        g_cycles = clkr () - t0;
        return;
      }
    }
}

// rec (d) is d frames below its caller; rec (1) acts.
static void __attribute__ ((noinline))
rec (int d)
{
  if (d <= 1)
    act ();
  else
    rec (d - 1);
  sink++;   // no tail call
}

struct Dtor
{
  ~Dtor () { sink++; }
};

static void __attribute__ ((noinline))
rec_d (int d)
{
  Dtor x;
  if (d <= 1)
    act ();
  else
    rec_d (d - 1);
}

// catch (...) { throw; } one frame below the catcher.
static void __attribute__ ((noinline))
rethrower (int d)
{
  try
    {
      rec (d);
    }
  catch (...)
    {
      throw;
    }
  sink++;
}

static void __attribute__ ((noinline))
passer (int d)
{
  rec (d);
  sink++;
}

enum
{
  C_SETJMP, C_SIGSETJMP1, C_SETJMP_RET, C_LONGJMP, C_SIGLONGJMP,
  C_ULONGJMP, C_SIGSETJMP_RET, C_TRY_RET, C_THROW, C_TRY_RET_D, C_THROW_D,
  C_RETHROW, C_THROW_PASS, C_INNER
};

// N iterations of case C at DEPTH; total cycles.
static unsigned long __attribute__ ((noinline))
run (int c, int depth, long n)
{
  unsigned long t0 = clkr ();
  switch (c)
    {
    case C_SETJMP:
      for (long i = 0; i < n; i++)
        if (setjmp (jb) == 0)
          sink++;
      break;
    case C_SIGSETJMP1:
      for (long i = 0; i < n; i++)
        if (sigsetjmp (sjb, 1) == 0)
          sink++;
      break;
    case C_SETJMP_RET:
      g_mode = M_RET;
      for (long i = 0; i < n; i++)
        if (setjmp (jb) == 0)
          rec (depth);
      break;
    case C_SIGSETJMP_RET:
      g_mode = M_RET;
      for (long i = 0; i < n; i++)
        if (sigsetjmp (sjb, 1) == 0)
          rec (depth);
      break;
    case C_LONGJMP:
      g_mode = M_LONGJMP;
      for (long i = 0; i < n; i++)
        if (setjmp (jb) == 0)
          rec (depth);
      break;
    case C_ULONGJMP:
      g_mode = M_ULONGJMP;
      for (long i = 0; i < n; i++)
        if (_setjmp (jb) == 0)
          rec (depth);
      break;
    case C_SIGLONGJMP:
      g_mode = M_SIGLONGJMP;
      for (long i = 0; i < n; i++)
        if (sigsetjmp (sjb, 1) == 0)
          rec (depth);
      break;
    case C_TRY_RET:
      g_mode = M_RET;
      for (long i = 0; i < n; i++)
        try { rec (depth); } catch (int) { sink++; }
      break;
    case C_THROW:
      g_mode = M_THROW;
      for (long i = 0; i < n; i++)
        try { rec (depth); } catch (int) { sink++; }
      break;
    case C_TRY_RET_D:
      g_mode = M_RET;
      for (long i = 0; i < n; i++)
        try { rec_d (depth); } catch (int) { sink++; }
      break;
    case C_THROW_D:
      g_mode = M_THROW;
      for (long i = 0; i < n; i++)
        try { rec_d (depth); } catch (int) { sink++; }
      break;
    case C_RETHROW:
      g_mode = M_THROW;
      for (long i = 0; i < n; i++)
        try { rethrower (depth); } catch (int) { sink++; }
      break;
    case C_THROW_PASS:
      g_mode = M_THROW;
      for (long i = 0; i < n; i++)
        try { passer (depth); } catch (int) { sink++; }
      break;
    case C_INNER:
      g_mode = M_INNER;
      g_n = n;
      rec (depth);
      return g_cycles;
    }
  return clkr () - t0;
}

static int
cmp_double (const void *a, const void *b)
{
  double x = *(const double *) a, y = *(const double *) b;
  return x < y ? -1 : x > y;
}

// Median over 7 runs of cycles per operation.
static double
measure (int c, int depth, double *mn)
{
  run (c, depth, 4);   // warm up: lazy binding, exception memory
  unsigned long t = run (c, depth, 8);
  double per = t / 8.0;
  long n = per > 0 ? (long) (6e6 / per) : 100000;
  if (n < 16)
    n = 16;
  if (n > 200000)
    n = 200000;
  double r[7];
  for (int k = 0; k < 7; k++)
    r[k] = (double) run (c, depth, n) / n;
  qsort (r, 7, sizeof r[0], cmp_double);
  *mn = r[0];
  return r[3];
}

struct Case
{
  const char *name;
  int c, inner;
};

static const Case cases[] = {
  { "setjmp alone (= _setjmp)", C_SETJMP, 0 },
  { "sigsetjmp (env, 1) alone", C_SIGSETJMP1, 0 },
  { "setjmp + return through rec", C_SETJMP_RET, 0 },
  { "setjmp + longjmp", C_LONGJMP, 0 },
  { "_setjmp + _longjmp", C_ULONGJMP, 0 },
  { "sigsetjmp (env, 1) + return", C_SIGSETJMP_RET, 0 },
  { "sigsetjmp (env, 1) + siglongjmp", C_SIGLONGJMP, 0 },
  { "try + return (no throw)", C_TRY_RET, 0 },
  { "throw int, caught", C_THROW, 0 },
  { "try + return, dtor per frame", C_TRY_RET_D, 0 },
  { "throw int, dtor per frame", C_THROW_D, 0 },
  { "throw, caught+rethrown 1 up, caught 1 more up", C_RETHROW, 0 },
  { "throw caught 1 frame further up (rethrow control)", C_THROW_PASS, 0 },
  { "_Unwind_Backtrace (count frames)", C_INNER, I_UWBT },
  { "backtrace (buf, 100000)", C_INNER, I_BT },
  { "backtrace (buf, 10)", C_INNER, I_BT10 },
  { "get_backtrace (buf, 10, 0, 0)", C_INNER, I_GETBT10 },
  { "access_hw_stacks 4 GET_CHAIN_STACK_SIZE", C_INNER, I_HW4 },
  { "access_hw_stacks 5 GET_PROCEDURE_STACK_SIZE", C_INNER, I_HW5 },
  { "access_hw_stacks 6 READ_CHAIN_STACK_EX, top 32 frames", C_INNER, I_HW6_32 },
  { "access_hw_stacks 6 READ_CHAIN_STACK_EX, whole", C_INNER, I_HW6_ALL },
  { "access_hw_stacks 7 READ_PROCEDURE_STACK_EX, top 1 KiB", C_INNER, I_HW7_1K },
  { "access_hw_stacks 7 READ_PROCEDURE_STACK_EX, whole", C_INNER, I_HW7_ALL },
  { "access_hw_stacks 6 READ_CHAIN_STACK_EX, top frame", C_INNER, I_HW6_TOP1 },
  { "access_hw_stacks 6 READ_CHAIN_STACK_EX, bottom 32 frames", C_INNER,
    I_HW6_BOT32 },
  { "access_hw_stacks 7 READ_PROCEDURE_STACK_EX, bottom 1 KiB", C_INNER,
    I_HW7_BOT1K },
  { "access_hw_stacks 8 WRITE_PROCEDURE_STACK_EX, 8 bytes", C_INNER, I_HW8 },
  { "getppid (reference system call)", C_INNER, I_GETPPID },
};

static const char *
find_case (const char *s, int *c, int *inner)
{
  static const struct { const char *s; int c, inner; } names[] = {
    { "longjmp", C_LONGJMP, 0 }, { "siglongjmp", C_SIGLONGJMP, 0 },
    { "throw", C_THROW, 0 }, { "throw-d", C_THROW_D, 0 },
    { "rethrow", C_RETHROW, 0 }, { "throw-pass", C_THROW_PASS, 0 },
    { "uwbt", C_INNER, I_UWBT }, { "bt", C_INNER, I_BT },
    { "getbt", C_INNER, I_GETBT10 },
  };
  for (auto &n : names)
    if (!strcmp (s, n.s))
      {
        *c = n.c;
        *inner = n.inner;
        return n.s;
      }
  return 0;
}

int
main (int argc, char **argv)
{
  if (argc >= 4 && !strcmp (argv[1], "once"))
    {
      int c, inner;
      if (!find_case (argv[2], &c, &inner))
        return 2;
      int depth = atoi (argv[3]);
      g_inner = inner;
      run (c, depth, 2);
      g_once = 1;
      getppid ();
      run (c, depth, 1);
      getppid ();
      return 0;
    }

  int cpu = argc > 1 ? atoi (argv[1]) : 0;
  cpu_set_t set;
  CPU_ZERO (&set);
  CPU_SET (cpu, &set);
  if (sched_setaffinity (0, sizeof set, &set))
    perror ("sched_setaffinity");

  // How many frames each depth really is (no inlining of rec).
  g_inner = I_HW4;
  for (int d : { 1, 10, 100 })
    {
      run (C_INNER, d, 1);
      printf ("depth %3d: chain stack %ld bytes (%ld frames) at rec (1)\n",
              d, g_result, g_result / 32);
    }
  g_inner = I_UWBT;
  for (int d : { 10, 100 })
    {
      run (C_INNER, d, 1);
      printf ("depth %3d: _Unwind_Backtrace sees %ld frames\n", d, g_result);
    }
  g_inner = I_BT;
  for (int d : { 10, 100 })
    {
      run (C_INNER, d, 1);
      printf ("depth %3d: backtrace returns %ld\n", d, g_result);
    }
  for (int op : { I_GETBT10, I_HW5, I_HW6_32, I_HW6_ALL, I_HW7_1K, I_HW7_ALL,
                  I_HW8, I_HW6_TOP1, I_HW6_BOT32, I_HW7_BOT1K })
    for (int d : { 10, 100 })
      {
        g_inner = op;
        errno = 0;
        run (C_INNER, d, 1);
        printf ("depth %3d: inner op %d returns %ld (errno %d)\n", d, op,
                g_result, errno);
      }

  printf ("%%clkr cycles per operation on CPU %d: median of 7 runs (min)\n",
          cpu);
  for (const Case &k : cases)
    {
      g_inner = k.inner;
      bool depthless = k.c == C_SETJMP || k.c == C_SIGSETJMP1;
      for (int d : { 1, 10, 100 })
        {
          if (depthless && d > 1)
            break;
          if (k.c == C_INNER && d == 1
              && k.inner != I_HW4 && k.inner != I_HW5 && k.inner != I_GETPPID)
            continue;
          double mn, med = measure (k.c, d, &mn);
          if (depthless)
            printf ("  %-55s        %9.0f (%.0f)\n", k.name, med, mn);
          else
            printf ("  %-55s d=%3d  %9.0f (%.0f)\n", k.name, d, med, mn);
          fflush (stdout);
        }
    }
  return 0;
}
