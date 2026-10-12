/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Protected-mode (e2k128) behaviour probe: what pointers look like, which
   misuses the hardware, the kernel and glibc's lib128 catch, and with what
   signal; system calls, signals, threads.  Each case that may fault runs
   in its own child process (killed after 20 s if it hangs).  Build and
   run with run.sh:

     tools/abi/probes/protected/run.sh --remote claude@HOST [--static v3]

   or by hand on a machine with /lib128 (the 8C2, the 2C3):

     lcc -mptr128 -O1 pmrun.c -o pmrun -lpthread
     ./pmrun 2>&1          all cases
     ./pmrun uaf 2>&1      the malloc/free cases (run under PM_* settings)
     ./pmrun sys 2>&1      the system call argument cases
     ./pmrun va 2>&1       the va_arg type-mismatch cases
     ./pmrun free1 2>&1    one malloc and free, without a child process

   (for the 4C, which has no /lib128, build a static program on the host
   with /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc ... -static).

   Every line is "case: result".  A case that faults prints
   "case: SIGNAME code=N addr=..." from its handler; a case killed without
   reaching the handler prints "case: killed by SIGNAME".  */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#define NOINLINE __attribute__ ((noinline))

/* ---- looking at a pointer ------------------------------------------- */

union pw { void *p; unsigned long w[2]; };

/* The bits and tags of the two doublewords of a 16-byte object, loaded
   with one ldapq into a register pair and read inside one asm statement
   (%H names the pair's second register): a compiler copy of a half
   (movtd) would turn tag 3 into 0.  out[0..1] = bits, out[2..3] = tags
   (bits 0-1 the low 32-bit half, 2-3 the high half).  */
static NOINLINE void rawq (const void *obj, unsigned long out[4])
{
	__int128 q;
	unsigned long b0, b1, t0, t1;
	__asm__ __volatile__ (
		"ldapq %5, 0x0, %0\n\t"
		"gettagd %0, %3\n\t"
		"gettagd %H0, %4\n\t"
		"puttagd %0, 0x0, %1\n\t"
		"puttagd %H0, 0x0, %2\n\t"
		"puttagd %3, 0x0, %3\n\t"
		"puttagd %4, 0x0, %4\n\t"
		"andd %3, 0xf, %3\n\t"
		"andd %4, 0xf, %4"
		: "=&r" (q), "=&r" (b0), "=&r" (b1), "=&r" (t0), "=&r" (t1)
		: "r" (obj) : "memory");
	out[0] = b0;
	out[1] = b1;
	out[2] = t0;
	out[3] = t1;
}

/* The same through two doubleword loads (ldapd): what code that reads a
   pointer's halves as integers gets.  */
static NOINLINE void rawd (const void *obj, unsigned long out[4])
{
	unsigned long b0, b1, t0, t1;
	__asm__ __volatile__ (
		"ldapd %4, 0x0, %0\n\t"
		"ldapd %4, 0x8, %1\n\t"
		"gettagd %0, %2\n\t"
		"gettagd %1, %3\n\t"
		"puttagd %0, 0x0, %0\n\t"
		"puttagd %1, 0x0, %1\n\t"
		"puttagd %2, 0x0, %2\n\t"
		"puttagd %3, 0x0, %3\n\t"
		"andd %2, 0xf, %2\n\t"
		"andd %3, 0xf, %3"
		: "=&r" (b0), "=&r" (b1), "=&r" (t0), "=&r" (t1)
		: "r" (obj) : "memory");
	out[0] = b0;
	out[1] = b1;
	out[2] = t0;
	out[3] = t1;
}

/* The tag of a doubleword value in a register (a copy may already have
   lost tag 3).  */
static unsigned long tagof (unsigned long x)
{
	unsigned long t;
	__asm__ __volatile__ ("gettagd %1, %0\n\tputtagd %0, 0, %0\n\t"
			      "andd %0, 0xf, %0" : "=r" (t) : "r" (x));
	return t;
}

static unsigned long untag (unsigned long x)
{
	unsigned long t;
	__asm__ __volatile__ ("puttagd %1, 0, %0" : "=r" (t) : "r" (x));
	return t;
}

/* A pointer's two doublewords, their tags and the AP fields as lcc's
   builtins read them.  */
static NOINLINE void dump (const char *what, void *p)
{
	union pw u;
	unsigned long r[4];

	unsigned long d[4];

	u.p = p;
	rawq (&u, r);
	rawd (&u, d);
	printf ("%s: lo=%016lx hi=%016lx tags=%lx,%lx (ldapd: %lx,%lx) base=%llx size=%llx cur=%llx\n",
		what, r[0], r[1], r[2], r[3], d[2], d[3],
		__builtin_e2k_get_ap_base (p), __builtin_e2k_get_ap_size (p),
		__builtin_e2k_get_ap_curptr (p));
}

/* ---- running a case in a child --------------------------------------- */

static const char *cur_case;

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
	case SIGSYS: return "SIGSYS";
	case SIGALRM: return "SIGALRM (hung: 20 s)";
	default: return "SIG?";
	}
}

/* lib128's siginfo_t declares si_addr as unsigned long (glibc
   bits/types/siginfo_t.h); print it and the doubleword after it.  */
static void fault_handler (int sig, siginfo_t *si, void *uc)
{
	unsigned long r[4];

	rawq (&si->si_addr, r);
	printf ("%s: %s code=%d addr=%016lx,%016lx tags=%lx,%lx\n", cur_case,
		signame (sig), si->si_code, r[0], r[1], r[2], r[3]);
	fflush (stdout);
	_exit (0);
}

static void run (const char *name, void (*fn) (void))
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
		alarm (20);	/* lib128's malloc can deadlock on a corrupted heap */
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

/* Keep values alive without letting lcc see through them.  */
static NOINLINE void *opaque (void *p) { return p; }
static NOINLINE long opaquel (long x) { return x; }
static volatile long sink;

/* ---- data layout ----------------------------------------------------- */

struct cp { char c; void *p; };
struct ip { int i; void *p; int j; };
struct fp_s { char c; int (*f) (void); };
union upl { void *p; long l; };

static void layout (void)
{
	printf ("layout: void*=%zu/%zu fn*=%zu/%zu long=%zu size_t=%zu "
		"intptr_t=%zu ptrdiff_t=%zu long double=%zu/%zu\n",
		sizeof (void *), _Alignof (void *),
		sizeof (int (*) (void)), _Alignof (int (*) (void)),
		sizeof (long), sizeof (size_t), sizeof (intptr_t),
		sizeof (ptrdiff_t), sizeof (long double),
		_Alignof (long double));
	printf ("layout: struct{char;void*}=%zu off(p)=%zu "
		"struct{int;void*;int}=%zu off(p)=%zu off(j)=%zu "
		"struct{char;fn*}=%zu union{void*;long}=%zu max_align_t=%zu\n",
		sizeof (struct cp), offsetof (struct cp, p),
		sizeof (struct ip), offsetof (struct ip, p),
		offsetof (struct ip, j), sizeof (struct fp_s),
		sizeof (union upl), _Alignof (max_align_t));
	printf ("layout: jmp_buf=%zu sigjmp_buf=%zu va_list=%zu "
		"siginfo_t=%zu ucontext_t=%zu struct sigaction=%zu "
		"pthread_t=%zu pthread_mutex_t=%zu FILE*=%zu\n",
		sizeof (jmp_buf), sizeof (sigjmp_buf), sizeof (va_list),
		sizeof (siginfo_t), sizeof (ucontext_t),
		sizeof (struct sigaction), sizeof (pthread_t),
		sizeof (pthread_mutex_t), sizeof (FILE *));
}

/* ---- what pointers look like ---------------------------------------- */

int g_arr[10];
static int s_arr[3];
int *g_init_ptr = &g_arr[2];
const char *g_str = "literal";
struct two { int a[4]; int b; };
__thread int t_var;
int (*g_fptr) (void);

static NOINLINE int ret7 (void) { return 7; }

static NOINLINE void dump_local (void)
{
	int l;
	int la[4];
	struct two t;
	dump ("ptr &local int", &l);
	dump ("ptr local int[4]", la);
	dump ("ptr &local[2]", &la[2]);
	dump ("ptr struct.a (int[4] member)", t.a);
	dump ("ptr &struct.b", &t.b);
	dump ("ptr alloca(24)", __builtin_alloca (24));
}

static void pointers (int argc, char **argv, char **envp)
{
	int *m = malloc (40);
	dump ("ptr malloc(40)", m);
	dump ("ptr malloc(40)+3", m + 3);
	dump ("ptr malloc(1)", malloc (1));
	dump ("ptr malloc(0)", malloc (0));
	dump ("ptr calloc(3,8)", calloc (3, 8));
	dump ("ptr &g_arr[3]", &g_arr[3]);
	dump ("ptr &s_arr[1]", &s_arr[1]);
	dump ("ptr g_init_ptr (=&g_arr[2])", g_init_ptr);
	dump ("ptr string literal", (void *) g_str);
	dump ("ptr &t_var (TLS)", &t_var);
	dump ("ptr argv", argv);
	dump ("ptr argv[0]", argv[0]);
	dump ("ptr envp", envp);
	dump ("ptr stdout", stdout);
	dump ("ptr &errno", &errno);
	dump ("ptr NULL", NULL);
	dump ("ptr (int*)(long)0x1234", (int *) opaquel (0x1234));
	dump_local ();
	printf ("ptr: argc=%d argv[0]=%s\n", argc, argv[0]);
	free (m);
}

/* Function pointers: the raw bits first, then the AP builtins (the
   2C3 stops with SIGILL somewhere in dump () for a v6 PL).  */
static void f_fnptr_dump (void)
{
	union pw u;
	unsigned long r[4];
	u.p = (void *) ret7;
	rawq (&u, r);
	printf ("f_fnptr_dump: ptr fn ret7 raw: lo=%016lx hi=%016lx tags=%lx,%lx\n",
		r[0], r[1], r[2], r[3]);
	fflush (stdout);
	rawd (&u, r);
	printf ("f_fnptr_dump: ldapd tags %lx,%lx\n", r[2], r[3]);
	fflush (stdout);
	printf ("f_fnptr_dump: get_ap_base %llx\n",
		__builtin_e2k_get_ap_base ((void *) ret7));
	fflush (stdout);
	dump ("ptr fn ret7", (void *) ret7);
	dump ("ptr fn printf", (void *) printf);
}

static void f_is_ap (void)
{
	int *m = malloc (4);
	printf ("f_is_ap: is_ap(malloc)=%d\n", __builtin_e2k_is_ptr_ap (m));
	printf ("f_is_ap: is_ap(fn)=%d\n", __builtin_e2k_is_ptr_ap ((void *) ret7));
	printf ("f_is_ap: is_ap((int*)(long)p)=%d is_nv=%d\n",
		__builtin_e2k_is_ptr_ap ((int *) opaquel ((long) m)),
		__builtin_e2k_is_ptr_nv ((int *) opaquel ((long) m)));
}
static void f_printf_p (void)
{
	int *m = malloc (4);
	printf ("f_printf_p: %%p of malloc=%p\n", (void *) m);
	printf ("f_printf_p: %%p of &g_arr[3]=%p\n", (void *) &g_arr[3]);
	printf ("f_printf_p: %%p of NULL=%p\n", NULL);
}

static void f_is_pl (void)
{
	int *m = malloc (4);
	int (*f) (void) = ret7;
	printf ("f_is_pl: is_pl(fn)=%d is_pl(malloc)=%d\n",
		__builtin_e2k_is_ptr_pl ((void *) f), __builtin_e2k_is_ptr_pl (m));
}
static void f_printf_fn (void)
{
	int (*f) (void) = ret7;
	printf ("f_printf_fn: %%p of fn=%p\n", (void *) f);
}
static void f_fn_to_long (void)
{
	int (*f) (void) = (int (*) (void)) opaque ((void *) ret7);
	printf ("f_fn_to_long: %lx\n", (long) (void *) f);
}

/* ---- pointer arithmetic and conversions ------------------------------ */

static void arith (void)
{
	int *p = malloc (40);
	int *q = malloc (40);
	int *e = (int *) opaque (p + 10);	/* one past the end */
	int *far = (int *) opaque (p + 100);	/* far outside, never used */
	int *neg = (int *) opaque (p - 1);
	long l = (long) p;
	int *back = (int *) l;

	dump ("arith p+10", e);
	dump ("arith p+100", far);
	dump ("arith p-1", neg);
	printf ("arith: (long)p=%lx (long)(p+1)-(long)p=%ld p+10-p=%ld "
		"q-p=%ld p<q=%d p==p+0=%d (long)p==base+cur=%d\n",
		l, (long) (p + 1) - l, (long) (e - p), (long) (q - p),
		p < q, p == (int *) opaque (p),
		l == (long) (__builtin_e2k_get_ap_base (p)
			     + __builtin_e2k_get_ap_curptr (p)));
	dump ("arith (int*)(long)p", back);
	printf ("arith: (p-1)+1==p %d, *((p-1)+1)=%d\n",
		(int *) opaque (neg + 1) == p, (p[0] = 5, *(neg + 1)));
	sink = (long) far;
}

/* ---- faults ---------------------------------------------------------- */

static int *heap40 (void) { int *p = malloc (40); memset (p, 0, 40); return p; }

static void f_oob_write_end (void) { int *p = heap40 (); ((int *) opaque (p))[10] = 1; }
static void f_oob_write_neg (void) { int *p = heap40 (); ((int *) opaque (p))[-1] = 1; }
static void f_oob_read_end (void) { int *p = heap40 (); sink = ((int *) opaque (p))[10]; }
static void f_oob_read_far (void) { int *p = heap40 (); sink = ((int *) opaque (p))[100000]; }
static void f_oob_partial (void)
{	/* an 8-byte read of the last 4 bytes */
	char *p = (char *) heap40 ();
	sink = *(long *) opaque (p + 36);
}
static void f_oob_global (void) { sink = ((int *) opaque (g_arr))[10]; }
static void f_oob_local (void)
{
	int la[4] = { 1, 2, 3, 4 };
	sink = ((int *) opaque (la))[4];
}
static void f_oob_member (void)
{	/* t.a[4] is still inside t: lcc narrows to the whole struct */
	struct two t = { { 1, 2, 3, 4 }, 5 };
	sink = ((int *) opaque (t.a))[4];
	printf ("f_oob_member: t.a[4]=%ld (t.b=%d)\n", sink, t.b);
}
static void f_oob_member2 (void)
{
	struct two t = { { 1, 2, 3, 4 }, 5 };
	sink = ((int *) opaque (t.a))[5];
}
static void f_strlit (void) { sink = ((const char *) opaque ((void *) g_str))[8]; }
static void f_strlit_write (void)
{	/* the literal's descriptor allows writing (rw = 3); the page? */
	((char *) opaque ((void *) g_str))[0] = 'L';
	printf ("f_strlit_write: wrote, now %s\n", g_str);
}
const int g_const[4] = { 1, 2, 3, 4 };
static void f_const_write (void)
{
	unsigned long r[4];
	const int *c = g_const;
	rawq (&c, r);
	printf ("f_const_write: &g_const lo=%016lx (rw=%lu)\n", r[0], (r[0] >> 59) & 3);
	fflush (stdout);
	((int *) opaque ((void *) g_const))[1] = 9;
	printf ("f_const_write: wrote, now %d\n", g_const[1]);
}
static void f_null_read (void) { sink = *(int *) opaque (NULL); }
static void f_null_write (void) { *(int *) opaque (NULL) = 1; }
static void f_int2ptr (void)
{	/* the address of a live object, as an integer */
	int *p = heap40 ();
	long a = (long) p;
	int *q = (int *) opaquel (a);
	printf ("f_int2ptr: a=%lx\n", a);
	fflush (stdout);
	sink = *q;
}
static void f_int2ptr_write (void)
{
	int *p = heap40 ();
	int *q = (int *) opaquel ((long) p);
	*q = 1;
}
static void f_uninit_heap (void)
{	/* malloc'd memory before it is written */
	int *p = malloc (16);
	int v = ((int *) opaque (p))[1];
	printf ("f_uninit_heap: read gives %d (tag %lx)\n", v, tagof (v));
	fflush (stdout);
	sink = v + 1;
}
static void f_uninit_heap_long (void)
{
	long *p = malloc (16);
	long v = ((long *) opaque (p))[0];
	printf ("f_uninit_heap_long: tag %lx raw %lx\n", tagof (v), untag (v));
	fflush (stdout);
	sink = v + 1;
}
static void f_uninit_ptr (void)
{	/* a pointer field never written */
	int **pp = malloc (32);
	int *p = ((int **) opaque (pp))[0];
	sink = *p;
}
static NOINLINE int uninit_local_helper (int c)
{
	int x;
	if (c)
		x = 3;
	return x + 1;
}
static void f_uninit_local (void)
{
	printf ("f_uninit_local: %d\n", uninit_local_helper ((int) opaquel (0)));
}
static void f_uaf_read (void)
{
	int *p = heap40 ();
	p[2] = 42;
	free (p);
	printf ("f_uaf_read: value %d\n", ((int *) opaque (p))[2]);
	fflush (stdout);
	dump ("f_uaf_read p after free", p);
}
static void f_uaf_write (void)
{
	int *p = heap40 ();
	free (p);
	((int *) opaque (p))[2] = 1;
	dump ("f_uaf_write p after free", p);
}
static void f_uaf_reuse (void)
{	/* free, then malloc the same size: does the old pointer alias? */
	int *p = heap40 ();
	int *q;
	free (p);
	q = heap40 ();
	q[0] = 7;
	printf ("f_uaf_reuse: same base %d, old[0]=%d\n",
		__builtin_e2k_get_ap_base (p) == __builtin_e2k_get_ap_base (q),
		((int *) opaque (p))[0]);
}
int *g_copy;
static void f_uaf_copies (void)
{	/* copies of a freed pointer in a global, the heap and a register */
	int *p = heap40 ();
	int **h = malloc (16);
	unsigned long r[4];
	g_copy = p;
	*h = p;
	free (p);
	rawq (&g_copy, r);
	printf ("f_uaf_copies: global copy after free tags=%lx,%lx\n", r[2], r[3]);
	rawq (h, r);
	printf ("f_uaf_copies: heap copy after free tags=%lx,%lx\n", r[2], r[3]);
	dump ("f_uaf_copies: register copy after free", p);
	fflush (stdout);
	sink = g_copy[1];
	printf ("f_uaf_copies: read through the global copy: %ld\n", sink);
	fflush (stdout);
	sink = (*h)[1];
	printf ("f_uaf_copies: read through the heap copy: %ld\n", sink);
	fflush (stdout);
	sink = ((int *) opaque (p))[1];
	printf ("f_uaf_copies: read through the register copy: %ld\n", sink);
}
static void f_double_free (void) { int *p = heap40 (); free (p); free (opaque (p)); }
static void f_free_interior (void) { int *p = heap40 (); free (opaque (p + 1)); }
static void f_free_stack (void) { int l[4]; free (opaque (l)); }
static void f_realloc_old (void)
{
	int *p = heap40 ();
	int *q = realloc (p, 4000);
	printf ("f_realloc_old: moved %d\n", p != q);
	fflush (stdout);
	sink = ((int *) opaque (p))[1];
}

/* Pointers through integer memory.  */
static void f_halves_copy (void)
{	/* copy the two doublewords separately */
	int *p = heap40 ();
	union pw a, b;
	a.p = p;
	b.w[0] = a.w[0];
	b.w[1] = a.w[1];
	((int *) b.p)[1] = 3;
	printf ("f_halves_copy: p[1]=%d\n", p[1]);
}
static void f_halves_mix (void)
{	/* low half of one pointer, high half of another */
	int *p = heap40 (), *q = malloc (400);
	union pw a, b, c;
	a.p = p;
	b.p = q;
	c.w[0] = a.w[0];
	c.w[1] = b.w[1];
	dump ("f_halves_mix c", c.p);
	((int *) c.p)[50] = 3;
}
static void f_halves_arith (void)
{	/* integer arithmetic on the low (base) half */
	int *p = heap40 ();
	union pw a;
	a.p = p;
	a.w[0] = opaquel (a.w[0]) + 4;
	dump ("f_halves_arith", a.p);
	((int *) a.p)[1] = 3;
}
static void f_hi_arith (void)
{	/* integer arithmetic on the high (size, curptr) half */
	int *p = heap40 ();
	union pw a;
	a.p = p;
	a.w[1] = opaquel (a.w[1]) + 0x10000000000L;	/* size + 256 */
	dump ("f_hi_arith", a.p);
	((int *) a.p)[20] = 3;
}
static void f_bytecopy (void)
{
	int *p = heap40 ();
	int *q;
	unsigned char *s = (unsigned char *) &p, *d = (unsigned char *) &q;
	for (int i = 0; i < 16; i++)
		((volatile unsigned char *) d)[i] = ((volatile unsigned char *) s)[i];
	dump ("f_bytecopy q", q);
	q[1] = 3;
}
static void f_memcpy (void)
{
	int *p = heap40 ();
	int *q;
	memcpy (&q, &p, sizeof p);
	dump ("f_memcpy q", q);
	q[1] = 3;
	printf ("f_memcpy: p[1]=%d\n", p[1]);
}
static void f_memcpy_misaligned (void)
{
	int *p = heap40 ();
	char buf[48];
	int *q;
	memcpy (buf + 8, &p, sizeof p);
	memcpy (&q, buf + 8, sizeof p);
	dump ("f_memcpy_misaligned q", q);
	q[1] = 3;
}
static void f_memmove_struct (void)
{	/* struct copy of an array of pointers */
	struct { int *a[3]; } s, t;
	int *p = heap40 ();
	s.a[0] = s.a[1] = s.a[2] = p;
	t = s;
	t.a[2][3] = 9;
	printf ("f_memmove_struct: %d\n", p[3]);
}
static void f_misaligned_int (void)
{
	char *c = malloc (16);
	*(int *) opaque (c + 1) = 5;
	printf ("f_misaligned_int: %d\n", *(int *) opaque (c + 1));
}
static void f_misaligned_ptr_store (void)
{	/* a pointer stored at an 8-byte, not 16-byte, aligned address */
	char *c = malloc (48);
	int *p = heap40 ();
	*(int **) opaque (c + 8) = p;
	printf ("f_misaligned_ptr_store: stored\n");
	fflush (stdout);
	(*(int **) opaque (c + 8))[1] = 3;
}

/* Stack pointers escaping their frame.  */
int *g_esc;
int **g_escp;
static NOINLINE void esc_store (void)
{
	int l = 5;
	g_esc = &l;			/* stack address into a global */
	printf ("esc_store: stored\n");
	fflush (stdout);
}
static void f_stack_to_global (void) { esc_store (); }
static void f_stack_to_heap (void)
{
	int l = 5;
	int **h = malloc (16);
	*h = &l;
	printf ("f_stack_to_heap: %d\n", **h);
}
static NOINLINE void esc_down (int **slot)
{
	int l = 5;
	*slot = &l;			/* into the caller's (older) frame */
	printf ("esc_down: stored\n");
	fflush (stdout);
}
static void f_stack_to_older_frame (void)
{
	int *slot;
	esc_down (&slot);
	printf ("f_stack_to_older_frame: back, deref\n");
	fflush (stdout);
	sink = *slot;
}
static NOINLINE void esc_up (int **slot, int *l)
{
	*slot = l;			/* an older frame's address into a younger frame: fine */
}
static void f_stack_to_younger_frame (void)
{
	int l = 4;
	int *slot[1];
	esc_up (slot, &l);
	printf ("f_stack_to_younger_frame: %d\n", *slot[0]);
}
static NOINLINE int *ret_local (void)
{
	int l = 5;
	return (int *) opaque (&l);
}
static void f_return_local (void)
{
	int *p = ret_local ();
	printf ("f_return_local: returned\n");
	fflush (stdout);
	dump ("f_return_local p", p);
	sink = *p;
}

/* Calls through pointers.  */
static void f_call_fptr (void)
{
	int (*f) (void) = (int (*) (void)) opaque ((void *) ret7);
	printf ("f_call_fptr: %d, ==ret7 %d\n", f (), f == ret7);
}
static void f_call_int (void)
{
	union pw u;
	long a;
	int (*f) (void);
	u.p = (void *) ret7;
	a = untag (u.w[0]);		/* the code address, untagged */
	f = (int (*) (void)) opaquel (a);
	printf ("f_call_int: a=%lx\n", a);
	fflush (stdout);
	sink = f ();
}
static void f_call_data (void)
{
	int *p = heap40 ();
	int (*f) (void) = (int (*) (void)) opaque (p);
	sink = f ();
}
static void f_call_null (void)
{
	int (*f) (void) = (int (*) (void)) opaque (NULL);
	sink = f ();
}
static void f_deref_fptr (void)
{	/* read the code a function pointer points to */
	sink = *(int *) opaque ((void *) ret7);
}

/* Variadic misuse (lcc manual's -mprot-va-arg-semi-spec example).  */
static NOINLINE int va_two (int a, ...)
{
	va_list va;
	int b, c;
	va_start (va, a);
	b = va_arg (va, int);
	c = va_arg (va, int);
	va_end (va);
	if (a == 1)
		return b;
	return b + c;
}
static void f_va_missing (void)
{
	printf ("f_va_missing: two args %d\n", va_two (2, 1000, 2000));
	fflush (stdout);
	printf ("f_va_missing: one arg %d\n", va_two (1, 1000));
}
static NOINLINE long va_ptr (int n, ...)
{
	va_list va;
	long s = 0;
	va_start (va, n);
	for (int i = 0; i < n; i++)
		s += *va_arg (va, int *);
	va_end (va);
	return s;
}
static void f_va_int_as_ptr (void)
{	/* the lcc manual's -mprot-abi-promotion case: a long passed for %p */
	int x = 3;
	printf ("f_va_int_as_ptr: ptr %ld\n", va_ptr (1, &x));
	fflush (stdout);
	printf ("f_va_int_as_ptr: long %ld\n", va_ptr (1, (long) &x));
}

static NOINLINE long va_long (int n, ...)
{
	va_list va;
	long v;
	va_start (va, n);
	v = va_arg (va, long);
	va_end (va);
	return v;
}
static void f_va_int_as_long (void)
{	/* an int passed, read as long: the slot's upper half was never written */
	printf ("f_va_int_as_long: long passed %ld\n", va_long (1, 5L));
	fflush (stdout);
	printf ("f_va_int_as_long: int passed %ld\n", va_long (1, 5));
}
static void f_va_int_as_long_stale (void)
{	/* the same right after a call that left a pointer in that slot of
	   this function's argument area: the int overwrites only its low
	   4 bytes */
	int x = 3;
	long r1, r;
	r1 = va_ptr (1, &x);
	r = va_long (1, 5);
	printf ("f_va_int_as_long_stale: ptr first %ld, then int read as long: %lx\n", r1,
		untag (r));
	printf ("f_va_int_as_long_stale: its tag %lx\n", tagof (r));
	fflush (stdout);
	sink = r + 1;
	printf ("f_va_int_as_long_stale: used it: %ld\n", sink);
}

/* ---- system calls ---------------------------------------------------- */

static void f_sys_write_ok (void)
{
	char buf[8] = "abcdefg";
	long r = write (1, buf, 0);
	printf ("f_sys_write_ok: write(1, buf, 0)=%ld; ", r);
	fflush (stdout);
	r = write (1, buf, 4);
	printf (" -> %ld\n", r);
}
static void f_sys_write_over (void)
{	/* count larger than the buffer's descriptor: how much is written? */
	char buf[8] = "abcdefg";
	char in[200];
	int fd[2];
	long r, n;
	pipe (fd);
	errno = 0;
	r = write (fd[1], opaque (buf), 100);
	printf ("f_sys_write_over: write(pipe, char[8], 100)=%ld errno=%d\n", r, errno);
	close (fd[1]);
	n = read (fd[0], in, sizeof in);
	printf ("f_sys_write_over: the pipe held %ld bytes\n", n);
}
static void f_sys_read_over (void)
{	/* count larger than the buffer: is the next block overwritten? */
	char *buf = malloc (8);
	char *next = malloc (64);
	int fd = open ("/dev/zero", O_RDONLY);
	long r;
	memset (next, 0x55, 64);
	errno = 0;
	r = read (fd, opaque (buf), 100);
	printf ("f_sys_read_over: read(/dev/zero, malloc(8), 100)=%ld errno=%d "
		"next block (%ld bytes on) [0]=%x [63]=%x\n", r, errno,
		(long) next - (long) buf, next[0] & 0xff, next[63] & 0xff);
}
static void f_sys_big_malloc (void)
{
	void *p = malloc (0x80000000UL);
	printf ("f_sys_big_malloc: malloc(2 GiB)=%s errno=%d\n", p ? "ptr" : "NULL", errno);
}
static void f_sys_int_buf (void)
{	/* an integer as the buffer pointer */
	char buf[8] = "xyz\n";
	long r;
	errno = 0;
	r = write (1, (char *) opaquel ((long) buf), 4);
	printf ("f_sys_int_buf: write(1, (char*)(long)buf, 4)=%ld errno=%d\n", r, errno);
}
static void f_sys_unterminated (void)
{
	char name[3] = { 'a', 'b', 'c' };
	int r;
	errno = 0;
	r = open (opaque (name), O_RDONLY);
	printf ("f_sys_unterminated: open(char[3] without NUL)=%d errno=%d\n", r, errno);
}
static void f_sys_syscall (void)
{
	long r1 = syscall (SYS_getpid);
	long r2 = syscall (SYS_write, 1, "syscall-write\n", 14);
	printf ("f_sys_syscall: getpid %d write %ld\n", r1 == getpid (), r2);
}
static void f_sys_mmap (void)
{
	char *m = mmap (NULL, 8192, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	dump ("f_sys_mmap", m);
	m[8191] = 1;
	printf ("f_sys_mmap: wrote last byte\n");
	fflush (stdout);
	munmap (m, 8192);
	printf ("f_sys_mmap: unmapped, reading\n");
	fflush (stdout);
	sink = m[0];
}
static void f_sys_mmap_over (void)
{
	char *m = mmap (NULL, 100, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	dump ("f_sys_mmap_over", m);
	sink = ((char *) opaque (m))[200];	/* inside the page, past 100 */
}
static void f_sys_dbgmode (void)
{
	long m = syscall (SYS_arch_prctl, 8193 /* PR_PM_DBG_MODE_GET */);
	printf ("f_sys_dbgmode: pm_sc_debug_mode=%lx\n", m);
}

/* ---- signals --------------------------------------------------------- */

static volatile int got_sig;

static void usr_handler (int sig, siginfo_t *si, void *uc)
{
	int l;
	got_sig = sig;
	printf ("sig: handler sig=%d si_signo=%d si_code=%d\n", sig,
		si->si_signo, si->si_code);
	dump ("sig: si", si);
	dump ("sig: uc", uc);
	dump ("sig: &local in handler", &l);
	dump ("sig: &si->si_addr", &si->si_addr);
	fflush (stdout);
}

static void f_signal (void)
{
	struct sigaction sa;
	int l;
	memset (&sa, 0, sizeof sa);
	sa.sa_sigaction = usr_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction (SIGUSR1, &sa, NULL);
	dump ("sig: &local in main", &l);
	raise (SIGUSR1);
	printf ("sig: after raise got %d\n", got_sig);
}

static void f_sigqueue (void)
{
	struct sigaction sa;
	union sigval v;
	int x = 11;
	memset (&sa, 0, sizeof sa);
	sa.sa_sigaction = usr_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction (SIGUSR2, &sa, NULL);
	v.sival_ptr = &x;
	sigqueue (getpid (), SIGUSR2, v);
	printf ("sig: sigqueue done\n");
}

/* ---- misc runtime ---------------------------------------------------- */

static jmp_buf jb;
static NOINLINE void jumper (void) { longjmp (jb, 3); }
static void f_setjmp (void)
{
	int r = setjmp (jb);
	if (r == 0)
		jumper ();
	printf ("f_setjmp: returned %d\n", r);
}

static int cmp (const void *a, const void *b)
{
	return *(const int *) a - *(const int *) b;
}
static void f_qsort (void)
{
	int a[5] = { 5, 1, 4, 2, 3 };
	qsort (a, 5, sizeof a[0], cmp);
	printf ("f_qsort: %d %d %d %d %d\n", a[0], a[1], a[2], a[3], a[4]);
}

static void *thr (void *arg)
{
	int l;
	dump ("f_thread: &local in thread", &l);
	dump ("f_thread: arg", arg);
	t_var = 5;
	dump ("f_thread: &t_var in thread", &t_var);
	return arg;
}
static void f_thread (void)
{
	pthread_t t;
	void *r;
	int x;
	int e = pthread_create (&t, NULL, thr, &x);
	if (e) {
		printf ("f_thread: pthread_create=%d\n", e);
		return;
	}
	pthread_join (t, &r);
	printf ("f_thread: joined, same ptr %d\n", r == (void *) &x);
}

static void f_maps (void)
{
	char line[256];
	FILE *f = fopen ("/proc/self/maps", "r");
	if (!f)
		return;
	while (fgets (line, sizeof line, f))
		printf ("maps: %s", line);
	fclose (f);
}

/* ---------------------------------------------------------------------- */

int main (int argc, char **argv, char **envp)
{
	setvbuf (stdout, NULL, _IOLBF, 0);
	/* Print once here, so stdout's buffer is allocated before any child
	   corrupts the heap: lib128's malloc reports corruption with printf,
	   which would call malloc again under the malloc lock (a hang).  */
	printf ("pmrun %s\n", argc > 1 ? argv[1] : "all");
	if (argc > 1 && strcmp (argv[1], "sys") == 0) {
		/* the system call cases, for running under PROTECTED_MODE_SOFT=0 */
		run ("f_sys_dbgmode", f_sys_dbgmode);
		run ("f_sys_write_over", f_sys_write_over);
		run ("f_sys_read_over", f_sys_read_over);
		run ("f_sys_int_buf", f_sys_int_buf);
		run ("f_sys_unterminated", f_sys_unterminated);
		printf ("done\n");
		return 0;
	}
	if (argc > 1 && strcmp (argv[1], "free1") == 0) {
		/* one malloc and free in this process, without fork (run.sh
		   runs it under PM_MM_CHECK_4_DANGLING_POINTERS=1, where
		   lib128's free dies) */
		int *p = malloc (40);
		p[1] = 42;
		free (p);
		printf ("free1: freed, then read %d\n", ((int *) opaque (p))[1]);
		return 0;
	}
	if (argc > 1 && strcmp (argv[1], "va") == 0) {
		run ("f_va_int_as_long", f_va_int_as_long);
		run ("f_va_int_as_long_stale", f_va_int_as_long_stale);
		printf ("done\n");
		return 0;
	}
	if (argc > 1 && strcmp (argv[1], "uaf") == 0) {
		/* the cases that depend on lib128's malloc mode (PM_MM_* and
		   PM_MALLOC_MODE environment variables) */
		run ("f_sys_dbgmode", f_sys_dbgmode);
		run ("f_uninit_heap", f_uninit_heap);
		run ("f_uninit_heap_long", f_uninit_heap_long);
		run ("f_uaf_read", f_uaf_read);
		run ("f_uaf_write", f_uaf_write);
		run ("f_uaf_reuse", f_uaf_reuse);
		run ("f_uaf_copies", f_uaf_copies);
		run ("f_double_free", f_double_free);
		run ("f_realloc_old", f_realloc_old);
		printf ("done\n");
		return 0;
	}
	layout ();
	pointers (argc, argv, envp);
	arith ();
#define R(f) run (#f, f)
	R (f_fnptr_dump);
	R (f_is_ap);
	R (f_printf_p);
	R (f_is_pl);
	R (f_printf_fn);
	R (f_fn_to_long);
	R (f_oob_write_end);
	R (f_oob_write_neg);
	R (f_oob_read_end);
	R (f_oob_read_far);
	R (f_oob_partial);
	R (f_oob_global);
	R (f_oob_local);
	R (f_oob_member);
	R (f_oob_member2);
	R (f_strlit);
	R (f_strlit_write);
	R (f_const_write);
	R (f_null_read);
	R (f_null_write);
	R (f_int2ptr);
	R (f_int2ptr_write);
	R (f_uninit_heap);
	R (f_uninit_heap_long);
	R (f_uninit_ptr);
	R (f_uninit_local);
	R (f_uaf_read);
	R (f_uaf_write);
	R (f_uaf_reuse);
	R (f_uaf_copies);
	R (f_double_free);
	R (f_free_interior);
	R (f_free_stack);
	R (f_realloc_old);
	R (f_halves_copy);
	R (f_halves_mix);
	R (f_halves_arith);
	R (f_hi_arith);
	R (f_bytecopy);
	R (f_memcpy);
	R (f_memcpy_misaligned);
	R (f_memmove_struct);
	R (f_misaligned_int);
	R (f_misaligned_ptr_store);
	R (f_stack_to_global);
	R (f_stack_to_heap);
	R (f_stack_to_older_frame);
	R (f_stack_to_younger_frame);
	R (f_return_local);
	R (f_call_fptr);
	R (f_call_int);
	R (f_call_data);
	R (f_call_null);
	R (f_deref_fptr);
	R (f_va_missing);
	R (f_va_int_as_ptr);
	R (f_va_int_as_long);
	R (f_va_int_as_long_stale);
	R (f_sys_write_ok);
	R (f_sys_write_over);
	R (f_sys_read_over);
	R (f_sys_big_malloc);
	R (f_sys_int_buf);
	R (f_sys_unterminated);
	R (f_sys_syscall);
	R (f_sys_mmap);
	R (f_sys_mmap_over);
	R (f_sys_dbgmode);
	R (f_signal);
	R (f_sigqueue);
	R (f_setjmp);
	R (f_qsort);
	R (f_thread);
	R (f_maps);
	printf ("done\n");
	return 0;
}
