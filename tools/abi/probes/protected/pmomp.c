/* SPDX-License-Identifier: GPL-3.0-or-later */
/* OpenMP in protected mode (e2k128): lcc 1.27.07 added it (lcc-doc,
   changelog 1.27).  A parallel loop through a heap pointer, a reduction,
   critical and atomic updates, threadprivate data, tasks, pointers
   handed between threads, and a bounds fault inside a worker thread.
   Built and run by run.sh --left:

     lcc -mptr128 -O1 -fopenmp pmomp.c -o pmomp        (on the 8C2, 2C3)
     /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -mptr128 -O1 -fopenmp \
         -static pmomp.c -o pmomp -lpthread             (for the 4C)
     OMP_NUM_THREADS=2 ./pmomp; ./pmomp oob  */

#include <omp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tp;
#pragma omp threadprivate (tp)

static long fib (int n)
{
	long a, b;
	if (n < 2)
		return n;
#pragma omp task shared (a)
	a = fib (n - 1);
#pragma omp task shared (b)
	b = fib (n - 2);
#pragma omp taskwait
	return a + b;
}

static void handler (int sig, siginfo_t *si, void *uc)
{
	printf ("oob: thread %d got %s code=%d\n", omp_get_thread_num (),
		sig == SIGSEGV ? "SIGSEGV" : sig == SIGILL ? "SIGILL" : "SIG?",
		si->si_code);
	fflush (stdout);
	_exit (0);
}

int main (int argc, char **argv)
{
	int n = 1 << 20, threads = 0, crit = 0, seen[64];
	long atom = 0;
	double *a = malloc (n * sizeof *a), sum = 0;
	int **slots;

	setvbuf (stdout, NULL, _IOLBF, 0);
	if (argc > 1 && strcmp (argv[1], "oob") == 0) {
		/* thread 1 reads one element past a 4-element array */
		int *small = malloc (4 * sizeof *small);
		volatile long sink = 0;
		struct sigaction sa;
		memset (&sa, 0, sizeof sa);
		sa.sa_sigaction = handler;
		sa.sa_flags = SA_SIGINFO;
		sigaction (SIGSEGV, &sa, NULL);
		sigaction (SIGILL, &sa, NULL);
		memset (small, 0, 4 * sizeof *small);
#pragma omp parallel num_threads (2)
		{
			if (omp_get_thread_num () == 1)
				sink += small[4];
		}
		printf ("oob: no fault (%ld)\n", (long) sink);
		return 0;
	}
	memset (seen, 0, sizeof seen);
#pragma omp parallel for
	for (int i = 0; i < n; i++)
		a[i] = i * 0.5;
#pragma omp parallel for reduction (+:sum)
	for (int i = 0; i < n; i++)
		sum += a[i];
	printf ("omp: reduction sum=%.1f (expected %.1f)\n", sum,
		0.5 * ((double) n * (n - 1) / 2));
#pragma omp parallel
	{
		int t = omp_get_thread_num ();
#pragma omp critical
		{
			crit++;
			if (t < 64)
				seen[t] = 1;
		}
#pragma omp atomic
		atom += t + 1;
#pragma omp single
		threads = omp_get_num_threads ();
		tp = t * 10;
	}
	printf ("omp: threads=%d critical=%d atomic=%ld\n", threads, crit, atom);
	/* each thread's own threadprivate copy */
#pragma omp parallel
	{
		if (tp != omp_get_thread_num () * 10)
#pragma omp critical
			printf ("omp: threadprivate wrong in thread %d: %d\n",
				omp_get_thread_num (), tp);
	}
	printf ("omp: threadprivate checked\n");
	/* pointers made in one thread and used in another */
	slots = calloc (threads, sizeof *slots);
#pragma omp parallel
	{
		int t = omp_get_thread_num ();
		slots[t] = malloc (sizeof (int));
		*slots[t] = t + 7;
#pragma omp barrier
		int o = (t + 1) % omp_get_num_threads ();
		if (*slots[o] != o + 7)
#pragma omp critical
			printf ("omp: thread %d read %d from thread %d's block\n",
				t, *slots[o], o);
	}
	printf ("omp: pointers across threads checked\n");
	{
		long r;
#pragma omp parallel
#pragma omp single
		r = fib (20);
		printf ("omp: tasks fib(20)=%ld\n", r);
	}
	printf ("omp: wtime>0 %d\n", omp_get_wtime () > 0);
	printf ("done\n");
	return 0;
}
