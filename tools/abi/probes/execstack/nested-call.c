/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A GNU C nested function called directly; its address is never taken,
   so GCC needs no trampoline (tools/abi/probes/execstack.sh compiles it). */
int outer(int n)
{
	int base = 100;
	int inner(int x) { return base + x; }
	return inner(n) + inner(1);
}
