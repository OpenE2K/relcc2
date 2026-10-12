/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Does a plain std of a register whose upper half holds an *empty* tagged
   value (tag 1, bit 30 clear) trap, as one with a *diagnostic* upper half
   does (tagtrap.c)?  And what do the machines and MCST's qemu put in
   the registers a window gains?  (calling-convention.md, "Unused parts
   of a slot"; spec/isa/speculation.md, "Diagnostic values".)

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/abi/probes/tagempty.c tools/abi/probes/tagempty.s -o tagempty
     ./tagempty g|h|i|j|k|l|n|o|p                 # on hardware
     toolchains/qemu-mcst/bin/qemu-e2k -cpu elbrus-v3 \
         -L /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/fs ./tagempty X

   Each probe stores into *out, which starts as 1111111111111111:
     g  an empty doubleword (0, tag 5) from puttagd, a 32-bit adds of
        0x12345678 into its low half under dbl = 0, then a plain std
     h  as g with adds,sm (lcc's form for an argument slot)
     i  the empty doubleword itself, plain std
     j  a register the window grows into (setwd 4, then setwd 8), the
        same adds, plain std
     k  as j without the adds
     l  as j, then copied with a plain addd and the copy stored
     n  one call writes 5555555555555555 to %dr13, the next call (same
        caller, same wbs) reads %dr13 (std,sm) and its tag (gettagd)
     o  the same within one function: %dr13 = 6666666666666666, setwd 4,
        setwd 8, then read as n does
     p  as o with 1234567812345678 (bit 30 clear in both halves)

   Elbrus 4C (v3), 8C2 (v5) and 2C3 (v6) alike: g, h, j and l store
   0000000012345678, i and k 0000000000000000; nothing traps, so a plain
   std takes an empty upper half.  n, o and p show that a register the
   window gains is emptied: the 8C2 and the 2C3 give 0000000000000000,
   tag 5; the 4C keeps the old bits but clears bit 30 of
   each half, also with tag 5 (n 1555555515555555, o 2666666626666666,
   p 1234567812345678).
   MCST's qemu-e2k 11.0.1 (release 1.2) fills such registers with 0, tag
   5 as well (cpu.h e2k_greg_make_empty), but cannot store a register
   with an empty half: g, h, i, j and k never finish (the std's wide
   instruction runs again and again, no exception), l stores
   0000000012345678, and n, o and p give 0000000000000000, tag 1 (qemu's
   gettagd returns the low bits of its unpacked tag).  With -e2k-opt
   no-rf-clean, j stores 0000000012345678, k 0000000000000000, and n, o
   and p give the values written, tag 0; g, h and i still hang.

   An earlier version read %dr13 in the instruction that holds
   probe_n_get's setwd and died with SIGSEGV on all three machines (an
   exception under qemu): an operation there still sees the window the
   call handed over (inferred), so the read now comes one instruction
   later.  */

#include <stdio.h>

void probe_g(long *, long *, int);
void probe_h(long *, long *, int);
void probe_i(long *, long *, int);
void probe_j(long *, long *, int);
void probe_k(long *, long *, int);
void probe_l(long *, long *, int);
void probe_n_set(void);
void probe_n_get(long *, long *);
void probe_o(long *, long *);
void probe_p(long *, long *);

int main(int argc, char **argv)
{
	long out = 0x1111111111111111L, spare = 0;

	if (argc < 2)
		return 2;
	switch (argv[1][0]) {
	case 'g': probe_g(&out, &spare, 0x12345678); break;
	case 'h': probe_h(&out, &spare, 0x12345678); break;
	case 'i': probe_i(&out, &spare, 0x12345678); break;
	case 'j': probe_j(&out, &spare, 0x12345678); break;
	case 'k': probe_k(&out, &spare, 0x12345678); break;
	case 'l': probe_l(&out, &spare, 0x12345678); break;
	case 'n': probe_n_set(); probe_n_get(&out, &spare); break;
	case 'o': probe_o(&out, &spare); break;
	case 'p': probe_p(&out, &spare); break;
	default: return 2;
	}
	if (argv[1][0] >= 'n')
		printf("%s: stored %016lx, tag %ld\n", argv[1], out, spare);
	else
		printf("%s: stored %016lx\n", argv[1], out);
	return 0;
}
