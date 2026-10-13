/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Protected-mode (e2k128) program using user shared libraries built with
   -mptr128 -shared: libpmlib.so (pmlib.c) linked in, libpmdl.so
   (pmdl.c) loaded with dlopen.  Each line is "case: result"; a case
   that may fault runs in a child and prints "case: SIGNAME code=N
   addr=..." from its handler.  Built and run on the machine (8C2, 2C3:
   their ld makes the new protected PLT) by run.sh --left:

     lcc -mptr128 -O1 -fPIC -shared pmlib.c -o libpmlib.so
     lcc -mptr128 -O1 -fPIC -shared pmdl.c -o libpmdl.so
     lcc -mptr128 -O1 pmlibmain.c -o pmlibmain -L. -lpmlib -ldl -lpthread
     LD_LIBRARY_PATH=. ./pmlibmain  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define NOINLINE __attribute__ ((noinline))
typedef unsigned long u64;

extern int lib_var;
extern int lib_arr[4];
extern int *lib_ptr;
extern const char lib_str[];
extern __thread int lib_tls;
int lib_fn (int);
int (*lib_get_fn (void)) (int);
int *lib_get_arr (void);
int *lib_get_var (void);
int lib_call (int (*) (int), int);
int lib_sum (const int *, int);
int lib_read (const int *, int);
int *lib_tls_addr (void);
int lib_main_var (void);
void lib_regs (u64 out[6]);

int main_var = 9;

/* ---- a 16-byte value with its tags (as pmasm.c) ----------------------- */

struct raw { u64 lo, hi, tlo, thi; };

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

static NOINLINE struct raw memq (const void *p)
{
	struct raw r;
	__int128 q;
	__asm__ __volatile__ ("ldapq %1, 0x0, %0" : "=r" (q) : "r" (p) : "memory");
	RAWQ (r, q);
	return r;
}

static NOINLINE void dumpp (const char *what, const void *p)
{
	const void *slot[1];
	struct raw r;
	slot[0] = p;
	r = memq (slot);
	printf ("%s: lo=%016lx hi=%016lx tags=%lx,%lx base=%lx size=%lx curptr=%lx\n",
		what, r.lo, r.hi, r.tlo, r.thi, r.lo & 0xffffffffffffUL,
		r.hi >> 32, r.hi & 0xffffffffUL);
}

static NOINLINE void dumpf (const char *what, int (*f) (int))
{
	int (*slot[1]) (int);
	struct raw r;
	slot[0] = f;
	r = memq (slot);
	printf ("%s: lo=%016lx hi=%016lx tags=%lx,%lx\n", what, r.lo, r.hi,
		r.tlo, r.thi);
}

/* ---- running a case in a child (as pmrun.c) -------------------------- */

static const char *cur_case;

static const char *signame (int s)
{
	switch (s) {
	case SIGSEGV: return "SIGSEGV";
	case SIGBUS: return "SIGBUS";
	case SIGILL: return "SIGILL";
	case SIGFPE: return "SIGFPE";
	case SIGABRT: return "SIGABRT";
	case SIGKILL: return "SIGKILL";
	case SIGALRM: return "SIGALRM (hung)";
	default: return "SIG?";
	}
}

static void fault_handler (int sig, siginfo_t *si, void *uc)
{
	struct raw r = memq (&si->si_addr);
	printf ("%s: %s code=%d addr=%016lx\n", cur_case, signame (sig),
		si->si_code, r.lo);
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
		cur_case = name;
		alarm (20);
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

/* ---- cases ------------------------------------------------------------- */

static int main_cb (int x) { return x + 100; }

static void data (void)
{
	printf ("data: lib_var=%d lib_fn(1)=%d\n", lib_var, lib_fn (1));
	lib_var = 10;
	printf ("data: after main wrote lib_var=10: lib_fn(1)=%d, "
		"*lib_get_var()=%d\n", lib_fn (1), *lib_get_var ());
	dumpp ("data &lib_var in main", &lib_var);
	dumpp ("data &lib_var in the library", lib_get_var ());
	printf ("data: lib_ptr[0]=%d lib_arr[3]=%d\n", lib_ptr[0], lib_arr[3]);
	dumpp ("data lib_ptr", lib_ptr);
	dumpp ("data lib_arr in main", lib_arr);
	dumpp ("data lib_arr in the library", lib_get_arr ());
	printf ("data: lib_str=\"%s\"\n", lib_str);
	dumpp ("data lib_str", lib_str);
	printf ("data: the library reads main_var=%d\n", lib_main_var ());
}

static void fnptr (void)
{
	int (*f) (int) = lib_fn;
	int (*g) (int) = lib_get_fn ();
	dumpf ("fnptr lib_fn taken in main", f);
	dumpf ("fnptr lib_fn taken in the library", g);
	dumpf ("fnptr main_cb", main_cb);
	printf ("fnptr: equal=%d f(2)=%d g(2)=%d\n", f == g, f (2), g (2));
	printf ("fnptr: lib_call(main_cb, 1)=%d\n", lib_call (main_cb, 1));
}

static void arrays (void)
{
	int a[4] = { 1, 2, 3, 4 };
	printf ("arrays: lib_sum(main's int[4])=%d\n", lib_sum (a, 4));
}

static void c_lib_oob (void)
{
	int a[4] = { 1, 2, 3, 4 };
	sink = lib_read (a, 4);
}

static void *tls_thr (void *arg)
{
	printf ("tls thread: lib_tls=%d\n", lib_tls);
	lib_tls = 8;
	dumpp ("tls thread &lib_tls in main", &lib_tls);
	dumpp ("tls thread &lib_tls in the library", lib_tls_addr ());
	return arg;
}

static void tls (void)
{
	pthread_t t;
	printf ("tls: lib_tls=%d\n", lib_tls);
	dumpp ("tls &lib_tls in main", &lib_tls);
	dumpp ("tls &lib_tls in the library", lib_tls_addr ());
	pthread_create (&t, NULL, tls_thr, NULL);
	pthread_join (t, NULL);
	printf ("tls: main's lib_tls after the thread wrote its own: %d\n", lib_tls);
}

static void c_regs (void)
{
	u64 m[6], l[6];
	__asm__ __volatile__ ("rrd %%cud.lo, %0\n\trrd %%cud.hi, %1\n\t"
			      "rrd %%gd.lo, %2\n\trrd %%gd.hi, %3\n\t"
			      "rrd %%usd.lo, %4\n\trrd %%usd.hi, %5"
			      : "=r" (m[0]), "=r" (m[1]), "=r" (m[2]),
				"=r" (m[3]), "=r" (m[4]), "=r" (m[5]));
	lib_regs (l);
	printf ("c_regs main: cud=%016lx:%016lx gd=%016lx:%016lx usd.lo=%016lx\n",
		m[0], m[1], m[2], m[3], m[4]);
	printf ("c_regs library: cud=%016lx:%016lx gd=%016lx:%016lx usd.lo=%016lx\n",
		l[0], l[1], l[2], l[3], l[4]);
}

static int (*dl_f) (int);
static int *dl_p;
static void *dl_h;

static void dl (void)
{
	int *(*pf) (void);
	dl_h = dlopen ("libpmdl.so", RTLD_NOW);
	if (!dl_h) {
		printf ("dl: dlopen failed: %s\n", dlerror ());
		return;
	}
	dl_f = (int (*) (int)) dlsym (dl_h, "dl_fn");
	pf = (int *(*) (void)) dlsym (dl_h, "dl_ptr");
	dl_p = pf ();
	dumpf ("dl dlsym dl_fn", dl_f);
	dumpp ("dl dl_ptr ()", dl_p);
	dumpp ("dl dlsym dl_var", dlsym (dl_h, "dl_var"));
	printf ("dl: dl_fn(3)=%d *dl_ptr()=%d\n", dl_f (3), *dl_p);
}

static void c_dl_after_close_call (void)
{
	printf ("c_dl_after_close_call: dlclose=%d\n", dlclose (dl_h));
	fflush (stdout);
	sink = dl_f (3);
}

static void c_dl_after_close_read (void)
{
	printf ("c_dl_after_close_read: dlclose=%d\n", dlclose (dl_h));
	fflush (stdout);
	sink = *dl_p;
}

static void c_dl_reopen (void)
{
	int (*old) (int) = dl_f;
	printf ("c_dl_reopen: dlclose=%d\n", dlclose (dl_h));
	dl ();
	printf ("c_dl_reopen: same PL as before: %d\n", old == dl_f);
}

/* A 64-bit library: what dlopen says.  */
static void dl64 (void)
{
	static const char *const paths[] = {
		"/lib64/libm.so.6", "/usr/lib64/libm.so.6", "/lib/libm.so.6",
	};
	for (unsigned i = 0; i < sizeof paths / sizeof paths[0]; i++) {
		if (access (paths[i], R_OK) != 0)
			continue;
		void *h = dlopen (paths[i], RTLD_NOW);
		printf ("dl64: dlopen (\"%s\") %s: %s\n", paths[i],
			h ? "succeeded" : "failed", h ? "" : dlerror ());
		return;
	}
	printf ("dl64: no 64-bit libm found\n");
}

static void maps (void)
{
	FILE *f = fopen ("/proc/self/maps", "r");
	char line[512];
	while (f && fgets (line, sizeof line, f))
		if (strstr (line, "libpm") || strstr (line, "[cut]")
		    || strstr (line, "pmlibmain"))
			printf ("maps: %s", line);
	if (f)
		fclose (f);
}

int main (void)
{
	setvbuf (stdout, NULL, _IOLBF, 0);
	printf ("pmlibmain\n");
	data ();
	fnptr ();
	arrays ();
	run ("c_lib_oob", c_lib_oob);
	tls ();
	run ("c_regs", c_regs);
	dl ();
	dl64 ();
	maps ();
	run ("c_dl_after_close_call", c_dl_after_close_call);
	run ("c_dl_after_close_read", c_dl_after_close_read);
	run ("c_dl_reopen", c_dl_reopen);
	printf ("done\n");
	return 0;
}
