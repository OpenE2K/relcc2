/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Protected-mode (e2k128) leftovers: the open items of
   spec/abi/protected-mode.md ("Not checked yet").  Each command prints
   lines "case: result"; a case that may fault runs in its own child
   process and prints "case: SIGNAME code=N addr=..." from its handler
   (or "case: killed by SIGNAME").  Built and run by run.sh --left, or by
   hand:

     lcc -mptr128 -O1 pmleft.c -o pmleft -lpthread     (on the 8C2, 2C3)
     /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -mptr128 -O1 -static \
         pmleft.c -o pmleft -lpthread                   (for the 4C)

     ./pmleft usd [N]   when an rrd %usd.lo after a getsap sees it: N
                        (default 2000) samples at each distance in cycles
     ./pmleft cud       cudtoap, gdtoap, ldcud*, %cud/%gd, and the rw field
                        of every kind of descriptor the kernel and lib128
                        hand out (read-only APs)
     ./pmleft clean     the clean_descriptors system call (424) called
                        directly: the dangling-pointer walk
     ./pmleft mremap    a moving mremap; run it with and without
                        PM_MM_CHECK_4_DANGLING_POINTERS=1
     ./pmleft psl       recursion until %usd.psl (16 bits) overflows: in a
                        thread, with a bigger frame, and from main  */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define NOINLINE __attribute__ ((noinline))
typedef unsigned long u64;

/* ---- looking at a 16-byte value with its tags (as pmasm.c) ----------- */

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

/* The 16 bytes at p as one quad load (ldapq keeps tags).  */
static NOINLINE struct raw memq (const void *p)
{
	struct raw r;
	__int128 q;
	__asm__ __volatile__ ("ldapq %1, 0x0, %0" : "=r" (q) : "r" (p) : "memory");
	RAWQ (r, q);
	return r;
}

/* An array pointer's fields (kernel include/asm/e2k_ptypes.h, v3-v6).  */
static void prap (const char *what, struct raw r)
{
	printf ("%s: lo=%016lx hi=%016lx tags=%lx,%lx base=%lx size=%lx "
		"curptr=%lx rw=%lu itag=%lu bit58=%lu\n", what, r.lo, r.hi,
		r.tlo, r.thi, r.lo & 0xffffffffffffUL, r.hi >> 32,
		r.hi & 0xffffffffUL, (r.lo >> 59) & 3, r.lo >> 61,
		(r.lo >> 58) & 1);
}

/* A pointer variable's value with its tags.  */
static NOINLINE void dumpp (const char *what, const void *p)
{
	const void *slot[1];
	slot[0] = p;
	prap (what, memq (slot));
}

/* ---- running a case in a child (as pmrun.c) -------------------------- */

static const char *cur_case;
static volatile long progress = -1;	/* a case's step, printed on a fault */

static const char *signame (int s)
{
	switch (s) {
	case SIGSEGV: return "SIGSEGV";
	case SIGBUS: return "SIGBUS";
	case SIGILL: return "SIGILL";
	case SIGFPE: return "SIGFPE";
	case SIGTRAP: return "SIGTRAP";
	case SIGABRT: return "SIGABRT";
	case SIGKILL: return "SIGKILL";
	case SIGALRM: return "SIGALRM (hung)";
	default: return "SIG?";
	}
}

static void fault_handler (int sig, siginfo_t *si, void *uc)
{
	struct raw r = memq (&si->si_addr);
	printf ("%s: %s code=%d addr=%016lx", cur_case, signame (sig),
		si->si_code, r.lo);
	if (progress >= 0)
		printf (" (step %ld)", progress);
	printf ("\n");
	fflush (stdout);
	_exit (0);
}

static void catch_faults (void)
{
	struct sigaction sa;
	memset (&sa, 0, sizeof sa);
	sa.sa_sigaction = fault_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction (SIGSEGV, &sa, NULL);
	sigaction (SIGBUS, &sa, NULL);
	sigaction (SIGILL, &sa, NULL);
	sigaction (SIGFPE, &sa, NULL);
	sigaction (SIGTRAP, &sa, NULL);
}

static void run (const char *name, void (*fn) (void), int secs)
{
	pid_t pid;
	int st;

	fflush (stdout);
	fflush (stderr);
	pid = fork ();
	if (pid < 0) {
		printf ("%s: fork failed errno=%d\n", name, errno);
		return;
	}
	if (pid == 0) {
		catch_faults ();
		cur_case = name;
		alarm (secs);
		fn ();
		printf ("%s: no fault\n", name);
		fflush (stdout);
		_exit (0);
	}
	waitpid (pid, &st, 0);
	if (WIFSIGNALED (st))
		printf ("%s: killed by %s\n", name, signame (WTERMSIG (st)));
	else if (WEXITSTATUS (st) != 0)
		printf ("%s: exit %d\n", name, WEXITSTATUS (st));
}

static NOINLINE void *opaque (void *p) { return p; }
static volatile long sink;

/* ---- usd: when rrd %usd.lo sees a getsap ------------------------------ */

/* Each function reads %usd.lo (a), lets 16 cycles pass, reads it again
   (b), runs "getsap -32" and reads it at a fixed distance after the
   getsap (c), lets 16 cycles pass and reads it once more (e).  b - e
   is always 32; b - c is 32 if the read at that distance saw the
   getsap and 0 if it did not.  ".ignore strict_delay" keeps gas from
   padding.  A wide instruction with "nop k" is followed by k empty
   cycles: the distance is counted in cycles, assuming nothing stalls
   (nothing here waits for an operand).  rrd runs only in ALC0, and
   getsap takes ALC0 whichever of ALC0 and ALC1 it is given (a quad
   result: gas says "attempting to place several instructions into ALC0"
   for the two in one instruction), so the shortest distance is 1.  */
#define SETTLE "{\n\tnop 7\n\t}\n\t{\n\tnop 7\n\t}\n\t"
#define GS "getsap,1 0xffffffe0, %4\n\t"
#define RD "rrd,0 %%usd.lo, %2\n\t"
#define USD_FN(name, GAP)						\
static NOINLINE void name (u64 *o)					\
{									\
	u64 a, b, c, e;							\
	__int128 s;							\
	__asm__ __volatile__ (						\
		".ignore strict_delay\n\t"				\
		"{\n\trrd,0 %%usd.lo, %0\n\t}\n\t"			\
		SETTLE							\
		"{\n\trrd,0 %%usd.lo, %1\n\t}\n\t"			\
		SETTLE							\
		GAP							\
		SETTLE							\
		"{\n\trrd,0 %%usd.lo, %3\n\t}\n\t"			\
		".accept strict_delay"					\
		: "=&r" (a), "=&r" (b), "=&r" (c), "=&r" (e),		\
		  "=&r" (s));						\
	o[0] = a;							\
	o[1] = b;							\
	o[2] = c;							\
	o[3] = e;							\
}

USD_FN (usd_d1, "{\n\t" GS "}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d2, "{\n\tnop 1\n\t" GS "}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d3, "{\n\tnop 2\n\t" GS "}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d4, "{\n\tnop 3\n\t" GS "}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d5, "{\n\tnop 4\n\t" GS "}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d6, "{\n\tnop 5\n\t" GS "}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d7, "{\n\tnop 6\n\t" GS "}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d8, "{\n\tnop 7\n\t" GS "}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d10, "{\n\tnop 7\n\t" GS "}\n\t{\n\tnop 1\n\t}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d12, "{\n\tnop 7\n\t" GS "}\n\t{\n\tnop 3\n\t}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d16, "{\n\tnop 7\n\t" GS "}\n\t{\n\tnop 7\n\t}\n\t{\n\t" RD "}\n\t")
USD_FN (usd_d24, "{\n\tnop 7\n\t" GS "}\n\t{\n\tnop 7\n\t}\n\t{\n\tnop 7\n\t}\n\t{\n\t" RD "}\n\t")

/* Two getsaps in consecutive instructions (lcc's prologue may end with
   one), the read one instruction after the second: b - c is 64 if both
   were seen.  o[4] is the first area's base minus the second's: 32 if
   the second getsap saw the first.  */
static NOINLINE void usd_pair (u64 *o)
{
	u64 a, b, c, e;
	__int128 s, t;
	struct raw rs, rt;
	__asm__ __volatile__ (
		".ignore strict_delay\n\t"
		"{\n\trrd,0 %%usd.lo, %0\n\t}\n\t"
		SETTLE
		"{\n\trrd,0 %%usd.lo, %1\n\t}\n\t"
		SETTLE
		"{\n\tgetsap,1 0xffffffe0, %4\n\t}\n\t"
		"{\n\tgetsap,1 0xffffffe0, %5\n\t}\n\t"
		"{\n\trrd,0 %%usd.lo, %2\n\t}\n\t"
		SETTLE
		"{\n\trrd,0 %%usd.lo, %3\n\t}\n\t"
		".accept strict_delay"
		: "=&r" (a), "=&r" (b), "=&r" (c), "=&r" (e), "=&r" (s),
		  "=&r" (t));
	RAWQ (rs, s);
	RAWQ (rt, t);
	o[0] = a;
	o[1] = b;
	o[2] = c;
	o[3] = e;
	o[4] = (rs.lo - rt.lo) & 0xffffffffffffUL;
}

/* The read right after the getsap, with the getsap's result used by an
   aptoapb in between (the result is ready before the read).  */
static NOINLINE void usd_use (u64 *o)
{
	u64 a, b, c, e;
	__int128 s, t;
	__asm__ __volatile__ (
		".ignore strict_delay\n\t"
		"{\n\trrd,0 %%usd.lo, %0\n\t}\n\t"
		SETTLE
		"{\n\trrd,0 %%usd.lo, %1\n\t}\n\t"
		SETTLE
		"{\n\tgetsap,1 0xffffffe0, %4\n\t}\n\t"
		"{\n\taptoapb,0 %4, 0x10, %5\n\t}\n\t"
		"{\n\trrd,0 %%usd.lo, %2\n\t}\n\t"
		SETTLE
		"{\n\trrd,0 %%usd.lo, %3\n\t}\n\t"
		".accept strict_delay"
		: "=&r" (a), "=&r" (b), "=&r" (c), "=&r" (e), "=&r" (s),
		  "=&r" (t));
	o[0] = a;
	o[1] = b;
	o[2] = c;
	o[3] = e;
}

static void usd_sweep (const char *name, void (*fn) (u64 *), int n)
{
	int seen = 0, unseen = 0, other = 0, settled_bad = 0, entry_diff = 0;
	u64 other_val = 0, o[5] = { 0, 0, 0, 0, 0 };
	u64 psl_lo = ~0UL, psl_hi = 0;

	for (int i = 0; i < n; i++) {
		u64 d, de, psl;
		fn (o);
		d = (o[1] - o[2]) & 0xffffffffUL;
		de = (o[1] - o[3]) & 0xffffffffUL;
		if (o[0] != o[1])
			entry_diff++;
		if (de != 32 && de != 64)
			settled_bad++;
		if (d == de)
			seen++;
		else if (d == 0)
			unseen++;
		else {
			other++;
			other_val = d;
		}
		psl = (o[1] >> 32) & 0xffff;
		if (psl < psl_lo)
			psl_lo = psl;
		if (psl > psl_hi)
			psl_hi = psl;
	}
	printf ("usd %s: n=%d seen=%d unseen=%d other=%d (last %#lx) "
		"settled-not-32=%d entry-read-differs=%d psl=%#lx..%#lx "
		"usd.lo=%016lx", name, n, seen, unseen, other, other_val,
		settled_bad, entry_diff, psl_lo, psl_hi, o[1]);
	if (fn == usd_pair)
		printf (" (last pair: first base - second base = %#lx)", o[4]);
	printf ("\n");
}

static void cmd_usd (int n)
{
	static const struct { const char *name; void (*fn) (u64 *); } t[] = {
		{ "d1", usd_d1 }, { "d2", usd_d2 },
		{ "d3", usd_d3 }, { "d4", usd_d4 }, { "d5", usd_d5 },
		{ "d6", usd_d6 }, { "d7", usd_d7 }, { "d8", usd_d8 },
		{ "d10", usd_d10 }, { "d12", usd_d12 }, { "d16", usd_d16 },
		{ "d24", usd_d24 }, { "pair", usd_pair }, { "use", usd_use },
	};
	for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
		usd_sweep (t[i].name, t[i].fn, n);
}

/* ---- cud: cudtoap, gdtoap, ldcud, and read-only APs ------------------ */

static NOINLINE int code_fn (int x) { return x * 3 + 1; }

static NOINLINE void c_cud_regs (void)
{
	u64 cl, ch, gl, gh, ul, uh;
	__asm__ __volatile__ ("rrd %%cud.lo, %0\n\trrd %%cud.hi, %1\n\t"
			      "rrd %%gd.lo, %2\n\trrd %%gd.hi, %3\n\t"
			      "rrd %%usd.lo, %4\n\trrd %%usd.hi, %5"
			      : "=r" (cl), "=r" (ch), "=r" (gl), "=r" (gh),
				"=r" (ul), "=r" (uh));
	printf ("c_cud_regs: cud.lo=%016lx cud.hi=%016lx gd.lo=%016lx "
		"gd.hi=%016lx usd.lo=%016lx usd.hi=%016lx\n", cl, ch, gl, gh,
		ul, uh);
}

static NOINLINE struct raw cudtoap_at0 (void)
{
	struct raw r;
	__int128 q;
	__asm__ __volatile__ ("cudtoap 0x0, %0" : "=r" (q));
	RAWQ (r, q);
	return r;
}

static NOINLINE struct raw cudtoap_at100 (void)
{
	struct raw r;
	__int128 q;
	__asm__ __volatile__ ("cudtoap 0x100, %0" : "=r" (q));
	RAWQ (r, q);
	return r;
}

static NOINLINE struct raw gdtoap_at0 (void)
{
	struct raw r;
	__int128 q;
	__asm__ __volatile__ ("gdtoap 0x0, %0" : "=r" (q));
	RAWQ (r, q);
	return r;
}

/* The code address of code_fn from its PL (low doubleword, bits 47-0).  */
static NOINLINE u64 code_addr (void)
{
	int (*f) (int) = code_fn;
	return memq (&f).lo & 0xffffffffffffUL;
}

static NOINLINE u64 ldcudd_at (unsigned off)
{
	u64 v;
	__asm__ __volatile__ ("ldcudd 0x0, %1, %0" : "=r" (v) : "r" (off) : "memory");
	return v;
}

static NOINLINE u64 ldcudw_at (unsigned off)
{
	u64 v;
	__asm__ __volatile__ ("ldcudw 0x0, %1, %0" : "=r" (v) : "r" (off) : "memory");
	return v;
}

static void c_cudtoap (void)
{
	struct raw c = cudtoap_at0 ();
	prap ("c_cudtoap 0x0", c);
	prap ("c_cudtoap 0x100", cudtoap_at100 ());
	prap ("c_gdtoap 0x0", gdtoap_at0 ());
	printf ("c_cudtoap: code_fn at %lx, offset in CUD %lx\n", code_addr (),
		code_addr () - (c.lo & 0xffffffffffffUL));
}

/* 64 bytes read with ldcudd at CUD offset 0 and at code_fn's offset,
   each searched in /proc/self/exe: if ldcud reads the module's code,
   the two are found exactly code_fn's offset apart.  */
static long find_all (const char *buf, long n, const u64 *v, const char *what)
{
	long first = -1;
	int hits = 0;
	for (const char *p = buf; p + 64 <= buf + n; p++) {
		p = memmem (p, buf + n - p, v, 64);
		if (!p)
			break;
		if (first < 0)
			first = p - buf;
		hits++;
	}
	printf ("c_ldcud: 64 bytes %s found %d time(s) in /proc/self/exe, "
		"first at %#lx\n", what, hits, first);
	return first;
}

static void c_ldcud (void)
{
	struct raw c = cudtoap_at0 ();
	unsigned off = (unsigned) (code_addr () - (c.lo & 0xffffffffffffUL));
	u64 v0[8], v1[8];
	char *buf;
	long n = 0, r, f0, f1;
	int fd;

	for (int i = 0; i < 8; i++) {
		v0[i] = ldcudd_at (8 * i);
		v1[i] = ldcudd_at (off + 8 * i);
	}
	printf ("c_ldcud: ldcudd at %#x: %016lx %016lx; ldcudw %08lx\n", off,
		v1[0], v1[1], ldcudw_at (off));
	printf ("c_ldcud: ldcudd at 0: %016lx\n", v0[0]);
	fflush (stdout);
	fd = open ("/proc/self/exe", O_RDONLY);
	buf = malloc (1 << 22);
	while (fd >= 0 && (r = read (fd, buf + n, (1 << 22) - n)) > 0)
		n += r;
	if (fd >= 0)
		close (fd);
	f0 = find_all (buf, n, v0, "at CUD offset 0");
	f1 = find_all (buf, n, v1, "at code_fn's CUD offset");
	printf ("c_ldcud: file offsets differ by %#lx, code_fn's CUD offset "
		"is %#x\n", f1 - f0, off);
}

/* An ldcudd past the end of CUD.  */
static void c_ldcud_over (void)
{
	struct raw c = cudtoap_at0 ();
	unsigned size = (unsigned) (c.hi >> 32);
	printf ("c_ldcud_over: CUD size %#x, reading at it\n", size);
	fflush (stdout);
	sink = ldcudd_at (size);
}

/* An ldapd through cudtoap's AP at code_fn's offset.  */
static void c_cudap_load (void)
{
	unsigned off = (unsigned) (code_addr () - (cudtoap_at0 ().lo & 0xffffffffffffUL));
	__int128 q;
	u64 v, t;
	printf ("c_cudap_load: ldcudd at %#x: %016lx\n", off, ldcudd_at (off));
	fflush (stdout);
	__asm__ __volatile__ ("cudtoap 0x0, %1\n\t"
			      "ldapd %1, %3, %0\n\t"
			      "gettagd %0, %2\n\t"
			      "puttagd %2, 0x0, %2"
			      : "=&r" (v), "=&r" (q), "=&r" (t) : "r" (off) : "memory");
	printf ("c_cudap_load: ldapd through cudtoap's AP at %#x: %016lx tag %lx\n",
		off, v, t);
}

/* A store through cudtoap's AP (offset 0x100: inside CUD).  The fault
   address tells a descriptor check (the instruction, inside this
   function) from a page fault (the data address, CUD base + 0x100).  */
static void c_cudap_store (void);
static void c_cudap_store (void)
{
	__int128 q;
	u64 v = 0x1234;
	void (*self) (void) = c_cudap_store;
	printf ("c_cudap_store: this function at %lx, the store's target %lx\n",
		memq (&self).lo & 0xffffffffffffUL,
		(cudtoap_at0 ().lo & 0xffffffffffffUL) + 0x100);
	fflush (stdout);
	__asm__ __volatile__ ("cudtoap 0x0, %0" : "=r" (q));
	__asm__ __volatile__ ("stapd %0, 0x100, %1" : : "r" (q), "r" (v) : "memory");
	printf ("c_cudap_store: stored\n");
}

/* aptoapb of cudtoap's AP: are its rights kept?  */
static void c_cudap_narrow (void)
{
	__int128 q, n;
	struct raw r;
	__asm__ __volatile__ ("cudtoap 0x0, %0\n\taptoapb %0, 0x40, %1"
			      : "=&r" (q), "=&r" (n));
	RAWQ (r, n);
	prap ("c_cudap_narrow aptoapb 0x40", r);
}

/* cudtoap's AP as a system call buffer: write () reads through it,
   read () writes through it (the kernel checks no rights on buffers:
   only mprotect and mremap look at rw, protected_syscalls.c).  */
static void c_cudap_syscall (void)
{
	void *p;
	int fd = open ("/dev/null", O_WRONLY), fz = open ("/dev/zero", O_RDONLY);
	long w, r;
	int we, re;
	__asm__ __volatile__ ("cudtoap 0x0, %0" : "=r" (p));
	errno = 0;
	w = write (fd, p, 16);
	we = errno;
	errno = 0;
	r = read (fz, (char *) p + 0x100, 8);
	re = errno;
	printf ("c_cudap_syscall: write (/dev/null, cudap, 16) = %ld errno %d; "
		"read (/dev/zero, cudap + 0x100, 8) = %ld errno %d\n", w, we, r, re);
}

/* __builtin_e2k_create_ap_subarray of cudtoap's AP.  */
static void c_cudap_builtin (void)
{
	void *p, *q;
	__asm__ __volatile__ ("cudtoap 0x0, %0" : "=r" (p));
	q = __builtin_e2k_create_ap_subarray (p, 0x10, 0x20);
	dumpp ("c_cudap_builtin create_ap_subarray (0x10, 0x20)", q);
}

/* A pointer an ordinary load of code returns: the PL path for
   comparison (pmrun f_deref_fptr: SIGILL ILL_ILLOPN).  */
static void c_pl_load (void)
{
	int (*f) (int) = code_fn;
	sink = *(volatile long *) opaque ((void *) f);
}

/* rw of the descriptors the kernel and lib128 hand out.  */
static void si_rw_handler (int sig, siginfo_t *si, void *uc)
{
	dumpp ("rw siginfo", si);
	dumpp ("rw ucontext", uc);
}

static void c_rw (int argc, char **argv, char **envp)
{
	struct sigaction sa;
	char *m;

	dumpp ("rw argv", argv);
	dumpp ("rw argv[0]", argv[0]);
	dumpp ("rw envp", envp);
	m = mmap (NULL, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	dumpp ("rw mmap PROT_READ", m);
	m = mmap (NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	dumpp ("rw mmap PROT_NONE", m);
	m = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	mprotect (m, 4096, PROT_READ);
	dumpp ("rw mmap RW then mprotect PROT_READ (the old AP)", m);
	{
		int fd = open ("/proc/self/exe", O_RDONLY);
		char *f = mmap (NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
		dumpp ("rw mmap of a file opened O_RDONLY", f);
		close (fd);
	}
	dumpp ("rw string literal", "literal");
	dumpp ("rw malloc", malloc (8));
	dumpp ("rw alloca", __builtin_alloca (32));
	memset (&sa, 0, sizeof sa);
	sa.sa_sigaction = si_rw_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction (SIGUSR1, &sa, NULL);
	raise (SIGUSR1);
	fflush (stdout);
}

static void cmd_cud (int argc, char **argv, char **envp)
{
	run ("c_cud_regs", c_cud_regs, 10);
	run ("c_cudtoap", c_cudtoap, 10);
	run ("c_ldcud", c_ldcud, 10);
	run ("c_ldcud_over", c_ldcud_over, 10);
	run ("c_cudap_load", c_cudap_load, 10);
	run ("c_cudap_store", c_cudap_store, 10);
	run ("c_cudap_narrow", c_cudap_narrow, 10);
	run ("c_cudap_builtin", c_cudap_builtin, 10);
	run ("c_cudap_syscall", c_cudap_syscall, 10);
	run ("c_pl_load", c_pl_load, 10);
	c_rw (argc, argv, envp);
}

/* ---- clean: the clean_descriptors system call ------------------------- */

#define NR_clean_descriptors 424	/* uapi asm/unistd.h */
#define CLEAN_SINGLE 1			/* uapi asm/protected_mode.h */
#define CLEAN_NO_GARB_COLL 2

union pw { void *p; u64 w[2]; };
struct img { u64 lo, hi; } __attribute__ ((aligned (16)));

void *g_copy;				/* a copy in the module's globals */

/* A copy kept in this function's registers across the call (the caller
   of syscall () spills it to the procedure stack).  */
static NOINLINE void clean_and_check (const char *name, char *big, long size,
				     long flags, int tagged_list)
{
	char *sub = __builtin_e2k_create_ap_subarray (big, 0, 4096);
	char *outside = __builtin_e2k_create_ap_subarray (big, 4096, 4096);
	void **heap = malloc (16);
	void *loc[2];
	union pw u;
	long r;

	memset (big, 0x11, 8192);
	g_copy = sub;
	heap[0] = sub;
	loc[0] = sub;
	loc[1] = outside;
	opaque (loc);
	u.p = sub;
	printf ("%s: sub-array base=%lx size=%lx\n", name, u.w[0] & 0xffffffffffffUL,
		u.w[1] >> 32);
	fflush (stdout);
	if (flags & CLEAN_SINGLE) {
		r = syscall (NR_clean_descriptors, sub, size, flags);
	} else {
		struct img *list = malloc (16);
		if (tagged_list) {
			*(void **) list = sub;	/* a descriptor, tags 0xCF */
		} else {
			list->lo = u.w[0];	/* the same bits as numbers */
			list->hi = u.w[1];
		}
		r = syscall (NR_clean_descriptors, list, size, flags);
	}
	printf ("%s: returned %ld errno %d\n", name, r, r < 0 ? errno : 0);
	prap ("  copy in a global", memq (&g_copy));
	prap ("  copy in the heap", memq (heap));
	prap ("  copy in a local array", memq (&loc[0]));
	prap ("  pointer to the next page", memq (&loc[1]));
	{
		struct raw x;
		RAWQ (x, sub);
		prap ("  copy in a register", x);
	}
	prap ("  memory at the cleaned page (through the 8 KiB AP)", memq (big));
	prap ("  memory at the next page", memq (big + 4096));
	fflush (stdout);
	sink = sub[0];
	printf ("%s: read through the register copy: %ld\n", name, sink);
}

static char *map2 (void)
{
	return mmap (NULL, 8192, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static void c_clean_tagged (void) { clean_and_check ("c_clean_tagged", map2 (), 1, 0, 1); }
static void c_clean_untagged (void) { clean_and_check ("c_clean_untagged", map2 (), 1, 0, 0); }
static void c_clean_single (void) { clean_and_check ("c_clean_single", map2 (), 4096, CLEAN_SINGLE, 0); }
static void c_clean_single_nogc (void)
{
	clean_and_check ("c_clean_single_nogc", map2 (), 4096,
			 CLEAN_SINGLE | CLEAN_NO_GARB_COLL, 0);
}

/* lib128's own path: free () of a block whose copies are kept.  */
static void c_clean_free (void)
{
	int *p = malloc (64);
	g_copy = p;
	p[1] = 42;
	free (p);
	prap ("c_clean_free copy in a global", memq (&g_copy));
	printf ("c_clean_free: freed\n");
}

/* Many frees: when (if ever) lib128's deferred list is walked.  */
static void c_clean_free_many (void)
{
	for (int i = 1; i <= 5000; i++) {
		int *p = malloc (64);
		p[0] = i;
		progress = i;
		free (p);
		if (i % 1000 == 0 || i < 4) {
			printf ("c_clean_free_many: %d freed\n", i);
			fflush (stdout);
		}
	}
}

static void cmd_clean (void)
{
	long m = syscall (SYS_arch_prctl, 8193 /* PR_PM_DBG_MODE_GET */);
	printf ("clean: pm_sc_debug_mode=%lx\n", m);
	run ("c_clean_tagged", c_clean_tagged, 20);
	run ("c_clean_untagged", c_clean_untagged, 20);
	run ("c_clean_single", c_clean_single, 20);
	run ("c_clean_single_nogc", c_clean_single_nogc, 20);
	run ("c_clean_free", c_clean_free, 20);
	run ("c_clean_free_many", c_clean_free_many, 60);
}

/* ---- mremap: a moving mremap, with copies of the old pointer ---------- */

/* The first page of a two-page mapping grown to 64 MiB: the second page
   is in the way, so it moves.  */
static void c_mremap (void)
{
	char *two = mmap (NULL, 8192, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	char *m = __builtin_e2k_create_ap_subarray (two, 0, 4096);
	char *n;
	void *loc[1];

	m[0] = 5;
	g_copy = m;
	loc[0] = m;
	opaque (loc);
	n = mremap (m, 4096, 64 << 20, MREMAP_MAYMOVE);
	dumpp ("c_mremap old", m);
	dumpp ("c_mremap new", n);
	dumpp ("c_mremap the whole two-page mapping", two);
	prap ("c_mremap copy in a global", memq (&g_copy));
	prap ("c_mremap copy in a local array", memq (&loc[0]));
	printf ("c_mremap: moved=%d new[0]=%d\n",
		((union pw) { .p = m }).w[0] != ((union pw) { .p = n }).w[0],
		n[0]);
	fflush (stdout);
	sink = ((char *) loc[0])[0];
	printf ("c_mremap: read through the old pointer: %ld\n", sink);
}

static void cmd_mremap (void)
{
	long m = syscall (SYS_arch_prctl, 8193 /* PR_PM_DBG_MODE_GET */);
	printf ("mremap: pm_sc_debug_mode=%lx\n", m);
	run ("c_mremap", c_mremap, 20);
}

/* ---- psl: recursion until %usd.psl overflows -------------------------- */

#define PSL_LIMIT 200000L
static volatile long *shared;		/* MAP_SHARED: survives the child */
#define SH_DEEPEST 20			/* %usd.lo at the deepest level */

static long rec (long n);
static long (*volatile recp) (long) = rec;
static long rec_big (long n);
static long (*volatile rec_bigp) (long) = rec_big;
static volatile long vsrc[24];

/* %usd.lo is sampled at these levels into shared[1 + i].  */
static const long psl_levels[] = { 1, 1000, 10000, 32767, 32768, 60000,
				   65000, 65400, 65480, 65490, 65500 };
#define NLEVELS (sizeof psl_levels / sizeof psl_levels[0])

/* Inline, so that %usd.lo is the caller's.  */
static inline __attribute__ ((always_inline)) void psl_note (long n)
{
	u64 u;
	__asm__ __volatile__ ("rrd %%usd.lo, %0" : "=r" (u));
	shared[0] = n;
	shared[SH_DEEPEST] = (long) u;
	for (unsigned i = 0; i < NLEVELS; i++)
		if (n == psl_levels[i])
			shared[1 + i] = (long) u;
}

static NOINLINE long rec (long n)
{
	shared[0] = n;
	for (unsigned i = 0; i < NLEVELS; i++)
		if (n == psl_levels[i]) {
			u64 u;
			__asm__ __volatile__ ("rrd %%usd.lo, %0" : "=r" (u));
			shared[1 + i] = (long) u;
		}
	{
		u64 u;
		__asm__ __volatile__ ("rrd %%usd.lo, %0" : "=r" (u));
		shared[SH_DEEPEST] = (long) u;
	}
	if (n >= PSL_LIMIT)
		return n;
	return recp (n + 1) + 1;
}

/* The same with a bigger register window (24 values live across the
   call) and a 256-byte addressed local (a getsap per level): if the
   limit were the size of a stack, it would be reached sooner.  */
#define V8(k) vsrc[k] + n, vsrc[k + 1] + n, vsrc[k + 2] + n, vsrc[k + 3] + n, \
	vsrc[k + 4] + n, vsrc[k + 5] + n, vsrc[k + 6] + n, vsrc[k + 7] + n
static NOINLINE long rec_big (long n)
{
	volatile long buf[32];
	long v[24] = { V8 (0), V8 (8), V8 (16) };
	long s = 0;
	buf[n & 31] = n;
	psl_note (n);
	if (n >= PSL_LIMIT)
		return n;
	s = rec_bigp (n + 1);
	for (int i = 0; i < 24; i++)
		s += v[i];
	return s + buf[n & 31];
}

static void *psl_thread (void *arg)
{
	long r = rec (1);
	printf ("c_psl: returned from depth %ld (result %ld)\n", shared[0], r);
	return arg;
}

static void *psl_thread_big (void *arg)
{
	long r = rec_big (1);
	printf ("c_psl_big: returned from depth %ld (result %ld)\n", shared[0], r);
	return arg;
}

static void psl_in_thread (const char *name, void *(*fn) (void *))
{
	pthread_attr_t a;
	pthread_t t;

	pthread_attr_init (&a);
	pthread_attr_setstacksize (&a, 64 << 20);
	if (pthread_create (&t, &a, fn, NULL)) {
		printf ("%s: pthread_create failed\n", name);
		return;
	}
	pthread_join (t, NULL);
}

static void c_psl (void) { psl_in_thread ("c_psl", psl_thread); }
static void c_psl_big (void) { psl_in_thread ("c_psl_big", psl_thread_big); }

/* From the main thread (of the child): a higher starting psl.  */
static void c_psl_main (void)
{
	long r = rec (1);
	printf ("c_psl_main: returned from depth %ld (result %ld)\n", shared[0], r);
}

static void psl_report (const char *name)
{
	printf ("%s: deepest level reached %ld, psl there %#lx\n", name,
		shared[0], ((u64) shared[SH_DEEPEST] >> 32) & 0xffff);
	for (unsigned i = 0; i < NLEVELS; i++)
		if (shared[1 + i])
			printf ("%s: level %ld usd.lo=%016lx psl=%#lx\n", name,
				psl_levels[i], (u64) shared[1 + i],
				((u64) shared[1 + i] >> 32) & 0xffff);
	memset ((void *) shared, 0, 4096);
}

static void cmd_psl (void)
{
	shared = mmap (NULL, 4096, PROT_READ | PROT_WRITE,
		       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	memset ((void *) shared, 0, 4096);
	{
		u64 u;
		__asm__ __volatile__ ("rrd %%usd.lo, %0" : "=r" (u));
		long (*f) (long) = rec;
		printf ("psl: main usd.lo=%016lx psl=%#lx; rec at %lx\n", u,
			(u >> 32) & 0xffff, memq (&f).lo & 0xffffffffffffUL);
	}
	run ("c_psl", c_psl, 120);
	psl_report ("psl");
	run ("c_psl_big", c_psl_big, 120);
	psl_report ("psl big");
	run ("c_psl_main", c_psl_main, 120);
	psl_report ("psl main");
}

/* ---------------------------------------------------------------------- */

int main (int argc, char **argv, char **envp)
{
	setvbuf (stdout, NULL, _IOLBF, 0);
	printf ("pmleft %s\n", argc > 1 ? argv[1] : "(no command)");
	if (argc > 1 && strcmp (argv[1], "usd") == 0)
		cmd_usd (argc > 2 ? atoi (argv[2]) : 2000);
	else if (argc > 1 && strcmp (argv[1], "cud") == 0)
		cmd_cud (argc, argv, envp);
	else if (argc > 1 && strcmp (argv[1], "clean") == 0)
		cmd_clean ();
	else if (argc > 1 && strcmp (argv[1], "mremap") == 0)
		cmd_mremap ();
	else if (argc > 1 && strcmp (argv[1], "psl") == 0)
		cmd_psl ();
	else {
		printf ("usage: pmleft usd [N] | cud | clean | mremap | psl\n");
		return 2;
	}
	printf ("done\n");
	return 0;
}
