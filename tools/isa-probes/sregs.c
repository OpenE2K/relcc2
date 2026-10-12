/* SPDX-License-Identifier: GPL-3.0-or-later */
/* User-mode state registers and the 80-bit FP status (spec/abi/registers.md,
   "Floating-point control and status"; spec/isa/semantics.md, "80-bit
   operations"): what rrs and rrd of %pfpfr, %fpcr, %fpsr, %upsr, %idr,
   %clkr, %core_mode and %ip write under setwd dbl = 0 and 1; which bits of
   the three FP registers take a write; %core_mode with and without ,sm;
   %idr on every CPU against sched_getcpu and getcpu, and what each costs;
   whether rrd %clkr waits for earlier operations; when the %pfpfr bits
   26-31, %fpcr.ic and %fpsr es, c1 and bf change, and which signal an
   unmasked FP exception gives; which 80-bit operations write %fpsr.c1
   (for check_sregs.c); and the fxdivt* operations (their results, for
   check_sregs.c on an x86 host, and their latency, also by the gap
   method, with the edges between the 80-bit unit and ordinary FP
   operations, and from a load or an integer operation); %fpsr es and bf
   under one unmasked exception at a time; C1 after fxdivt* and their
   ,sm forms with C1 and the flags preset.  sregs.s is hand-written.
   Cases that may trap run in their own process.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc
     $LCC -O2 tools/isa-probes/sregs.c tools/isa-probes/sregs.s -o sregs
     ./sregs [width|write|idr|clkr|fp|dt|c1|lat|latg|dc|lp] > out.txt
     $LCC -O2 -fPIE -pie tools/isa-probes/sregs.c tools/isa-probes/sregs.s \
       -o sregs-pie; ./sregs-pie width    # %ip above 4 GiB, for rrs %ip
     cc -O1 tools/isa-probes/check_sregs.c -o check_sregs  # x86 host
     ./check_sregs < out.txt         # the "dt" and "c1" lines against x87

   Output lines: "width NAME: VALUE tag TAG" (or the signal), "write
   NAME VAL: rrd R rrs S", "es2 fpcr C fpsr written W: reads R", "idr
   cpu C: ...", "clkr NAME: min M median D", "fp OP SET ...", "dt OP
   FPCR A Y.M Y.E -> R.LO R.HI FPSR" and "c1 OP PRESET A.M A.E B.M B.E ->
   R.LO R.HI FPSR" in hex, "lat NAME: C cycles per operation", "latg
   NAME gap G: C cycles per step" (also "lp"), "dc OP pre P A / Y.M Y.E
   -> R fpsr F" and a summary per operation.  The idr, clkr, lat, latg
   and lp sections time with %clkr on CPU 0: wrap them in flock on a
   shared machine.  */

#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/rseq.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <asm/e2k_api.h>
#include <asm/unistd.h>

typedef unsigned long u64;

/* Signal handling in the forked children: print the signal, its code,
   si_addr relative to the function under test and the FP registers as
   the handler finds them, then exit.  */
void sr_rdfp (u64 *);
static void *cur_fn;

static void
handler (int s, siginfo_t *si, void *uc)
{
  u64 fp[3];
  char b[160];
  (void) uc;
  sr_rdfp (fp);
  int n = snprintf (b, sizeof b, "signal %d code %d addr %+ld; in handler "
                    "pfpfr %08lx fpcr %04lx fpsr %04lx\n", s, si->si_code,
                    (long) ((char *) si->si_addr - (char *) cur_fn),
                    fp[0] & 0xffffffff, fp[1] & 0xffff, fp[2] & 0xffff);
  write (1, b, n);
  _exit (0);
}

static void
catch (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGILL, &sa, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);
  sigaction (SIGFPE, &sa, NULL);
}

/* Run F (ARG) in a child; the parent reports a death by signal.  */
static void
in_child (void (*f) (void *), void *arg)
{
  fflush (stdout);
  pid_t p = fork ();
  if (p == 0)
    {
      catch ();
      f (arg);
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (p, &st, 0);
  if (WIFSIGNALED (st))
    printf ("killed by signal %d\n", WTERMSIG (st));
}

/* 1. Reads: what rrs and rrd write.  */
#define RDS(X) X (rrs_pfpfr_0) X (rrs_pfpfr_1) X (rrd_pfpfr) \
  X (rrs_fpcr_0) X (rrs_fpcr_1) X (rrd_fpcr) X (rrs_fpsr_0) X (rrs_fpsr_1) \
  X (rrd_fpsr) X (rrs_upsr_0) X (rrs_upsr_1) X (rrd_upsr) X (rrs_idr_0) \
  X (rrs_idr_1) X (rrd_idr) X (rrs_clkr_0) X (rrs_clkr_1) X (rrd_clkr) \
  X (rrs_ip_0) X (rrs_ip_1) X (rrd_ip) X (rrs_core_mode_0) \
  X (rrs_core_mode_1) X (rrd_core_mode) X (rrs_core_mode_sm_0) \
  X (rrs_core_mode_sm_1) X (rrd_core_mode_sm)
#define DECL(n) void sr_##n (u64 *);
RDS (DECL)
#undef DECL

struct rd { const char *name; void (*f) (u64 *); };

static void
rd_one (void *a)
{
  const struct rd *r = a;
  u64 out[2];
  printf ("width %-20s ", r->name);
  fflush (stdout);
  cur_fn = (void *) r->f;
  r->f (out);
  printf ("%016lx tag %lx", out[0], out[1] & 0xff);
  if (strstr (r->name, "_ip"))
    printf ("  (function at %016lx: value - function %+ld, low 32 bits "
            "- function's %+ld)", (u64) r->f, (long) (out[0] - (u64) r->f),
            (long) ((unsigned) out[0] - (unsigned) (u64) r->f));
  printf ("\n");
}

static void
width (void)
{
#define ENTRY(n) { #n, sr_##n },
  static const struct rd t[] = { RDS (ENTRY) };
#undef ENTRY
  printf ("# rrs/rrd into a register preset to 1234567887654321; "
          "_0/_1: setwd dbl\n");
  for (unsigned i = 0; i < sizeof t / sizeof *t; i++)
    in_child (rd_one, (void *) &t[i]);
}

/* 2. Writes: which bits of %pfpfr, %fpcr, %fpsr take a write.  */
#define WRS(X) X (rws_pfpfr) X (rwd_pfpfr) X (rws_fpcr) X (rwd_fpcr) \
  X (rws_fpsr) X (rwd_fpsr)
#define DECL(n) void sr_w_##n (u64, u64 *);
WRS (DECL)
#undef DECL

struct wr { const char *name; void (*f) (u64, u64 *); u64 v; };

static void
wr_one (void *a)
{
  const struct wr *w = a;
  u64 out[2];
  printf ("write %-10s %016lx: ", w->name, w->v);
  fflush (stdout);
  cur_fn = (void *) w->f;
  w->f (w->v, out);
  printf ("rrd %016lx rrs %016lx\n", out[0], out[1]);
}

void sr_es (u64 *);
u64 sr_es2 (u64, u64);

static void
es_one (void *a)
{
  u64 out[3];
  (void) a;
  cur_fn = (void *) sr_es;
  sr_es (out);
  printf ("es: %%fpsr after writing %%fpcr 0x340 and %%fpsr 0x3f: %04lx; "
          "then %%fpcr 0x37f: %04lx; then %%fpcr 0x35f: %04lx\n",
          out[0] & 0xffff, out[1] & 0xffff, out[2] & 0xffff);
}

static void
write_masks (void)
{
  static const struct wr t[] = {
    { "rws_pfpfr", sr_w_rws_pfpfr, 0x1f80 },
    { "rws_pfpfr", sr_w_rws_pfpfr, 0xfc00ffff },
    { "rws_pfpfr", sr_w_rws_pfpfr, 0x03ff0000 },
    { "rws_pfpfr", sr_w_rws_pfpfr, 0 },
    { "rwd_pfpfr", sr_w_rwd_pfpfr, 0xffffffff00001f80 },
    { "rws_fpcr", sr_w_rws_fpcr, 0x37f },
    { "rws_fpcr", sr_w_rws_fpcr, 0xffff },
    { "rws_fpcr", sr_w_rws_fpcr, 0xffff0000 },
    { "rws_fpcr", sr_w_rws_fpcr, 0 },
    { "rwd_fpcr", sr_w_rwd_fpcr, 0xffffffff0000037f },
    { "rws_fpsr", sr_w_rws_fpsr, 0x3f },
    { "rws_fpsr", sr_w_rws_fpsr, 0xffff },
    { "rws_fpsr", sr_w_rws_fpsr, 0xffff0000 },
    { "rws_fpsr", sr_w_rws_fpsr, 0 },
    { "rws_fpsr", sr_w_rws_fpsr, 0x40 },
    { "rws_fpsr", sr_w_rws_fpsr, 0x7f00 },
    { "rwd_fpsr", sr_w_rwd_fpsr, 0xffffffff0000003f },
  };
  for (unsigned i = 0; i < sizeof t / sizeof *t; i++)
    in_child (wr_one, (void *) &t[i]);
  in_child (es_one, NULL);
  /* One exception unmasked at a time, its flag set or not; bits 6, 7,
     9 and 15 written alone.  No FP operation runs, so nothing traps.  */
  static const u64 e2[][2] = {
    { 0x35f, 0x1f }, { 0x35f, 0x20 }, { 0x37e, 0x01 }, { 0x37e, 0x3e },
    { 0x37d, 0x02 }, { 0x37d, 0x3d }, { 0x37b, 0x04 }, { 0x377, 0x08 },
    { 0x36f, 0x10 }, { 0x340, 0x00 }, { 0x340, 0x200 }, { 0x37f, 0x40 },
    { 0x340, 0x40 }, { 0x37f, 0x3f }, { 0x340, 0x8000 }, { 0x340, 0x80 },
  };
  for (unsigned i = 0; i < sizeof e2 / sizeof *e2; i++)
    printf ("es2 fpcr %04lx fpsr written %04lx: reads %04lx\n", e2[i][0],
            e2[i][1], sr_es2 (e2[i][0], e2[i][1]) & 0xffff);
}

/* 3. %idr on every CPU; the cost of reading it.  */
long sr_rdidr (void);
long sr_rdclkr (void);
long sr_empty (void);
char *sr_tp (void);

/* The cpu_id field of glibc's rseq area (glibc 2.35 registers one per
   thread when the kernel has rseq), or -1.  */
static int
rseq_cpu (void)
{
  if (__rseq_size == 0)
    return -1;
  return ((volatile struct rseq *) (sr_tp () + __rseq_offset))->cpu_id;
}
long sr_lat_idr16 (void), sr_lat_idr48 (void);
long sr_lat_clkr16 (void), sr_lat_clkr48 (void);

static long
fast_getcpu (unsigned *cpu, unsigned *node)
{
  return __E2K_SYSCALL_2 (LINUX_FAST_SYSCALL64_TRAPNUM, 2, cpu, node);
}

static int
cmpl (const void *a, const void *b)
{
  long x = *(const long *) a, y = *(const long *) b;
  return x < y ? -1 : x > y;
}

/* Minimum and median of K runs of F.  */
static void
minmed (long (*f) (void), int k, long *mn, long *md)
{
  long v[201];
  for (int i = 0; i < k; i++)
    v[i] = f ();
  qsort (v, k, sizeof *v, cmpl);
  *mn = v[0];
  *md = v[k / 2];
}

static double
slope (long (*f16) (void), long (*f48) (void))
{
  long a, b, m;
  minmed (f16, 51, &a, &m);
  minmed (f48, 51, &b, &m);
  return (b - a) / 32.0;
}

static void
idr (void)
{
  int n = sysconf (_SC_NPROCESSORS_ONLN);
  printf ("# %%idr fields (kernel e2k_idr_t): mdl 0-7, rev 8-11, wbl 12-14, "
          "core 15-19, pn 20-23, hw_virt 24, hw_virt_ver 25-28\n");
  for (int c = 0; c < n; c++)
    {
      cpu_set_t s;
      CPU_ZERO (&s);
      CPU_SET (c, &s);
      if (sched_setaffinity (0, sizeof s, &s))
        {
          printf ("idr cpu %d: setaffinity failed\n", c);
          continue;
        }
      sched_yield ();
      u64 v = sr_rdidr ();
      int g = sched_getcpu ();
      unsigned fc = -1, fn = -1, sc = -1, sn = -1;
      long fr = fast_getcpu (&fc, &fn);
      syscall (SYS_getcpu, &sc, &sn, NULL);
      u64 v2 = sr_rdidr ();
      printf ("idr cpu %2d: %%idr %08lx (mdl %#lx rev %lu wbl %lu core %lu "
              "pn %lu hw_virt %lu ver %lu, rest %#lx)%s; sched_getcpu %d; "
              "fast getcpu %ld cpu %u node %u; getcpu syscall cpu %u "
              "node %u; rseq cpu_id %d\n", c, v, v & 0xff, (v >> 8) & 0xf,
              (v >> 12) & 7, (v >> 15) & 0x1f, (v >> 20) & 0xf, (v >> 24) & 1,
              (v >> 25) & 0xf, v >> 29, v2 == v ? "" : " (changed)",
              g, fr, fc, fn, sc, sn, rseq_cpu ());
    }
  /* Costs on CPU 0, cycles per call.  */
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (0, &s);
  sched_setaffinity (0, sizeof s, &s);
  const long k = 100000;
  long t[7], acc = 0;
  unsigned cc, nn;
  t[0] = sr_rdclkr ();
  for (long i = 0; i < k; i++)
    acc += sr_empty ();
  t[1] = sr_rdclkr ();
  for (long i = 0; i < k; i++)
    acc += sr_rdidr ();
  t[2] = sr_rdclkr ();
  for (long i = 0; i < k; i++)
    acc += sched_getcpu ();
  t[3] = sr_rdclkr ();
  for (long i = 0; i < k; i++)
    acc += fast_getcpu (&cc, &nn) + cc;
  t[4] = sr_rdclkr ();
  for (long i = 0; i < k; i++)
    acc += syscall (SYS_getcpu, &cc, &nn, NULL) + cc;
  t[5] = sr_rdclkr ();
  char *rs = sr_tp () + __rseq_offset;
  for (long i = 0; i < k; i++)
    acc += __rseq_size ? ((volatile struct rseq *) rs)->cpu_id : 0;
  t[6] = sr_rdclkr ();
  printf ("idr cost: cycles per call in a loop of %ld (with the loop): "
          "empty asm function %.1f, function returning rrd %%idr %.1f, "
          "sched_getcpu () %.1f, fast call 2 (getcpu) %.1f, getcpu system "
          "call %.1f, load of rseq cpu_id (rseq size %u) %.1f (sum %ld)\n",
          k, (double) (t[1] - t[0]) / k,
          (double) (t[2] - t[1]) / k, (double) (t[3] - t[2]) / k,
          (double) (t[4] - t[3]) / k, (double) (t[5] - t[4]) / k,
          __rseq_size, (double) (t[6] - t[5]) / k, acc & 1);
  printf ("idr cost: back-to-back rrd %%idr %.2f cycles each, rrd %%clkr "
          "%.2f (16 and 48 in a row, slope)\n",
          slope (sr_lat_idr16, sr_lat_idr48),
          slope (sr_lat_clkr16, sr_lat_clkr48));
}

/* 4. Does rrd %clkr wait for earlier operations?  */
#define CKS(X) X (back) X (fdiv) X (fdiv_use) X (fdiv8) X (fdiv8_use) \
  X (fdiv_use_1) X (fdiv_use_2) X (fdiv_use_3) X (fdiv_use_nop7) X (ld) \
  X (ld_use) X (rd_idr) X (rd_idr_base) X (rd_clkr) X (rd_clkr_base) \
  X (rd_pfpfr) X (rd_pfpfr_base) X (rd_core_mode) X (rd_core_mode_base) \
  X (rd_idr_gap2) X (rd_idr_gap3) X (rd_idr_gap4) X (rd_idr_gap5) \
  X (rd_clkr_gap2) X (rd_clkr_gap3) X (rd_clkr_gap4) X (rd_clkr_gap5) \
  X (rd_core_mode_gap2) X (rd_core_mode_gap3) X (rd_core_mode_gap4) \
  X (rd_core_mode_gap5) X (fdiv_use_n1) X (fdiv_use_n2) X (fdiv_use_e1)
#define DECL(n) long sr_ck_##n (u64 *);
CKS (DECL)
#undef DECL

static void
clkr (void)
{
#define ENTRY(n) { #n, sr_ck_##n },
  static const struct { const char *name; long (*f) (u64 *); } t[] = {
    CKS (ENTRY)
  };
#undef ENTRY
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (0, &s);
  sched_setaffinity (0, sizeof s, &s);
  /* A 256 MiB buffer for loads that miss every cache.  */
  const size_t sz = 256ul << 20;
  u64 *big = malloc (sz);
  for (size_t i = 0; i < sz / 8; i += 512)
    big[i] = i;
  printf ("# two rrd %%clkr with the operations named between them; "
          "_use: an addd reads the result before the second read (_use_N: "
          "then N more instructions, _use_nop7: then a nop 7; _use_n1, _n2: "
          "the addd with nop 1 or 2; _use_e1: then a nop 1 instruction); "
          "ld: a load "
          "from a random line of 256 MiB; rd_R: a read of R, an addd of the "
          "result and two more instructions (_base: the addd reads another "
          "register; _gapN: the addd N cycles after the read)\n");
  for (unsigned i = 0; i < sizeof t / sizeof *t; i++)
    {
      long v[201];
      unsigned r = 12345;
      for (int j = 0; j < 201; j++)
        {
          u64 *p = big + 8 * ((r = r * 1103515245 + 12345) % (sz / 64));
          u64 q[2] = { 0, 0 };
          v[j] = t[i].f (t[i].name[0] == 'l' ? p : q);
        }
      qsort (v, 201, sizeof *v, cmpl);
      printf ("clkr %-10s: min %ld median %ld max %ld\n", t[i].name, v[0],
              v[100], v[200]);
    }
  free (big);
}

/* 5. FP status.  */
#define FPS(X) X (faddd) X (faddd_sm) X (fmuld) X (fmuld_sm) X (fdivd) \
  X (fdivd_sm) X (fxaddxx) X (fxaddxx_sm) X (fxmulxx) X (fxmulxx_sm) \
  X (fxdivxx) X (fxdivxx_sm) X (fxdivtsx) X (fxdivtsx_sm) X (fxcmpudxf) \
  X (fxcmpodxf) X (pfadds) X (pfadds_sm) X (fdtoid) X (fdtoid_sm) \
  X (faddd_use) X (fxaddxx_use) X (fxdivtss)
#define DECL(n) void sr_f_##n (const u64 *, u64 *, u64, u64, u64);
FPS (DECL)
#undef DECL

/* An 80-bit value from a long double (lcc: x87 extended, 16 bytes).  */
static void
x80 (long double v, u64 *m, u64 *e)
{
  unsigned char b[16];
  memcpy (b, &v, sizeof v < 16 ? sizeof v : 16);
  memcpy (m, b, 8);
  *e = b[8] | (b[9] << 8);
}

static u64
dbits (double d)
{
  u64 u;
  memcpy (&u, &d, 8);
  return u;
}

static u64
fbits (float f)
{
  unsigned u;
  memcpy (&u, &f, 4);
  return u;
}

struct fpcase { const char *op; void (*f) (const u64 *, u64 *, u64, u64, u64);
                char kind; const char *label; long double a, b; };
struct fpset { const char *name; u64 pfpfr, fpcr, fpsr; };
struct fprun { const struct fpcase *c; const struct fpset *s; };

static void
fp_one (void *arg)
{
  const struct fprun *r = arg;
  const struct fpcase *c = r->c;
  u64 in[4] = { 0, 0, 0, 0 }, out[6];
  if (c->kind == 'd')
    {
      in[0] = dbits (c->a);
      in[2] = dbits (c->b);
    }
  else if (c->kind == 'p')
    {
      in[0] = fbits (c->a) | fbits (c->a) << 32;
      in[2] = fbits (c->b) | fbits (c->b) << 32;
    }
  else
    {
      if (c->kind == 't')
        in[0] = fbits (c->a);
      else
        x80 (c->a, &in[0], &in[1]);
      x80 (c->b, &in[2], &in[3]);
    }
  printf ("fp %-11s %-4s %-6s: ", c->op, r->s->name, c->label);
  fflush (stdout);
  cur_fn = (void *) c->f;
  c->f (in, out, r->s->pfpfr, r->s->fpcr, r->s->fpsr);
  printf ("%016lx tag %lx ext %04lx; pfpfr %08lx fpsr %04lx fpcr %04lx\n",
          out[0], out[1] & 0xff, out[5] & 0xffff, out[2] & 0xffffffff,
          out[3] & 0xffff, out[4] & 0xffff);
}

static void
fp (void)
{
  const long double inf = __builtin_infl (), nan = __builtin_nanl ("");
  const long double maxx = 1.18973149535723176502e+4932L;
  const long double tinyx = 3.36210314311209350626e-4932L;  /* min normal */
  const long double denx = 3.64519953188247460253e-4951L;   /* min denormal */
  const double maxd = 1.7976931348623157e308, tinyd = 1e-200, dend = 4.9e-324;
  static struct fpcase cs[128];
  int n = 0;
#define C(o, k, l, a, b) cs[n++] = (struct fpcase) { #o, sr_f_##o, k, l, a, b };
  C (faddd, 'd', "exact", 1, 1) C (faddd, 'd', "ie", inf, -inf)
  C (faddd, 'd', "pe", 1, 0x1p-70) C (faddd, 'd', "oe", maxd, maxd)
  C (faddd, 'd', "de", 1, dend)
  C (faddd_sm, 'd', "exact", 1, 1) C (faddd_sm, 'd', "ie", inf, -inf)
  C (faddd_sm, 'd', "pe", 1, 0x1p-70) C (faddd_sm, 'd', "oe", maxd, maxd)
  C (faddd_sm, 'd', "de", 1, dend)
  C (fmuld, 'd', "ue", tinyd, tinyd) C (fmuld_sm, 'd', "ue", tinyd, tinyd)
  C (fdivd, 'd', "ze", 1, 0) C (fdivd, 'd', "pe", 1, 3)
  C (fdivd_sm, 'd', "ze", 1, 0) C (fdivd_sm, 'd', "pe", 1, 3)
  C (fxaddxx, 'x', "exact", 1, 1) C (fxaddxx, 'x', "ie", inf, -inf)
  C (fxaddxx, 'x', "pe", 1, 0x1p-70) C (fxaddxx, 'x', "oe", maxx, maxx)
  C (fxaddxx, 'x', "de", 1, denx) C (fxaddxx, 'x', "inf+1", inf, 1)
  C (fxaddxx_sm, 'x', "exact", 1, 1) C (fxaddxx_sm, 'x', "ie", inf, -inf)
  C (fxaddxx_sm, 'x', "pe", 1, 0x1p-70) C (fxaddxx_sm, 'x', "oe", maxx, maxx)
  C (fxaddxx_sm, 'x', "de", 1, denx)
  C (fxaddxx, 'x', "rnddn", 1, 0x1p-70)
  C (fxmulxx, 'x', "tiny", tinyx, 0.5L) C (fxmulxx, 'x', "ue", tinyx, 1.0L / 3)
  C (fxmulxx, 'x', "rnddn", 3, 1.0L / 3)
  C (fxmulxx_sm, 'x', "tiny", tinyx, 0.5L) C (fxmulxx_sm, 'x', "ue", tinyx, 1.0L / 3)
  C (fxdivxx, 'x', "ze", 1, 0) C (fxdivxx, 'x', "pe", 1, 3)
  C (fxdivxx, 'x', "rndup", 2, 3) C (fxdivxx, 'x', "-ze", 1, -0.0L)
  C (fxdivxx_sm, 'x', "ze", 1, 0) C (fxdivxx_sm, 'x', "pe", 1, 3)
  C (fxdivtsx, 't', "exact", 1, 2) C (fxdivtsx, 't', "ze", 1, 0)
  C (fxdivtsx, 't', "pe", 1, 3) C (fxdivtsx, 't', "ie", 0, 0)
  C (fxdivtsx_sm, 't', "ze", 1, 0) C (fxdivtsx_sm, 't', "pe", 1, 3)
  C (fxcmpudxf, 'x', "1<2", 1, 2) C (fxcmpudxf, 'x', "inf>1", inf, 1)
  C (fxcmpudxf, 'x', "-inf<inf", -inf, inf) C (fxcmpudxf, 'x', "nan", nan, 1)
  C (fxcmpodxf, 'x', "inf>1", inf, 1) C (fxcmpodxf, 'x', "-inf<inf", -inf, inf)
  C (fxcmpodxf, 'x', "nan", nan, 1)
  C (pfadds, 'p', "pe", 1, 0x1p-30) C (pfadds, 'p', "ie", inf, -inf)
  C (pfadds_sm, 'p', "pe", 1, 0x1p-30) C (pfadds_sm, 'p', "ie", inf, -inf)
  C (fdtoid, 'd', "ie", 1e300, 0) C (fdtoid, 'd', "pe", 1.5, 0)
  C (fdtoid_sm, 'd', "ie", 1e300, 0) C (fdtoid_sm, 'd', "pe", 1.5, 0)
  /* _use: faddd,sm or fxaddxx,sm, then the same operation without ,sm
     on its result.  */
  C (faddd_use, 'd', "exact", 1, 1) C (faddd_use, 'd', "ie", inf, -inf)
  C (faddd_use, 'd', "pe", 1, 0x1p-70)
  C (fxaddxx_use, 'x', "exact", 1, 1) C (fxaddxx_use, 'x', "ie", inf, -inf)
  C (fxaddxx_use, 'x', "pe", 1, 0x1p-70)
  C (fxdivtss, 't', "pe", 1, 3) C (fxdivtss, 't', "exact", 1, 2)
#undef C
  /* pfpfr, fpcr, fpsr: masked with flags clear (M0) or set as at process
     start (M1); unmasked in %pfpfr (Up), in %fpcr (Uc) or both (Upc);
     %fpcr.ic set (IC); %fpsr.es set (ES); flags set and unmasked
     beforehand (STp in %pfpfr, STc in %fpcr/%fpsr); %fpsr.c1 set
     beforehand (C1, and with the flags C1s); %pfpfr bits 26-31 set,
     masked (D) or unmasked (Dp); %pfpfr unmasked while %fpsr holds an
     unmasked invalid flag (UpX: which register the kernel's si_code
     comes from).  */
  static const struct fpset ss[] = {
    { "M0", 0x1f80, 0x37f, 0 }, { "M1", 0x1fbf, 0x37f, 0x3f },
    { "Up", 0x0000, 0x37f, 0 }, { "Uc", 0x1f80, 0x340, 0 },
    { "Upc", 0x0000, 0x340, 0 }, { "IC", 0x1f80, 0x137f, 0 },
    { "ES", 0x1f80, 0x37f, 0x80 }, { "STp", 0x003f, 0x37f, 0 },
    { "STc", 0x1f80, 0x340, 0x3f }, { "C1", 0x1f80, 0x37f, 0x200 },
    { "C1s", 0x1fbf, 0x37f, 0x23f }, { "D", 0xfc001f80, 0x37f, 0 },
    { "Dp", 0xfc000000, 0x37f, 0 }, { "UpX", 0x0000, 0x340, 0x001 },
  };
  printf ("# settings (pfpfr fpcr fpsr):");
  for (unsigned j = 0; j < sizeof ss / sizeof *ss; j++)
    printf (" %s %04lx %04lx %03lx", ss[j].name, ss[j].pfpfr, ss[j].fpcr,
            ss[j].fpsr);
  printf ("\n# addr is si_addr minus the function's address\n");
  for (int i = 0; i < n; i++)
    for (unsigned j = 0; j < sizeof ss / sizeof *ss; j++)
      {
        struct fprun r = { &cs[i], &ss[j] };
        in_child (fp_one, &r);
      }
}

/* 6. fxdivt*: src1 a single or double, src2 80-bit.  */
void sr_dt_fxdivtss (const u64 *, u64 *, u64);
void sr_dt_fxdivtdd (const u64 *, u64 *, u64);
void sr_dt_fxdivtsx (const u64 *, u64 *, u64);
void sr_dt_fxdivtdx (const u64 *, u64 *, u64);

static unsigned long rs = 88172645463325252UL;

static u64
rnd (void)
{
  rs ^= rs << 13;
  rs ^= rs >> 7;
  rs ^= rs << 17;
  return rs;
}

static void
dt (void)
{
  static const struct { const char *name; void (*f) (const u64 *, u64 *, u64);
                        char a, r; } ops[] = {
    { "fxdivtss", sr_dt_fxdivtss, 's', 's' },
    { "fxdivtdd", sr_dt_fxdivtdd, 'd', 'd' },
    { "fxdivtsx", sr_dt_fxdivtsx, 's', 'x' },
    { "fxdivtdx", sr_dt_fxdivtdx, 'd', 'x' },
  };
  static const double as[] = {
    0.0, -0.0, 1.0, -1.5, 3.0, 0.1, 1e30, 1e300, __builtin_inf (),
    -__builtin_inf (), __builtin_nan (""), 4.9e-324, 1.4e-45,
    1.7976931348623157e308, 2.2250738585072014e-308, 1.1754943508222875e-38,
  };
  static const u64 ys[][2] = {
    { 0, 0 }, { 0, 0x8000 }, { 0x8000000000000000UL, 0x3fff },
    { 0xc000000000000000UL, 0x4000 }, { 0xcccccccccccccccdUL, 0xbffb },
    { 0x8000000000000000UL, 0x7fff }, { 0x8000000000000000UL, 0xffff },
    { 0xc000000000000000UL, 0x7fff }, { 0xa000000000000000UL, 0x7fff },
    { 0x0000000000000001UL, 0 }, { 0x8000000000000001UL, 0 },
    { 0x4000000000000000UL, 0x3fff }, { 0x0000000000000000UL, 0x7fff },
    { 0xffffffffffffffffUL, 0x7ffe }, { 0x8000000000000000UL, 0x0001 },
    { 0x8000000000000000UL, 0x3fff - 16000 }, { 0x8000000000000000UL, 0x3fbf },
    { 0xaaaaaaaaaaaaaaabUL, 0x3ffd },
  };
  static const u64 cws[] = { 0x37f, 0x77f, 0xb7f, 0xf7f, 0x27f, 0x07f };
  for (unsigned o = 0; o < 4; o++)
    for (unsigned c = 0; c < sizeof cws / sizeof *cws; c++)
      {
        rs = 88172645463325252UL;
        unsigned na = sizeof as / sizeof *as, ny = sizeof ys / sizeof *ys;
        for (unsigned k = 0; k < na * ny + 400; k++)
          {
            u64 in[4], out[3];
            if (k < na * ny)
              {
                double a = as[k / ny];
                in[0] = ops[o].a == 's' ? fbits ((float) a) : dbits (a);
                in[2] = ys[k % ny][0];
                in[3] = ys[k % ny][1];
              }
            else
              {
                /* Random: a with a random significand and an exponent
                   within 2^+-40, y likewise (integer bit set).  */
                u64 r1 = rnd (), r2 = rnd (), r3 = rnd ();
                if (ops[o].a == 's')
                  in[0] = (r1 & 0x807fffff) | ((u64) (127 - 40 + r3 % 81) << 23);
                else
                  in[0] = (r1 & 0x800fffffffffffffUL)
                          | ((u64) (1023 - 40 + r3 % 81) << 52);
                in[2] = r2 | 0x8000000000000000UL;
                in[3] = (0x3fff - 40 + (r3 >> 8) % 81) | ((r3 >> 20) & 0x8000);
              }
            in[1] = 0;
            ops[o].f (in, out, cws[c]);
            u64 r = out[0];
            if (ops[o].r == 's')
              r &= 0xffffffff;
            printf ("dt %s %03lx %016lx %016lx %04lx -> %016lx %04lx %04lx\n",
                    ops[o].name, cws[c], in[0], in[2], in[3], r,
                    ops[o].r == 'x' ? out[1] & 0xffff : 0, out[2] & 0xffff);
          }
      }
}

/* 7. Latencies of fxdivt* chains (and fxdivxx, fdivd, fxdivss, fxdivdd,
   fxaddss and fxadddd for reference): each result feeds the next
   operation's single or double operand (tss, tdd: src1; ss, dd, addss,
   adddd: src2) or its 80-bit one (tsx, tdx, xx); the pairs fadd_fxadddd
   (faddd, then fxadddd of its result), fadd_fadd and fxadddx_fxtofd
   (fxadddx, then fxtofd of its 80-bit result) give the
   cost of a two-operation step.  */
#define LATS(X) X (tsx) X (tdx) X (tss) X (tdd) X (xx) X (fdivd) X (ss) X (dd) \
  X (addss) X (adddd) X (fadd_fxadddd) X (fadd_fadd) X (fxadddx_fxtofd)
#define DECL(n) long sr_lat_##n##16 (void), sr_lat_##n##48 (void);
LATS (DECL)
#undef DECL

static void
lat (void)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (0, &s);
  sched_setaffinity (0, sizeof s, &s);
#define RUN(n) printf ("lat %-6s: %.2f cycles per operation\n", #n, \
                       slope (sr_lat_##n##16, sr_lat_##n##48));
  LATS (RUN)
#undef RUN
}

/* 8. C1 (%fpsr bit 9): every operation below on pairs of special and
   random 80-bit operands, with %fpsr cleared (preset 000) or C1 set
   (200) beforehand; masks as at process start.  check_sregs.c compares
   C1 with x87's.  */
#define C1OPS(X) X (fxaddxx, 2) X (fxsubxx, 2) X (fxmulxx, 2) X (fxdivxx, 2) \
  X (fxdivxd, 2) X (fxtofd, 1) X (fxtoid, 1) X (fxsqrt, 1)
#define DECL(n, k) void sr_f_##n (const u64 *, u64 *, u64, u64, u64);
C1OPS (DECL)
#undef DECL

static void
c1 (void)
{
#define ENTRY(n, k) { #n, sr_f_##n, k },
  static const struct { const char *name;
                        void (*f) (const u64 *, u64 *, u64, u64, u64);
                        int k; } ops[] = { C1OPS (ENTRY) };
#undef ENTRY
  /* 0, -0, 1, -1.5, 2, 3, 1/3 (rounded up), 0.1, 1 + 2^-63, 1.75,
     3 * 2^-65 (1 + it rounds up), 2^-70 (1 + it rounds down), the
     largest, the smallest normal, the smallest denormal, inf, -inf, a
     quiet NaN, 2^62 + 0.5 (fxtoid rounds), 1.5 * 2^-16382.  */
  static const u64 v[][2] = {
    { 0, 0 }, { 0, 0x8000 }, { 0x8000000000000000UL, 0x3fff },
    { 0xc000000000000000UL, 0xbfff }, { 0x8000000000000000UL, 0x4000 },
    { 0xc000000000000000UL, 0x4000 }, { 0xaaaaaaaaaaaaaaabUL, 0x3ffd },
    { 0xcccccccccccccccdUL, 0x3ffb }, { 0x8000000000000001UL, 0x3fff },
    { 0xe000000000000000UL, 0x3fff }, { 0xc000000000000000UL, 0x3fbe },
    { 0x8000000000000000UL, 0x3fb9 }, { 0xffffffffffffffffUL, 0x7ffe },
    { 0x8000000000000000UL, 0x0001 }, { 0x0000000000000001UL, 0 },
    { 0x8000000000000000UL, 0x7fff }, { 0x8000000000000000UL, 0xffff },
    { 0xc000000000000000UL, 0x7fff }, { 0x8000000000000001UL, 0x403d },
    { 0xc000000000000000UL, 0x0001 },
  };
  const unsigned nv = sizeof v / sizeof *v;
  for (u64 pre = 0; pre <= 0x200; pre += 0x200)
    for (unsigned o = 0; o < sizeof ops / sizeof *ops; o++)
      {
        rs = 88172645463325252UL;
        unsigned nsp = ops[o].k == 2 ? nv * nv : nv;
        for (unsigned k = 0; k < nsp + 300; k++)
          {
            u64 in[4], out[6];
            if (k < nsp)
              {
                unsigned i = ops[o].k == 2 ? k / nv : k, j = k % nv;
                in[0] = v[i][0];
                in[1] = v[i][1];
                in[2] = v[j][0];
                in[3] = v[j][1];
              }
            else
              {
                /* Random significands, exponents within 2^+-12, either
                   sign; for one operand an exponent 60-70 below the
                   other's half the time (sums that round).  */
                u64 r3 = rnd ();
                in[0] = rnd () | 0x8000000000000000UL;
                in[2] = rnd () | 0x8000000000000000UL;
                in[1] = (0x3fff - 12 + r3 % 25) | ((r3 >> 16) & 0x8000);
                in[3] = (0x3fff - 12 + (r3 >> 24) % 25)
                        | ((r3 >> 40) & 0x8000);
                if (r3 >> 63)
                  in[3] = (in[3] & 0x8000)
                          | ((in[1] & 0x7fff) - 60 - (r3 >> 48) % 11);
              }
            ops[o].f (in, out, 0x1f80, 0x37f, pre);
            printf ("c1 %s %03lx %016lx %04lx %016lx %04lx -> %016lx %04lx "
                    "%04lx\n", ops[o].name, pre, in[0], in[1], in[2], in[3],
                    out[0], out[5] & 0xffff, out[3] & 0xffff);
          }
      }
}

/* 9. Scheduled latencies by the gap method: per-step cycles of a chain
   with each step G cycles after the previous one (sr_lg_*), and of
   chains A, G cycles, B, 12 cycles (sr_le_*).  */
#define GAPS_DIV(X, n) X (n, 13) X (n, 14) X (n, 15) X (n, 16) X (n, 17) \
  X (n, 18) X (n, 19) X (n, 20)
#define GAPS_EDGE(X, n) X (n, 1) X (n, 2) X (n, 3) X (n, 4) X (n, 5) \
  X (n, 6) X (n, 7) X (n, 8) X (n, 9)
#define LGS(X) GAPS_DIV (X, tsx) GAPS_DIV (X, tdx) GAPS_DIV (X, tss) \
  GAPS_DIV (X, tdd) GAPS_DIV (X, xx) GAPS_DIV (X, dd) GAPS_DIV (X, fdivd)
#define LES(X) GAPS_EDGE (X, xd_f) GAPS_EDGE (X, f_xd) GAPS_EDGE (X, xd_xd) \
  GAPS_EDGE (X, xx_fd) GAPS_EDGE (X, fd_xx)
#define DECL(n, g) long sr_lg_##n##_##g##_16 (void), sr_lg_##n##_##g##_48 (void);
LGS (DECL)
#undef DECL
#define DECL(n, g) long sr_le_##n##_##g##_16 (void), sr_le_##n##_##g##_48 (void);
LES (DECL)
#undef DECL

static void
latg (void)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (0, &s);
  sched_setaffinity (0, sizeof s, &s);
  printf ("# latg: chain steps G cycles apart (one operation a step); "
          "edge A_B: A, G cycles, B, 12 cycles (xd fxadddd, f faddd, xx "
          "fxadddx, fd fxtofd)\n");
#define RUN(n, g) printf ("latg %-6s gap %2d: %.2f cycles per step\n", #n, g, \
                          slope (sr_lg_##n##_##g##_16, sr_lg_##n##_##g##_48));
  LGS (RUN)
#undef RUN
#define RUN(n, g) printf ("latg %-6s gap %2d: %.2f cycles per step (G + 12 " \
                          "= %d)\n", #n, g, slope (sr_le_##n##_##g##_16, \
                          sr_le_##n##_##g##_48), g + 12);
  LES (RUN)
#undef RUN
}

/* 10. C1 after fxdivt* and their ,sm forms, with %fpsr preset to 0,
   0x200 (C1), 0x3f (all flags, so ,sm forms give normal results) and
   0x23f; %fpcr 0x37f.  */
#define DCS(X) X (fxdivtss, 's') X (fxdivtdd, 'd') X (fxdivtsx, 's') \
  X (fxdivtdx, 'd') X (fxdivtss_sm, 's') X (fxdivtdd_sm, 'd') \
  X (fxdivtsx_sm, 's') X (fxdivtdx_sm, 'd')
#define DECL(n, a) void sr_dc_##n (const u64 *, u64 *, u64);
DCS (DECL)
#undef DECL

static void
dc (void)
{
#define ENTRY(n, a) { #n, sr_dc_##n, a },
  static const struct { const char *name;
                        void (*f) (const u64 *, u64 *, u64); char a; } ops[] = {
    DCS (ENTRY)
  };
#undef ENTRY
  static const u64 pres[] = { 0, 0x200, 0x3f, 0x23f };
  /* src1: 1, 2, -1, 0.1, 1e30, 5; src2 (80-bit): 3, 1.5, 0.1, 7, 1 */
  static const double as[] = { 1.0, 2.0, -1.0, 0.1, 1e30, 5.0 };
  static const u64 ys[][2] = {
    { 0xc000000000000000UL, 0x4000 }, { 0xc000000000000000UL, 0x3fff },
    { 0xcccccccccccccccdUL, 0x3ffb }, { 0xe000000000000000UL, 0x4001 },
    { 0x8000000000000000UL, 0x3fff },
  };
  for (unsigned o = 0; o < sizeof ops / sizeof *ops; o++)
    {
      int n[4][2];
      memset (n, 0, sizeof n);
      for (unsigned p = 0; p < 4; p++)
        for (unsigned i = 0; i < sizeof as / sizeof *as; i++)
          for (unsigned j = 0; j < sizeof ys / sizeof *ys; j++)
            {
              u64 in[4], out[3];
              in[0] = ops[o].a == 's' ? fbits ((float) as[i]) : dbits (as[i]);
              in[1] = 0;
              in[2] = ys[j][0];
              in[3] = ys[j][1];
              ops[o].f (in, out, pres[p]);
              n[p][(out[2] & 0x200) != 0]++;
              printf ("dc %s pre %03lx %016lx / %016lx %04lx -> %016lx "
                      "fpsr %04lx\n", ops[o].name, pres[p], in[0], in[2],
                      in[3], ops[o].name[7] == 's' ? out[0] & 0xffffffff
                      : out[0], out[2] & 0xffff);
            }
      printf ("dc %s summary, C1 clear/set after: preset 000 %d/%d, "
              "200 %d/%d, 03f %d/%d, 23f %d/%d\n", ops[o].name, n[0][0],
              n[0][1], n[1][0], n[1][1], n[2][0], n[2][1], n[3][0], n[3][1]);
    }
}

/* 11. Edges into an 80-bit operation's double operand from a load
   (ALC0 or ALC3) or an integer operation, and a load into faddd, by the
   gap method (sr_lp_*: A, G cycles, B, 12 cycles; B doesn't feed A).  */
#define LPS(X) GAPS_EDGE (X, ld_xd) GAPS_EDGE (X, ld3_xd) \
  GAPS_EDGE (X, i_xd) GAPS_EDGE (X, ld_tdx) GAPS_EDGE (X, ld_f)
#define DECL(n, g) long sr_lp_##n##_##g##_16 (const double *), \
  sr_lp_##n##_##g##_48 (const double *);
LPS (DECL)
#undef DECL

static double lpbuf[8] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

static long
minp (long (*f) (const double *))
{
  long v[51];
  for (int i = 0; i < 51; i++)
    v[i] = f (lpbuf);
  qsort (v, 51, sizeof *v, cmpl);
  return v[0];
}

static void
lp (void)
{
  cpu_set_t s;
  CPU_ZERO (&s);
  CPU_SET (0, &s);
  sched_setaffinity (0, sizeof s, &s);
  printf ("# lp: A, G cycles, B, 12 cycles (ld ldd in ALC0, ld3 in ALC3, "
          "i addd, xd fxadddd's double operand, tdx fxdivtdx's double "
          "src1, f faddd)\n");
#define RUN(n, g) printf ("lp %-6s gap %2d: %.2f cycles per step (G + 12 = " \
                          "%d)\n", #n, g, (minp (sr_lp_##n##_##g##_48) \
                          - minp (sr_lp_##n##_##g##_16)) / 32.0, g + 12);
  LPS (RUN)
#undef RUN
}

int
main (int argc, char **argv)
{
  const char *w = argc > 1 ? argv[1] : "all";
  int all = !strcmp (w, "all");
  setvbuf (stdout, NULL, _IOLBF, 0);
  if (all || !strcmp (w, "width"))
    width ();
  if (all || !strcmp (w, "write"))
    write_masks ();
  if (all || !strcmp (w, "idr"))
    idr ();
  if (all || !strcmp (w, "clkr"))
    clkr ();
  if (all || !strcmp (w, "fp"))
    fp ();
  if (all || !strcmp (w, "dt"))
    dt ();
  if (all || !strcmp (w, "c1"))
    c1 ();
  if (all || !strcmp (w, "lat"))
    lat ();
  if (all || !strcmp (w, "latg"))
    latg ();
  if (all || !strcmp (w, "dc"))
    dc ();
  if (all || !strcmp (w, "lp"))
    lp ();
  return 0;
}
