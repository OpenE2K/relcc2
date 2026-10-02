/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Executable side of the dso probe: IE access to the library's TLS, LE
   access to its own, a PLT call, and function pointer equality with the
   address taken in the library. Prints "152 42 5 1".  */
#include <stdio.h>

extern __thread int lib_tls;
extern int lib_var;
int lib_fn(int);
int (*lib_fn_addr(void))(int);
__thread int exe_tls = 5;

int main(void)
{
	lib_var += 1;
	int r = lib_fn(1);
	printf("%d %d %d %d\n", r, lib_tls, exe_tls, lib_fn_addr() == lib_fn);
	return 0;
}
