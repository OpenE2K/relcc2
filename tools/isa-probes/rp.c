/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Recovery points: what srp, slrp and crp do in user mode and what
   %rpr (state register 0xa0/0xa2) holds; the kernel's e2k_rpr_t has the
   recovery IP and two BR images.  rp.s has the operations.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O3 tools/isa-probes/rp.c tools/isa-probes/rp.s -o rp
     ./rp                                 # on hardware  */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

long rp_rd_lo (long *);
long rp_rd_hi (long *);
long rp_wr (long, long);
long rp_srp (long *);
long rp_slrp (long *);
long rp_crp (long *);
long rp_srp_cond (long *, long);
long rp_store (long *, long);
extern char rp_srp_at[], rp_slrp_at[], rp_srpc_at[];

static sigjmp_buf env;
static int code;
static void *addr;

static void
handler (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  code = si->si_code;
  addr = si->si_addr;
  siglongjmp (env, sig);
}

/* Where a faulting address lies: the line of /proc/self/maps.  */
static void
where (void *a)
{
  FILE *f = fopen ("/proc/self/maps", "r");
  char line[512];
  unsigned long lo, hi, off;
  while (f && fgets (line, sizeof line, f))
    if (sscanf (line, "%lx-%lx %*s %lx", &lo, &hi, &off) == 3
        && (unsigned long) a >= lo && (unsigned long) a < hi)
      {
        printf ("    in %s    file offset %#lx\n", strtok (line, "\n"),
                (unsigned long) a - lo + off);
        break;
      }
  if (f)
    fclose (f);
}

/* Run CASE in a child: it prints what it can; the parent reports how
   it ended.  */
static void
child (const char *what, void (*fn) (void))
{
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      fn ();
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("  [%s: child killed by signal %d]\n", what, WTERMSIG (st));
}

#define TRY(what, stmt) \
  ({ int sig_ = sigsetjmp (env, 1); \
     if (!sig_) { stmt; } \
     else { printf ("  %-28s signal %d code %d at %p\n", what, sig_, code, \
                    addr); where (addr); } \
     sig_; })

static void
show (const char *what, long *out, char *at)
{
  unsigned long lo = out[0], hi = out[1];
  printf ("  %-28s lo %016lx (ip %s%+ld, bits 48-63 %04lx)  hi %016lx\n",
          what, lo, at ? "op" : "", at ? (long) ((lo & 0xffffffffffffUL)
                                              - (unsigned long) at) : 0,
          lo >> 48, hi);
}

static long smem[2];

#define STORE(n, h, what) \
static void \
store##n (void) \
{ \
  long v = -1; \
  smem[0] = 0; \
  if (!TRY (what, v = rp_store (smem, h))) \
    printf ("  %-28s returned %ld, mem[0] = %ld\n", what, v, smem[0]); \
  else \
    printf ("  %-28s mem[0] = %ld after the signal\n", what, smem[0]); \
}
STORE (0, 0, "srp, store, return")
STORE (1, 1, "srp, store, crp")
STORE (2, 2, "srp, store, NULL load")
STORE (3, 3, "srp, store, load, crp")

static void
srp_write (void)
{
  static long out[2];
  if (!TRY ("srp", rp_srp (out)))
    {
      static const char msg[] = "  srp returned; a write(2) went through\n";
      write (1, msg, sizeof msg - 1);
      show ("srp", out, rp_srp_at);
    }
}

static void
slrp_write (void)
{
  static long out[2];
  if (!TRY ("slrp", rp_slrp (out)))
    {
      static const char msg[] = "  slrp returned; a write(2) went through\n";
      write (1, msg, sizeof msg - 1);
      show ("slrp", out, rp_slrp_at);
    }
}

int
main (void)
{
  static long out[2];
  struct sigaction sa;
  long v = 0;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  sigaction (SIGILL, &sa, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);

  printf ("%%rpr at start:\n");
  if (!TRY ("rrd %rpr.lo", v = rp_rd_lo (out)))
    printf ("  lo %016lx\n", v);
  if (!TRY ("rrd %rpr.hi", v = rp_rd_hi (out)))
    printf ("  hi %016lx\n", v);
  printf ("rwd %%rpr (lo 0x123456789a, hi 0x5):\n");
  if (!TRY ("rwd %rpr", v = rp_wr (0x123456789aL, 5)))
    printf ("  reads back lo %016lx, hi %016lx\n", v, rp_rd_hi (out));
  TRY ("rwd %rpr 0", rp_wr (0, 0));

  printf ("after srp (setbn rbs 4 rsz 3 rcur 1, setbp psz 2):\n");
  memset (out, 0, sizeof out);
  if (!TRY ("srp", rp_srp (out)))
    show ("srp", out, rp_srp_at);
  printf ("after slrp:\n");
  TRY ("rwd %rpr 0", rp_wr (0, 0));
  memset (out, 0, sizeof out);
  if (!TRY ("slrp", rp_slrp (out)))
    show ("slrp", out, rp_slrp_at);
  printf ("after srp, crp:\n");
  memset (out, 0, sizeof out);
  if (!TRY ("srp; crp", rp_crp (out)))
    show ("srp; crp", out, NULL);
  printf ("after srp ? %%pred0 (false, true), %%rpr cleared before:\n");
  for (long c = 0; c < 2; c++)
    {
      TRY ("rwd %rpr 0", rp_wr (0, 0));
      memset (out, 0, sizeof out);
      if (!TRY (c ? "srp ? true" : "srp ? false", rp_srp_cond (out, c)))
        show (c ? "srp ? true" : "srp ? false", out, rp_srpc_at);
    }

  printf ("a store after srp, each in a child (mem[0] 0 before):\n");
  child ("then return", store0);
  child ("then crp", store1);
  child ("then a fault", store2);
  child ("then load it back, crp", store3);
  printf ("srp or slrp alone, in a child, then a raw write(2) before "
          "libc:\n");
  child ("srp then write", srp_write);
  child ("slrp then write", slrp_write);
  printf ("%%rpr at the end:\n");
  if (!TRY ("rrd %rpr.lo", v = rp_rd_lo (out)))
    printf ("  lo %016lx  hi %016lx\n", v, rp_rd_hi (out));
  return 0;
}
