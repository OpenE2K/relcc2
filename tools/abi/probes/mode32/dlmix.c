/* SPDX-License-Identifier: GPL-3.0-or-later */
/* dlopen() the library named on the command line and call its mix_fn,
   printing dlerror() on failure. run.sh builds it as dlmix32 (-mptr32)
   and dlmix64 and gives each the other mode's libmix (mixlib.c):
     lcc -mptr32 dlmix.c -o dlmix32; ./dlmix32 ./libmix64.so  */

#include <dlfcn.h>
#include <stdio.h>

int main (int argc, char **argv)
{
	printf ("dlmix: %d-bit, main at %p\n", (int) sizeof (void *) * 8,
		(void *) main);
	for (int i = 1; i < argc; i++) {
		void *h = dlopen (argv[i], RTLD_NOW);
		if (!h) {
			printf ("dlopen %s (%d-bit caller): %s\n", argv[i],
				(int) sizeof (void *) * 8, dlerror ());
			continue;
		}
		int (*f) (int) = (int (*) (int)) dlsym (h, "mix_fn");
		printf ("dlopen %s (%d-bit caller): ok, mix_fn(1) = %d\n",
			argv[i], (int) sizeof (void *) * 8, f ? f (1) : -1);
	}
	return 0;
}
