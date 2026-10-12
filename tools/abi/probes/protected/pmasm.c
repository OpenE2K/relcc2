/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Protected-mode (e2k128) probe at the instruction level: what getsap,
   aptoap, aptoapb, getva and getpl produce, what %usd holds, and what
   the hardware does when a stack array pointer (SAP) is stored outside
   its frame or used after its frame is gone.  Each case that may fault
   runs in its own child process.  Built and run by run.sh, or by hand:

     lcc -mptr128 -O1 pmasm.c -o pmasm && ./pmasm      (on the 8C2, 2C3)
     /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -mptr128 -O1 -static \
         pmasm.c -o pmasm                               (for the 4C)

   Every line is "case: result"; a case that faults prints
   "case: SIGNAME code=N" from its handler.  */

#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define NOINLINE __attribute__ ((noinline))
typedef unsigned long u64;

/* ---- reading a 16-byte value with its tags ----------------------------- */

struct raw { u64 lo, hi, tlo, thi; };

/* The value in a register pair: bits and tags of both halves, read in
   the asm statement that receives it (a compiler copy could drop tags).  */
#define RAWQ(r, v)							\
	__asm__ __volatile__ (						\
		"gettagd %4, %2\n\t"					\
		"gettagd %H4, %3\n\t"					\
		"puttagd %4, 0x0, %0\n\t"				\
		"puttagd %H4, 0x0, %1\n\t"				\
		"puttagd %2, 0x0, %2\n\t"				\
		"puttagd %3, 0x0, %3\n\t"				\
		"andd %2, 0xf, %2\n\t"					\
		"andd %3, 0xf, %3"					\
		: "=&r" ((r).lo), "=&r" ((r).hi), "=&r" ((r).tlo),	\
		  "=&r" ((r).thi)					\
		: "r" (v))

static void prraw (const char *what, struct raw r)
{
	printf ("%s: lo=%016lx hi=%016lx tags=%lx,%lx\n", what, r.lo, r.hi,
		r.tlo, r.thi);
}

/* The 16 bytes at p as one quad load (ldapq keeps tags).  */
static NOINLINE struct raw memq (const void *p)
{
	struct raw r;
	__int128 q;
	__asm__ __volatile__ ("ldapq %1, 0x0, %0" : "=r" (q) : "r" (p) : "memory");
	RAWQ (r, q);
	return r;
}

/* ---- running a case in a child ------------------------------------------ */

static const char *cur_case;

static const char *signame (int s)
{
	switch (s) {
	case SIGSEGV: return "SIGSEGV";
	case SIGBUS: return "SIGBUS";
	case SIGILL: return "SIGILL";
	case SIGFPE: return "SIGFPE";
	case SIGTRAP: return "SIGTRAP";
	case SIGKILL: return "SIGKILL";
	default: return "SIG?";
	}
}

static void fault_handler (int sig, siginfo_t *si, void *uc)
{
	printf ("%s: %s code=%d\n", cur_case, signame (sig), si->si_code);
	fflush (stdout);
	_exit (0);
}

static void run (const char *name, void (*fn) (void))
{
	pid_t pid;
	int st;

	fflush (stdout);
	pid = fork ();
	if (pid == 0) {
		struct sigaction sa;
		memset (&sa, 0, sizeof sa);
		sa.sa_sigaction = fault_handler;
		sa.sa_flags = SA_SIGINFO;
		sigaction (SIGSEGV, &sa, NULL);
		sigaction (SIGBUS, &sa, NULL);
		sigaction (SIGILL, &sa, NULL);
		sigaction (SIGFPE, &sa, NULL);
		sigaction (SIGTRAP, &sa, NULL);
		cur_case = name;
		fn ();
		printf ("%s: no fault\n", name);
		fflush (stdout);
		_exit (0);
	}
	waitpid (pid, &st, 0);
	if (WIFSIGNALED (st))
		printf ("%s: killed by %s\n", name, signame (WTERMSIG (st)));
}

static volatile long sink;

/* ---- what getsap, aptoap, aptoapb, getva make ---------------------------- */

static NOINLINE void usd (const char *what)
{
	u64 lo, hi, sbr;
	__asm__ __volatile__ ("rrd %%usd.lo, %0\n\trrd %%usd.hi, %1\n\t"
			      "rrd %%sbr, %2" : "=r" (lo), "=r" (hi), "=r" (sbr));
	printf ("%s: usd.lo=%016lx usd.hi=%016lx sbr=%016lx\n", what, lo, hi, sbr);
}

static NOINLINE void conversions (void)
{
	__int128 s, a0, a16, b8, b8a, sub;
	struct raw r;
	u64 va, va2;

	{
		/* bit 1 is no_stack_prot ("no check stack pointers"), which
		   the kernel sets on every CPU before v7 (kernel
		   e2k_syswork.c, "bug 115090") */
		unsigned int cm;
		__asm__ __volatile__ ("rrs %%core_mode, %0" : "=r" (cm));
		printf ("core_mode: %#x (no_stack_prot %u)\n", cm, (cm >> 1) & 1);
	}
	{
		u64 lo0, hi0, lo1, hi1;
		/* %usd around the getsap, in one statement of this function */
		__asm__ __volatile__ ("rrd %%usd.lo, %1\n\trrd %%usd.hi, %2\n\t"
				      "getsap 0xffffffe0, %0\n\t"
				      "rrd %%usd.lo, %3\n\trrd %%usd.hi, %4"
				      : "=&r" (s), "=&r" (lo0), "=&r" (hi0),
					"=&r" (lo1), "=&r" (hi1));
		printf ("usd before getsap -32: lo=%016lx hi=%016lx\n", lo0, hi0);
		printf ("usd after getsap -32: lo=%016lx hi=%016lx\n", lo1, hi1);
	}
	usd ("usd in a callee");
	RAWQ (r, s);
	prraw ("getsap -32", r);
	/* aptoap SRC, OFF: the part of SRC from OFF on; aptoapb SRC, SIZE:
	   the first SIZE bytes.  */
	__asm__ __volatile__ ("aptoap %1, 0x0, %0" : "=r" (a0) : "r" (s));
	RAWQ (r, a0);
	prraw ("aptoap sap, 0", r);
	__asm__ __volatile__ ("aptoap %1, 0x10, %0" : "=r" (a16) : "r" (s));
	RAWQ (r, a16);
	prraw ("aptoap sap, 16", r);
	__asm__ __volatile__ ("aptoapb %1, 0x8, %0" : "=r" (b8) : "r" (a0));
	RAWQ (r, b8);
	prraw ("aptoapb ap, 8", r);
	__asm__ __volatile__ ("aptoapb %1, 0x8, %0" : "=r" (b8a) : "r" (s));
	RAWQ (r, b8a);
	prraw ("aptoapb sap, 8", r);
	sub = (__int128) 0;
	__asm__ __volatile__ ("aptoap %1, 0x8, %0\n\taptoapb %0, 0x4, %0"
			      : "=&r" (sub) : "r" (a0));
	RAWQ (r, sub);
	prraw ("aptoap ap, 8 + aptoapb 4", r);
	__asm__ __volatile__ ("getva %1, 0x0, %0" : "=r" (va) : "r" (s));
	__asm__ __volatile__ ("getva %1, 0x0, %0" : "=r" (va2) : "r" (a16));
	printf ("getva: sap %016lx, aptoap(sap,16) %016lx\n", va, va2);
}

/* A malloc'd pointer moved past its end with aptoap: what happens.  */
static void c_aptoap_over (void)
{
	void *m = malloc (16);
	__int128 r;
	struct raw x;
	__asm__ __volatile__ ("aptoap %1, 0x20, %0" : "=r" (r) : "r" (m));
	RAWQ (x, r);
	prraw ("c_aptoap_over (aptoap malloc(16), 32)", x);
}
static void c_aptoapb_over (void)
{
	void *m = malloc (16);
	__int128 r;
	struct raw x;
	__asm__ __volatile__ ("aptoapb %1, 0x20, %0" : "=r" (r) : "r" (m));
	RAWQ (x, r);
	prraw ("c_aptoapb_over (aptoapb malloc(16), 32)", x);
}

/* ---- stack array pointers outside their frame ------------------------------ */

void *g_slot;
void *g_slots[2];

/* A raw SAP to 32 bytes of this frame (a C cast keeps its tags: the asm
   hands back the register pair).  */
#define GETSAP(p) __asm__ __volatile__ ("getsap 0xffffffe0, %0" : "=r" (p))

static NOINLINE void sap_to_global_in (void)
{
	void *p;
	GETSAP (p);
	g_slot = p;			/* stgdq into the module's globals */
}
static void c_sap_to_global (void) { sap_to_global_in (); printf ("c_sap_to_global: stored\n"); }

static NOINLINE void sap_to_heap_in (void **h)
{
	void *p;
	GETSAP (p);
	*h = p;				/* stapq through a heap AP */
}
static void c_sap_to_heap (void)
{
	void **h = malloc (16);
	sap_to_heap_in (h);
	printf ("c_sap_to_heap: stored\n");
}

static NOINLINE void sap_to_older_in (void **slot)
{
	void *p;
	GETSAP (p);
	*slot = p;			/* into the caller's frame */
}
static void c_sap_to_older_frame (void)
{
	void *slot[1];
	sap_to_older_in (slot);
	printf ("c_sap_to_older_frame: stored\n");
}

static NOINLINE void sap_store_younger (void **slot, void *p) { *slot = p; }
static NOINLINE void sap_to_younger_in (void)
{
	void *p;
	void *slot[1];
	GETSAP (p);
	sap_store_younger (slot, p);	/* a callee stores it into our frame */
	printf ("c_sap_to_younger: callee stored it\n");
	fflush (stdout);
	{
		void *q[1];
		sap_store_younger (q, p);
	}
}
static void c_sap_to_younger (void) { sap_to_younger_in (); }

static NOINLINE void sap_in_callee (void *p, void **slot)
{
	void *mine[1];
	mine[0] = p;			/* older frame's SAP into this frame */
	*slot = mine[0];
}
static NOINLINE void sap_pass_down_in (void)
{
	void *p, *slot[1];
	GETSAP (p);
	sap_in_callee (p, slot);
	printf ("c_sap_pass_down: callee kept it in its frame\n");
}
static void c_sap_pass_down (void) { sap_pass_down_in (); }

static NOINLINE void *sap_return_in (void)
{
	void *p;
	GETSAP (p);
	return p;			/* the SAP of a frame being left */
}
static void c_sap_return (void)
{
	void *p = sap_return_in ();
	struct raw r;
	printf ("c_sap_return: returned\n");
	fflush (stdout);
	RAWQ (r, p);
	prraw ("c_sap_return value", r);
	fflush (stdout);
	sink = *(int *) p;
}
static void c_sap_return_store (void)
{
	void *slot[1];
	slot[0] = sap_return_in ();
	printf ("c_sap_return_store: stored in caller's frame\n");
}

static NOINLINE void sap_use (void)
{
	int *p;
	GETSAP (p);
	p[3] = 5;
	printf ("c_sap_use: wrote and read %d", p[3]);
	fflush (stdout);
	sink = p[8];			/* past the 32 bytes */
}
static void c_sap_use (void) { sap_use (); }

/* The AP lcc makes from a local (aptoap) stored into a global: no psl.  */
static NOINLINE void ap_local_to_global_in (void)
{
	int l = 7;
	g_slot = &l;
}
static void c_ap_local_to_global (void)
{
	struct raw r;
	ap_local_to_global_in ();
	r = memq (&g_slot);
	prraw ("c_ap_local_to_global stored", r);
	printf ("c_ap_local_to_global: reading the dead local: %d\n", *(int *) g_slot);
}

/* alloca's result (lcc passes getsap's result on as is).  */
static NOINLINE void alloca_to_global_in (int n)
{
	char *a = __builtin_alloca (n);
	a[0] = 1;
	g_slot = a;
}
static void c_alloca_to_global (void)
{
	struct raw r;
	alloca_to_global_in ((int) (sink + 24));
	r = memq (&g_slot);
	prraw ("c_alloca_to_global stored", r);
}

/* ---- procedure labels ------------------------------------------------------ */

static NOINLINE int ret7 (void) { return 7; }
static NOINLINE int other (void) { return 8; }
int (*g_fp) (void);

static void c_pl (void)
{
	struct raw r;
	g_fp = ret7;
	r = memq (&g_fp);
	prraw ("c_pl stored PL of ret7", r);
	g_fp = other;
	r = memq (&g_fp);
	prraw ("c_pl stored PL of other", r);
	g_fp = (int (*) (void)) printf;
	r = memq (&g_fp);
	prraw ("c_pl stored PL of printf", r);
}

/* ---------------------------------------------------------------------------- */

int main (void)
{
	setvbuf (stdout, NULL, _IOLBF, 0);
	conversions ();
#define R(f) run (#f, f)
	R (c_aptoap_over);
	R (c_aptoapb_over);
	R (c_sap_to_global);
	R (c_sap_to_heap);
	R (c_sap_to_older_frame);
	R (c_sap_to_younger);
	R (c_sap_pass_down);
	R (c_sap_return);
	R (c_sap_return_store);
	R (c_sap_use);
	R (c_ap_local_to_global);
	R (c_alloca_to_global);
	R (c_pl);
	printf ("done\n");
	return 0;
}
