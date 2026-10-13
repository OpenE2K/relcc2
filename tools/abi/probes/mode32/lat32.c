/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Latencies in 32-bit mode (spec/abi/mode32.md, "Addresses and memory
   access"): ldgd* and stgd* against ld* and st*, and getpl -> ct or call
   against movtd and disp, all in one 32-bit process.  lat32.s,
   lat32-cases.h (and, for v5 and later, lat32-v5.s and
   lat32-v5-cases.h) are written by gen_lat32.py, which describes the
   kernels.

     python3 tools/abi/probes/mode32/gen_lat32.py
     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc    # the 4C
     $L -mptr32 -O1 -static lat32.c lat32.s -o lat32
     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc   # the 8C2;
     $L -mptr32 -O1 -DLAT32_V5 lat32.c lat32.s lat32-v5.s -o lat32
                                          # v6.2c3 for the 2C3
     flock -w 3600 /tmp/relcc2-claude-timing.lock ./lat32 [SECTION]

   SECTION is lat, rt, tp or ct (default: all); `one CASE' runs the
   shorter kernel of one case (a name in lat32-cases.h) once, in the
   process itself, for gdb.  Each kernel runs in two
   lengths, each the best of 40 calls; a line gives the difference in
   cycles divided by the difference in steps:
     lat LABEL: g1 .. gN   cycles a step with the consumer g cycles after
                           its producer | L = the smallest g from which
                           every step costs g (no stall)
     rt LABEL s S: C       store, S-1 empty cycles, load, 2, ALU op
     tp LABEL: C           cycles a wide instruction
     ct LABEL: d1 .. dN    cycles a block, prepare D cycles before the
                           transfer | delay = the smallest D from which
                           every block costs D + 1 (a call: D + 8)
   Every case runs in a child process, so a trap (a movtd in a 32-bit
   process, say) is reported and the rest still run.  */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define LAT(f, l, g, p) long long l32_##f##_n16 (long long); \
  long long l32_##f##_n32 (long long);
#define RT(f, l, s) LAT (f, l, s, 0)
#define TP(f, l) LAT (f, l, 0, 0)
#define CT(f, l, d, p) long long l32_##f##_n4 (long long); \
  long long l32_##f##_n10 (long long);
#include "lat32-cases.h"
#ifdef LAT32_V5
#include "lat32-v5-cases.h"
#endif
#undef LAT
#undef RT
#undef TP
#undef CT

typedef long long (*kfn) (long long);
enum kind { K_LAT, K_RT, K_TP, K_CT };
struct kcase
{
  enum kind kind;
  const char *base, *label;
  int x;			/* g, s or d */
  int plus;			/* the step's fixed part */
  kfn shortk, longk;
  int nshort, nlong;
};

static const struct kcase cases[] = {
#define LAT(f, l, g, p) \
  { K_LAT, #f, l, g, p, l32_##f##_n16, l32_##f##_n32, 16, 32 },
#define RT(f, l, s) \
  { K_RT, #f, l, s, 0, l32_##f##_n16, l32_##f##_n32, 16, 32 },
#define TP(f, l) \
  { K_TP, #f, l, 0, 0, l32_##f##_n16, l32_##f##_n32, 16, 32 },
#define CT(f, l, d, p) \
  { K_CT, #f, l, d, p, l32_##f##_n4, l32_##f##_n10, 4, 10 },
#include "lat32-cases.h"
#ifdef LAT32_V5
#include "lat32-v5-cases.h"
#endif
#undef LAT
#undef RT
#undef TP
#undef CT
};
#define NCASES (int) (sizeof cases / sizeof cases[0])

static unsigned long long buf[256] __attribute__ ((aligned (4096)));

static void
init_buf (void)
{
  unsigned p = (unsigned) (unsigned long) buf;
  memset (buf, 0, sizeof buf);
  buf[0] = p;				/* p+0: p */
  ((unsigned *) buf)[2] = p + 8 + 0x80000000u;	/* p+8: wrap chase */
  buf[64] = p + 512;			/* p+512: p+512, then 0 */
}

static long long
best (kfn f)
{
  long long b = -1;
  for (int r = 0; r < 40; r++)
    {
      long long c = f ((long long) (unsigned long) buf);
      if (b < 0 || c < b)
	b = c;
    }
  return b;
}

/* Run case i in a child; write its cycles a step to *out.  Returns 0, or
   the signal that stopped the child.  */
static int
run_case (int i, double *out)
{
  int fd[2];
  if (pipe (fd))
    return -1;
  pid_t pid = fork ();
  if (pid == 0)
    {
      close (fd[0]);
      long long a = best (cases[i].shortk), b = best (cases[i].longk);
      double v = (double) (b - a) / (cases[i].nlong - cases[i].nshort);
      if (write (fd[1], &v, sizeof v) != sizeof v)
	_exit (2);
      _exit (0);
    }
  close (fd[1]);
  int st;
  ssize_t got = read (fd[0], out, sizeof *out);
  close (fd[0]);
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    return WTERMSIG (st);
  return got == sizeof *out ? 0 : -1;
}

int
main (int argc, char **argv)
{
  const char *sec = argc > 1 ? argv[1] : NULL;
  static const char *const names[] = { "lat", "rt", "tp", "ct" };
  init_buf ();
  printf ("buffer at %p\n", (void *) buf);
  if (sec && !strcmp (sec, "one") && argc > 2)
    {
      /* One case's shorter kernel, once, in this process (for gdb).  */
      for (int i = 0; i < NCASES; i++)
	if (!strcmp (cases[i].base, argv[2]))
	  {
	    printf ("%s: %lld cycles\n", argv[2],
		    cases[i].shortk ((long long) (unsigned long) buf));
	    return 0;
	  }
      printf ("%s: no such case\n", argv[2]);
      return 1;
    }
  for (int i = 0; i < NCASES;)
    {
      /* A group: consecutive cases with the same label.  */
      int j = i;
      while (j < NCASES && !strcmp (cases[j].label, cases[i].label))
	j++;
      if (sec && strcmp (sec, names[cases[i].kind]))
	{
	  i = j;
	  continue;
	}
      if (cases[i].kind == K_TP)
	{
	  double v;
	  int s = run_case (i, &v);
	  if (s)
	    printf ("tp %s: signal %d\n", cases[i].label, s);
	  else
	    printf ("tp %s: %.2f\n", cases[i].label, v);
	  i = j;
	  continue;
	}
      if (cases[i].kind == K_RT)
	{
	  for (int k = i; k < j; k++)
	    {
	      double v;
	      int s = run_case (k, &v);
	      if (s)
		printf ("rt %s s %d: signal %d\n", cases[k].label, cases[k].x, s);
	      else
		printf ("rt %s s %d: %.2f\n", cases[k].label, cases[k].x, v);
	    }
	  i = j;
	  continue;
	}
      printf ("%s %s:", names[cases[i].kind], cases[i].label);
      double v[32];
      int bad = 0;
      for (int k = i; k < j; k++)
	{
	  int s = run_case (k, &v[k - i]);
	  if (s)
	    {
	      printf (" %d:signal %d", cases[k].x, s);
	      bad = 1;
	    }
	  else
	    printf (" %.2f", v[k - i]);
	}
      if (!bad)
	{
	  /* The smallest x from which every step costs x + plus (lat: the
	     fixed part of a two-gap step; ct: 1, a call 8), within a
	     quarter cycle.  */
	  int plus = cases[i].plus, lim = -1;
	  for (int k = j - 1; k >= i; k--)
	    if (v[k - i] <= cases[k].x + plus + 0.25)
	      lim = cases[k].x;
	    else
	      break;
	  if (lim < 0)
	    printf (" | > %d", cases[j - 1].x);
	  else
	    printf (" | %s %d", cases[i].kind == K_CT ? "delay" : "L", lim);
	}
      printf ("\n");
      fflush (stdout);
      i = j;
    }
  return 0;
}
