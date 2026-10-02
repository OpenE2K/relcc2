/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which 32-bit operations does WD.dbl zero-extend into the full register?
   (spec/abi/registers.md, "dbl").

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/abi/probes/dbl.c tools/abi/probes/dbl.s -o dbl && ./dbl

   Each p_OP_DBL loads 0x2222222222222222 into %dr3, writes a 32-bit result
   0x12345678 (or a float result) into %r3 with OP under setwd dbl = DBL,
   and stores the whole 64-bit register.

   Elbrus 4C (v3) and MCST's qemu-e2k: adds, shls, muls keep the upper half
   with dbl = 0 and zero-extend with dbl = 1; ldw and sxt always write the
   whole register; fadds and movts keep the upper half with either value.  */

#include <stdio.h>
void p_adds_0(long *, int *, int);
void p_shls_0(long *, int *, int);
void p_muls_0(long *, int *, int);
void p_ldw_0(long *, int *, int);
void p_fadds_0(long *, int *, int);
void p_movts_0(long *, int *, int);
void p_sxt_0(long *, int *, int);
void p_adds_1(long *, int *, int);
void p_shls_1(long *, int *, int);
void p_muls_1(long *, int *, int);
void p_ldw_1(long *, int *, int);
void p_fadds_1(long *, int *, int);
void p_movts_1(long *, int *, int);
void p_sxt_1(long *, int *, int);
int main(void) {
	int src = 0x12345678; long out;
	out = 0x2222222222222222L; p_adds_0(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 0, "adds", out);
	out = 0x2222222222222222L; p_shls_0(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 0, "shls", out);
	out = 0x2222222222222222L; p_muls_0(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 0, "muls", out);
	out = 0x2222222222222222L; p_ldw_0(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 0, "ldw", out);
	out = 0x2222222222222222L; p_fadds_0(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 0, "fadds", out);
	out = 0x2222222222222222L; p_movts_0(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 0, "movts", out);
	out = 0x2222222222222222L; p_sxt_0(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 0, "sxt", out);
	out = 0x2222222222222222L; p_adds_1(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 1, "adds", out);
	out = 0x2222222222222222L; p_shls_1(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 1, "shls", out);
	out = 0x2222222222222222L; p_muls_1(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 1, "muls", out);
	out = 0x2222222222222222L; p_ldw_1(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 1, "ldw", out);
	out = 0x2222222222222222L; p_fadds_1(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 1, "fadds", out);
	out = 0x2222222222222222L; p_movts_1(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 1, "movts", out);
	out = 0x2222222222222222L; p_sxt_1(&out, &src, 0x12345678); printf("dbl=%d %-6s %016lx\n", 1, "sxt", out);
	return 0;
}
