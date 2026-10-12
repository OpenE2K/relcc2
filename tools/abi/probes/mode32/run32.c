/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 32-bit mode (lcc -mptr32) at run time, for spec/abi/mode32.md: type
   sizes, calls across a library, variadic calls, results, system calls
   through each entry, errno, signals and their frames, TLS in threads,
   setjmp and contexts, the address space, and the address forms the
   hardware accepts (mode32.s). Arguments: `maps' also prints
   /proc/self/maps, `t64' stops after the sizes and times, `qemu' skips
   what the community qemu-e2k32 can't run. run.sh builds it (dynamic,
   with librun32.so, and static), as run32t with -D_TIME_BITS=64
   -D_FILE_OFFSET_BITS=64, and as the 64-bit run64 for comparison (the
   32-bit system call entries from a 64-bit process; ldgd* and getpl are
   left out there), and runs them on hardware or under qemu-e2k32:

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -mptr32 -O2 -fPIC -shared run32-lib.c -o librun32.so
     $L -mptr32 -O2 run32.c mode32.s -L. -lrun32 -Wl,-rpath,'$ORIGIN' \
         -lpthread -o run32
     ./run32  */

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>
#include <wchar.h>
#include "run32.h"

#define SZ(T) printf ("size %-22s %3d %3d\n", #T, (int) sizeof (T), \
		      (int) _Alignof (T))

static void sizes (void)
{
	SZ (char); SZ (short); SZ (int); SZ (long); SZ (long long);
	SZ (__int128); SZ (void *); SZ (void (*) (void)); SZ (float);
	SZ (double); SZ (long double); SZ (__float128); SZ (_Complex double);
	SZ (size_t); SZ (ptrdiff_t); SZ (intptr_t); SZ (wchar_t);
	SZ (max_align_t); SZ (va_list); SZ (time_t); SZ (off_t);
	SZ (off64_t); SZ (clock_t); SZ (struct timespec); SZ (struct stat);
	SZ (struct stat64); SZ (sigset_t); SZ (jmp_buf); SZ (sigjmp_buf);
	SZ (ucontext_t); SZ (mcontext_t); SZ (stack_t); SZ (siginfo_t);
	SZ (struct sigaction); SZ (pthread_t); SZ (pthread_mutex_t);
	SZ (pthread_attr_t); SZ (FILE *);
	printf ("offset ucontext_t.uc_mcontext %d .uc_sigmask %d .uc_extra %d\n",
		(int) offsetof (ucontext_t, uc_mcontext),
		(int) offsetof (ucontext_t, uc_sigmask),
		(int) offsetof (ucontext_t, uc_extra));
	struct bf { char c; long long ll; char d; };
	printf ("struct {char; long long; char;} size %d align %d ll at %d\n",
		(int) sizeof (struct bf), (int) _Alignof (struct bf),
		(int) offsetof (struct bf, ll));
	printf ("char signed %d, wchar_t signed %d\n", (char) -1 < 0,
		(wchar_t) -1 < 0);
}

static int st_fn (int x) { return x * 7; }

static void calls (void)
{
	struct s12 s = { 1, 2, 3 };
	struct s16 t = { 4, 5 };
	struct s4 u = { 6, 7 };
	long long want = MIXED_SUM ((char) -8, 1LL << 40, s, -9, t, 2.5, u,
				    1.5f, (long double) 3.25, &s,
				    (unsigned char) 200, (short) -300);
	long long got = lib_mixed (-8, 1LL << 40, s, -9, t, 2.5, u, 1.5f,
				   3.25L, &s, 200, -300);
	printf ("call mixed: got %lld want %lld %s\n", got, want,
		got == want ? "ok" : "BAD");

	struct s12 v = { 10, 20, 30 };
	long long w = 0;
	w = w * 3 + 1; w = w * 3 + 2; w = w * 3 + 3;
	w = w * 3 + (1LL << 35); w = w * 3 + 6; w = w * 3 + 7;
	w = w * 3 + 3; w = w * 3 + 10 + 20 * 5 + 30 * 7;
	got = lib_va (3, 1, 2, 3, 1LL << 35, 6.0, 7.0L, "abc", v);
	printf ("call va: got %lld want %lld %s\n", got, w,
		got == w ? "ok" : "BAD");

	struct s68 r = lib_ret68 (100);
	printf ("call ret68: a[0] %d a[16] %d %s\n", r.a[0], r.a[16],
		r.a[0] == 100 && r.a[16] == 116 ? "ok" : "BAD");
	struct s12 q = lib_ret12 (5);
	printf ("call ret12: %d %d %d\n", q.a, q.b, q.c);
	printf ("call narrow results: char %d ushort %d long %ld llong %lld\n",
		lib_rc (100), lib_rus (30000), lib_rl (-5), lib_rll (1LL << 40));
	printf ("call fp: lib static %d, lib_fn %d, equal %d\n",
		lib_fp () (2), lib_fn_addr () (2), lib_fn_addr () == lib_fn);
	printf ("call raw int arg: %016llx\n", m32_raw (0x12345678));
	printf ("lib data %d at %p, lib tls %d at %p\n", lib_data,
		(void *) lib_data_addr (), lib_tls, (void *) lib_tls_addr ());
}

/* `qemu' on the command line: the community qemu-e2k32 has only entries
   1 and 5 (a call through entry 3 or 6 kills it), so skip those, and it
   kills the program at its second fault, so make only one.  */
static int only32;

static long long sys3 (long long nr, long long a1, long long a2, long long a3)
{
	return only32 ? -1000 : m32_sys3 (nr, a1, a2, a3);
}

static long long sys6 (long long nr, long long a1, long long a2, long long a3)
{
	return only32 ? -1000 : m32_sys6 (nr, a1, a2, a3);
}

static void sysc (void)
{
	/* Static, so that its address fits in 32 bits in run64 too.  */
	static char msg[] = "sys write, upper half 0xdeadbeef\n";
	static int w32[4];
	static long long w64[2];
	long long buf = (long long) (uintptr_t) msg | 0xdeadbeef00000000LL;
	pid_t pid = getpid ();

	printf ("sys getpid %d; entry 1 %lld; entry 3 %lld; syscall() %ld\n",
		(int) pid, m32_sys1 (SYS_getpid, 0, 0, 0),
		sys3 (SYS_getpid, 0, 0, 0), syscall (SYS_getpid));
	fflush (stdout);
	long long r1 = m32_sys1 (SYS_write, 1, buf, strlen (msg));
	long long r3 = sys3 (SYS_write, 1, buf, strlen (msg));
	printf ("sys write: entry 1 %lld, entry 3 %lld\n", r1, r3);
	printf ("sys bad fd: entry 1 %016llx, entry 3 %016llx\n",
		m32_sys1 (SYS_close, -1, 0, 0), sys3 (SYS_close, -1, 0, 0));
	/* A number with an upper half: entry 1 zero-extends it from 32 bits. */
	printf ("sys getpid, nr upper half set: entry 1 %lld, entry 3 %lld\n",
		m32_sys1 (SYS_getpid | 0x100000000LL, 0, 0, 0),
		sys3 (SYS_getpid | 0x100000000LL, 0, 0, 0));
	/* clock_gettime (267) through each entry: entry 1 is the compat
	   (32-bit time) version, entry 3 the 64-bit one.  */
	memset (w32, 0xff, sizeof w32);
	long long c1 = m32_sys1 (SYS_clock_gettime, CLOCK_MONOTONIC,
				 (uintptr_t) w32, 0);
	printf ("sys clock_gettime entry 1: %lld, words %d %d %d %d\n", c1,
		w32[0], w32[1], w32[2], w32[3]);
	memset (w32, 0xff, sizeof w32);
	c1 = sys3 (SYS_clock_gettime, CLOCK_MONOTONIC, (uintptr_t) w32, 0);
	printf ("sys clock_gettime entry 3: %lld, words %d %d %d %d\n", c1,
		w32[0], w32[1], w32[2], w32[3]);
	memset (w64, 0xff, sizeof w64);

	memset (w32, 0xff, sizeof w32);
	long long f5 = m32_sys5 (1, CLOCK_MONOTONIC, (uintptr_t) w32, 0);
	printf ("sys fast entry 5 clock_gettime: %lld, words %d %d %d %d\n",
		f5, w32[0], w32[1], w32[2], w32[3]);
	long long f6 = sys6 (1, CLOCK_MONOTONIC, (uintptr_t) w64, 0);
	printf ("sys fast entry 6 clock_gettime: %lld, %lld %lld\n", f6,
		w64[0], w64[1]);
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	printf ("sys clock_gettime: tv_sec %ld\n", (long) ts.tv_sec);

	errno = 0;
	long c = syscall (SYS_close, -1);
	printf ("errno: syscall(close, -1) %ld errno %d; ", c, errno);
	errno = 0;
	c = close (-1);
	printf ("close(-1) %ld errno %d; &errno %p\n", c, errno,
		(void *) &errno);

	/* 64-bit file offsets: split into two 32-bit arguments.  */
	int fd = open ("run32.tmp", O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd >= 0) {
		errno = 0;
		int rt = ftruncate64 (fd, 0x140000005LL);
		printf ("sys ftruncate64 without O_LARGEFILE: %d errno %d\n",
			rt, errno);
		close (fd);
	}
	fd = open ("run32.tmp", O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE, 0600);
	if (fd >= 0) {
		long long big = 0x140000005LL;
		int rt = ftruncate64 (fd, big);
		struct stat64 st;
		fstat64 (fd, &st);
		ssize_t w = pwrite64 (fd, "Z", 1, 0x100000003LL);
		char ch = 0;
		ssize_t rd = pread64 (fd, &ch, 1, 0x100000003LL);
		long long end = lseek64 (fd, 0, SEEK_END);
		printf ("sys 64-bit offsets: ftruncate64 %d size %llx pwrite64 %d pread64 %d '%c' lseek64 end %llx\n",
			rt, (long long) st.st_size, (int) w, (int) rd, ch, end);
		/* ftruncate64 (194) by hand: offset low word, then high.  */
		long long r = m32_sys1 (SYS_ftruncate64, fd, 0x00000007, 0x2);
		fstat64 (fd, &st);
		printf ("sys raw ftruncate64(fd, low 7, high 2): %lld size %llx\n",
			r, (long long) st.st_size);
		close (fd);
		unlink ("run32.tmp");
	}

	errno = 0;
	void *m = mmap ((void *) 0xf0000000u, 4096, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
			-1, 0);
	printf ("mmap MAP_FIXED_NOREPLACE at 0xf0000000: %p errno %d\n", m,
		m == MAP_FAILED ? errno : 0);
	if (m != MAP_FAILED)
		munmap (m, 4096);
	errno = 0;
	m = mmap ((void *) 0xf0000000u, 4096, PROT_READ | PROT_WRITE,
		  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	printf ("mmap hint 0xf0000000: %p errno %d\n", m,
		m == MAP_FAILED ? errno : 0);
	if (m != MAP_FAILED)
		munmap (m, 4096);
}

/* How much address space a 32-bit process can map: 64 MiB PROT_NONE
   blocks until mmap fails, first without a hint (the kernel searches
   upward from mmap_base), then with a hint of 16 MiB (below mmap_base);
   then all are unmapped.  */
static void exhaust (void)
{
	static void *blk[128];
	int n = 0, phase;
	for (phase = 0; phase < 2; phase++) {
		int n0 = n;
		uintptr_t lo = UINTPTR_MAX, hi = 0;
		errno = 0;
		while (n < 128) {
			void *p = mmap (phase ? (void *) 0x1000000 : 0,
					64u << 20, PROT_NONE,
					MAP_PRIVATE | MAP_ANONYMOUS
					| MAP_NORESERVE, -1, 0);
			if (p == MAP_FAILED)
				break;
			blk[n++] = p;
			if ((uintptr_t) p < lo)
				lo = (uintptr_t) p;
			if ((uintptr_t) p + (64u << 20) > hi)
				hi = (uintptr_t) p + (64u << 20);
		}
		printf ("mmap 64 MiB PROT_NONE blocks, %s, until failure: %d (%d MiB), from %lx to %lx, errno %d\n",
			phase ? "hint 0x1000000" : "no hint", n - n0,
			(n - n0) * 64, (unsigned long) lo, (unsigned long) hi,
			errno);
	}
	while (n > 0)
		munmap (blk[--n], 64u << 20);
}

/* time_t: 32 bits unless _TIME_BITS=64 (run32t), and what a 64-bit
   time_t gets from the kernel.  */
static void times (void)
{
	struct timespec ts;
	int r = clock_gettime (CLOCK_REALTIME, &ts);
	printf ("time: sizeof (time_t) %d, clock_gettime %d sec %lld\n",
		(int) sizeof (time_t), r, (long long) ts.tv_sec);
	if (sizeof (time_t) == 8) {
		long long big = 0x100000000LL;	/* 2106 */
		struct timespec t[2];
		int fd = open ("run32.tmp", O_RDWR | O_CREAT | O_TRUNC, 0600);
		t[0].tv_sec = t[1].tv_sec = (time_t) big;
		t[0].tv_nsec = t[1].tv_nsec = 0;
		errno = 0;
		r = utimensat (AT_FDCWD, "run32.tmp", t, 0);
		printf ("time: utimensat to 2^32 s: %d errno %d\n", r, errno);
		t[0].tv_sec = t[1].tv_sec = (time_t) 0x7fffff00;
		r = utimensat (AT_FDCWD, "run32.tmp", t, 0);
		struct stat st;
		fstat (fd, &st);
		printf ("time: utimensat to 0x7fffff00 s: %d, st_mtime %llx\n",
			r, (long long) st.st_mtime);
		close (fd);
		unlink ("run32.tmp");
	}
}

static sigjmp_buf env;
static volatile int last_sig, last_code;
static volatile uintptr_t last_addr;

static void on_fault (int sig, siginfo_t *si, void *uc)
{
	(void) uc;
	last_sig = sig;
	last_code = si->si_code;
	last_addr = (uintptr_t) si->si_addr;
	siglongjmp (env, 1);
}

static siginfo_t *h_si;
static void *h_uc, *h_ra;
static int h_sig, h_code;
static long long h_sp, h_ip, h_usd;

static void on_usr1 (int sig, siginfo_t *si, void *uc)
{
	h_sig = sig;
	h_code = si->si_code;
	h_si = si;
	h_uc = uc;
	h_sp = m32_sp ();
	h_ra = __builtin_return_address (0);
	h_ip = (long long) ((ucontext_t *) uc)->uc_mcontext.cr0_hi;
	h_usd = (long long) ((ucontext_t *) uc)->uc_mcontext.usd_lo;
}

#define TRY(label, expr)						\
	do {								\
		last_sig = 0;						\
		if (sigsetjmp (env, 1) == 0) {				\
			long long v_ = (long long) (expr);		\
			printf ("%s: value %llx\n", label, v_);		\
		} else							\
			printf ("%s: signal %d code %d addr %lx\n", label, \
				last_sig, last_code, (unsigned long) last_addr); \
	} while (0)

static int gvar = 0x5a5a1234;

static void sigs (void)
{
	struct sigaction sa;
	memset (&sa, 0, sizeof sa);
	sa.sa_sigaction = on_usr1;
	sa.sa_flags = SA_SIGINFO;
	sigaction (SIGUSR1, &sa, 0);
	long long sp = m32_sp ();
	raise (SIGUSR1);
	printf ("signal: sig %d code %d; uc - si %d; frame si below caller sp by %lld; handler sp %llx\n",
		h_sig, h_code, (int) ((char *) h_uc - (char *) h_si),
		sp - (long long) (uintptr_t) h_si, h_sp);
	printf ("signal: uc_mcontext.cr0_hi %llx (IP bits 3-47 %llx)\n",
		h_ip, h_ip & 0xfffffffffff8LL);
	long long usd = h_usd & 0xffffffffffffLL;
	printf ("signal: interrupted USD %llx, frame (si - 64) below it by %lld, uc_mcontext at uc + %d, sizeof (siginfo_t) %d; handler returns to %p\n",
		usd, usd - ((long long) (uintptr_t) h_si - 64),
		(int) offsetof (ucontext_t, uc_mcontext), (int) sizeof (siginfo_t),
		h_ra);

	sa.sa_sigaction = on_fault;
	sa.sa_flags = SA_SIGINFO;
	sigaction (SIGSEGV, &sa, 0);
	sigaction (SIGBUS, &sa, 0);
	sigaction (SIGILL, &sa, 0);

	unsigned a = (unsigned) (uintptr_t) &gvar;
	if (only32) {
		TRY ("ldgdw gvar + 0xf0000000 (TASK32_SIZE)", m32_ldgdw2 (a, 0xf0000000u));
		goto out;
	}
#ifdef __ptr32__
	/* ldgd* and getpl only in the 32-bit build: in a 64-bit process
	   the descriptors they use don't describe the task
	   (spec/abi/mode32.md), and an early run64 faulted outside it, so
	   run64 leaves them out.  */
	TRY ("ldgdw gvar", m32_ldgdw2 (a, 0));
	TRY ("ldgdw gvar+16 + 0xfffffff0 (wraps?)", m32_ldgdw2 (a + 16, 0xfffffff0u));
	TRY ("ldgdw 0x80000000 + (gvar+0x80000000) (wraps?)",
	     m32_ldgdw2 (0x80000000u, a + 0x80000000u));
	TRY ("ldgdw 0 (null)", m32_ldgdw2 (0, 0));
	TRY ("ldgdw 0xeffffff0 (just below TASK32_SIZE)", m32_ldgdw2 (0xeffffff0u, 0));
	TRY ("ldgdw 0xf0000000 (TASK32_SIZE)", m32_ldgdw2 (0xf0000000u, 0));
	TRY ("ldgdw 0xfffffff0", m32_ldgdw2 (0xfffffff0u, 0));
#endif
	TRY ("flat ldw gvar", m32_ldw64 (a));
	TRY ("flat ldw gvar + 2^32", m32_ldw64 (a + 0x100000000LL));
	m32_stw64 (a, 0x77);
	printf ("flat stw gvar: gvar now %x\n", gvar);
#ifdef __ptr32__
	unsigned fa = (unsigned) (uintptr_t) st_fn;
	TRY ("getpl into %ctpr1 and call", m32_callpl (st_fn, 3));
	TRY ("getpl into %dr0", m32_getpl (fa));
	TRY ("getpl into %dr0, %dr1 after it", m32_getpl_hi (fa));
	TRY ("getpl into %dr0, tags of %dr0 | %dr1 << 8", m32_getpl_tag (fa));
	TRY ("getpl into %dr1", m32_getpl_odd (fa));
#endif
	printf ("addresses: st_fn %p, m32_ldgdw2 %p, m32_ldw64 %p, m32_getpl %p\n",
		(void *) st_fn, (void *) m32_ldgdw2, (void *) m32_ldw64,
		(void *) m32_getpl);
	TRY ("C null deref", *(volatile int *) 0);
out:
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigaction (SIGSEGV, &sa, 0);
	sigaction (SIGBUS, &sa, 0);
	sigaction (SIGILL, &sa, 0);
}

__thread int tls_var = 42;

struct tinfo { void *tls, *g13, *err, *libtls; };
static void thr_info (struct tinfo *t)
{
	t->tls = &tls_var;
	t->g13 = (void *) (uintptr_t) m32_g13 ();
	t->err = &errno;
	t->libtls = lib_tls_addr ();
}
static void *thr (void *arg) { tls_var++; thr_info (arg); return 0; }

/* Atomics (ldgd* with MAS 0x7 and stgd* with MAS 0x2 in 32-bit code):
   two threads add to a shared int and long long.  */
static int at_i;
static long long at_ll;
static void *adder (void *arg)
{
	(void) arg;
	for (int i = 0; i < 200000; i++) {
		__atomic_fetch_add (&at_i, 1, __ATOMIC_SEQ_CST);
		__atomic_fetch_add (&at_ll, 0x100000001LL, __ATOMIC_SEQ_CST);
	}
	return 0;
}

static void tls (void)
{
	struct tinfo m, t;
	pthread_t th;
	thr_info (&m);
	pthread_create (&th, 0, thr, &t);
	pthread_join (th, 0);
	printf ("tls main: &tls %p g13 %llx (%p) &errno %p lib %p value %d\n",
		m.tls, m32_g13 (), m.g13, m.err, m.libtls, tls_var);
	printf ("tls thread: &tls %p g13 %p &errno %p lib %p; tls - g13 main %d thread %d\n",
		t.tls, t.g13, t.err, t.libtls,
		(int) ((char *) m.tls - (char *) m.g13),
		(int) ((char *) t.tls - (char *) t.g13));
	pthread_t a1, a2;
	pthread_create (&a1, 0, adder, 0);
	pthread_create (&a2, 0, adder, 0);
	pthread_join (a1, 0);
	pthread_join (a2, 0);
	printf ("atomics: int %d long long %llx (want 400000 and 61a8000061a80) %s\n",
		at_i, at_ll,
		at_i == 400000 && at_ll == 0x61a8000061a80LL ? "ok" : "BAD");
}

static jmp_buf jb;
static ucontext_t uc_main, uc_fn;
static void deep (int n) { if (n == 0) longjmp (jb, 7); deep (n - 1); }
static void ctx_fn (int a, int b) { printf ("context: fn got %d %d\n", a, b); }

static void jumps (void)
{
	int r = setjmp (jb);
	if (r == 0)
		deep (20);
	printf ("longjmp: returned %d\n", r);
	static char stk[65536];
	getcontext (&uc_fn);
	uc_fn.uc_stack.ss_sp = stk;
	uc_fn.uc_stack.ss_size = sizeof stk;
	uc_fn.uc_link = &uc_main;
	int mk = makecontext_e2k (&uc_fn, (void (*) (void)) ctx_fn, 2, 11, 22);
	int s = swapcontext (&uc_main, &uc_fn);
	printf ("context: makecontext_e2k %d, back in main, swapcontext %d, freecontext_e2k %d\n",
		mk, s, freecontext_e2k (&uc_fn));
}

int main (int, char **);

static void space (int maps)
{
	int local;
	void *small = malloc (16), *large = malloc (1 << 20);
	printf ("addr text main %p, data gvar %p, bss env %p, heap %p, sbrk %p, mmap'd malloc %p\n",
		(void *) main, (void *) &gvar, (void *) &env, small, sbrk (0),
		large);
	printf ("addr stack local %p, getsp %llx, ip %llx, lib_fn %p\n",
		(void *) &local, m32_sp (), m32_ip (), (void *) lib_fn);
	printf ("auxv: AT_FAST_SYSCALLS %lx AT_SYSTEM_INFO %lx AT_HWCAP %lx AT_PAGESZ %lu AT_SYSINFO_EHDR %lx AT_PLATFORM %lx\n",
		getauxval (32), getauxval (34), getauxval (AT_HWCAP),
		getauxval (AT_PAGESZ), getauxval (AT_SYSINFO_EHDR),
		getauxval (AT_PLATFORM));
	free (small);
	free (large);
	if (maps) {
		FILE *f = fopen ("/proc/self/maps", "r");
		char line[512];
		while (f && fgets (line, sizeof line, f))
			printf ("maps %s", line);
		if (f)
			fclose (f);
	}
}

int main (int argc, char **argv)
{
	int maps = argc > 1 && !strcmp (argv[1], "maps");
	only32 = argc > 1 && !strcmp (argv[1], "qemu");
	setvbuf (stdout, 0, _IOLBF, 0);
	printf ("mode: %d-bit pointers, %d-bit time_t\n",
		(int) sizeof (void *) * 8, (int) sizeof (time_t) * 8);
	sizes ();
	times ();
	if (argc > 1 && !strcmp (argv[1], "t64"))
		return 0;
	calls ();
	sysc ();
	sigs ();
	tls ();
	jumps ();
	space (maps);
	exhaust ();
	return 0;
}
