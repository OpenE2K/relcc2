/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GCC-built code that stores a register holding no value: what the
   machines and MCST's qemu do (spec/abi/calling-convention.md, "Unused
   parts of a slot"; docs/spec-poc-findings.md I56).  Built with the
   PoC cross (any GCC for E2K that keeps parse's yylval in a register
   written on one path only; check with -S):

     G=toolchains/poc-cross/cross-0.5+32-v3/gcc-e2k-0.5+32-v3/bin/e2k-linux-gnu-gcc
     $G -O2 tools/abi/probes/foldi56_undef.c tools/abi/probes/foldi56_undef.s \
         -o foldi56_undef
     ./foldi56_undef u|d|s|q|f                       # on hardware
     timeout 20 toolchains/qemu-mcst/bin/qemu-e2k [-e2k-opt no-rf-clean] \
         -cpu elbrus-v3 -L $($G -print-sysroot) ./foldi56_undef X

   parse is bison's copy of yylval, which the lexer need not have set:
   at -O2 the PoC (0.5+32) keeps yylval across the call in %r2, writes
   it only when c > 0 ("subd ... %r2 ? ~%pred0") and stores it with a
   plain "std,2 %r1, 8, %r2" after the call.  %r2 is the caller's %b[2]
   (call wbs = 5, so parse inherits %r0-%r7).
     u  parse(0): stores %r2, which nothing wrote (main's setwd gained
        it, so it is empty)
     d  parse(1): control, %r2 holds 7
     s  spoil_low(bad), a callee that leaves diagnostic values in its
        %dr2-%dr7 (main's %b[2]-%b[7]), then parse(0) from the same
        frame and wbs
     q  spoil_high(bad) leaves a diagnostic value in %dr13 at wsz = 8,
        then get_high, called next from the same frame, stores its
        %dr13 (gained by its setwd) with a plain std (assembly only)
     f  a plain std to 0x100000000000 (unmapped; past qemu's guest area)

   Elbrus 4C (ALT, PoC 0.5+32 -O2, 2026-10-24): u stores
   0000000000000000 (the empty register), d 0000000000000007, s dies
   with SIGILL ILL_ILLOPN at parse's std (0x12878: the diagnostic value
   spoil_low left in main's %b[2] is still there at the next call), q
   stores 0afafafa0afafafa (the gained %dr13 was emptied: the
   diagnostic 4afafafa with bit 30 cleared, as tagempty.c n shows for
   other values), f SIGSEGV.  Elbrus-8C2 (OS Elbrus 9.5.5, the same
   binary, 2026-10-24): u 0000000000000000, d 0000000000000007, s
   SIGILL, q 0000000000000000 (the 8C2 empties a gained register to 0,
   tagempty.c), f SIGSEGV.  Not run on the 2C3 (unreachable that day).

   MCST's qemu-e2k 11.0.1 (release 1.2, 35b8c88ff78), elbrus-v3: u and
   q never finish (the std's wide instruction is retried in slow mode
   for ever: -d exec,nochain repeats one block, flags 0x9, slow but not
   exception mode), d stores 7, s stops with "exception handling is
   unimplemented" at 0x12878, f never finishes (0x100000000000 is past
   the 1 GiB guest area qemu reserves by default).  With -e2k-opt
   no-rf-clean: u stores a stale value (000000001e493f00), q stops with
   "exception handling is unimplemented" at get_high's std (the stale
   diagnostic value is kept), s and f as without it.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

union u { long l; double d; };

void spoil_low(void *);
void spoil_high(void *);
void get_high(long *);

static inline int lex(union u *lv, int c)
{
	if (c > 0) {
		lv->l = c * 7L;
		return 1;
	}
	return 0;
}

__attribute__((noipa)) void step(void) { }

__attribute__((noipa)) int parse(int c, union u *stack)
{
	union u yylval;
	int t = lex(&yylval, c);

	step();
	stack[1] = yylval;
	return t;
}

int main(int argc, char **argv)
{
	union u stack[2];
	long out = 0x1111111111111111L;
	int t = -1;

	memset(stack, 0x11, sizeof stack);
	if (argc < 2)
		return 2;
	switch (argv[1][0]) {
	case 'u': t = parse(0, stack); break;
	case 'd': t = parse(1, stack); break;
	case 's': spoil_low((void *)8); t = parse(0, stack); break;
	case 'q':
		spoil_high((void *)8);
		get_high(&out);
		printf("q: stored %016lx\n", out);
		return 0;
	case 'f':
		*(volatile long *)0x100000000000UL = 1;
		puts("f: stored");
		return 0;
	default: return 2;
	}
	printf("%c: parse %d, stored %016lx\n", argv[1][0], t, stack[1].l);
	return 0;
}
