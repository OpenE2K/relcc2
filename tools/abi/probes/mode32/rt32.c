/* SPDX-License-Identifier: GPL-3.0-or-later */
/* lcc's runtime libraries in 32-bit mode (spec/abi/mode32.md, "C
   runtime"): AddressSanitizer, OpenMP and -pg. run.sh builds this file
   three times and runs each program on the target:

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc
     $L -mptr32 -g -fsanitize=address -DRT_ASAN rt32.c -o rt32-asan
     $L -mptr32 -O2 -fopenmp rt32.c -o rt32-omp
     $L -mptr32 -O1 -pg rt32.c -o rt32-pg
     ./rt32-asan; ./rt32-omp; ./rt32-pg && gprof -b -p rt32-pg gmon.out

   rt32-asan writes one byte past a heap block, which ASan should
   report; rt32-omp sums 0..999 in four threads; rt32-pg calls f a
   million times, which gprof should count.  */

#include <stdio.h>
#include <stdlib.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#if !defined RT_ASAN && !defined _OPENMP
static int __attribute__ ((noinline)) f (int x) { return x * 3; }
#endif

int main (int argc, char **argv)
{
	(void) argc;
	(void) argv;
#if defined RT_ASAN
	char *p = malloc (8);
	p[argc + 7] = 1;	/* one byte past the block */
	printf ("asan: no report, %p\n", (void *) p);
	free (p);
#elif defined _OPENMP
	long s = 0;
#pragma omp parallel for reduction (+:s) num_threads (4)
	for (int i = 0; i < 1000; i++)
		s += i;
	printf ("omp: sum %ld (want 499500), max threads %d\n", s,
		omp_get_max_threads ());
#else
	int s = 0;
	for (int i = 0; i < 1000000; i++)
		s += f (i);
	printf ("pg: %d\n", s);
#endif
	return 0;
}
