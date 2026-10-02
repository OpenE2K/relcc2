/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shared library side of the dso probe (spec/abi/elf.md): global and
   static TLS (GD in PIC code), a global variable the executable reaches
   by copy relocation or GOT, and a function address taken in the
   library.  */

__thread int lib_tls = 42;
static __thread int lib_tls_loc = 7;
int lib_var = 100;

int lib_fn(int x)
{
	lib_tls_loc++;
	return x + lib_var + lib_tls + lib_tls_loc;
}

int (*lib_fn_addr(void))(int)
{
	return lib_fn;
}
