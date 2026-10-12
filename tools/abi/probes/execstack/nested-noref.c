/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A nested function that uses nothing from its parent: GCC passes its
   plain address, no static chain and no trampoline
   (tools/abi/probes/execstack.sh compiles it). */
extern void apply(int (*fn)(int), int n);
int outer(int n)
{
	int twice(int x) { return 2 * x; }
	apply(twice, n);
	return 0;
}
