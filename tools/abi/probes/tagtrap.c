/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Does a 32-bit write under WD.dbl = 0 keep a diagnostic tag in the upper
   half of a register, so that spilling it with std traps?
   (calling-convention.md, "Unused parts of a slot").

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/abi/probes/tagtrap.c tools/abi/probes/tagtrap.s -o tagtrap
     ./tagtrap a|b|c [good]

   Each probe loads speculatively from BAD into a register, then stores it
   with std:
     a  directly (control: the register holds a diagnostic tag)
     b  after a 32-bit adds into it with dbl = 0
     c  the same with dbl = 1
     d  as b, but stored with a speculative std,sm
     e  as b, then copied with addd,sm (lcc's register copy) and the
        copy stored with std,sm
     f  as e, but copied with a plain addd
   With "good", BAD is a valid address, which shows what the store writes.

   Elbrus 4C (v3): a and b die with SIGILL; c stores 0000000012345678;
   d stores 4afafafa12345678 (no trap); e stores 4afafafa52345678 (the
   speculative copy is wholly diagnostic, low half included); f dies with
   SIGILL; with "good", b, d and e store 2222222212345678 and c
   0000000012345678.  */

#include <stdio.h>

void probe_a(long *, long *, int);
void probe_b(long *, long *, int);
void probe_c(long *, long *, int);
void probe_d(long *, long *, int);
void probe_e(long *, long *, int);
void probe_f(long *, long *, int);

int main(int argc, char **argv)
{
	long out = 0x1111111111111111L, good = 0x2222222222222222L;
	long *bad = argc > 2 ? &good : (long *)8;

	if (argc < 2)
		return 2;
	switch (argv[1][0]) {
	case 'a': probe_a(&out, bad, 0x12345678); break;
	case 'b': probe_b(&out, bad, 0x12345678); break;
	case 'c': probe_c(&out, bad, 0x12345678); break;
	case 'd': probe_d(&out, bad, 0x12345678); break;
	case 'e': probe_e(&out, bad, 0x12345678); break;
	case 'f': probe_f(&out, bad, 0x12345678); break;
	default: return 2;
	}
	printf("%s%s: stored %016lx\n", argv[1], argc > 2 ? " good" : "", out);
	return 0;
}
