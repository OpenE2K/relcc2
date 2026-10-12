/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A GNU C nested function that uses its parent's variable and whose
   address escapes: GCC builds a trampoline for it
   (tools/abi/probes/execstack.sh compiles it). */
extern void apply(int (*fn)(int), int n);
int outer(int n)
{
	int sum = 0;
	int add(int x) { sum += x; return sum; }
	apply(add, n);
	return sum;
}
